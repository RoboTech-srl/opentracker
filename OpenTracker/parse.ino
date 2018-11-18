
//parse remote commands from server
int parse_receive_reply() {
  //receive reply from modem and parse it
  int ret = 0;
  unsigned int len = 0;
  byte header = 0;
  int resp_code = 0;
  int ntry = 0;

  char *tmp;
  char cmd[100] = "";  //remote commands stored here

  DEBUG_FUNCTION_CALL();

  addon_event(ON_RECEIVE_STARTED);
  if (gsm_get_modem_status() == 4) {
    DEBUG_FUNCTION_PRINTLN("call interrupted");
    return 0; // abort
  }

  long last = millis();
  while ((long)(millis() - last) < SERVER_REPLY_TIMEOUT) {
#if MODEM_CMDSET
    gsm_get_reply(1); //flush buffer

    // query unread length
    gsm_port.print(AT_RECEIVE "0\r");
    gsm_wait_for_reply(1,0);

    tmp = strstr(modem_reply, "+QIRD:");
    if(tmp!=NULL) {
      tmp = strtok(tmp+6, ",");
      int total = (tmp != NULL) ? atoi(tmp) : -1;
      tmp = strtok(NULL, ",");
      int nread = (tmp != NULL) ? atoi(tmp) : -1;
      tmp = strtok(NULL, ",");
      int unred = (tmp != NULL) ? atoi(tmp) : -1;
      DEBUG_PRINTLN(total);
      DEBUG_PRINTLN(nread);
      DEBUG_PRINTLN(unred);
      // no more data to read?
      if ((total == 0 && ++ntry > SERVER_REPLY_RETRY) || (unred == 0 && total != 0)) {
        if (gsm_get_connection_status() != 1)
          break; // exit if no longer connected
      } else if (total == 0) {
        addon_delay(500);
        continue; // retry (wait for data rx)
      }
    }
    ntry = 0;
#endif
    gsm_get_reply(1); //flush buffer

    // read server reply
    gsm_port.print(AT_RECEIVE "100\r");

    gsm_wait_for_reply(1,0);

    //do we have data?
    tmp = strstr(modem_reply, "ERROR");
    if(tmp!=NULL) {
      DEBUG_PRINT("No more data available.");
      break;
    }
    
    tmp = strstr(modem_reply, "+QIRD:");
    if(tmp!=NULL) {
#if MODEM_CMDSET
      tmp += strlen("+QIRD:");
#else
      tmp = strstr(modem_reply, PROTO ","); //get data length
      if(tmp!=NULL)
        tmp += strlen(PROTO ",");
#endif
    }
    if(tmp==NULL) {
      // no data yet, keep looking
      addon_delay(500);
      continue;
    }

    // read data length
    len = atoi(tmp);
    DEBUG_PRINTLN(len);

    // read full buffer (data)
    gsm_get_reply(1);

    if(len==0) {
      // no data yet, keep looking
      addon_delay(500);
      continue;
    }

    // data is available, reset timeout
    last = millis();

    // remove trailing modem response (OK)
    if (len < sizeof(modem_reply) - 1)
      modem_reply[len] = '\0';
    else
      DEBUG_PRINT("Warning: data exceeds modem receive buffer!");

    DEBUG_PRINTLN(header);
    if (header == 0) {
      tmp = strstr(modem_reply, "HTTP/1.");
      if(tmp!=NULL) {
        DEBUG_PRINT("Found response: ");
        header = 1;

        resp_code = atoi(&tmp[9]);
        DEBUG_PRINTLN(resp_code);
#if PARSE_IGNORE_COMMANDS && PARSE_IGNORE_EOF
        // optimize and close connection earlier (without reading whole reply)
        break;
#endif
      } else {
        DEBUG_PRINT("Not and HTTP response!");
        break;
      }
    } else if (header == 1) {
      // looking for end of headers
      tmp = strstr(modem_reply, "\r\n\r\n");
      if(tmp!=NULL) {
        DEBUG_PRINT("End of header found!");
        header = 2;

        //all data from this packet and all next packets can be commands
        tmp += strlen("\r\n\r\n");
        strlcpy(cmd, tmp, sizeof(cmd));
      }
    } else {
      // packet contains only response body
      strlcat(cmd, modem_reply, sizeof(cmd));
    }
	
    addon_event(ON_RECEIVE_DATA);
    if (gsm_get_modem_status() == 4) {
      DEBUG_FUNCTION_PRINTLN("call interrupted");
      return 0; // abort
    }
  }

#if SEND_RAW
  DEBUG_PRINT("RAW data mode enabled, not checking whether the packet was received or not.");
  ret = 1;

#else // HTTP

  // any http reply is valid by default
  if (header != 0)
    ret = 1;

#ifdef PARSE_ACCEPTED_RESPONSE_CODES
#define RESP_CODE(x) && (resp_code != (x))
  // apply restrictions to response code
  if (1 PARSE_ACCEPTED_RESPONSE_CODES)
    ret = 0;
#undef RESP_CODE
#endif

#if !PARSE_IGNORE_EOF
  // valid only if "#eof" received
  tmp = strstr(cmd, "#eof");
  if(tmp==NULL)
    ret = 0;
#endif

#if !PARSE_IGNORE_COMMANDS
  parse_cmd(cmd);
#endif

#endif // SEND_RAW
  
  if (ret) {
    //all data was received by server
    DEBUG_PRINT("Data was fully received by the server.");
    addon_event(ON_RECEIVE_COMPLETED);
  } else {
    DEBUG_PRINT("Data was not received by the server.");
    addon_event(ON_RECEIVE_FAILED);
  }

  return ret;
}

void parse_cmd(char *cmd) {
  //parse commands info received from the server

  char *tmp;

  DEBUG_FUNCTION_CALL();

  DEBUG_PRINT("Received commands: ");
  DEBUG_PRINTLN(cmd);

  //check for settime command (#t:13/01/11,09:43:50+00)
  tmp = strstr((cmd), "#t:");
  if(tmp!=NULL) {
    DEBUG_PRINT("Found settime command.");

    tmp += 3; //strlen("#t:");
    tmp = strtok(tmp, "\r\n");   //all commands end with \n

    DEBUG_PRINTLN(tmp);

    if(strlen(tmp) == 20 && tmp[2] == '/' && tmp[5] == '/' && tmp[8] == ','
        && tmp[11] == ':' && tmp[14] == ':' && tmp[17] == '+') {
      DEBUG_PRINT("Valid time string found.");

      //setting current time
      strlcpy(time_char, tmp, sizeof(time_char));

      gsm_set_time();
    }
  }
}
