#include "SMSModule.h"

SMSModule::SMSModule(uint8_t rxPin, uint8_t txPin, unsigned long baud)
  : _rxPin(rxPin), _txPin(txPin), _baud(baud), _simSerial(rxPin, txPin) {}

void SMSModule::begin() {
  _simSerial.begin(_baud);
  delay(1000);
  powerOn();

  EEPROM.begin(1024);
  loadPhoneNumber();
}

void SMSModule::powerOn() {
  // Optional: implement power on sequence if you have control pin connected
  sendCommand("AT", "OK", 1000);
}

void SMSModule::powerOff() {
  // Optional: implement power off sequence if you have control pin connected
  sendCommand("AT+QPOWD", "OK", 3000);
}

bool SMSModule::sendCommand(const String& command, const String& expectedResponse, unsigned long timeout) {
  while (_simSerial.available()) _simSerial.read();  // Flush input
  if (command.length()) {
    _simSerial.println(command);
  }

  unsigned long start = millis();
  String response = "";
  while (millis() - start < timeout) {
    while (_simSerial.available()) {
      char c = _simSerial.read();
      response += c;
      if (response.indexOf(expectedResponse) != -1) {
        return true;
      }
    }
  }
  return false;
}

bool SMSModule::sendSMS(const String& phoneNumber, const String& message) {
  if (!sendCommand("AT+CMGF=1", "OK", 1000)) return false;  // Set SMS text mode
  if (!sendCommand("AT+CMGS=\"" + phoneNumber + "\"", ">", 1000)) return false;

  _simSerial.print(message);
  _simSerial.write(26);  // Ctrl+Z to send SMS

  return sendCommand("", "OK", 5000);
}

bool SMSModule::sendSMS(const String& message) {
  if (_storedPhoneNumber[0] == '\0') return false;
  return sendSMS(String(_storedPhoneNumber), message);
}

bool SMSModule::isNetworkRegistered() {
  if (!sendCommand("AT+CREG?", "OK", 1000)) return false;

  String response = readResponse(1000);
  int idx = response.indexOf("+CREG:");
  if (idx == -1) return false;
  int commaIdx = response.indexOf(',', idx);
  if (commaIdx == -1) return false;
  char statChar = response.charAt(commaIdx + 1);
  return (statChar == '1' || statChar == '5');
}

String SMSModule::readSMS(int index) {
  if (!sendCommand("AT+CMGF=1", "OK", 1000)) return "";
  if (!sendCommand("AT+CMGR=" + String(index), "+CMGR:", 3000)) return "";

  String response = readResponse(3000);
  int msgStart = response.indexOf("\r\n", response.indexOf("+CMGR:"));
  if (msgStart == -1) return "";
  msgStart += 2;
  int msgEnd = response.indexOf("\r\n", msgStart);
  if (msgEnd == -1) return "";
  return response.substring(msgStart, msgEnd);
}

void SMSModule::deleteSMS(int index) {
  sendCommand("AT+CMGD=" + String(index), "OK", 2000);
}

String SMSModule::readResponse(unsigned long timeout) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (_simSerial.available()) {
      char c = _simSerial.read();
      response += c;
    }
  }
  return response;
}

void SMSModule::loadPhoneNumber() {
  int len = EEPROM.read(PHONE_NUM_ADDR);
  if (len <= 0 || len > PHONE_NUM_MAX_LEN) {
    _storedPhoneNumber[0] = '\0';
    return;
  }
  for (int i = 0; i < len; i++) {
    _storedPhoneNumber[i] = EEPROM.read(PHONE_NUM_ADDR + 1 + i);
  }
  _storedPhoneNumber[len] = '\0';
}

void SMSModule::savePhoneNumber(const char* phoneNumber) {
  int len = strlen(phoneNumber);
  if (len > PHONE_NUM_MAX_LEN) len = PHONE_NUM_MAX_LEN;

  EEPROM.write(PHONE_NUM_ADDR, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(PHONE_NUM_ADDR + 1 + i, phoneNumber[i]);
  }
  for (int i = len; i < PHONE_NUM_MAX_LEN; i++) {
    EEPROM.write(PHONE_NUM_ADDR + 1 + i, 0);
  }
  EEPROM.commit();

  strncpy(_storedPhoneNumber, phoneNumber, len);
  _storedPhoneNumber[len] = '\0';
}

bool SMSModule::setStoredPhoneNumber(const char* phoneNumber) {
  if (phoneNumber == nullptr) return false;
  int len = strlen(phoneNumber);
  if (len == 0 || len > PHONE_NUM_MAX_LEN) return false;
  savePhoneNumber(phoneNumber);
  return true;
}

const char* SMSModule::getStoredPhoneNumber() const {
  return _storedPhoneNumber;
}

