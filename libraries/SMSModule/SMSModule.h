#ifndef SMSMODULE_H
#define SMSMODULE_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

#define PHONE_NUM_ADDR 600
#define PHONE_NUM_MAX_LEN 20

class SMSModule {
  public:
    SMSModule(uint8_t rxPin, uint8_t txPin, unsigned long baud = 9600);
    void begin();

    bool sendCommand(const String& command, const String& expectedResponse, unsigned long timeout = 2000);

    // Send SMS to specified number
    bool sendSMS(const String& phoneNumber, const String& message);

    // Send SMS to stored number
    bool sendSMS(const String& message);

    bool isNetworkRegistered();
    String readSMS(int index);
    void deleteSMS(int index);

    void powerOn();
    void powerOff();

    // Phone number persistent storage
    bool setStoredPhoneNumber(const char* phoneNumber);  // save to EEPROM and cache
    const char* getStoredPhoneNumber() const;

  private:
    SoftwareSerial _simSerial;
    uint8_t _rxPin;
    uint8_t _txPin;
    unsigned long _baud;

    char _storedPhoneNumber[PHONE_NUM_MAX_LEN + 1] = {0};  // null-terminated buffer

    String readResponse(unsigned long timeout);

    void loadPhoneNumber();
    void savePhoneNumber(const char* phoneNumber);
};

#endif

