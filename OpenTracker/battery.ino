void battery_init() {
#if OPENTRACKER_HW_REV > 0x0300 
#ifdef PIN_C_CHRG_ENA
  pinMode(PIN_C_CHRG_ENA, OUTPUT);
  digitalWrite(PIN_C_CHRG_ENA, LOW);
#endif

#ifdef PIN_S_BATT_SHDN
  pinMode(PIN_S_BATT_SHDN, INPUT_PULLUP);
#endif
#ifdef PIN_S_BATT_ENA
  pinMode(PIN_S_BATT_ENA, INPUT_PULLUP);
#endif
  pinMode(PIN_S_CHRG_STAT, INPUT);

  pinMode(PIN_C_CHRG_PROG, OUTPUT);
  digitalWrite(PIN_C_CHRG_PROG, HIGH);
#endif
}

enum { SUPPLY_MAIN, SUPPLY_SECONDARY };

int battery_get_source()
{
#if OPENTRACKER_HW_REV > 0x0300 
#ifdef PIN_S_BATT_SHDN
  pinMode(PIN_S_BATT_SHDN, INPUT_PULLUP);
  return digitalRead(PIN_S_BATT_SHDN) == LOW ? SUPPLY_SECONDARY : SUPPLY_MAIN;
#endif
#ifdef PIN_S_BATT_ENA
  pinMode(PIN_S_BATT_ENA, INPUT_PULLUP);
  return digitalRead(PIN_S_BATT_ENA) == LOW ? SUPPLY_MAIN : SUPPLY_SECONDARY;
#endif
#endif
  return SUPPLY_MAIN;
}

enum { CHARGE_NONE, CHARGE_BUSY, CHARGE_COMPLETE };

int battery_get_status()
{
#if OPENTRACKER_HW_REV > 0x0300 
  pinMode(PIN_S_CHRG_STAT, INPUT_PULLDOWN);
  delay(10);
  uint32_t stat = (digitalRead(PIN_S_CHRG_STAT) == HIGH);
  pinMode(PIN_S_CHRG_STAT, INPUT_PULLUP);
  delay(10);
  stat |= (digitalRead(PIN_S_CHRG_STAT) == HIGH) << 1;
  pinMode(PIN_S_CHRG_STAT, INPUT);
    debug_port.print("stat: ");
    debug_port.println(stat, BIN);
  return (stat == 0) ? CHARGE_BUSY : (stat == 3) ? CHARGE_COMPLETE : CHARGE_NONE;
#else
  return CHARGE_NONE;
#endif
}

void battery_set_charger(bool enable)
{
#if OPENTRACKER_HW_REV > 0x0300 
  pinMode(PIN_C_CHRG_PROG, OUTPUT);
  digitalWrite(PIN_C_CHRG_PROG, enable ? LOW : HIGH);
#endif
}
