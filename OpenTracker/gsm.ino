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

void gsm_init() {
  //setup modem pins
  debug_print(F("gsm_init() started"));

  pinMode(PIN_C_PWR_GSM, OUTPUT);
  digitalWrite(PIN_C_PWR_GSM, LOW);

  pinMode(PIN_C_KILL_GSM, OUTPUT);
  digitalWrite(PIN_C_KILL_GSM, LOW);

  pinMode(PIN_STATUS_GSM, INPUT_PULLUP);
  pinMode(PIN_RING_GSM, INPUT_PULLUP);
  
  pinMode(PIN_WAKE_GSM, OUTPUT); 
  digitalWrite(PIN_WAKE_GSM, HIGH);

  gsm_open();

  debug_print(F("gsm_init() finished"));
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
  debug_print(F("gsm_on() started"));

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
    debug_print(F("gsm_on(): failed auto-baudrate"));

    if (++k >= 5) // max attempts
      break;
      
    gsm_off(0);
    gsm_off(1);

    status_delay(1000);

    debug_print(F("gsm_on(): try again"));
    debug_print(k);
  }

  // make sure it's not sleeping
  gsm_wakeup();

  debug_print(F("gsm_on() finished"));
}

void gsm_off(int emergency) {
  //turn off the modem
  debug_print(F("gsm_off() started"));

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

  debug_print(F("gsm_off() finished"));
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
  debug_print(F("gsm_setup() started"));

  //turn off modem
  gsm_off(1);

  //blink modem restart
  blink_start();

  //turn on modem
  gsm_on();

#if MODEM_BG96
  pinMode(PD0, OUTPUT);
  digitalWrite(PD0, HIGH);
  gsm_port.print("AT+QGPSCFG=\"outport\",\"uartnmea\"\r");
  gsm_wait_for_reply(1,0);
  gsm_port.print("AT+QGPS=1\r");
  gsm_wait_for_reply(1,0);
#endif

  //configure
  gsm_config();

  debug_print(F("gsm_setup() completed"));
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

  //misc GSM startup commands (disable echo)
  gsm_startup_cmd();

  // wait for network availability (max 30s)
  gsm_wait_network_ready(30000);

  // informational only
  gsm_port.print("AT+COPS?\r");
  gsm_wait_for_reply(1,0);

  //set GSM APN
  gsm_set_apn();
}

void gsm_wait_modem_ready(int timeout) {
  // wait for modem ready (attached to network)
  unsigned long t = millis();
  do {
    int pas = gsm_get_modem_status();
    if(pas==0 || pas==3 || pas==4) break;
    status_delay(3000);
  }
  while ((long)(millis() - t) < timeout);
}

void gsm_wait_network_ready(int timeout) {
  // wait for modem ready (attached to network)
  unsigned long t = millis();
  do {
    int reg = gsm_get_network_status();
    if(reg==1 || reg==5 || reg==3) break;
    status_delay(3000);
  }
  while ((long)(millis() - t) < timeout);
}

bool gsm_clock_was_set = false;

void gsm_set_time() {
  debug_print(F("gsm_set_time() started"));

  //setting modems clock from current time var
  gsm_port.print("AT+CCLK=\"");
  gsm_port.print(time_char);
  gsm_port.print("\"\r");

  gsm_wait_for_reply(1,0);
  gsm_clock_was_set = true;

  debug_print(F("gsm_set_time() completed"));
}

void gsm_set_pin() {
  debug_print(F("gsm_set_pin() started"));

  for (int k=0; k<5; ++k) {
    //checking if PIN is set
    gsm_port.print("AT+CPIN?\r");
  
    gsm_wait_for_reply(1,1);
  
    char *tmp = strstr(modem_reply, "SIM PIN");
    if(tmp!=NULL) {
      debug_print(F("gsm_set_pin(): PIN is required"));
  
      //checking if pin is valid one
      if(config.sim_pin[0] == 255) {
        debug_print(F("gsm_set_pin(): PIN is not supplied."));
      } else {
        if(strlen(config.sim_pin) == 4) {
          debug_print(F("gsm_set_pin(): PIN supplied, sending to modem."));
  
          gsm_port.print("AT+CPIN=");
          gsm_port.print(config.sim_pin);
          gsm_port.print("\r");
  
          gsm_wait_for_reply(1,0);
  
          tmp = strstr(modem_reply, "OK");
          if(tmp!=NULL) {
            debug_print(F("gsm_set_pin(): PIN is accepted"));
          } else {
            debug_print(F("gsm_set_pin(): PIN is not accepted"));
          }
          break;
        } else {
          debug_print(F("gsm_set_pin(): PIN supplied, but has invalid length. Not sending to modem."));
          break;
        }
      }
    }
    tmp = strstr(modem_reply, "READY");
    if(tmp!=NULL) {
      debug_print(F("gsm_set_pin(): PIN is not required"));
      break;
    }
    status_delay(2000);
  }
  
  debug_print(F("gsm_set_pin() completed"));
}

void gsm_get_time() {
  debug_print(F("gsm_get_time() started"));

  //clean any serial data

  gsm_get_reply(0);

  //get time from modem
  gsm_port.print("AT+CCLK?\r");

  gsm_wait_for_reply(1,1);

  char *tmp = strstr(modem_reply, "+CCLK: \"");
  tmp += strlen("+CCLK: \"");
  char *tmpval = strtok(tmp, "\"");

  //copy data to main time var
  if (gsm_clock_was_set)
    strlcpy(time_char, tmpval, sizeof(time_char));

  debug_print(F("gsm_get_time() result:"));
  debug_print(time_char);

  debug_print(F("gsm_get_time() completed"));
}

void gsm_startup_cmd() {
  debug_print(F("gsm_startup_cmd() started"));

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

#if GSM_USE_QUECLOCATOR_TIMEOUT > 0
  //set QuectLocator timeout
  gsm_port.print("AT+QLOCCFG=\"timeout\",");
  gsm_port.print(GSM_USE_QUECLOCATOR_TIMEOUT);
  gsm_port.print("\r");

  gsm_wait_for_reply(1,0);
#endif

  debug_print(F("gsm_startup_cmd() completed"));
}

void gsm_get_imei() {
  debug_print(F("gsm_get_imei() started"));

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
  debug_print(F("gsm_get_imei() result:"));
  debug_print(config.imei);

  debug_print(F("gsm_get_imei() completed"));
}

void gsm_get_iccid() {
  debug_print(F("gsm_get_iccid() started"));

  //get modem's imei
  gsm_port.print("AT+CCID\r");

  status_delay(500);
  gsm_get_reply(1);

  //reply data stored to modem_reply
  char *tmp = strstr(modem_reply, "+CCID:");
  if (tmp) {
    char *tmpval = strtok(tmp, "\"");
    if (tmpval)
      tmpval = strtok(NULL, "\"");
      
    //copy data to main IMEI var
    if (tmpval)
      strlcpy(config.iccid, tmpval, sizeof(config.iccid));
  }
  debug_print(F("gsm_get_iccid() result:"));
  debug_print(config.iccid);
  
  debug_print(F("gsm_get_iccid() completed"));
}

int gsm_send_at() {
  debug_print(F("gsm_send_at() started"));

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
  debug_print(F("gsm_send_at() completed"));
  debug_print(ret);
  return ret;
}

int gsm_get_modem_status() {
  debug_print(F("gsm_get_modem_status() started"));

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
  
  debug_print(F("gsm_get_modem_status() returned: "));
  debug_print(pas);
  return pas;
}

int gsm_get_network_status() {
  debug_print(F("gsm_get_network_status() started"));

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
  
  debug_print(F("gsm_get_network_status() returned: "));
  debug_print(reg);
  return reg;
}

int gsm_disconnect() {
  int ret = 0;
  debug_print(F("gsm_disconnect() started"));
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

  debug_print(F("gsm_disconnect() completed"));
  return ret;
}

int gsm_deactivate() {
  // disable data session
  int ret = 0;
  debug_print(F("gsm_deactivate() started"));
  
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
    
  debug_print(F("gsm_deactivate() completed"));
  return ret;
}

int gsm_set_apn()  {
  debug_print(F("gsm_set_apn() started"));

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

  // set google DNS (TODO: make optional)
  gsm_port.print(AT_CONFIGDNS "\"8.8.8.8\"\r");
  gsm_wait_for_reply(1,0);
  if (strstr(modem_reply, "OK") == NULL)
    return 0;

  debug_print(F("gsm_set_apn() completed"));
  return 1;
}

int gsm_get_connection_status() {
  debug_print(F("gsm_get_connection_status() started"));
  
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
      debug_print(ret);
#if MODEM_UG96
      if (ret == 3)
#else
      if (ret == 2)
#endif
        ret = 1; // already connected
      else if (ret > 0)
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
  debug_print(F("gsm_get_connection_status() returned:"));
  debug_print(ret);
  return ret;
}

int gsm_connect() {
  int ret = 0;

  debug_print(F("gsm_connect() started"));

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
        
        gsm_port.print(AT_CONFIGDNS "\"8.8.8.8\"\r");
        gsm_wait_for_reply(1,0);
#endif
      }
      if (ipstat == 0) {
        debug_print(F("Connecting to remote server..."));
        debug_print(i);
    
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
        debug_print(F("Connected to remote server: "));
        debug_print(HOSTNAME);
  
        ret = 1;
        break;
      } else {
        debug_print(F("Can not connect to remote server: "));
        debug_print(HOSTNAME);
        // debug only:
        gsm_port.print("AT+CEER\r");
        gsm_wait_for_reply(1,0);
      }
    }

    addon_delay(2000); // wait 2s before retrying
  }
  debug_print(F("gsm_connect() completed"));
  return ret;
}

int gsm_validate_tcp() {
  int nonacked = 0;
  int ret = 0;

  debug_print(F("gsm_validate_tcp() started."));

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
      debug_print(F("gsm_validate_tcp() data delivered."));
      ret = 1;
      break;
    } else {
      debug_print(F("gsm_validate_tcp() data not yet delivered."));
    }
  }

  debug_print(F("gsm_validate_tcp() completed."));
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
const char HTTP_HEADER0[ ] =        //HTTP header line before GET params
  "GET " URL "?";
const char HTTP_HEADER1[ ] =        //HTTP header line before length
  " HTTP/1.0\r\n"
  "Host: " HOSTNAME "\r\n"
  "Content-length: 0";
#else
const char HTTP_HEADER1[ ] =        //HTTP header line before length
  "POST " URL " HTTP/1.0\r\n"
  "Host: " HOSTNAME "\r\n"
  "Content-type: application/x-www-form-urlencoded\r\n"
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

  debug_print(F("gsm_send_http(): sending current data."));
  debug_print(data_current);

  //getting length of data full package
#ifdef HTTP_USE_GET
  int http_len = strlen(config.imei)+strlen(config.key);
#else
  int http_len = strlen(config.imei)+strlen(config.key)+url_encoded_strlen(data_current);
#endif
  http_len += strlen(HTTP_PARAM_IMEI) + strlen(HTTP_PARAM_KEY) + strlen(HTTP_PARAM_DATA) + 5;    //imei= &key= &d=

  debug_print(F("gsm_send_http(): Length of data packet:"));
  debug_print(http_len);

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
    debug_print(F("gsm_send_http(): call interrupted"));
    return 0; // abort
  }

  debug_print(F("gsm_send_http(): Length of header packet:"));
  debug_print(tmp_len);

  //sending header packet to remote host
  if (!gsm_send_begin(tmp_len)) {
    debug_print(F("gsm_send_http(): send refused"));
    return 0; // abort
  }

  //sending header
#ifdef HTTP_USE_GET
  gsm_port.print(HTTP_HEADER0);

  debug_print(F("gsm_send_http(): Sending GET params"));
  debug_print(F("gsm_send_http(): Sending IMEI and Key"));
  debug_print(config.imei);
  // don't disclose the key

#else
  gsm_port.print(HTTP_HEADER1);
  gsm_port.print(tmp_http_len);
  gsm_port.print(HTTP_HEADER2);

  if (!gsm_send_done()) {
    debug_print(F("gsm_send_http(): send error"));
    return 0; // abort
  }

  //validate header delivery
  gsm_validate_tcp();

  addon_event(ON_SEND_DATA);
  if (gsm_get_modem_status() == 4) {
    debug_print(F("gsm_send_http(): call interrupted"));
    return 0; // abort
  }

  debug_print(F("gsm_send_http(): Sending IMEI and Key"));
  debug_print(config.imei);
  // don't disclose the key

  //sending imei and key first
  if (!gsm_send_begin(13+strlen(config.imei)+strlen(config.key))) {
    debug_print(F("gsm_send_http(): send refused"));
    return 0; // abort
  }
#endif

  gsm_port.print(HTTP_PARAM_IMEI "=");
  gsm_port.print(config.imei);
  gsm_port.print("&" HTTP_PARAM_KEY "=");
  gsm_port.print(config.key);
  gsm_port.print("&" HTTP_PARAM_DATA "=");

  if (!gsm_send_done()) {
    debug_print(F("gsm_send_http(): send error"));
    return 0; // abort
  }

  //validate header delivery
  gsm_validate_tcp();

  debug_print(F("gsm_send_http(): Sending body"));
  int tmp_ret = gsm_send_data_current();
  
#ifdef HTTP_USE_GET
  if (tmp_ret) {
    gsm_validate_tcp();
    
    // finish sending headers
    tmp_len = strlen(HTTP_HEADER1)+strlen(HTTP_HEADER2);
    
    addon_event(ON_SEND_DATA);
    if (gsm_get_modem_status() == 4) {
      debug_print(F("gsm_send_http(): call interrupted"));
      return 0; // abort
    }
    
    debug_print(F("gsm_send_http(): Length of header packet:"));
    debug_print(tmp_len);

    //sending header packet to remote host
    if (!gsm_send_begin(tmp_len)) {
      debug_print(F("gsm_send_http(): send refused"));
      return 0; // abort
    }

    gsm_port.print(HTTP_HEADER1);
    gsm_port.print(HTTP_HEADER2);
  
    if (!gsm_send_done()) {
      debug_print(F("gsm_send_http(): send error"));
      return 0; // abort
    }
  }
#endif
  
  debug_print(F("gsm_send_http(): data sent."));
  return tmp_ret;
}

int gsm_send_data_current() {
  // avoid large buffer on the stack (not reentrant)
  static char buf[PACKET_SIZE];

  debug_print(F("gsm_send_data_current(): sending data."));
  debug_print(data_current);

  int data_len = strlen(data_current);
  int chunk_len = 0;
  int chunk_pos = 0;

  debug_print(F("gsm_send_data_current(): Body packet size:"));
  debug_print(data_len);

  for(int i=0; i<data_len; ) {
    int done = url_encoded_strlcpy(buf, sizeof(buf), &data_current[i]);
    i += done;
    chunk_len = strlen(buf);
    
    addon_event(ON_SEND_DATA);
    if (gsm_get_modem_status() == 4) {
      debug_print(F("gsm_send_data_current(): call interrupted"));
      return 0; // abort
    }

    // start chunk
    debug_print(F("gsm_send_data_current(): Sending data chunk:"));
    debug_print(chunk_pos);

    debug_print(F("gsm_send_data_current(): chunk length:"));
    debug_print(chunk_len);

    //sending chunk
    if (!gsm_send_begin(chunk_len)) {
      debug_print(F("gsm_send_data_current(): send refused"));
      return 0; // abort
    }

    //sending data
    gsm_port.print(buf);
    debug_print(buf);
    
    // end chunk
    if (!gsm_send_done()) {
      debug_print(F("gsm_send_data_current(): send error"));
      return 0; // abort
    }
    
    chunk_pos += chunk_len;

    //validate previous transmission
    gsm_validate_tcp();
  }

  debug_print(F("gsm_send_data_current(): completed"));
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
    debug_print(F("Error, can not send data or no connection."));

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
    debug_print(F("Modem Reply:"));
    debug_print(modem_reply);

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
      debug_print(F("Modem Line:"));
      debug_print(&modem_reply[index]);
      
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
    debug_print(F("Warning: timed out waiting for last modem reply"));
  }

  if(index > 0) {
    debug_print(F("Modem Reply:"));
    debug_print(modem_reply);
  }

  // check that modem is actually alive and sending replies to commands
  if (modem_reply[0] == 0) {
    debug_print(F("Reply failure count:"));
    gsm_reply_failures++;
    debug_print(gsm_reply_failures);
  } else {
    gsm_reply_failures = 0;
  }
  if (GSM_REPLY_FAILURES_REBOOT > 0 && gsm_reply_failures >= GSM_REPLY_FAILURES_REBOOT) {
    reboot(); // reboot immediately
  }
}

int gsm_is_final_result(const char* reply, int allowOK) {
  unsigned int reply_len = strlen(reply);
  // debug_print(allowOK);
  // debug_print(reply_len);
    
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

  debug_print(F("gsm_scan_known_apn() started"));

  //try to connect multiple times
  for (int apn_num = 0; apn_num < KNOWN_APN_COUNT; ++apn_num)
  {
    debug_port.print(F("Testing APN: "));
    debug_print(config.apn);

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
      debug_print(F("gsm_scan_known_apn(): found valid APN settings"));
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

  debug_print(F("gsm_scan_known_apn() completed"));
  return ret;
}

#endif // KNOWN_APN_LIST

int gsm_get_queclocator()
{
#if GSM_USE_QUECLOCATOR_TIMEOUT > 0
  debug_print(F("gsm_get_queclocator() started"));

  gsm_port.print("AT+QCELLLOC=1\r");

  gsm_wait_for_reply(1,1,GSM_USE_QUECLOCATOR_TIMEOUT);

  char* tmp = strstr(modem_reply, "+QCELLLOC:");
  if (tmp == NULL)
    return 0;
  tmp += 10; //strlen("+QCELLLOC:");
  char* lon = strtok(tmp, " ,\r");
  char* lat = strtok(NULL, " ,\r");
  
  if (lon != NULL && lat != NULL)
  {
    strlcpy(lon_current, lon, sizeof(lon_current));
    strlcpy(lat_current, lat, sizeof(lat_current));
    return 1;
  }
#endif
  debug_print(F("gsm_get_queclocator() completed"));
  return 0;
}

#ifdef GSM_USE_NTP_SERVER
void gsm_ntp_update()
{
  debug_print(F("gsm_ntp_update() started"));

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
  
  debug_print(F("gsm_ntp_update() completed"));
}
#endif

