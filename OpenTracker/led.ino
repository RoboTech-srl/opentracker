
//blink led

int led_interval = 1000; //interval at which to blink status led (milliseconds)

void status_led() {
  //blink led
  unsigned long currentMillis = millis();
  if((long)(currentMillis - previousMillis) > led_interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if(ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW;

    // set the LED with the ledState of the variable
    digitalWrite(PIN_POWER_LED, ledState);
  }
}

void status_delay(long ms) {
  // delay and update status led
  status_led();
  if (ms <= 0)
    return;
  long n = ms & 63;
  ms >>= 6;
  while (ms--) {
    delay(64);
    status_led();
  }
  delay(n);
}

void blink_start() {
  //blink start
  for(int i = 0; i < 6; i++) {
    if(ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW;

    digitalWrite(PIN_POWER_LED, ledState);   // set the LED on
    delay(200);
  }
}

void blink_debug() {
  //blink start
  for(int i = 0; i < 50; i++) {
    if(ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW;

    digitalWrite(PIN_POWER_LED, ledState);   // set the LED on
    delay(200);
  }
}

void blink_got_gps() {
  //blink start
  for(int i = 0; i < 4; i++) {
    if(ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW;

    digitalWrite(PIN_POWER_LED, ledState);   // set the LED on
    delay(100);
  }
}
