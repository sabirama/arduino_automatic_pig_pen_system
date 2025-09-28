#include "TimeStore.h"

TimeStore::TimeStore(int eepromStart, int eepromSize, uint8_t maxTimes) {
    _eepromStart = eepromStart;
    _eepromSize = eepromSize;
    _maxTimes = maxTimes;
}

void TimeStore::begin() {
    // Verify EEPROM is initialized
    if (EEPROM.read(_eepromStart) > _maxTimes) {
        saveCount(0); // Reset if corrupted
    }
}

void TimeStore::saveCount(uint8_t count) {
    EEPROM.write(_eepromStart, count);
    EEPROM.commit();
    delay(10); // Small delay for EEPROM write
}

uint8_t TimeStore::loadCount() {
    uint8_t count = EEPROM.read(_eepromStart);
    return (count <= _maxTimes) ? count : 0;
}

bool TimeStore::addTime(const char* timeStr) {
    // Validate input format (HH:MM)
    if (strlen(timeStr) != 5) {
        Serial.println("Invalid time length");
        return false;
    }
    if (timeStr[2] != ':') {
        Serial.println("Missing colon");
        return false;
    }
    if (!isdigit(timeStr[0]) || !isdigit(timeStr[1]) || 
        !isdigit(timeStr[3]) || !isdigit(timeStr[4])) {
        Serial.println("Non-digit characters");
        return false;
    }
    
    // Validate time range
    int hour = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    int minute = (timeStr[3] - '0') * 10 + (timeStr[4] - '0');
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        Serial.println("Time out of range");
        return false;
    }

    uint8_t count = loadCount();
    if (count >= _maxTimes) {
        Serial.println("Max times reached");
        return false;
    }

    // Check if time already exists
    char existingTime[6];
    for (uint8_t i = 0; i < count; i++) {
        if (getTime(i, existingTime)) {
            if (strcmp(existingTime, timeStr) == 0) {
                Serial.println("Time already exists");
                return false;
            }
        }
    }

    int addr = _eepromStart + 1 + (count * TIME_STR_LEN);
    
    // Write time string to EEPROM
    for (int i = 0; i < 5; i++) {
        EEPROM.write(addr + i, timeStr[i]);
    }
    EEPROM.write(addr + 5, '\0'); // Null terminator

    count++;
    saveCount(count);
    
    Serial.printf("Time added: %s at position %d (addr %d), total: %d\n", 
                  timeStr, count-1, addr, count);
    
    // Verify write
    char verifyTime[6];
    if (getTime(count-1, verifyTime)) {
        Serial.printf("Verified: %s\n", verifyTime);
    }
    
    return true;
}

bool TimeStore::removeTime(const char* timeStr) {
    uint8_t count = loadCount();
    if (count == 0) {
        Serial.println("No times to remove");
        return false;
    }

    bool found = false;
    uint8_t foundIndex = 0;
    
    // Find the index of the time to remove
    char storedTime[6];
    for (uint8_t i = 0; i < count; i++) {
        if (getTime(i, storedTime)) {
            if (strcmp(storedTime, timeStr) == 0) {
                found = true;
                foundIndex = i;
                break;
            }
        }
    }

    if (!found) {
        Serial.println("Time not found for removal");
        return false;
    }

    Serial.printf("Removing time: %s at index %d\n", timeStr, foundIndex);

    // Shift all subsequent times up by one position
    for (uint8_t i = foundIndex; i < count - 1; i++) {
        int fromAddr = _eepromStart + 1 + (i + 1) * TIME_STR_LEN;
        int toAddr = _eepromStart + 1 + i * TIME_STR_LEN;
        
        for (int b = 0; b < TIME_STR_LEN; b++) {
            byte data = EEPROM.read(fromAddr + b);
            EEPROM.write(toAddr + b, data);
        }
    }

    // Clear the last time slot
    int lastAddr = _eepromStart + 1 + (count - 1) * TIME_STR_LEN;
    for (int b = 0; b < TIME_STR_LEN; b++) {
        EEPROM.write(lastAddr + b, 0xFF); // Write 0xFF instead of 0
    }

    count--;
    saveCount(count);
    
    Serial.printf("Time removed successfully. New count: %d\n", count);
    return true;
}

void TimeStore::clearTimes() {
    saveCount(0);
    for (int i = _eepromStart + 1; i < _eepromStart + _eepromSize; i++) {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    Serial.println("All times cleared");
}

uint8_t TimeStore::getCount() {
    return loadCount();
}

bool TimeStore::getTime(uint8_t index, char* buffer) {
    uint8_t count = loadCount();
    if (index >= count) {
        return false;
    }

    int addr = _eepromStart + 1 + index * TIME_STR_LEN;
    
    for (int i = 0; i < TIME_STR_LEN; i++) {
        buffer[i] = EEPROM.read(addr + i);
        if (buffer[i] == 0 || buffer[i] == 0xFF) break;
    }
    buffer[5] = '\0'; // Ensure null termination
    
    // Validate the read time
    if (strlen(buffer) != 5 || buffer[2] != ':') {
        buffer[0] = '\0';
        return false;
    }
    
    return true;
}

void TimeStore::printTimes() {
    uint8_t count = loadCount();
    Serial.printf("Stored times count: %d (start addr: %d)\n", count, _eepromStart);
    
    if (count == 0) {
        Serial.println("No times stored");
        return;
    }
    
    char buf[TIME_STR_LEN];
    for (uint8_t i = 0; i < count; i++) {
        if (getTime(i, buf)) {
            Serial.printf("Time %d: %s (addr: %d)\n", i, buf, _eepromStart + 1 + i * TIME_STR_LEN);
        } else {
            Serial.printf("Time %d: ERROR reading\n", i);
        }
    }
}
