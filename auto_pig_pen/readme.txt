handlers
<--WiFiConfig-->

<--ControlPin-->
//controls
setHigh() //set pin to high
setLow() //set pin to low
toggle() // switches depending on pin state
//stattus checks
isHigh() // check is pin is on

<--ScaleModule-->
//controls
tare() //set hx711 tare
read() //read the scale reading
//stattus checks
isReady() //checks if scale is ready

<--ServoControl-->
open() //opens servo
close() //closes servo

<--SMSModule-->
sendSMS(const String& message) //send a message to saved number
loadPhoneNumber() //load number from EPPROM
setStoredPhoneNumber(const char* phoneNumber) //save the number to EPPROM
getStoredPhoneNumber() //returns the loaded phone number

<--TimeStore-->
addTime(const char* timeStr) //adds a time to array hh:mm
removeTime(const char* timeStr) //removes a time from array
getCount() //returns the stored array
