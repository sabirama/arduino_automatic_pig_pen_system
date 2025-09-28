#ifndef TIMESTORE_H
#define TIMESTORE_H

#include <EEPROM.h>
#include <Arduino.h>

#define TIME_STR_LEN 6  // "HH:MM" + null terminator

class TimeStore {
public:
    TimeStore(int eepromStart = 0, int eepromSize = 256, uint8_t maxTimes = 10);
    void begin();
    bool addTime(const char* timeStr);
    bool removeTime(const char* timeStr);
    void clearTimes();
    uint8_t getCount();
    bool getTime(uint8_t index, char* buffer);
    void printTimes();

private:
    int _eepromStart;
    int _eepromSize;
    uint8_t _maxTimes;
    
    void saveCount(uint8_t count);
    uint8_t loadCount();
};

#endif
