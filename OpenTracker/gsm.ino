//gsm functions

#if MODEM_UG96 || MODEM_EG91 || MODEM_BG96
#define MODEM_CMDSET 1
#define AT_CONTEXT "AT+QICSGP=1,1,"
#define AT_ACTIVATE "AT+QIACT=1\r"
#define AT_DEACTIVATE "AT+QIDEACT=1\r"
#define AT_CONFIGDNS "AT+QIDNSCFG=1,"
#define AT_LOCALIP "AT+QIACT?\r"
#define AT_OPEN "AT+QIOPEN=1,0,"
#define AT_CLOSE "AT+QICLOSE=0\r"
#define AT_SEND "AT+QISEND=0,"
#define AT_RECEIVE "AT+QIRD=0,"
#define AT_STAT "AT+QISTATE=1,0\r"
#define AT_QUERYACK "AT+QISEND=0,0\r"
#define AT_ACKRESP "+QISEND: "
#define AT_NTP "AT+QNTP=1,"
#else
#define MODEM_CMDSET 0
#define AT_CONTEXT "AT+QIREGAPP="
#define AT_ACTIVATE "AT+QIACT\r"
#define AT_DEACTIVATE "AT+QIDEACT\r"
#define AT_CONFIGDNS "AT+QIDNSCFG="
#define AT_LOCALIP "AT+QILOCIP\r"
#define AT_OPEN "AT+QIOPEN=0,"
#define AT_CLOSE "AT+QICLOSE=0\r"
#define AT_SEND "AT+QISEND=0,"
#define AT_RECEIVE "AT+QIRD=0,1,0,"
#define AT_STAT "AT+QISTATE\r"
#define AT_QUERYACK "AT+QISACK=0\r"
#define AT_ACKRESP "+QISACK: "
#define AT_NTP "AT+QNTP="
#endif

#define GSM_BAND_900  1
#define GSM_BAND_1800 2
#define GSM_BAND_850  4
#define GSM_BAND_1900 8

#define LTE_BAND_B1   0x1
#define LTE_BAND_B2   0x2
#define LTE_BAND_B3   0x4
#define LTE_BAND_B4   0x8
#define LTE_BAND_B5   0x10
#define LTE_BAND_B8   0x80
#define LTE_BAND_B12  0x800
#define LTE_BAND_B13  0x1000
#define LTE_BAND_B18  0x20000
#define LTE_BAND_B19  0x40000
#define LTE_BAND_B20  0x80000
#define LTE_BAND_B26  0x2000000
#define LTE_BAND_B28  0x8000000
#define LTE_BAND_B39  0x4000000000ull

#define ALL_GSM_BAND    0xF
#define ALL_CATM1_BAND  0x400A0E189Full
#define ALL_CATNB1_BAND 0xA0E189F

void gsm_init() {
  //setup modem pins
  DEBUG_FUNCTION_CALL();

  pinMode(PIN_C_PWR_GSM, OUTPUT);
  digitalWrite(PIN_C_PWR_GSM, LOW);

  pinMode(PIN_C_KILL_GSM, OUTPUT);
  digitalWrite(PIN_C_KILL_GSM, LOW);

  pinMode(PIN_STATUS_GSM, INPUT_PULLUP);
  pinMode(PIN_RING_GSM, INPUT_PULLUP);
  
  pinMode(PIN_WAKE_GSM, OUTPUT); 
  digitalWrite(PIN_WAKE_GSM, HIGH);

  gsm_open();
}

void gsm_open() {
  gsm_port.begin(115200);
}

void gsm_close() {
  gsm_port.end();
}

bool gsm_power_status() {
#if (OPENTRACKER_HW_REV >= 0x0300) || MODEM_UG96
  // inverted status signal
  return digitalRead(PIN_STATUS_GSM) != HIGH;
#else
  // help discharge floating pin, by temporarily setting as output low
  PIO_Configure(
    g_APinDescription[PIN_STATUS_GSM].pPort,
    PIO_OUTPUT_0,
    g_APinDescription[PIN_STATUS_GSM].ulPin,
    g_APinDescription[PIN_STATUS_GSM].ulPinConfiguration);
  pinMode(PIN_STATUS_GSM, INPUT);
  delay(1);
  // read modem power status
  return digitalRead(PIN_STATUS_GSM) != LOW;
#endif
}

void gsm_on() {
  //turn on the modem
  DEBUG_FUNCTION_CALL();

  int k=0;
  for (;;) {
    if(!gsm_power_status()) { // now off, turn on
      unsigned long t = millis();
      digitalWrite(PIN_C_PWR_GSM, HIGH);
      while (!gsm_power_status() && (millis() - t < 1000));
      t = millis();
      digitalWrite(PIN_C_PWR_GSM, LOW);
      while (!gsm_power_status() && (millis() - t < 15000))
        delay(100);
      status_delay(1000);
    }
  
    // auto-baudrate
    if (gsm_send_at())
      break;
    DEBUG_FUNCTION_PRINTLN("failed auto-baudrate");

    if (++k >= 5) // max attempts
      break;
      
    gsm_off(0);
    gsm_off(1);

    status_delay(1000);

    DEBUG_FUNCTION_PRINT("try again ");
    DEBUG_PRINTLN(k);
  }

  // make sure it's not sleeping
  gsm_wakeup();
}

void gsm_off(int emergency) {
  //turn off the modem
  DEBUG_FUNCTION_CALL();

  unsigned long t = millis();

  if(emergency) {
    digitalWrite(PIN_C_KILL_GSM, HIGH);
    while (gsm_power_status() && (millis() - t < 5000))
      delay(100);
    digitalWrite(PIN_C_KILL_GSM, LOW);
    status_delay(1000);
  }
  else
  if(gsm_power_status()) { // now on, turn off
#if MODEM_UG96
    // 3G modem, normal power down
    gsm_port.print("AT+QPOWD=1\r");
    gsm_wait_for_reply(1,0);
#else
    digitalWrite(PIN_C_PWR_GSM, HIGH);
    delay(750);
    digitalWrite(PIN_C_PWR_GSM, LOW);
#endif

    // power-off timeout 12s except EG9x modules which have 30s
    while (gsm_power_status() && (millis() - t < (12500
#if MODEM_EG91
      + 18000
#endif
      )))
      delay(100);
  }
  gsm_get_reply(1);
}

void gsm_standby() {
  // clear wake signal
#if OPENTRACKER_HW_REV >= 0x0300
  digitalWrite(PIN_WAKE_GSM, LOW);
#else
  digitalWrite(PIN_WAKE_GSM, HIGH);
#endif
  // standby GSM
  gsm_port.print("AT+CFUN=4\r");
  gsm_wait_for_reply(1,0);
  gsm_port.print("AT+QSCLK=1\r");
  gsm_wait_for_reply(1,0);
}

void gsm_wakeup() {
  // wake GSM
#if OPENTRACKER_HW_REV >= 0x0300
  digitalWrite(PIN_WAKE_GSM, HIGH);
#else
  digitalWrite(PIN_WAKE_GSM, LOW);
#endif
  delay(1000);
  gsm_port.print("AT+QSCLK=0\r");
  gsm_wait_for_reply(1,0);
  gsm_port.print("AT+CFUN=1\r");
  gsm_wait_for_reply(1,0);
}

void gsm_setup() {
  DEBUG_FUNCTION_CALL();

  //turn off modem
  gsm_off(1);

  //blink modem restart
  blink_start();

  //turn on modem
  gsm_on();

  gsm_port.print("AT+COPS=3,2\r");
  gsm_wait_for_reply(1,0);

#if MODEM_BG96
  // configure GPS receiver inside BG96
  pinMode(PIN_C_ANTON, OUTPUT);
  digitalWrite(PIN_C_ANTON, HIGH);
  delay(10);
  
  gsm_port.print("AT+QGPS=1\r");
  gsm_wait_for_reply(1,0);
  gsm_port.print("AT+QGPSCFG=\"outport\",\"uartnmea\"\r");
  gsm_wait_for_reply(1,0);

  gps_setup();

#ifndef GSM_USE_GSM_BAND
#define GSM_USE_GSM_BAND ALL_GSM_BAND
#endif
#ifndef GSM_USE_CATM1_BAND
#define GSM_USE_CATM1_BAND ALL_CATM1_BAND
#endif
#ifndef GSM_USE_CATNB1_BAND
#define GSM_USE_CATNB1_BAND ALL_CATNB1_BAND
#endif
  gsm_port.print("AT+QCFG=\"band\",");
  gsm_port.print(GSM_USE_GSM_BAND, HEX);
  gsm_port.print(",");
  char tmp[32] = {0};
  snprintf(tmp, sizeof(tmp), "%X%08X", (uint32_t)(GSM_USE_CATM1_BAND>>32), (uint32_t)GSM_USE_CATM1_BAND);
  gsm_port.print(tmp);
  gsm_port.print(",");
  gsm_port.print(GSM_USE_CATNB1_BAND, HEX);
  gsm_port.print(",1\r");
  gsm_wait_for_reply(1,0);

#ifndef GSM_SCAN_LTE_MODE
#define GSM_SCAN_LTE_MODE 2
#endif
  gsm_port.print("AT+QCFG=\"iotopmode\",");
  gsm_port.print(GSM_SCAN_LTE_MODE);
  gsm_port.print(",1\r");
  gsm_wait_for_reply(1,0);

  gsm_set_scanseq();
  
  gsm_set_scanmode();
#endif

  //configure
  gsm_config();
}

void gsm_set_scanmode() {
  DEBUG_FUNCTION_CALL();

  // read scan mode and update if necessary
  gsm_port.print("AT+QCFG=\"nwscanmode\"\r");
  gsm_wait_for_reply(1,1);

  if (strstr(modem_reply, "OK\r") == NULL)
    return; // not supported
    
  char* tmp = strstr(modem_reply, "+QCFG:");
  int scanmode = -1;
  if (tmp != NULL) {
    tmp = strchr(tmp, ',');
    if (tmp != NULL)
      scanmode = atoi(tmp+1);
  }
#if !defined(GSM_SCAN_MODE) || (GSM_SCAN_MODE != 1 && GSM_SCAN_MODE != 3)
#undef GSM_SCAN_MODE
#define GSM_SCAN_MODE 0
#endif
  if (scanmode != GSM_SCAN_MODE) {
    gsm_port.print("AT+QCFG=\"nwscanmode\",");
    gsm_port.print(GSM_SCAN_MODE);
    gsm_port.print(",1\r");
    gsm_wait_for_reply(1,0);
  }
}

void gsm_set_scanseq() {
  DEBUG_FUNCTION_CALL();

  // read scan mode and update if necessary
  gsm_port.print("AT+QCFG=\"nwscanseq\"\r");
  gsm_wait_for_reply(1,1);

  if (strstr(modem_reply, "OK\r") == NULL)
    return; // not supported
    
  char* tmp = strstr(modem_reply, "+QCFG:");
  int scanseq = -1;
  if (tmp != NULL) {
    tmp = strchr(tmp, ',');
    if (tmp != NULL)
      scanseq = strtol(tmp+1, NULL, 8);
  }
#if !defined(GSM_SCAN_SEQUENCE)
#define GSM_SCAN_SEQUENCE 020301
#endif
  if (scanseq != GSM_SCAN_SEQUENCE) {
    gsm_port.print("AT+QCFG=\"nwscanseq\",");
    gsm_port.write('0');
    gsm_port.print((GSM_SCAN_SEQUENCE>>12) & 3);
    gsm_port.write('0');
    gsm_port.print((GSM_SCAN_SEQUENCE>>6) & 3);
    gsm_port.write('0');
    gsm_port.print(GSM_SCAN_SEQUENCE & 3);
    gsm_port.print(",1\r");
    gsm_wait_for_reply(1,0);
  }
}

void gsm_enable_time_sync() {
  // disable time-sync reports
  gsm_port.print("AT+CTZR=0\r");
  gsm_wait_for_reply(1,0);

  // enable GSM time synchronization
#if MODEM_M95
  gsm_port.print("AT+QNITZ=1\r");
  gsm_wait_for_reply(1,0);

  gsm_port.print("AT+CTZU=2\r");
#else
  gsm_port.print("AT+CTZU=1\r");
#endif
  gsm_wait_for_reply(1,0);
}

void gsm_config() {
  //supply PIN code if needed
  gsm_set_pin();

  //get SIM ICCID
  gsm_get_iccid();

  // wait up to 1 minute
  gsm_wait_modem_ready(60000);
  
  //get GSM IMEI
  gsm_get_imei();

  //enable synchronization to GSM time
  gsm_enable_time_sync();

  //misc GSM startup commands (disable echo)
  gsm_startup_cmd();

  // wait for network availability (max 60s)
  gsm_wait_network_ready(GSM_NETWORK_SEARCH_TIMEOUT);

  //synchronize modem clock
  gsm_check_time_sync();
}

void gsm_print_signal_info(int longer) {
  // informational only
  gsm_port.print("AT+CSQ\r");
  gsm_wait_for_reply(1,0);
  if (longer > 0) {
    gsm_port.print("AT+COPS?\r");
    gsm_wait_for_reply(1,0);
#if MODEM_BG96
    gsm_port.print("AT+QNWINFO\r");
    gsm_wait_for_reply(1,0);
#endif
#if MODEM_CMDSET
    gsm_port.print("AT+QENG=\"servingcell\"\r");
    gsm_wait_for_reply(1,0);
#else
    gsm_port.print("AT+QENG=1,0\r");
    gsm_wait_for_reply(1,0);
#endif
  }
  if (longer > 1) {
    gsm_port.print("AT+COPS=?\r");
    gsm_wait_for_reply(1,0);
  }
}

void gsm_wait_modem_ready(int timeout) {
  DEBUG_FUNCTION_CALL();
  
  // wait for modem ready (attached to network)
  unsigned long t = millis();
  do {
    gsm_print_signal_info(0); // debug

    int pas = gsm_get_modem_status();
    if(pas==0 || pas==3 || pas==4) break;
    status_delay(3000);
  }
  while ((long)(millis() - t) < timeout);
}

int gsm_wait_network_ready(int timeout) {
  DEBUG_FUNCTION_CALL();
  
  int reg = -1, ret = 0;
  // wait for modem ready (attached to network)
  unsigned long t = millis();
  do {
    gsm_print_signal_info(1); // debug

#if MODEM_BG96
    reg = gsm_get_eps_status();
    if(reg==1 || reg==5 || reg==3) {
      ret = 1;
      break;
    }
#endif
    reg = gsm_get_gprs_status();
    if(reg==1 || reg==5 || reg==3) {
      ret = 1;
      break;
    }
    status_delay(3000);
  }
  while ((long)(millis() - t) < timeout);
  
  DEBUG_FUNCTION_PRINT("result=");
  DEBUG_PRINTLN(ret);
  return ret;
}

bool gsm_clock_was_set = false;

void gsm_set_time() {
  DEBUG_FUNCTION_CALL();

  //setting modems clock from current time var
  gsm_port.print("AT+CCLK=\"");
  gsm_port.print(time_char);
  gsm_port.print("\"\r");

  gsm_wait_for_reply(1,0);
  gsm_clock_was_set = true;
}

void gsm_check_time_sync() {
  gsm_port.print("AT+QLTS\r");
  gsm_wait_for_reply(1,1);

  // check reply is ok and exclude empty strings
  if (strstr(modem_reply, "OK\r") != NULL && strstr(modem_reply, "+QLTS:") != NULL && strstr(modem_reply, "\"\"") == NULL)
    gsm_clock_was_set = true;

  gsm_get_time();
}

void gsm_set_pin() {
  DEBUG_FUNCTION_CALL();

  for (int k=0; k<5; ++k) {
    //checking if PIN is set
    gsm_port.print("AT+CPIN?\r");
  
    gsm_wait_for_reply(1,1);
  
    char *tmp = strstr(modem_reply, "SIM PIN");
    if(tmp!=NULL) {
      DEBUG_FUNCTION_PRINTLN("PIN is required");
  
      //checking if pin is valid one
      if(config.sim_pin[0] == 255) {
        DEBUG_FUNCTION_PRINTLN("PIN is not supplied.");
      } else {
        if(strlen(config.sim_pin) == 4) {
          DEBUG_FUNCTION_PRINTLN("PIN supplied, sending to modem.");
  
          gsm_port.print("AT+CPIN=");
          gsm_port.print(config.sim_pin);
          gsm_port.print("\r");
  
          gsm_wait_for_reply(1,0);
  
          tmp = strstr(modem_reply, "OK");
          if(tmp!=NULL) {
            DEBUG_FUNCTION_PRINTLN("PIN is accepted");
          } else {
            DEBUG_FUNCTION_PRINTLN("PIN is not accepted");
          }
          break;
        } else {
          DEBUG_FUNCTION_PRINTLN("PIN supplied, but has invalid length. Not sending to modem.");
          break;
        }
      }
    }
    tmp = strstr(modem_reply, "READY");
    if(tmp!=NULL) {
      DEBUG_FUNCTION_PRINTLN("PIN is not required");
      break;
    }
    status_delay(2000);
  }
}

void gsm_get_time() {
  DEBUG_FUNCTION_CALL();
  //copy data to main time var only if valid
  if (gsm_clock_was_set)
    gsm_get_time(time_char);

  if (memcmp(time_char, "19", 2) < 0) {
    time_char[0] = 0;
    gsm_clock_was_set = false;
    DEBUG_FUNCTION_PRINTLN("clock has invalid year!");
  }
}

void gsm_get_time(char buf[20]) {
  DEBUG_FUNCTION_CALL();

  //clean any serial data
  gsm_get_reply(0);

  //get time from modem
  gsm_port.print("AT+CCLK?\r");

  gsm_wait_for_reply(1,1);

  char *tmp = strstr(modem_reply, "+CCLK: ");
  if (tmp)
    tmp = strtok(tmp + 7, "\"\r");
  if (tmp) {
    strlcpy(buf, tmp, 20);

    DEBUG_FUNCTION_PRINT("result=");
    DEBUG_PRINTLN(buf);
  }
}

void gsm_startup_cmd() {
  DEBUG_FUNCTION_CALL();

  //disable echo for TCP data
  gsm_port.print("AT+QISDE=0\r");

  gsm_wait_for_reply(1,0);

#if MODEM_M95
  //set receiving TCP data by command
  gsm_port.print("AT+QINDI=1\r");

  gsm_wait_for_reply(1,0);

  //set multiple socket support
  gsm_port.print("AT+QIMUX=1\r");

  gsm_wait_for_reply(1,0);
#endif

  //set SMS as text format
  gsm_port.print("AT+CMGF=1\r");

  gsm_wait_for_reply(1,0);
}

void gsm_get_imei() {
  DEBUG_FUNCTION_CALL();

  //get modem's imei
  gsm_port.print("AT+GSN\r");

  status_delay(500);
  gsm_get_reply(1);

  //reply data stored to modem_reply
  char *tmp = strstr(modem_reply, "AT+GSN\r\r\n");
  if (tmp) {
    tmp += strlen("AT+GSN\r\r\n");
    char *tmpval = strtok(tmp, "\r");
  
    //copy data to main IMEI var
    if (tmpval)
      strlcpy(config.imei, tmpval, sizeof(config.imei));
  }
  DEBUG_FUNCTION_PRINT("result=");
  DEBUG_PRINTLN(config.imei);
}

void gsm_get_iccid() {
  DEBUG_FUNCTION_CALL();
  config.iccid[0] = 0;
  
  //get modem's imei
  gsm_port.print("AT+CCID\r");

  status_delay(500);
  gsm_get_reply(1);

  //reply data stored to modem_reply
  char *tmp = strstr(modem_reply, "+CCID:");
  if (tmp) {
    char *tmpval = strtok(tmp, "\" ");
    if (tmpval)
      tmpval = strtok(NULL, "\"\r\n");
      
    //copy data to main IMEI var
    if (tmpval)
      strlcpy(config.iccid, tmpval, sizeof(config.iccid));
  }
  DEBUG_FUNCTION_PRINT("result=");
  DEBUG_PRINTLN(config.iccid);
}

int gsm_send_at() {
  DEBUG_FUNCTION_CALL();

  int ret = 0;
  for (int k=0; k<5; ++k) {
    gsm_port.print("ATE1\r");
    status_delay(50);
  
    gsm_get_reply(1);
    ret = (strstr(modem_reply, "ATE1") != NULL)
      && (strstr(modem_reply, "OK") != NULL);
    if (ret) break;

    status_delay(1000);
  }
  DEBUG_FUNCTION_PRINT("returned=");
  DEBUG_PRINTLN(ret);
  return ret;
}

int gsm_get_modem_status() {
  DEBUG_FUNCTION_CALL();

  gsm_port.print("AT+CPAS\r");

  int pas = -1; // unexpected reply
  for (int k=0; k<10; ++k) {
    status_delay(50);
    gsm_get_reply(0);
  
    char *tmp = strstr(modem_reply, "+CPAS:");
    if(tmp!=NULL) {
      pas = atoi(tmp+6);
      break;
    }
  }
  gsm_wait_for_reply(1,0);
  
  DEBUG_FUNCTION_PRINT("returned=");
  DEBUG_PRINTLN(pas);
  return pas;
}

int gsm_get_gprs_status() {
  DEBUG_FUNCTION_CALL();

  gsm_port.print("AT+CGREG?\r");

  int reg = -1; // unexpected reply
  for (int k=0; k<10; ++k) {
    status_delay(50);
    gsm_get_reply(0);
  
    char *tmp = strstr(modem_reply, "+CGREG:");
    if(tmp!=NULL)
      tmp = strchr(tmp+7,',');
    if(tmp!=NULL) {
      reg = atoi(tmp+1);
      break;
    }
  }
  gsm_wait_for_reply(1,0);
  
  DEBUG_FUNCTION_PRINT("returned=");
  DEBUG_PRINTLN(reg);
  return reg;
}

int gsm_get_eps_status() {
  DEBUG_FUNCTION_CALL();

  gsm_port.print("AT+CEREG?\r");

  int reg = -1; // unexpected reply
  for (int k=0; k<10; ++k) {
    status_delay(50);
    gsm_get_reply(0);
  
    char *tmp = strstr(modem_reply, "+CEREG:");
    if(tmp!=NULL)
      tmp = strchr(tmp+7,',');
    if(tmp!=NULL) {
      reg = atoi(tmp+1);
      break;
    }
  }
  gsm_wait_for_reply(1,0);
  
  DEBUG_FUNCTION_PRINT("returned=");
  DEBUG_PRINTLN(reg);
  return reg;
}

int gsm_disconnect() {
  int ret = 0;
  DEBUG_FUNCTION_CALL();
#if GSM_DISCONNECT_AFTER_SEND
  // option to close data session after each server connection
  ret = gsm_deactivate();
#else
  //close connection, if previous attempts left it open
  gsm_port.print(AT_CLOSE);
  gsm_wait_for_reply(MODEM_CMDSET,0);

  //ignore errors (will be taken care during connect)
  ret = 1;
#endif
  return ret;
}

int gsm_deactivate() {
  // disable data session
  int ret = 0;
  DEBUG_FUNCTION_CALL();
  
  //disconnect GSM
  gsm_port.print(AT_DEACTIVATE);
  gsm_wait_for_reply(MODEM_CMDSET,0);

#if MODEM_CMDSET
  //check if result contains OK
  char *tmp = strstr(modem_reply, "OK");
#else
  //check if result contains DEACT OK
  char *tmp = strstr(modem_reply, "DEACT OK");
#endif

  if(tmp!=NULL)
    ret = 1;
    
  return ret;
}

int gsm_set_apn()  {
  DEBUG_FUNCTION_CALL();

  //disconnect GSM
  gsm_port.print(AT_DEACTIVATE);
  gsm_wait_for_reply(MODEM_CMDSET,0);

  addon_event(ON_MODEM_ACTIVATION);
  
  //set all APN data, dns, etc
  gsm_port.print(AT_CONTEXT "\"");
  gsm_port.print(config.apn);
  gsm_port.print("\",\"");
  gsm_port.print(config.user);
  gsm_port.print("\",\"");
  gsm_port.print(config.pwd);
  gsm_port.print("\"");
  gsm_port.print("\r");

  gsm_wait_for_reply(1,0);

#if MODEM_M95
  gsm_port.print("AT+QIDNSIP=1\r");
  gsm_wait_for_reply(1,0);
#endif

  gsm_port.print(AT_ACTIVATE);

  // wait for GPRS contex activation (first time)
  unsigned long t = millis();
  do {
    gsm_wait_for_reply(1,0);
    if(modem_reply[0] != 0) break;
  }
  while (millis() - t < 60000);
  if (strstr(modem_reply, "OK") == NULL)
    return 0;

  // verify we have a local IP address
  gsm_port.print(AT_LOCALIP);
#if MODEM_M95
  status_delay(500);
  gsm_get_reply(1);
  if (strstr(modem_reply, ".") == NULL)
    return 0;
#else
  gsm_wait_for_reply(1,0);
  if (strstr(modem_reply, "OK") == NULL)
    return 0;
#endif
  gsm_send_at();

#ifdef GSM_USE_CUSTOM_DNS
  gsm_port.print(AT_CONFIGDNS "\"" GSM_USE_CUSTOM_DNS "\"\r");
  gsm_wait_for_reply(1,0);
#endif
  if (strstr(modem_reply, "OK") == NULL)
    return 0;

  return 1;
}

int gsm_get_connection_status() {
  DEBUG_FUNCTION_CALL();
  
  int ret = -1; //unknown

  gsm_get_reply(1); //flush buffer
  gsm_port.print(AT_STAT);

  gsm_wait_for_reply(1,0);

#if MODEM_CMDSET
  char *tmp = strtok(modem_reply, ",");
  if (tmp != NULL && strstr(modem_reply, "+QISTATE:") != NULL) {
    for (int k=0; k<5; ++k) {
      tmp = strtok(NULL, ",");
    }
    if (tmp != NULL) {
      ret = atoi(tmp);
      DEBUG_PRINTLN(ret);
#if MODEM_UG96
      if (ret == 3)
#else
      if (ret == 2)
#endif
        ret = 1; // already connected
      else
        ret = 2; // previous connection failed, should close
    }
    
    gsm_wait_for_reply(1,0); // catch OK
  }
  else if (strstr(modem_reply, "OK") != NULL)
    ret = 0; // ready to connect

  // check also data packet connection is active
  
  gsm_get_reply(1); //flush buffer
  gsm_port.print("AT+CGACT?\r");

  gsm_wait_for_reply(1,1);

  tmp = strstr(modem_reply, "+CGACT:");
  if(tmp!=NULL) {
    tmp = strtok(tmp + 7, ",");
    if(tmp!=NULL) {
      tmp = strtok(NULL, ",");
      if(tmp!=NULL) {
        if (atoi(tmp) != 1)
          ret = -2; // force deactivation
      }
    }
  }

#else
  if (strstr(modem_reply, "OK\r\n") != NULL) {
    gsm_wait_for_reply(0,0);
    if (strstr(modem_reply, "IP IND") != NULL ||
      strstr(modem_reply, "PDP DEACT") != NULL) {
      ret = -2; // force deactivation
    }
    // find socket status
    for (int i=0; i<6; ++i) {
      gsm_wait_for_reply(0,0);
  
      if (ret == -1 && strstr(modem_reply, "+QISTATE: 0,") != NULL) {
        if (strstr(modem_reply, "INITIAL") != NULL ||
          strstr(modem_reply, "CLOSE") != NULL)
          ret = 0; // ready to connect
      
        if (strstr(modem_reply, "CONNECTED") != NULL)
          ret = 1; // already connected
      
        if (strstr(modem_reply, "CONNECTING") != NULL)
          ret = 2; // previous connection failed, should close
      }
    }
    gsm_wait_for_reply(1,0); // catch final OK
  }
#endif
  DEBUG_FUNCTION_PRINT("returned=");
  DEBUG_PRINTLN(ret);
  return ret;
}

int gsm_connect() {
  int ret = 0;

  DEBUG_FUNCTION_CALL();

  //try to connect multiple times
  for(int i=0;i<CONNECT_RETRY;i++) {
    // connect only when modem is ready
    if (gsm_get_modem_status() == 0) {
      // check if connected from previous attempts
      int ipstat = gsm_get_connection_status();  

      if (ipstat > 1) {
        //close connection, if previous attempts failed
        gsm_port.print(AT_CLOSE);
        gsm_wait_for_reply(MODEM_CMDSET,0);
        ipstat = 0;
      }
      if (ipstat < 0) {
        //deactivate required
        gsm_port.print(AT_DEACTIVATE);
        gsm_wait_for_reply(MODEM_CMDSET,0);
        ipstat = 0;

#if MODEM_CMDSET
        gsm_port.print(AT_ACTIVATE);
        gsm_wait_for_reply(1,0);
#ifdef GSM_USE_CUSTOM_DNS
        gsm_port.print(AT_CONFIGDNS "\"" GSM_USE_CUSTOM_DNS "\"\r");
        gsm_wait_for_reply(1,0);
#endif
#endif
      }
      if (ipstat == 0) {
        DEBUG_PRINT("Connecting to remote server... ");
        DEBUG_PRINTLN(i);
    
        //open socket connection to remote host
        //opening connection
        gsm_port.print(AT_OPEN "\"");
        gsm_port.print(PROTO);
        gsm_port.print("\",\"");
        gsm_port.print(HOSTNAME);
        gsm_port.print("\",");
#if MODEM_M95
        gsm_port.print("\"");
#endif
        gsm_port.print(HTTP_PORT);
#if MODEM_M95
        gsm_port.print("\"");
#endif
        gsm_port.print("\r");
    
        gsm_wait_for_reply(1, 0); // OK sent first

        long timer = millis();
        if(strstr(modem_reply, "OK")==NULL)
          ipstat = 0;
        else
        do {
          gsm_get_reply(1);

#if MODEM_CMDSET
          char *tmp = strstr(modem_reply, "+QIOPEN: 0,");
          if(tmp!=NULL) {
            tmp += strlen("+QIOPEN: 0,");
            if (atoi(tmp)==0)
              ipstat = 1;
            else
              ipstat = 0;
            break;
          }
#else
          if(strstr(modem_reply, "CONNECT OK")!=NULL) {
            ipstat = 1;
            break;
          }
          if(strstr(modem_reply, "CONNECT FAIL")!=NULL ||
            strstr(modem_reply, "ERROR")!=NULL) {
            ipstat = 0;
            break;
          }
#endif
          addon_delay(100);
        } while (millis() - timer < CONNECT_TIMEOUT);
      }
      
      if(ipstat == 1) {
        DEBUG_PRINT("Connected to remote server: ");
        DEBUG_PRINTLN(HOSTNAME);
  
        ret = 1;
        break;
      } else {
        DEBUG_PRINT("Can not connect to remote server: ");
        DEBUG_PRINTLN(HOSTNAME);
        // debug only:
        gsm_port.print("AT+CEER\r");
        gsm_wait_for_reply(1,0);
        gsm_port.print("AT+QIGETERROR\r");
        gsm_wait_for_reply(1,0);
      }
    }

    addon_delay(2000); // wait 2s before retrying
  }
  return ret;
}

int gsm_validate_tcp() {
  int nonacked = 0;
  int ret = 0;

  DEBUG_FUNCTION_CALL();

  //todo check in the loop if everything delivered
  for(int k=0;k<10;k++) {
    gsm_port.print(AT_QUERYACK);
    gsm_wait_for_reply(1,1);

    //todo check if everything is delivered
    char *tmp = strstr(modem_reply, AT_ACKRESP);
    tmp += strlen(AT_ACKRESP);

    //checking how many bytes NON-acked
    tmp = strtok(tmp, ", \r\n");
    tmp = strtok(NULL, ", \r\n");
    tmp = strtok(NULL, ", \r\n");

    //non-acked value
    nonacked = atoi(tmp);

    if(nonacked <= PACKET_SIZE_DELIVERY) {
      //all data has been delivered to the server , if not wait and check again
      DEBUG_FUNCTION_PRINTLN("data delivered.");
      ret = 1;
      break;
    } else {
      DEBUG_FUNCTION_PRINTLN("data not yet delivered.");
    }
  }

  return ret;
}

int gsm_send_begin(int data_len) {
  //sending header packet to remote host
  gsm_port.print(AT_SEND);
  gsm_port.print(data_len);
  gsm_port.print("\r");

  gsm_wait_for_reply(1,0);
  if (strncmp(modem_reply, "> ", 2) == 0)
    return 1; // accepted, can send data
  return 0; // error, cannot send data
}

int gsm_send_done() {
  gsm_wait_for_reply(1,0);
  if (strncmp(modem_reply, "SEND OK", 7) == 0)
    return 1; // send successful
  return 0; // error
}

#ifdef HTTP_USE_GET
#ifdef HTTP_USE_JSON
#error "Cannot use JSON format with HTTP GET request!"
#endif
const char HTTP_HEADER0[ ] =        //HTTP header line before GET params
  "GET " URL "?";
const char HTTP_HEADER1[ ] =        //HTTP header line before length
  " HTTP/1.1\r\n"
  "Host: " HOSTNAME "\r\n"
  "Content-length: 0";
#else
const char HTTP_HEADER1[ ] =        //HTTP header line before length
  "POST " URL " HTTP/1.1\r\n"
  "Host: " HOSTNAME "\r\n"
#ifdef HTTP_USE_JSON
  "Content-Type:application/json\r\n"
#else
  "Content-type: application/x-www-form-urlencoded\r\n"
#endif
  "Content-length: ";
#endif
const char HTTP_HEADER2[ ] =          //HTTP header line after length
  "\r\n"
  "User-Agent: " HTTP_USER_AGENT "\r\n"
  "Connection: close\r\n"
  "\r\n";

int gsm_send_http_current() {
  //send HTTP request, after connection if fully opened
  //this will send Current data

  DEBUG_FUNCTION_PRINTLN("Sending current data:");
  DEBUG_PRINTLN(data_current);

  //getting length of data full package
  int http_len = strlen(HTTP_PARAM_IMEI) + strlen(HTTP_PARAM_KEY) + strlen(HTTP_PARAM_DATA) + 5;    //imei= &key= &d=
#ifdef HTTP_USE_GET
  http_len += strlen(config.imei)+strlen(config.key);
#else
#ifdef HTTP_USE_JSON
  // override length for JSON format
  http_len = strlen(data_current);
#else
  http_len += strlen(config.imei)+strlen(config.key)+url_encoded_strlen(data_current);
#endif
#endif

  DEBUG_FUNCTION_PRINT("Length of data packet=");
  DEBUG_PRINTLN(http_len);

#ifdef HTTP_USE_GET
  int tmp_len = strlen(HTTP_HEADER0)+http_len;
#else
  //length of header package
  char tmp_http_len[7];
  itoa(http_len, tmp_http_len, 10);

  int tmp_len = strlen(HTTP_HEADER1)+strlen(tmp_http_len)+strlen(HTTP_HEADER2);
#endif

  addon_event(ON_SEND_DATA);
  if (gsm_get_modem_status() == 4) {
    DEBUG_FUNCTION_PRINTLN("call interrupted");
    return 0; // abort
  }

  DEBUG_FUNCTION_PRINT("Length of header packet=");
  DEBUG_PRINTLN(tmp_len);

  //sending header packet to remote host
  if (!gsm_send_begin(tmp_len)) {
    DEBUG_FUNCTION_PRINTLN("send refused");
    return 0; // abort
  }

  //sending header
#ifdef HTTP_USE_GET
  gsm_port.print(HTTP_HEADER0);

  DEBUG_FUNCTION_PRINTLN("Sending GET params");
  DEBUG_FUNCTION_PRINT("Sending IMEI and Key=");
  DEBUG_PRINTLN(config.imei);
  // don't disclose the key

#else
  gsm_port.print(HTTP_HEADER1);
  gsm_port.print(tmp_http_len);
  gsm_port.print(HTTP_HEADER2);

  if (!gsm_send_done()) {
    DEBUG_FUNCTION_PRINTLN("send error");
    return 0; // abort
  }

  //validate header delivery
  gsm_validate_tcp();

  addon_event(ON_SEND_DATA);
  if (gsm_get_modem_status() == 4) {
    DEBUG_FUNCTION_PRINTLN("call interrupted");
    return 0; // abort
  }

#ifndef HTTP_USE_JSON
  DEBUG_FUNCTION_PRINT("Sending IMEI and Key=");
  DEBUG_PRINTLN(config.imei);
  // don't disclose the key
  //sending imei and key first
  if (!gsm_send_begin(13+strlen(config.imei)+strlen(config.key))) {
    DEBUG_FUNCTION_PRINT("send refused");
    return 0; // abort
  }
#endif
#endif

#ifndef HTTP_USE_JSON
  gsm_port.print(HTTP_PARAM_IMEI "=");
  gsm_port.print(config.imei);
  gsm_port.print("&" HTTP_PARAM_KEY "=");
  gsm_port.print(config.key);
  gsm_port.print("&" HTTP_PARAM_DATA "=");

  if (!gsm_send_done()) {
    DEBUG_FUNCTION_PRINTLN("send error");
    return 0; // abort
  }

  //validate header delivery
  gsm_validate_tcp();
#endif

  DEBUG_FUNCTION_PRINTLN("Sending body");
  int tmp_ret = gsm_send_data_current();
  
#ifdef HTTP_USE_GET
  if (tmp_ret) {
    gsm_validate_tcp();
    
    // finish sending headers
    tmp_len = strlen(HTTP_HEADER1)+strlen(HTTP_HEADER2);
    
    addon_event(ON_SEND_DATA);
    if (gsm_get_modem_status() == 4) {
      DEBUG_FUNCTION_PRINTLN("call interrupted");
      return 0; // abort
    }
    
    DEBUG_FUNCTION_PRINT("Length of header packet=");
    DEBUG_PRINTLN(tmp_len);

    //sending header packet to remote host
    if (!gsm_send_begin(tmp_len)) {
      DEBUG_FUNCTION_PRINTLN("send refused");
      return 0; // abort
    }

    gsm_port.print(HTTP_HEADER1);
    gsm_port.print(HTTP_HEADER2);
  
    if (!gsm_send_done()) {
      DEBUG_FUNCTION_PRINTLN("send error");
      return 0; // abort
    }
  }
#endif
  
  DEBUG_FUNCTION_PRINTLN("data sent.");
  return tmp_ret;
}

int gsm_send_data_current() {
  // avoid large buffer on the stack (not reentrant)
  static char buf[PACKET_SIZE];

  DEBUG_FUNCTION_PRINTLN("sending data:");
  DEBUG_PRINTLN(data_current);

  int data_len = strlen(data_current);
  int chunk_len = 0;
  int chunk_pos = 0;

  DEBUG_FUNCTION_PRINT("Body packet size=");
  DEBUG_PRINTLN(data_len);

  for(int i=0; i<data_len; ) {
#ifdef HTTP_USE_JSON
    int done = strlcpy(buf, &data_current[i], sizeof(buf));
#else
    int done = url_encoded_strlcpy(buf, sizeof(buf), &data_current[i]);
#endif
    i += done;
    chunk_len = strlen(buf);
    
    addon_event(ON_SEND_DATA);
    if (gsm_get_modem_status() == 4) {
      DEBUG_FUNCTION_PRINTLN("call interrupted");
      return 0; // abort
    }

    // start chunk
    DEBUG_FUNCTION_PRINT("Sending chunk pos=");
    DEBUG_PRINTLN(chunk_pos);

    DEBUG_FUNCTION_PRINT("chunk length=");
    DEBUG_PRINTLN(chunk_len);

    //sending chunk
    if (!gsm_send_begin(chunk_len)) {
      DEBUG_FUNCTION_PRINTLN("send refused");
      return 0; // abort
    }

    //sending data
    gsm_port.print(buf);
    DEBUG_PRINTLN(buf);
    
    // end chunk
    if (!gsm_send_done()) {
      DEBUG_FUNCTION_PRINTLN("send error");
      return 0; // abort
    }
    
    chunk_pos += chunk_len;

    //validate previous transmission
    gsm_validate_tcp();
  }

  DEBUG_FUNCTION_PRINTLN("send completed");
  return 1;
}

int gsm_send_data() {
  int ret_tmp = 0;

  //send 2 ATs
  gsm_send_at();

  //make sure there is no connection
  gsm_disconnect();

  addon_event(ON_SEND_STARTED);
    
  //opening connection
  ret_tmp = gsm_connect();
  if(ret_tmp == 1) {
    //connection opened, just send data
#if SEND_RAW
      ret_tmp = gsm_send_data_current();
#else
      // send data, if ok then parse reply
      ret_tmp = gsm_send_http_current() && parse_receive_reply();
#endif
  }
  gsm_disconnect(); // always
  
  if(ret_tmp) {
    gsm_send_failures = 0;

    addon_event(ON_SEND_COMPLETED);
  } else {
    DEBUG_PRINT("Error, can not send data or no connection.");

    gsm_send_failures++;
    addon_event(ON_SEND_FAILED);
  }

  if(GSM_SEND_FAILURES_REBOOT > 0 && gsm_send_failures >= GSM_SEND_FAILURES_REBOOT) {
    power_reboot = 1;
  }

  return ret_tmp;
}

// update and return index to modem_reply buffer
int gsm_read_line(int index = 0) {
  char inChar = 0; // Where to store the character read
  long last = millis();

  do {
    if(gsm_port.available()) {
      inChar = gsm_port.read(); // always read if available
      last = millis();
      if(index < (int)sizeof(modem_reply)-1) { // One less than the size of the array
        modem_reply[index] = inChar; // Store it
        index++; // Increment where to write next
  
        if(index == sizeof(modem_reply)-1 || (inChar == '\n')) { //some data still available, keep it in serial buffer
          break;
        }
      }
    }
  } while(gsm_port.available() || (signed long)(millis() - last) < 10); // allow some inter-character delay
  
  if (index > 0 && (inChar == '\r') && index < (int)sizeof(modem_reply)-1) {
    modem_reply[index] = '\n'; // sometimes newline is missing, fix it
    ++index;
  }
  modem_reply[index] = '\0'; // Null terminate the string
  return index;
}

// use fullBuffer != 0 if you want to read multiple lines
void gsm_get_reply(int fullBuffer) {
  //get reply from the modem
  int index = 0, end = 0;

  do {
    end = gsm_read_line(index);
    if (end > index)
      index = end;
    else
      break;
  } while(fullBuffer && index < (int)sizeof(modem_reply)-1);
  
  if(index > 0) {
    DEBUG_PRINT("Modem Reply:");
    DEBUG_PRINTLN(modem_reply);

    addon_event(ON_MODEM_REPLY);
  }
}

// use allowOK = 0 if OK comes before the end of the modem reply
void gsm_wait_for_reply(int allowOK, int fullBuffer) {
  gsm_wait_for_reply(allowOK, fullBuffer, GSM_MODEM_COMMAND_TIMEOUT);
}

void gsm_wait_for_reply(int allowOK, int fullBuffer, int maxseconds) {
  unsigned long timeout = millis();
  
  modem_reply[0] = '\0';
  int ret = 0;

  //get reply from the modem
  int index = 0, end = 0;

  do {
    if (fullBuffer) //keep past lines
      index = end;
    else // overwrite
      index = 0;
    end = gsm_read_line(index);
  
    if(end > index) {
      DEBUG_PRINT("Modem Line:");
      DEBUG_PRINTLN(&modem_reply[index]);
      
      addon_event(ON_MODEM_REPLY);
  
      if (gsm_is_final_result(&modem_reply[index], allowOK)) {
        ret = 1;
        break;
      }
    } else if ((signed long)(millis() - timeout) > (maxseconds * 1000)) {
      break;
    } else {
      status_delay(50);
    }
  } while(index < (int)sizeof(modem_reply)-1);
  
  if (ret == 0) {
    DEBUG_PRINT("Warning: timed out waiting for last modem reply");
  }

  if(index > 0) {
    DEBUG_PRINT("Modem Reply:");
    DEBUG_PRINTLN(modem_reply);
  }

  // check that modem is actually alive and sending replies to commands
  if (modem_reply[0] == 0) {
    DEBUG_PRINT("Reply failure count:");
    gsm_reply_failures++;
    DEBUG_PRINTLN(gsm_reply_failures);
  } else {
    gsm_reply_failures = 0;
  }
  if (GSM_REPLY_FAILURES_REBOOT > 0 && gsm_reply_failures >= GSM_REPLY_FAILURES_REBOOT) {
    reboot(); // reboot immediately
  }
}

int gsm_is_final_result(const char* reply, int allowOK) {
  unsigned int reply_len = strlen(reply);
  // DEBUG_PRINTLN(allowOK);
  // DEBUG_PRINTLN(reply_len);
    
  #define STARTS_WITH(b) ( reply_len >= strlen(b) && strncmp(reply, (b), strlen(b)) == 0)
  #define ENDS_WITH(b) ( reply_len >= strlen(b) && strcmp(reply + reply_len - strlen(b), (b)) == 0)
  #define CONTAINS(b) ( reply_len >= strlen(b) && strstr(reply, (b)) != NULL)
  
  if(allowOK && ENDS_WITH("\r\nOK\r\n")) {
    return true;
  }
  if(allowOK && STARTS_WITH("OK\r\n")) {
    return true;
  }
  if(STARTS_WITH("+CME ERROR:")) {
    return true;
  }
  if(STARTS_WITH("+CMS ERROR:")) {
    return true;
  }
  if(STARTS_WITH("+QIRD:")) {
    return true;
  }
  if(STARTS_WITH("+QISTATE: ")) {
    return true;
  }
  if(STARTS_WITH("> ")) {
    return true;
  }
  if(STARTS_WITH("ALREADY CONNECT\r\n")) {
    return true;
  }
  if(STARTS_WITH("BUSY\r\n")) {
    return true;
  }
  if(STARTS_WITH("CONNECT\r\n")) {
    return true;
  }
  if(ENDS_WITH("CONNECT OK\r\n")) {
    return true;
  }
  if(ENDS_WITH("CONNECT FAIL\r\n")) {
    return true;
  }
  if(STARTS_WITH("CLOSED\r\n")) {
    return true;
  }
  if(ENDS_WITH("CLOSE OK\r\n")) {
    return true;
  }
  if(STARTS_WITH("DEACT OK\r\n")) {
    return true;
  }
  if(STARTS_WITH("ERROR")) {
    return true;
  }
  if(STARTS_WITH("NO ANSWER\r\n")) {
    return true;
  }
  if(STARTS_WITH("NO CARRIER\r\n")) {
    return true;
  }
  if(STARTS_WITH("NO DIALTONE\r\n")) {
    return true;
  }
  if(STARTS_WITH("SEND OK\r\n")) {
    return true;
  }
  if(STARTS_WITH("SEND FAIL\r\n")) {
    return true;
  }
  if(STARTS_WITH("STATE: ")) {
    return true;
  }
  return false;
}

void gsm_debug() {
  gsm_port.print("AT+QLOCKF=?\r");
  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+QBAND?\r");
  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+CGMR\r");
  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+CGMM\r");
  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+CGSN\r");
  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+CREG?\r");

  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+CSQ\r");

  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+QENG?\r");

  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+COPS?\r");

  status_delay(2000);
  gsm_get_reply(0);

  gsm_port.print("AT+COPS=?\r");

  status_delay(6000);
  gsm_get_reply(0);
}

#ifdef KNOWN_APN_LIST

#if KNOWN_APN_SCAN_MODE < 0 || KNOWN_APN_SCAN_MODE > 2
#error Invalid option KNOWN_APN_SCAN_MODE in tracker.h
#endif

// Use automatic APN configuration
int gsm_scan_known_apn()
{
  typedef struct
  {
    const char *apnname;
    const char *user;
    const char *pass;
    //const char *servicename;  // not required, for further expansion and requirement
    //const char *value_str;  // not required, for further expansion and requirement
  } APNSET;

  static char apn[sizeof(config.apn)];
  static char user[sizeof(config.user)];
  static char pwd[sizeof(config.pwd)];

  strlcpy(apn, config.apn, sizeof(config.apn));
  strlcpy(user, config.user, sizeof(config.user));
  strlcpy(pwd, config.pwd, sizeof(config.pwd));
  
  #define KNOWN_APN(apn,usr,pwd,isp,nul) { apn, usr, pwd/*, isp, nul*/ },
  static const APNSET apnlist[] =
  {
    KNOWN_APN_LIST
    // last element must be the current APN config
    KNOWN_APN(apn, user, pwd, "", NULL)
  };
  #undef KNOWN_APN
  
  enum { KNOWN_APN_COUNT = sizeof(apnlist)/sizeof(*apnlist) };
  
  int ret = 0;

  DEBUG_FUNCTION_CALL();

  //try to connect multiple times
  for (int apn_num = 0; apn_num < KNOWN_APN_COUNT; ++apn_num)
  {
    DEBUG_PRINT("Testing APN: ");
    DEBUG_PRINTLN(config.apn);

#if KNOWN_APN_SCAN_MODE < 2
    if (gsm_get_connection_status() >= 0)
#endif
    {
#if KNOWN_APN_SCAN_MODE > 0
      data_current[0] = 0;
      ret = gsm_send_data();
#else
      ret = 1;
#endif
    }
    if (ret == 1) {
      DEBUG_FUNCTION_PRINTLN("found valid APN settings");
      break;
    }
    
    // try next APN on the list
    strlcpy(config.apn, apnlist[apn_num].apnname, sizeof(config.apn));
    strlcpy(config.user, apnlist[apn_num].user, sizeof(config.user));
    strlcpy(config.pwd, apnlist[apn_num].pass, sizeof(config.pwd));

#if KNOWN_APN_SCAN_USE_RESET
    //restart GSM with new config
    gsm_off(0);
    gsm_setup();
#else
    // apply new config
    gsm_set_apn();
#endif    
  }
  // the last APN in the array is not tested and it's applied only as default

  return ret;
}

#endif // KNOWN_APN_LIST

#ifdef GSM_USE_NTP_SERVER
void gsm_ntp_update()
{
  if (gsm_clock_was_set) {
    DEBUG_FUNCTION_PRINTLN("skipped");
    return;
  }
  DEBUG_FUNCTION_CALL();

  gsm_port.print(AT_NTP "\"");
  gsm_port.print(GSM_USE_NTP_SERVER);
  gsm_port.print("\"\r");

  gsm_wait_for_reply(1,0); // wait OK

  // NOTE: default timeout does not guarantee we get result here
  // but receiving it asynchronously later should not be a problem
  long last = millis();
  while (!gsm_port.available() && (signed long)(millis() - last) < GSM_NTP_COMMAND_TIMEOUT * 1000)
   status_led();
  
  gsm_get_reply(1);
  
  char* tmp = strstr(modem_reply, "+QNTP:");
  if (tmp != NULL)
    tmp = strtok(tmp + 6, " \r");
  if (tmp != NULL && *tmp == '0')
    gsm_clock_was_set = true;
}
#endif
