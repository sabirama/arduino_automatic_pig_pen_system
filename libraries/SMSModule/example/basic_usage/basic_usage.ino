SMSModule sim800l(D6, D7);

void setup() {
  Serial.begin(115200);
  sim800l.begin();

  // Save phone number persistently
  sim800l.setStoredPhoneNumber("+1234567890");

  // Send SMS to stored number
  if (sim800l.sendSMS("Hello from NodeMCU!")) {
    Serial.println("SMS sent successfully");
  } else {
    Serial.println("Failed to send SMS");
  }
}

