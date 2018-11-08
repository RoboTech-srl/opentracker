
void reboot() {
  debug_print(F("reboot() started"));

  //reset GPS
  gps_off();
  //emergency power off GSM
  gsm_off(1);

#if defined(_SAM3XA_)
  //disable USB to allow reboot
  //serial monitor on the PC won't work anymore if you don't close it before reset completes
  //otherwise, close the serial monitor, detach the USB cable and connect it again

  // debug_port.end() does nothing, manually disable USB
  UDD_Detach(); // detach from Host

  cpu_irq_disable();
  rstc_start_software_reset(RSTC);
  for (;;)
  {
    // If we do not keep the watchdog happy and it times out during this wait,
    // the reset reason will be wrong when the board starts the next time around.
    WDT_Restart(WDT);
  }
#else
  __disable_irq();
  NVIC_SystemReset();
#endif
}

void usb_console_disable() {
#if defined(_SAM3XA_)
  cpu_irq_disable();
  
  // debug_port.end() does nothing, manually disable USB serial console
  UDD_Detach(); // detach from Host
  // de-init procedure (reverses UDD_Init)
  otg_freeze_clock();
  otg_disable_pad();
  otg_disable();
  pmc_disable_udpck();
  pmc_disable_upll_clock();
  pmc_disable_periph_clk(ID_UOTGHS);
  NVIC_DisableIRQ((IRQn_Type) ID_UOTGHS);
  NVIC_ClearPendingIRQ((IRQn_Type) ID_UOTGHS);

  cpu_irq_enable();
#else
  usbd_interface_deinit();
#endif
}

void usb_console_restore() {
#if defined(_SAM3XA_)
  if (!Is_otg_enabled()) {
    // re-initialize USB
    UDD_Init();
    UDD_Attach();
  }
#else
  usbd_interface_init();
#endif
}

// override for lower power consumption (wait for interrupt)
extern "C" void yield(void) {
#if defined(INC_FREERTOS_H)
#if ((INCLUDE_xTaskGetSchedulerState == 1) || (configUSE_TIMERS == 1))
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
#endif
  {
    vTaskDelay(1); // not using taskYIELD() because lower priority tasks would not run otherwise!
    return;
  }
#endif
#if defined(_SAM3XA_)
  pmc_enable_sleepmode(0);
#else
  __WFI();
#endif
}

void cpu_slow_down() {
  addon_event(ON_CLOCK_PAUSE);

#if defined(_SAM3XA_)
  SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // temp disable interrupt
  
  // slow down CPU
  pmc_mck_set_prescaler(PMC_MCKR_PRES_CLK_64); // master clock prescaler

  // update timer settings
  SystemCoreClockUpdate();
  SysTick_Config(SystemCoreClock / 1000);
#endif

  addon_event(ON_CLOCK_RESUME);
}

void cpu_full_speed() {
  addon_event(ON_CLOCK_PAUSE);

#if defined(_SAM3XA_)
  // re-init clocks to full speed
  SystemInit();
  SysTick_Config(SystemCoreClock / 1000);
#endif

  addon_event(ON_CLOCK_RESUME);
}

void enter_low_power() {
  debug_print(F("enter_low_power() started"));

  addon_event(ON_DEVICE_STANDBY);

  // enter standby/sleep mode

  gps_standby();
  gps_close();

  gsm_standby();
  gsm_close();

  cpu_slow_down();
  
  debug_print(F("enter_low_power() completed"));
}

void exit_low_power() {
  debug_print(F("exit_low_power() started"));

  cpu_full_speed();
  
  // enable serial ports
  gsm_open();
  gsm_wakeup();

  gps_open();
  gps_wakeup();

  addon_event(ON_DEVICE_WAKEUP);

  debug_print(F("exit_low_power() completed"));
}

void kill_power() {
  debug_print(F("kill_power() called"));
  addon_event(ON_DEVICE_KILL);
  // save as much power as possible
  gps_off();
  gps_close();
  gsm_off(1);
  gsm_close();
  usb_console_disable();
#if defined(_SAM3XA_)
  // slow down cpu even more
  pmc_switch_mainck_to_fastrc(CKGR_MOR_MOSCRCF_4_MHz);
  cpu_slow_down();
  cpu_irq_disable();
#endif
  // turn off LED and CAN
  digitalWrite(PIN_POWER_LED, HIGH);
  pinMode(PIN_CAN_RS, OUTPUT);
  digitalWrite(PIN_CAN_RS, HIGH);
#if defined(_SAM3XA_)
  // disable peripherals and use wait mode
  pmc_set_writeprotect(0);
  pmc_disable_all_periph_clk();
  pmc_enable_waitmode();
#endif
#if (OPENTRACKER_HW_REV >= 0x0300)
  RTC_StartAlarm(0, 0, 0, 0, 0, AM, OFF_MSK);
  RTC_StopAlarm();
  RTC_SetOutput(RTC_OUTPUT_ALARMA, RTC_OUTPUT_POLARITY_LOW, RTC_OUTPUT_TYPE_PUSHPULL);
#endif
  for(;;) // freeze in low power mode
  reboot();
}
