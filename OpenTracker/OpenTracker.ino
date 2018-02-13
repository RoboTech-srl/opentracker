//tracker config
#include "tracker.h"
#include "addon.h"

//External libraries
#include <TinyGPS.h>
#include <DueFlashStorage.h>

//optimization
#define dtostrf(val, width, prec, sout) (void) sprintf(sout, "%" #width "." #prec "f", val)

bool debug_enable = true; // runtime flag to disable debug console

#if DEBUG
#define debug_print(...)  do { if(debug_enable) debug_port.println(__VA_ARGS__); } while(0)
#else
#define debug_print(...)
#endif

#if SEND_RAW
#define collect_data(i)  collect_all_data_raw(i);
#else
#define collect_data(i)  collect_all_data(i);
#endif

// Variables will change:
int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated
long watchdogMillis = 0;        // will store last time modem watchdog was reset
int SEND_DATA = 1;

long time_start, time_stop, time_diff;             //count execution time to trigger interval
int interval_count = 0;         //current interval count (increased on each data collection and reset after sending)
int sms_check_count = 0;        //counter for SMS check (increased on each cycle and reset after check)

char data_current[DATA_LIMIT+2];  //data collected in one go, max 2500 chars
int data_index = 0;             //current data index (where last data record stopped)
char time_char[24];             //time attached to every data line
char modem_reply[200];          //data received from modem, max 200 chars
uint32_t logindex = STORAGE_DATA_START;
bool save_config = 0;           //flag to save config to flash
bool power_reboot = 0;          //flag to reboot everything (used after new settings have been saved)
bool power_cutoff = 0;          //flag to cut-off power to avoid deep-discharge (no more operational afterwards)
bool low_power = 0;             //flag for low power mode

char lat_current[15];
char lon_current[15];

unsigned long last_time_gps = -1, last_date_gps = 0, last_fix_gps = 0;

int engineRunning = -1;
unsigned long engineRunningTime = 0;
unsigned long engine_start;

DueFlashStorage dueFlashStorage;

int gsm_send_failures = 0;
int gsm_reply_failures = 0;

//settings structure
struct settings {
  byte first_run;         //set to 1 after first run
  char apn[64];
  char user[20];
  char pwd[20];
  long interval;          //how often to collect data (milli sec, 600000 - 10 mins)
  int interval_send;      //how many times to collect data before sending (times), sending interval interval*interval_send
  char key[12];           //key for connection, will be sent with every data transmission
  char sim_pin[5];        //PIN for SIM card
  char sms_key[12];       //password for SMS commands
  char imei[20];          //IMEI number
  byte alarm_on;
  char alarm_phone[20];   //alarm phone number
  byte queclocator;       //flag to use QuecLocator fallback when GPS not available
  byte debug;             //flag to enable/disable debug console (USB)
  byte powersave;         //flag to enable/disable low power mode (with engine off)
};

settings config;

//define serial ports
#define gps_port Serial1
#define debug_port SerialUSB
#define gsm_port Serial2

void setup() {
  //common hardware initialization
  device_init();
  
  debug_port.begin(115200);
  delay(2000);

  //initialize GSM and GPS hardware
  gsm_init();
  gps_init();
  
  //initialize addon board hardware
  addon_init();

  //setting debug serial port
  debug_print(F("setup() started"));

  //blink software start
  blink_start();

  settings_load();

  //get current log index
#if STORAGE
  storage_get_index();
#endif

  //GPS setup
  gps_setup();

#if DEBUG == 20
  debug_gps_terminal();
#endif

  //GSM setup
  gsm_setup();

#if DEBUG == 10
  debug_gsm_terminal();
#endif

  // reply to Alarm SMS command
  if(config.alarm_on) {
    sms_send_msg("Alarm Activated", config.alarm_phone);
  }

  // make sure we start with empty data
  data_reset();

#ifdef KNOWN_APN_LIST
  // auto scanning of apn details configuration
  int ap = gsm_scan_known_apn();
  if (ap) {
    save_config = 1; // found good APN, save it as default
  }
#endif

#ifdef GSM_USE_NTP_SERVER
  // attempt clock update (over data connection)
  gsm_ntp_update();
#endif

  // setup addon board functionalities
  addon_setup();

  debug_print(F("setup() completed"));

  // ensure SMS command check at power on or reset
  sms_check();
  
  // apply runtime debug option (USB console) after setup
  if (config.debug == 1)
    usb_console_restore();
  else
    usb_console_disable();
}

void loop() {
  if(save_config == 1) {
    //config should be saved
    settings_save();
    save_config = 0;
  }

  if(power_reboot == 1) {
    //reboot unit
    reboot();
    power_reboot = 0;
  }

  if (power_cutoff) {
    kill_power();
  }

  // Check if ignition is turned on
  int IGNT_STAT = digitalRead(PIN_S_DETECT);
  debug_print(F("Ignition status:"));
  debug_print(IGNT_STAT);

  // detect transitions from engine on and off
  if(IGNT_STAT == 0) {
    if(engineRunning != 0) {
      // engine started
      engine_start = millis();
      engineRunning = 0;

      if(config.alarm_on == 1) {
        sms_send_msg("Ignition ON", config.alarm_phone);
      }
      if(config.powersave == 1) {
        // restore full speed for serial communication
        cpu_full_speed();
        gsm_open();
      }
    }
  } else {
    if(engineRunning != 1) {
      // engine stopped
      if(engineRunning == 0) {
        engineRunningTime += (millis() - engine_start);
        if(config.alarm_on == 1) {
          sms_send_msg("Ignition OFF", config.alarm_phone);
        }
        // force sending last data
        interval_count = config.interval_send;
        collect_data(IGNT_STAT);
        send_data();
      }
      engineRunning = 1;
      // save power when engine is off
      gsm_deactivate(); // ~20mA less
      if(config.powersave == 1) {
        gsm_close();
        cpu_slow_down(); // ~20mA less
      }
    }
  }

  if(!ENGINE_RUNNING_LOG_FAST_AS_POSSIBLE || IGNT_STAT != 0 || !SEND_DATA) {
    time_stop = millis();
  
    //signed difference is good if less than MAX_LONG
    time_diff = time_stop-time_start;
    time_diff = config.interval-time_diff;
  
    debug_print(F("Sleeping for:"));
    debug_print(time_diff);
    debug_print(F("ms"));
  
    if(time_diff < 1000) {
      addon_delay(1000); // minimal wait to let addon code execute
    } else {
      addon_delay(time_diff);
    }
  } else {
    addon_delay(1000); // minimal wait to let addon code execute
  }

  //start counting time
  time_start = millis();

  if(ALWAYS_ON || IGNT_STAT == 0) {
    if(IGNT_STAT == 0) {
      debug_print(F("Ignition is ON!"));
      // Insert here only code that should be processed when Ignition is ON
    }

    //collecting GPS data
    collect_data(IGNT_STAT);
    send_data();

#if SMS_CHECK_INTERVAL_ENGINE_RUNNING > 0
    // perform SMS check
    if (++sms_check_count >= SMS_CHECK_INTERVAL_ENGINE_RUNNING) {
      sms_check_count = 0;

      // facilitate SMS exchange by turning off data session
      gsm_deactivate();
      
      sms_check();
    }
#endif
  } else {
    debug_print(F("Ignition is OFF!"));
    // Insert here only code that should be processed when Ignition is OFF
    
#if SMS_CHECK_INTERVAL_COUNT > 0
    // perform SMS check
    if (++sms_check_count >= SMS_CHECK_INTERVAL_COUNT) {
      sms_check_count = 0;
      
      if(config.powersave == 1) {
        // restore full speed for serial communication
        cpu_full_speed();
        gsm_open();
      }
      
      sms_check();
      
      if(config.powersave == 1) {
        // back to power saving
        gsm_close();
        cpu_slow_down();
      }
    }
#endif
  }
    
  status_led();

  debug_check_input();
  
  addon_loop();
}

void device_init() {
  //setup led pin
  pinMode(PIN_POWER_LED, OUTPUT);
  digitalWrite(PIN_POWER_LED, LOW);

#ifdef PIN_C_REBOOT
  pinMode(PIN_C_REBOOT, OUTPUT);
  digitalWrite(PIN_C_REBOOT, LOW);  //this is required for HW rev 2.3 and earlier
#endif

  //setup ignition detection
  pinMode(PIN_S_DETECT, INPUT);

  // setup relay outputs
  pinMode(PIN_C_OUT_1, OUTPUT);
  digitalWrite(PIN_C_OUT_1, LOW);
  pinMode(PIN_C_OUT_2, OUTPUT);
  digitalWrite(PIN_C_OUT_2, LOW);
}

// when DEBUG is defined >= 2 then serial monitor accepts test commands
void debug_check_input() {
#if DEBUG > 1
#warning "Do not use DEBUG=2 in production code!"

  if(!debug_enable)
    return;
    
  while(debug_port.available()) {
    int c = debug_port.read();
    debug_port.print(F("debug_check_input() got: "));
    debug_port.println((char)c);
    switch (c)
    {
    case 'r':
      reboot();
      break;
    case 'l':
      enter_low_power();
      addon_delay(15000);
      exit_low_power();
      break;
    case 'd':
      storage_dump();
      storage_send_logs(0);
      break;
    case '^':
      debug_gsm_terminal();
      break;
    case '|':
      debug_gps_terminal();
      break;
    }
  }
#endif
}

void debug_gsm_terminal()
{
  debug_port.print(F("Started GSM terminal"));
  for(;;) {
    int c = debug_port.read();
    if (c == '^') break;
    if (c > 0)
      gsm_port.write(c);
    c = gsm_port.read();
    if (c > 0)
      debug_port.write(c);
  }
  debug_port.print(F("Exited GSM terminal"));
}

void debug_gps_terminal()
{
  debug_port.print(F("Started GPS terminal"));
  for(;;) {
    int c = debug_port.read();
    if (c == '|') break;
    if (c > 0)
      gps_port.write(c);
    c = gps_port.read();
    if (c > 0)
      debug_port.write(c);
  }
  debug_port.print(F("Exited GPS terminal"));
}

