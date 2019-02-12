
int settings_load(int readonly) {
  //load all settings from EEPROM
  byte tmp;
  static int defaults = 0;

  DEBUG_FUNCTION_CALL();

  byte* b = dueFlashStorage.readAddress(STORAGE_CONFIG_MAIN); // byte array which is read from flash at adress
  memcpy(&config, b, sizeof(settings)); // copy byte array to temporary struct

  DEBUG_FUNCTION_PRINT("First run flag=");
  DEBUG_PRINTLN(config.first_run);

  if(config.first_run != 1) {
    //first run was not set, this is first even run of the board use config from tracker.h
    DEBUG_FUNCTION_PRINTLN("first run, using defaults");
    config.interval = INTERVAL;
    config.interval_send = INTERVAL_SEND;
    config.powersave = POWERSAVE;
    config.alarm_on =  DEFAULT_ALARM_ON;
    config.queclocator = QUECLOCATOR;
    config.debug = DEBUG ? 1 : 0;

    strlcpy(config.key, KEY, sizeof(config.key));
    strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
    strlcpy(config.sim_pin, SIM_PIN, sizeof(config.sim_pin));
    strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
    strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
    strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));
    strlcpy(config.alarm_phone, DEFAULT_ALARM_SMS, sizeof(config.alarm_phone));

    config.first_run = 1;  //set first run flag
    save_config = 1;
    defaults = 1;
  }

  //setting defaults in case something is incorrect
  DEBUG_FUNCTION_PRINT("config.interval=");
  DEBUG_PRINTLN(config.interval);

  if((config.interval == -1) || (config.interval < 0) || (config.interval > 5184000)) {
    DEBUG_FUNCTION_PRINTLN("interval not found, setting default");
    config.interval = INTERVAL;
    save_config = 1;

    DEBUG_FUNCTION_PRINT("set config.interval=");
    DEBUG_PRINTLN(config.interval);
  }

  //interval send
  DEBUG_FUNCTION_PRINT("config.interval_send=");
  DEBUG_PRINTLN(config.interval_send);

  if((config.interval_send == -1) || (config.interval_send < 0) || (config.interval_send > 100)) {
    DEBUG_FUNCTION_PRINTLN("interval_send not found, setting default");
    config.interval_send = INTERVAL_SEND;
    save_config = 1;

    DEBUG_FUNCTION_PRINT("set config.interval_send=");
    DEBUG_PRINTLN(config.interval_send);
  }

  //powersave
  DEBUG_FUNCTION_PRINT("config.powersave=");
  DEBUG_PRINTLN(config.powersave);

  if((config.powersave != 1) && (config.powersave != 0)) {
    DEBUG_FUNCTION_PRINTLN("powersave not found, setting default");
    config.powersave = POWERSAVE;
    save_config = 1;

    DEBUG_FUNCTION_PRINT("set config.powersave=");
    DEBUG_PRINTLN(config.powersave);
  }

  //powersave
  DEBUG_FUNCTION_PRINT("config.debug=");
  DEBUG_PRINTLN(config.debug);

  if((config.debug != 1) && (config.debug != 0)) {
    DEBUG_FUNCTION_PRINTLN("debug not found, setting default");
    config.debug = DEBUG ? 1 : 0;
    save_config = 1;

    DEBUG_FUNCTION_PRINT("set config.debug=");
    DEBUG_PRINTLN(config.debug);
  }

#if GSM_USE_QUECLOCATOR_TIMEOUT > 0
  //queclocator
  DEBUG_FUNCTION_PRINT("config.queclocator=");
  DEBUG_PRINTLN(config.queclocator);

  if((config.queclocator != 1) && (config.queclocator != 0)) {
    DEBUG_FUNCTION_PRINTLN("queclocator not found, setting default");
    config.queclocator = QUECLOCATOR;
    save_config = 1;

    DEBUG_FUNCTION_PRINT("set config.queclocator=");
    DEBUG_PRINTLN(config.queclocator);
  }
#endif

  tmp = config.key[0];
  if(tmp == 255) { //this check is not sufficient
    DEBUG_FUNCTION_PRINTLN("key not found, setting default");
    strlcpy(config.key, KEY, sizeof(config.key));
    save_config = 1;
  }

  tmp = config.sms_key[0];
  if(tmp == 255) { //this check is not sufficient
    DEBUG_FUNCTION_PRINTLN("SMS key not found, setting default");
    strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
    save_config = 1;
  }

  tmp = config.sim_pin[0];
  if(tmp == 255) { //this check is not sufficient
    DEBUG_FUNCTION_PRINTLN("SIM pin not found, setting default");
    strlcpy(config.sim_pin, SIM_PIN, sizeof(config.sim_pin));
    save_config = 1;
  }

  tmp = config.apn[0];
  if(tmp == 255) {
    DEBUG_FUNCTION_PRINTLN("APN not set, setting default");
    strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
    save_config = 1;
  }

  tmp = config.user[0];
  if(tmp == 255) {
    DEBUG_FUNCTION_PRINTLN("APN user not set, setting default");
    strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
    save_config = 1;
  }

  tmp = config.pwd[0];
  if(tmp == 255) {
    DEBUG_FUNCTION_PRINTLN("APN password not set, setting default");
    strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));
    save_config = 1;
  }

  tmp = config.alarm_phone[0];
  if(tmp == 255) {
    DEBUG_FUNCTION_PRINTLN("Alarm SMS number not set, setting default");
    strlcpy(config.alarm_phone, DEFAULT_ALARM_SMS, sizeof(config.alarm_phone));
    save_config = 1;
  }

  // let addon load its own settings (or initialize defaults)
  if(defaults == 1)
    addon_event(ON_SETTINGS_DEFAULT);
  else
    addon_event(ON_SETTINGS_LOAD);

  if (readonly)
    save_config = 0;
    
  if (save_config == 1)
    settings_save();
  return defaults;
}

int settings_load() {
  return settings_load(0);
}

void settings_save() {
  DEBUG_FUNCTION_CALL();

  //save all settings to flash
  dueFlashStorage.write(STORAGE_CONFIG_MAIN, (byte*)&config, sizeof(settings));

  addon_event(ON_SETTINGS_SAVE);
}

int settings_compare(size_t offset, size_t len) {
  // compare part of the stored settings with the same part in volatile memory
  byte* b = dueFlashStorage.readAddress(STORAGE_CONFIG_MAIN);
  return len > 0 ? memcmp((byte*)&config + offset, b + offset, len) : -2;
}
