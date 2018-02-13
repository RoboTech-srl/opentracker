#define STORAGE_FREE_CHAR 255

void storage_save_current() {
  debug_print(F("storage_save_current() started"));

  int data_len = strlen(data_current) + 1; // include '\0' as block marker
  //check for flash limit
  if (logindex + data_len >= STORAGE_DATA_END) {
    debug_print(F("storage_save_current(): flash memory is full, starting to overwrite"));
    logindex = STORAGE_DATA_START;
  }
  //adding index marker, it will be overwritten with next flash write
  data_current[data_len] = STORAGE_FREE_CHAR;

  //saving data_current to flash memory, including index marker
  dueFlashStorage.write(logindex, (byte*)data_current, data_len + 1);
  logindex += data_len;

  debug_print(F("storage_save_current() ended"));
  //storage_dump();
}

void storage_get_index() {
  //storage_dump();
  debug_print(F("storage_get_index() started"));
  debug_port.print("storage start: 0x");
  debug_port.println(FLASH_STORAGE_START, HEX);
  debug_port.print("storage size: ");
  debug_port.println(FLASH_STORAGE_SIZE);

  //scan flash for current log position (new log writes will continue from there)
  byte *tmp = dueFlashStorage.readAddress(STORAGE_DATA_START);
  byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
  bool block = false;
  while (tmp < tmpend) {
    if (!block && (*tmp != STORAGE_FREE_CHAR))
      block = true;
    else if (block && (*tmp == STORAGE_FREE_CHAR)) {
      //found log index
      logindex = tmp - dueFlashStorage.readAddress(0);

      debug_print(F("storage_get_index(): Found log position:"));
      break;  //we only need first found index
    }
    ++tmp;
  }
  if (tmp >= tmpend) { // probable corruption, re-initialize
    logindex = STORAGE_DATA_START;
    dueFlashStorage.write(logindex, STORAGE_FREE_CHAR);
    debug_print(F("storage_get_index(): Not found, initialize log position:"));
  }
  debug_print(logindex);

  debug_print(F("storage_get_index() ended"));
}

void storage_send_logs(int really_send) {
  int num_sent = 0;
  
  debug_print(F("storage_send_logs() started"));

  //check if some logs were saved
  uint32_t sent_position = logindex;  //empty set
  
  byte *tmp = dueFlashStorage.readAddress(logindex);
  byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
  while (tmp < tmpend) {
    if (*tmp != STORAGE_FREE_CHAR) {
      //found sent position
      sent_position = tmp - dueFlashStorage.readAddress(0);

      debug_print(F("storage_send_logs(): Found log sent position:"));
      break;  //we only need first found index
    }
    ++tmp;
  }
  if (tmp >= tmpend) { // re-start from the beginning
    tmp = dueFlashStorage.readAddress(STORAGE_DATA_START);
    tmpend = dueFlashStorage.readAddress(logindex);
    while (tmp < tmpend) {
      if (*tmp != STORAGE_FREE_CHAR) {
        //found sent position
        sent_position = tmp - dueFlashStorage.readAddress(0);
  
        debug_print(F("storage_send_logs(): Found log sent position:"));
        break;  //we only need first found index
      }
      ++tmp;
    }
  }
  debug_print(sent_position);

  if (sent_position != logindex) {
    bool err = false;
    do {
      debug_print(F("storage_send_logs(): Sending data from:"));
      debug_print(sent_position);
      
      // read current block
      strlcpy(data_current, (char*)dueFlashStorage.readAddress(sent_position), sizeof(data_current));
      int data_len = strlen(data_current) + 1; // include string terminator (zero)

      debug_print(F("Log data:"));
      debug_print(data_current);

      if (!really_send) {
        sent_position += data_len;
      } else {
        // send block
        if (gsm_send_data() == 1) {
          debug_print(F("storage_send_logs(): Success, erase sent data"));
    
          // erase block (after sent)
          dueFlashStorage.write(sent_position, STORAGE_FREE_CHAR, data_len);
          sent_position += data_len;
          // apply send limit
          num_sent++;
          if (STORAGE_MAX_SEND_OLD > 0 && num_sent >= STORAGE_MAX_SEND_OLD) {
            debug_print(F("storage_send_logs(): reached send limit"));
            break;
          }
        } else {
          err = true;
          break;
        }
      }
      if (sent_position >= STORAGE_DATA_END) {
        debug_print(F("storage_send_logs(): Wrap around sending data"));
        sent_position = STORAGE_DATA_START;
      }
    } while (dueFlashStorage.read(sent_position) != STORAGE_FREE_CHAR);
  
    if (!err) {
      debug_print(F("storage_send_logs(): Logs were sent successfully"));
    } else {
      debug_print(F("storage_send_logs(): Error sending logs"));
    }
  } else {
    debug_print(F("storage_send_logs(): No logs present"));
  }

  debug_print(F("storage_send_logs() ended"));
}

void storage_dump() {
  debug_print(F("storage_dump() started"));
  debug_port.print(F("start = "));
  debug_print(STORAGE_DATA_START);
  debug_port.print(F("end = "));
  debug_print(STORAGE_DATA_END);
  debug_port.print(F("logindex = "));
  debug_print(logindex);
  byte *tmp = dueFlashStorage.readAddress(STORAGE_DATA_START);
  byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
  int k=0;
  char buf[10];
  while (tmp < tmpend) {
    if ((k & 31) == 0)
      debug_port.println();
    snprintf(buf, 10, "%02X", *tmp);
    debug_port.print(buf);
    ++k;
    ++tmp;
  }
  debug_port.println();

  debug_print(F("storage_dump() ended"));
}

