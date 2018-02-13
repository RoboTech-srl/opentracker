
void gps_init() {
  debug_print(F("gps_init() started"));

#ifdef PIN_STANDBY_GPS
  pinMode(PIN_STANDBY_GPS, OUTPUT);
  digitalWrite(PIN_STANDBY_GPS, LOW);
#endif

  pinMode(PIN_RESET_GPS, OUTPUT);
  digitalWrite(PIN_RESET_GPS, LOW);

  gps_open();

  debug_print(F("gps_init() completed"));
}

void gps_open() {
#if MODEM_BG96
  gps_port.begin(115200);
#else
  gps_port.begin(9600);
#endif
}

void gps_close() {
  gps_port.end();
}

void gps_setup() {
  debug_print(F("gps_setup() started"));

  gps_on();
  gps_wakeup();
  
  // read the first 4 lines within timeout
  unsigned long t = millis();
  for(int i=0; i<4; ++i) {
    int c = -1;
    while (c != '\n' && (millis() - t < 5000))
      if(gps_port.available()) {
        c = gps_port.read();
        if(DEBUG)
          debug_port.write(c);
      }
  }
  if(millis() - t > 5000) debug_print(F("GPS not responding"));
  
  debug_print(F("gps_setup() completed"));
}

void gps_on() {
  //turn off GPS
  debug_print(F("gps_on() started"));

  delay(100);
  digitalWrite(PIN_RESET_GPS, LOW);

  debug_print(F("gps_on() completed"));
}

void gps_off() {
  //turn off GPS
  debug_print(F("gps_off() started"));

  digitalWrite(PIN_RESET_GPS, HIGH);
  delay(100);

  debug_print(F("gps_off() completed"));
}

void gps_standby() {
  // standby GPS
  gps_port.print("$PMTK161,0*28\r\n");
}

void gps_wakeup() {
  // exit GPS standby
  gps_port.print("\r\n");
}

//collect GPS data from serial port
void collect_gps_data() {
  int fix = 0;

  char tmp[15];

  float flat, flon;
  unsigned long age_pos, age_time, time_gps, date_gps;
  unsigned long chars;
  unsigned short sentences, failed_checksum;

  long timer = millis();

  // drain receive buffer (discard old data)
  while (gps_port.available() && (signed long)(millis() - timer) < GPS_COLLECT_TIMEOUT * 1000)
    gps_port.read();

  // use local variable to reset old data
  TinyGPS gps;

  // looking for valid fix
  do {
    while (gps_port.available()) {
      char c = gps_port.read();

      //debug
      #ifdef DEBUG
        debug_port.print(c);
      #endif

      if(fix == 1) { //fix already acquired
        debug_print(F("GPS already available, breaking"));
        break;
      }

      if(gps.encode(c)) {
        // process new gps info here

        // time in hhmmsscc, date in ddmmyy
        gps.get_datetime(&date_gps, &time_gps, &age_time);

        // get latitude and longitude
        gps.f_get_position(&flat, &flon, &age_pos);

        // check if timestamp and position are current (not from previous attempts)
        if(age_time == TinyGPS::GPS_INVALID_AGE || age_time > 1100) {
          debug_print(F("Invalid date/time age, retrying."));
          continue;
        }
        if(age_pos == TinyGPS::GPS_INVALID_AGE || age_pos > 1100) {
          debug_print(F("Invalid position age, retrying."));
          continue;
        }
        //check if this fix is already received
        if((last_time_gps == time_gps) && (last_date_gps == date_gps)) {
          debug_print(F("Warning: this fix date/time already logged, retrying"));
          continue;
        }

#if DATA_INCLUDE_SPEED
        float fkmph = gps.f_speed_kmph(); // speed in km/hr
        
        if(fkmph == TinyGPS::GPS_INVALID_F_SPEED) {
          debug_print(F("Invalid speed, retrying."));
          continue;
        }
#endif
#if DATA_INCLUDE_ALTITUDE
        float falt = gps.f_altitude(); // +/- altitude in meters
        
        if(falt == TinyGPS::GPS_INVALID_F_ALTITUDE) {
          debug_print(F("Invalid altitude, retrying."));
          continue;
        }
#endif
#if DATA_INCLUDE_HEADING
        float fc = gps.f_course(); // course in degrees
        
        if(fc == TinyGPS::GPS_INVALID_F_ANGLE) {
          debug_print(F("Invalid course, retrying."));
          continue;
        }
#endif
#if DATA_INCLUDE_HDOP
        unsigned long hdop = gps.hdop(); //hdop
        
        if(hdop == TinyGPS::GPS_INVALID_HDOP) {
          debug_print(F("Invalid HDOP, retrying."));
          continue;
        }
#endif    
#if DATA_INCLUDE_SATELLITES
        long sats = gps.satellites(); //satellites
        
        if(sats == TinyGPS::GPS_INVALID_SATELLITES) {
          debug_print(F("Invalid satellites, retrying."));
          continue;
        }
#endif      

        debug_print(F("Valid GPS fix received."));
        fix = 1;
        last_fix_gps = millis();

        //update current time var - format 04/12/98,00:35:45+00
        // Add 1000000 to ensure the position of the digits
        ltoa(date_gps + 1000000, tmp, 10);  //1ddmmyy
        time_char[0] = tmp[5];
        time_char[1] = tmp[6];
        time_char[2] = '/';
        time_char[3] = tmp[3];
        time_char[4] = tmp[4];
        time_char[5] = '/';
        time_char[6] = tmp[1];
        time_char[7] = tmp[2];
        time_char[8] = ',';

        // Add 1000000 to ensure the position of the digits
        ltoa(time_gps + 100000000, tmp, 10);  //1hhmmssms
        time_char[9] = tmp[1];
        time_char[10] = tmp[2];
        time_char[11] = ':';
        time_char[12] = tmp[3];
        time_char[13] = tmp[4];
        time_char[14] = ':';
        time_char[15] = tmp[5];
        time_char[16] = tmp[6];
        time_char[17] = '+';
        time_char[18] = '0';
        time_char[19] = '0';
        time_char[20] = '\0';

        debug_print(F("Current time set from GPS time:"));
        debug_print(time_char);

        //set modem time from fresh fix
        gsm_set_time();

        // construct GPS data packet

        if(DATA_INCLUDE_GPS_DATE) {
          data_field_separator(',');
          //converting date to data packet
          ltoa(date_gps + 1000000, tmp, 10);
          data_append_string(tmp + 1);
        }

        if(DATA_INCLUDE_GPS_TIME) {
          data_field_separator(',');
          //time
          ltoa(time_gps + 100000000, tmp, 10);
          data_append_string(tmp + 1);
        }

        if(DATA_INCLUDE_LATITUDE) {
          data_field_separator(',');
          dtostrf(flat,1,6,tmp);
          data_append_string(tmp);
        }

        if(DATA_INCLUDE_LONGITUDE) {
          data_field_separator(',');
          dtostrf(flon,1,6,tmp);
          data_append_string(tmp);
        }
        
        if(DATA_INCLUDE_SPEED) {
          data_field_separator(',');
          dtostrf(fkmph,1,2,tmp);
          data_append_string(tmp);
        }

        if(DATA_INCLUDE_ALTITUDE) {
          data_field_separator(',');
          dtostrf(falt,1,2,tmp);
          data_append_string(tmp);
        }

        if(DATA_INCLUDE_HEADING) {
          data_field_separator(',');
          dtostrf(fc,1,2,tmp);
          data_append_string(tmp);
        }

        if(DATA_INCLUDE_HDOP) {
          data_field_separator(',');
          ltoa(hdop, tmp, 10);
          data_append_string(tmp);
        }

        if(DATA_INCLUDE_SATELLITES) {
          data_field_separator(',');
          ltoa(sats, tmp, 10);
          data_append_string(tmp);
        }

        //save last gps data date/time
        last_time_gps = time_gps;
        last_date_gps = date_gps;

        //save current position
        dtostrf(flat,1,6,lat_current);
        dtostrf(flon,1,6,lon_current);

        blink_got_gps();
      }
    }

    if(fix == 1) {
      //fix was found
      debug_print(F("collect_gps_data(): fix acquired"));
      addon_event(ON_LOCATION_FIXED);
      break;
    } else {
      // allow some other processing
      addon_delay(5); 
    }
  } while ((signed long)(millis() - timer) < GPS_COLLECT_TIMEOUT * 1000);

  gps.stats(&chars, &sentences, &failed_checksum);
  debug_print(F("Failed checksums:"));
  debug_print(failed_checksum);

  if(fix != 1) {
    debug_print(F("collect_gps_data(): fix not acquired, given up."));
    addon_event(ON_LOCATION_LOST);
  }
}
