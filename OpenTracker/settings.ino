
void settings_load() {
  //load all settings from EEPROM
  byte tmp;

  debug_print(F("settings_load() started"));

  byte* b = dueFlashStorage.readAddress(STORAGE_CONFIG_MAIN); // byte array which is read from flash at adress
  memcpy(&config, b, sizeof(settings)); // copy byte array to temporary struct

  debug_print(F("settings_load(): First run flag:"));
  debug_print(config.first_run);

  if(config.first_run != 1) {
    //first run was not set, this is first even run of the board use config from tracker.h
    debug_print(F("settings_load(): first run, using defaults"));
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
  }

  //setting defaults in case something is incorrect
  debug_print(F("settings_load(): config.interval:"));
  debug_print(config.interval);

  if((config.interval == -1) || (config.interval < 0) || (config.interval > 5184000)) {
    debug_print(F("settings_load(): interval not found, setting default"));
    config.interval = INTERVAL;
    save_config = 1;

    debug_print(F("settings_load(): set config.interval:"));
    debug_print(config.interval);
  }

  //interval send
  debug_print(F("settings_load(): config.interval_send:"));
  debug_print(config.interval_send);

  if((config.interval_send == -1) || (config.interval_send < 0) || (config.interval_send > 100)) {
    debug_print(F("settings_load(): interval_send not found, setting default"));
    config.interval_send = INTERVAL_SEND;
    save_config = 1;

    debug_print(F("settings_load(): set config.interval_send:"));
    debug_print(config.interval_send);
  }

  //powersave
  debug_print(F("settings_load(): config.powersave:"));
  debug_print(config.powersave);

  if((config.powersave != 1) && (config.powersave != 0)) {
    debug_print(F("settings_load(): powersave not found, setting default"));
    config.powersave = POWERSAVE;
    save_config = 1;

    debug_print(F("settings_load(): set config.powersave:"));
    debug_print(config.powersave);
  }

  //powersave
  debug_print(F("settings_load(): config.debug:"));
  debug_print(config.debug);

  if((config.debug != 1) && (config.debug != 0)) {
    debug_print(F("settings_load(): debug not found, setting default"));
    config.debug = DEBUG ? 1 : 0;
    save_config = 1;

    debug_print(F("settings_load(): set config.debug:"));
    debug_print(config.debug);
  }

#if GSM_USE_QUECLOCATOR_TIMEOUT > 0
  //queclocator
  debug_print(F("settings_load(): config.queclocator:"));
  debug_print(config.queclocator);

  if((config.queclocator != 1) && (config.queclocator != 0)) {
    debug_print(F("settings_load(): queclocator not found, setting default"));
    config.queclocator = QUECLOCATOR;
    save_config = 1;

    debug_print(F("settings_load(): set config.queclocator:"));
    debug_print(config.queclocator);
  }
#endif

  tmp = config.key[0];
  if(tmp == 255) { //this check is not sufficient
    debug_print(F("settings_load(): key not found, setting default"));
    strlcpy(config.key, KEY, sizeof(config.key));
    save_config = 1;
  }

  tmp = config.sms_key[0];
  if(tmp == 255) { //this check is not sufficient
    debug_print("settings_load(): SMS key not found, setting default");
    strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
    save_config = 1;
  }

  tmp = config.sim_pin[0];
  if(tmp == 255) { //this check is not sufficient
    debug_print("settings_load(): SIM pin not found, setting default");
    strlcpy(config.sim_pin, SIM_PIN, sizeof(config.sim_pin));
    save_config = 1;
  }

  tmp = config.apn[0];
  if(tmp == 255) {
    debug_print("settings_load(): APN not set, setting default");
    strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
    save_config = 1;
  }

  tmp = config.user[0];
  if(tmp == 255) {
    debug_print("settings_load(): APN user not set, setting default");
    strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
    save_config = 1;
  }

  tmp = config.pwd[0];
  if(tmp == 255) {
    debug_print("settings_load(): APN password not set, setting default");
    strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));
    save_config = 1;
  }

  tmp = config.alarm_phone[0];
  if(tmp == 255) {
    debug_print("settings_load(): Alarm SMS number not set, setting default");
    strlcpy(config.alarm_phone, DEFAULT_ALARM_SMS, sizeof(config.alarm_phone));
    save_config = 1;
  }

  // let addon load its own settings (or initialize defaults)
  if(config.first_run == 1)
    addon_event(ON_SETTINGS_DEFAULT);
  else
    addon_event(ON_SETTINGS_LOAD);

  if (save_config == 1)
    settings_save();

  debug_print(F("settings_load() finished"));
}

void settings_save() {
  debug_print(F("settings_save() started"));

  //save all settings to flash
  dueFlashStorage.write(STORAGE_CONFIG_MAIN, (byte*)&config, sizeof(settings));

  addon_event(ON_SETTINGS_SAVE);
  
  debug_print(F("settings_save() finished"));
}
