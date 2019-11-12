/*
  (c) 2019 by JU Sommer
*/

//#include <LiquidCrystal.h>
//LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
#include <QuadratureEncoder.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_Sensor.h>
#include <DHT_U.h>


Encoders Encoder(2, 3);

#define ONE_WIRE_BUS 11
#define DHTPIN 12
#define DHTTYPE DHT21

DHT_Unified humTempSensor(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature onewireSensors(&oneWire);
int NumberOfOnewireSensors = 0;

const bool debugEnabled = false;
const unsigned int debounceTime = 333;
const unsigned int maxDisplayLines = 1;
const unsigned int encoderButtonPin = 7;
const unsigned int mainsRelayPin = 6;
const unsigned int ventRelayPin = 5;
const unsigned int heatRelayPin = 4;
const unsigned int internalLED = 13;
const unsigned int timeoutvalue = 30000;
const unsigned int SensorUpdateIntervall = 5000;
const unsigned int OutdoorTempsensorIndex = 0;
const unsigned int FloorTempsensorIndex = 1;

int encoderButtonState = 0;
long lastButtonPress = 0;
int activeMenuEntry = 0;
bool editMode = false;
long lastmenuMove = 0;
long lastSensorUpdate = 0;
bool timeout = false;







struct menu {
  char text[6];
  String type;
  long valueint;
  bool editable;
}

menu[] = {
  //text    type  valueint,editable
  {"In  ",  "temp", 999   , false  },
  {"Rel.",  "perc", 0     , false  },
  {"Auss",  "temp", 0     , false  },
  {"Bodn",  "temp", 999   , false  },
  {"S-In",  "temp", 210   , true },
  {"S-Bd",  "temp", 270   , true },
  {"Zeit",  "time", 0     , true },
  {"Vent",  "bool", 0     , true },
  {"Heiz",  "bool", 0     , true }
};

const byte menuLen = sizeof(menu) / sizeof(menu[0]);

void setup() {
  lcd.init();
  lcd.backlight();
  //lcd.begin(16, 2);
  Serial.begin(115000);
  debug("hello world");

  pinMode(encoderButtonPin, INPUT_PULLUP);
  pinMode(mainsRelayPin, OUTPUT);
  pinMode(ventRelayPin, OUTPUT);
  pinMode(heatRelayPin, OUTPUT);
  pinMode(internalLED, OUTPUT);

  onewireSensors.begin();
  NumberOfOnewireSensors = onewireSensors.getDeviceCount();
  debug("Number of onewire devices found: " + String(NumberOfOnewireSensors));

  //Initializie AM2301 sensor
  humTempSensor.begin();
  sensor_t humTempSensorObject;
  humTempSensor.temperature().getSensor(&humTempSensorObject);
  debug ("------------------------------------");
  debug ("Temperature Sensor");
  debug ("Sensor Type: " + String(humTempSensorObject.name));
  debug ("Driver Ver:  " + String(humTempSensorObject.version));
  debug ("Unique ID:   " + String(humTempSensorObject.sensor_id));
  debug ("Max Value:   " + String(humTempSensorObject.max_value));
  debug ("Min Value:   " + String(humTempSensorObject.min_value) + "°C");
  debug ("Resolution:  " + String(humTempSensorObject.resolution) + "°C");
  debug ("------------------------------------");
  humTempSensor.humidity().getSensor(&humTempSensorObject);
  debug ("Humidity Sensor");
  debug ("Sensor Type: " + String(humTempSensorObject.name));
  debug ("Driver Ver:  " + String(humTempSensorObject.version));
  debug ("Unique ID:   " + String(humTempSensorObject.sensor_id));
  debug ("Max Value:   " + String(humTempSensorObject.max_value) + "%");
  debug ("Min Value:   " + String(humTempSensorObject.min_value) + "%");
  debug ("Resolution:  " + String(humTempSensorObject.resolution) + "%");
  debug ("------------------------------------");

}

void debug (String debugtext) {
  if (debugEnabled) {
    Serial.println(debugtext);
  }
}

String constructLineEntry (unsigned int line, unsigned int position, unsigned int activeMenuEntry) {
  //perform sanity checks
  if (line < 0 || line > maxDisplayLines) {
    debug("value of line out of range...");
    return ("ERR ");
  }
  if (activeMenuEntry < 0 || activeMenuEntry > menuLen) {
    debug("value of activeMenuEntry out of range...");
    return ("ERR ");
  }

  if (line == 0) {
    return (getMenuText(position, activeMenuEntry));
  }
  else if (line == 1) {
    return (getMenuValue(position, activeMenuEntry));
  }

}

String getMenuText (unsigned int position, unsigned int activeMenuEntry) {
  int entryno = activeMenuEntry + position - 1;
  if (entryno < 0 || entryno >= menuLen) { //if out of range: leave blank
    return ("    ");
  }
  return (String(menu[entryno].text));
}

String getMenuValue (unsigned int position, unsigned int activeMenuEntry) {
  int entryno = activeMenuEntry + position - 1;
  char tempchar [6];
  String tempstr;

  if (entryno < 0 || entryno >= menuLen) { //if out of range: leave blank
    return ("    ");
  }
  // begin to decide which type of value is needed

  if (menu[entryno].type == "temp") {
    if (menu[activeMenuEntry].valueint < 0) {
      tempstr = String(menu[entryno].valueint / 10 * -1 ) + "." + String(menu[entryno].valueint % 10 * -1);
      tempstr = "-" + tempstr;
    }
    else {
      tempstr = String(menu[entryno].valueint / 10 ) + "." + String(menu[entryno].valueint % 10);
    }
    while (tempstr.length() != 4) {
      if (tempstr.length() > 4) {
        tempstr = tempstr.substring(0, tempstr.indexOf("."));
      }
      if (tempstr.length() < 4) {
        tempstr = tempstr + " ";
      }
    }
    return (tempstr);
  }
  else if (menu[entryno].type == "perc") {
    tempstr = String(menu[entryno].valueint);
    tempstr = tempstr + "%";
    while (tempstr.length() != 4) {
      if (tempstr.length() < 4) {
        tempstr = tempstr + " ";
      }
    }
    return (tempstr);
  }
  else if (menu[entryno].type == "time") {
    char secsToGoStr[15];
    char minsToGoStr[15];
    char hoursToGoStr[15];

    long msToGo = menu[entryno].valueint - millis();
    if (msToGo < 0) { //
      msToGo = 0;     //if timer is negative, set to zero
    }                 //
    long secsToGo = msToGo / 1000;
    long minsToGo = secsToGo / 60;
    long hoursToGo = minsToGo / 60;
    secsToGo = secsToGo - (minsToGo * 60);
    minsToGo = minsToGo - (hoursToGo * 60);
    if (msToGo < 600000) {
      sprintf(secsToGoStr, "%02d", secsToGo);
      sprintf(minsToGoStr, "%01d", minsToGo);
      return (String(minsToGoStr) + ":" + String(secsToGoStr));
    }
    else if (msToGo < 36000000) {
      sprintf(minsToGoStr, "%02d", minsToGo);
      sprintf(hoursToGoStr, "%01d", hoursToGo);
      return (String(hoursToGoStr) + ":" + String(minsToGoStr));
    }
    else {
      sprintf(hoursToGoStr, "%03d", hoursToGo);
      return (String(hoursToGoStr) + "h");
    }
  }

  else if (menu[entryno].type == "bool") {
    if (menu[entryno].valueint == 0) {
      return ("AUS ");
    }
    else {
      return (" AN ");
    }
  }

  return ("N/A ");

}

void updateDisplay () {
  String line0;
  String line1;

  if (millis() - lastmenuMove >= timeoutvalue) {
    timeout = true;
    activeMenuEntry = 1;
    editMode = false;
  }

  if (editMode == false) {
    if (timeout) {
      line0 = constructLineEntry(0, 0, activeMenuEntry) + "  " + constructLineEntry(0, 1, activeMenuEntry) + "  " + constructLineEntry(0, 2, activeMenuEntry);
      line1 = constructLineEntry(1, 0, activeMenuEntry) + "  " + constructLineEntry(1, 1, activeMenuEntry) + "  " + constructLineEntry(1, 2, activeMenuEntry);
    }
    else {
      line0 = constructLineEntry(0, 0, activeMenuEntry) + " >" + constructLineEntry(0, 1, activeMenuEntry) + "< " + constructLineEntry(0, 2, activeMenuEntry);
      line1 = constructLineEntry(1, 0, activeMenuEntry) + " >" + constructLineEntry(1, 1, activeMenuEntry) + "< " + constructLineEntry(1, 2, activeMenuEntry);
    }
  }
  else {
    line0 = (">>>>> " + constructLineEntry(0, 1, activeMenuEntry) + " <<<<<");
    line1 = (">>>>> " + constructLineEntry(1, 1, activeMenuEntry) + " <<<<<");
  }
  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
}

void readSensors () {
  if (millis() - lastSensorUpdate > SensorUpdateIntervall) {
    lastSensorUpdate = millis();
    //onewire sensors...
    if (NumberOfOnewireSensors == 0) {
      menu[2].valueint = 999;
      menu[3].valueint = 999;
    }
    else if (NumberOfOnewireSensors == 1) {
      onewireSensors.requestTemperatures();
      menu[2].valueint = onewireSensors.getTempCByIndex(OutdoorTempsensorIndex) * 10;
      //debug(String(onewireSensors.getTempCByIndex(OutdoorTempsensorIndex) * 10));
      menu[3].valueint = 999;
    }
    else if (NumberOfOnewireSensors == 2) {
      onewireSensors.requestTemperatures();
      //debug(String(onewireSensors.getTempCByIndex(OutdoorTempsensorIndex) * 10));
      //
      debug(String(onewireSensors.getTempCByIndex(FloorTempsensorIndex) * 10));
      menu[2].valueint = onewireSensors.getTempCByIndex(OutdoorTempsensorIndex) * 10;
      menu[3].valueint = onewireSensors.getTempCByIndex(FloorTempsensorIndex) * 10;
    }

    //i2c bus sensors
    sensors_event_t humTempSensorEvent;
    humTempSensor.temperature().getEvent(&humTempSensorEvent);
    if (isnan(humTempSensorEvent.temperature)) {
      debug ("Error reading temperature!");
    }
    else {
      menu[0].valueint = humTempSensorEvent.temperature * 10;
    }
    humTempSensor.humidity().getEvent(&humTempSensorEvent);
    if (isnan(humTempSensorEvent.relative_humidity)) {
      debug ("Error reading humidity!");
    }
    else {
      menu[1].valueint = humTempSensorEvent.relative_humidity;

    }
  }
}

void processEncoder() {
  int result = 0;
  encoderButtonState = digitalRead(encoderButtonPin);
  if (encoderButtonState == LOW) {
    if (millis() - lastButtonPress > debounceTime) {
      lastButtonPress = millis();
      debug("Button press at " + String(millis()) + " last was " + String(lastButtonPress));
      if (menu[activeMenuEntry].editable == true) {
        debug("Valid button press, continuing...");
        editMode = !editMode;
      }
    }
  }

  else {

    if (Encoder.getEncoderCount() < 4 && Encoder.getEncoderCount() > -4 ) {
      Encoder.setEncoderCount(0);
    }

    if (Encoder.getEncoderCount() > 0)  {
      debug(String(Encoder.getEncoderCount()));
      Encoder.setEncoderCount(Encoder.getEncoderCount() - 4 );

      result = 3;
    }
    else if (Encoder.getEncoderCount() < 0)  {
      debug(String(Encoder.getEncoderCount()));
      Encoder.setEncoderCount(Encoder.getEncoderCount() + 4 );

      result = 1;
    }
  }
  if (result != 0) {
    lastmenuMove = millis();
    timeout = false;
  }
  if (editMode == false) {
    switch (result) {
      case 1:
        activeMenuEntry--;
        if (activeMenuEntry < 0) {
          activeMenuEntry = 0;
        }
        break;

      case 3:
        activeMenuEntry++;
        if (activeMenuEntry > menuLen - 2) {
          activeMenuEntry = menuLen - 1;
        }
        break;
    }
  }

  else {
    switch (result) {
      case 1:
        modifyActiveMenuEntry(-1);
        break;

      case 3:
        modifyActiveMenuEntry(+1);
        break;
    }
  }
}

void modifyActiveMenuEntry(int direction) {
  if (menu[activeMenuEntry].type == "temp") {
    if (menu[activeMenuEntry].valueint <= -99) {
      menu[activeMenuEntry].valueint = -99;
    }
    else if (menu[activeMenuEntry].valueint >= 500) {
      menu[activeMenuEntry].valueint = 500;
    }
    else {
      menu[activeMenuEntry].valueint = menu[activeMenuEntry].valueint + direction;
    }
  }
  else if (menu[activeMenuEntry].type == "bool") {
    if (menu[activeMenuEntry].valueint == 1) {
      menu[activeMenuEntry].valueint = 0;
    }
    else {
      menu[activeMenuEntry].valueint = 1;
    }
  }
  else if (menu[activeMenuEntry].type == "time") {
    if (menu[activeMenuEntry].valueint <= millis()) {
      menu[activeMenuEntry].valueint = millis() + 5000;
    }
    else if (menu[activeMenuEntry].valueint > millis() && menu[activeMenuEntry].valueint < millis() + 60000) {
      menu[activeMenuEntry].valueint =  menu[activeMenuEntry].valueint + 5000 * direction;
    }
    else if (menu[activeMenuEntry].valueint > millis() + 60000 && menu[activeMenuEntry].valueint < millis() + 60 * 60000) {
      menu[activeMenuEntry].valueint =  menu[activeMenuEntry].valueint + 60000 * direction;
    }
    else if (menu[activeMenuEntry].valueint > millis() + 60 * 60000) {
      menu[activeMenuEntry].valueint =  menu[activeMenuEntry].valueint + 60 * 60000 * direction;
    }
  }

}

int readSerialEncoder() {
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();
    debug("Serial received: " + String(incomingByte));
    switch (incomingByte) {
      case 49:
        return (1);
        break;
      case 50:
        return (2);
        break;
      case 51:
        return (3);
        break;
    }
  }
}

void setRelays() {
  if (menu[7].valueint == 0) {
    digitalWrite(ventRelayPin, LOW);
  }
  else {
    digitalWrite(ventRelayPin, HIGH);
  }

  if (menu[8].valueint == 0) {
    digitalWrite(mainsRelayPin, LOW);
  }
  else {
    digitalWrite(mainsRelayPin, HIGH);
  }
}

void checkHeatingTimer() {
  long msToGo = menu[6].valueint - millis();
  if (msToGo < 0 && msToGo > -5000) {
    menu[8].valueint = 0;
  }
  else if (msToGo > 0) {
    menu[8].valueint = 1;
  }
}

void heatingThermostat() {
  if (menu[3].valueint < menu[5].valueint && menu[0].valueint < menu[4].valueint) { // Boden-Ist < Boden-Soll && Innen-Ist < Innen-Soll
    digitalWrite(heatRelayPin, HIGH);
    digitalWrite(internalLED, HIGH);
  }
  else {
    digitalWrite(heatRelayPin, LOW);
    digitalWrite(internalLED, LOW);
  }
}

void loop() {
  updateDisplay();
  readSensors();
  processEncoder();
  setRelays();
  checkHeatingTimer();
  heatingThermostat();
}


