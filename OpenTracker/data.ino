//collect and send GPS data for sending

void data_append_char(char c) {
  if (data_index < DATA_LIMIT) {
    data_current[data_index++] = c;
    data_current[data_index] = 0;
  }
}

void data_append_string(const char *str) {
  while (*str != 0 && data_index < DATA_LIMIT)
    data_current[data_index++] = *str++;
  data_current[data_index] = 0;
}

void data_reset() {
  // make sure there is always a string terminator
  memset(data_current, 0, sizeof(data_current));
  data_index = 0;
}

bool data_sep_flag = false;

void data_field_separator(char c) {
  if (data_sep_flag)
    data_append_char(c);
  data_sep_flag = true;
}

void data_field_restart() {
  data_sep_flag = false;
}

// url encoding functions

char to_hex(int nibble) {
  static const char hex[] = "0123456789abcdef";
  return hex[nibble & 15];
}

bool is_url_safe(char c) {
  if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
    || c == '-' || c == '_' || c == '.' || c == '~' || c == '!' || c == '*' || c == '\'' || c == '(' || c == ')')
    return true;
  return false;
}

int url_encoded_strlen(const char* s) {
  int len = strlen(s);
  int ret = 0;
  while (len--) {
    ret += is_url_safe(*s++) ? 1 : 3;
  }
  return ret;
}

// return count of consumed source characters (that fit the buffer after encoding)
int url_encoded_strlcpy(char* dst, int maxlen, const char* src) {
  int len = strlen(src);
  int count = 0;
  while (len > 0 && maxlen > 4) {
    char c = *src++;
    ++count;
    --len;
    if (is_url_safe(c)) {
      *dst++ = c;
      --maxlen;
    } else {
      *dst++ = '%';
      *dst++ = to_hex(c >> 4);
      *dst++ = to_hex(c & 15);
      maxlen -= 3;
    }
  }
  *dst = '\0';
  return count;
}

// read and convert analog input voltage
float analog_input_voltage(int pin, int range)
{
  float sensorValue = analogRead(pin);
  if (range == HIGH)
    sensorValue *= ANALOG_SCALE * ANALOG_VREF / 1024.0f;
  if (range == LOW)
    sensorValue *= ANALOG_SCALE_LOW * ANALOG_VREF / 1024.0f;
  return sensorValue;
}

/**
* This is default collect data function for HTTP
*/
void collect_all_data(int ignitionState) {
  debug_print(F("collect_all_data() started"));

  data_field_restart();
  
  //get current time and add to this data packet
  gsm_get_time();

  //attach time to data packet
  data_append_string(time_char);

  //collect all data
  //indicate start of GPS data packet
  data_append_char('[');

  data_field_restart();

  int idx = data_index;
  collect_gps_data();
  
#if GSM_USE_QUECLOCATOR_TIMEOUT > 0
  if (config.queclocator == 1 && idx == data_index) // no GPS fix available, use QuecLocator if enabled
  {
    if (gsm_get_queclocator())
    {
      debug_print("override missing GPS data with QuecLocator");

      // construct GPS data packet

      if(DATA_INCLUDE_GPS_DATE) {
        data_field_separator(',');
        //converting modem date to GPS format
        data_append_char(time_char[6]);
        data_append_char(time_char[7]);
        data_append_char(time_char[3]);
        data_append_char(time_char[4]);
        data_append_char(time_char[0]);
        data_append_char(time_char[1]);
      }

      if(DATA_INCLUDE_GPS_TIME) {
        data_field_separator(',');
        //converting modem time to GPS format
        data_append_char(time_char[9]);
        data_append_char(time_char[10]);
        data_append_char(time_char[12]);
        data_append_char(time_char[13]);
        data_append_char(time_char[15]);
        data_append_char(time_char[16]);
        data_append_char(time_char[18]);
        data_append_char(time_char[19]);
      }

      if(DATA_INCLUDE_LATITUDE) {
        data_field_separator(',');
        data_append_string(lat_current);
      }

      if(DATA_INCLUDE_LONGITUDE) {
        data_field_separator(',');
        data_append_string(lon_current);
      }

      if(DATA_INCLUDE_SPEED) {
        data_field_separator(',');
      }

      if(DATA_INCLUDE_ALTITUDE) {
        data_field_separator(',');
      }

      if(DATA_INCLUDE_HEADING) {
        data_field_separator(',');
      }

      if(DATA_INCLUDE_HDOP) {
        data_field_separator(',');
      }

      if(DATA_INCLUDE_SATELLITES) {
        data_field_separator(',');
        data_append_string("-1"); //-1 means QuecLocator
      }
    }
  }
#endif

  //indicate stop of GPS data packet
  data_append_char(']');

  data_field_restart();

  // append battery level to data packet
  if(DATA_INCLUDE_BATTERY_LEVEL) {
    data_field_separator(',');
    float sensorValue = analog_input_voltage(AIN_S_INLEVEL, HIGH);
    char batteryLevel[10];
    dtostrf(sensorValue,2,2,batteryLevel);
    data_append_string(batteryLevel);
  }

  // ignition state
  if(DATA_INCLUDE_IGNITION_STATE) {
    data_field_separator(',');
    if(ignitionState == -1) {
      data_append_char('2'); // backup source
    } else if(ignitionState == 0) {
      data_append_char('1');
    } else {
      data_append_char('0');
    }
  }

  // engine running time
  if(DATA_INCLUDE_ENGINE_RUNNING_TIME) {
    unsigned long currentRunningTime = engineRunningTime;
    char runningTimeString[32];

    if(engineRunning == 0) {
      currentRunningTime += (millis() - engine_start);
    }

    snprintf(runningTimeString,32,"%ld",(unsigned long) currentRunningTime / 1000);

    data_field_separator(',');
    data_append_string(runningTimeString);
  }

  addon_collect_data();

  //end of data packet
  data_append_char('\n');

  debug_print(F("collect_all_data() completed"));
}

/**
* This function collects data for RAW TCP
*/
void collect_all_data_raw(int ignitionState) {
  debug_print(F("collect_all_data_raw() started"));

  data_field_restart();

  if(SEND_RAW_INCLUDE_IMEI) {
    data_append_string(config.imei);
  }

  if(SEND_RAW_INCLUDE_KEY) {
    data_field_separator(',');
    data_append_string(config.key);
  }

  if(SEND_RAW_INCLUDE_TIMESTAMP) {
    data_field_separator(',');
    gsm_get_time();
    data_append_string(time_char);
  }

  collect_gps_data();

  // append battery level to data packet
  if(DATA_INCLUDE_BATTERY_LEVEL) {
    data_field_separator(',');
    float sensorValue = analog_input_voltage(AIN_S_INLEVEL, HIGH);
    char batteryLevel[10];
    dtostrf(sensorValue,2,2,batteryLevel);
    data_append_string(batteryLevel);
  }

  // ignition state
  if(DATA_INCLUDE_IGNITION_STATE) {
    data_field_separator(',');
    if(ignitionState == -1) {
      data_append_char('2'); // backup source
    } else if(ignitionState == 0) {
      data_append_char('1');
    } else {
      data_append_char('0');
    }
  }

  // engine running time
  if(DATA_INCLUDE_ENGINE_RUNNING_TIME) {
    data_field_separator(',');

    unsigned long currentRunningTime = engineRunningTime;
    char runningTimeString[32];

    if(engineRunning == 0) {
      currentRunningTime += (millis() - engine_start);
    }

    snprintf(runningTimeString,32,"%ld",(unsigned long) currentRunningTime / 1000);

    data_append_string(runningTimeString);
  }

  //end of data packet
  data_append_char('\n');

  debug_print(F("collect_all_data_raw() completed"));
}

/**
 * This function send collected data using HTTP or TCP
 */
void send_data() {
  debug_print(F("send_data() started"));

  debug_print(F("Current:"));
  debug_print(data_current);

  interval_count++;
  debug_print(F("Data accumulated:"));
  debug_print(interval_count);
  
  // send accumulated data
  if (interval_count >= config.interval_send) {
    // if data send disabled, use storage
    if (!SEND_DATA) {
      debug_print(F("Data send is turned off."));
#if STORAGE
      storage_save_current();   //in case this fails - data is lost
#endif
    } else if (gsm_send_data() != 1) {
      debug_print(F("Could not send data."));
#if STORAGE
      storage_save_current();   //in case this fails - data is lost
#endif
    } else {
      debug_print(F("Data sent successfully."));
#if STORAGE
      //connection seems ok, send saved data history
      storage_send_logs(1); // 0 = dump only, 1 = send data
#endif
    }
    
    //reset current data and counter
    data_reset();
    interval_count -= config.interval_send;
  }
  debug_print(F("send_data() completed"));
}
