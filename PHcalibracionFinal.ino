#include <EEPROM.h>

#define EEPROM_write(address, p) {int i = 0; byte *pp = (byte*)&(p); for(; i < sizeof(p); i++) EEPROM.write(address + i, pp[i]);}
#define EEPROM_read(address, p) {int i = 0; byte *pp = (byte*)&(p); for(; i < sizeof(p); i++) pp[i] = EEPROM.read(address + i);}

#define ReceivedBufferLength 20
char receivedBuffer[ReceivedBufferLength + 1];
byte receivedBufferIndex = 0;

#define SCOUNT 30
int analogBuffer[SCOUNT];
int analogBufferIndex = 0;

#define SlopeValueAddress 0
#define InterceptValueAddress (SlopeValueAddress + 4)
float slopeValue, interceptValue, averageVoltage;
boolean enterCalibrationFlag = 0;

#define SensorPin A0
#define VREF 5000

void setup() {
  Serial.begin(115200);
  readCharacteristicValues(); // read the slope and intercept of the pH probe
}

void loop() {
  if (serialDataAvailable()) {
    byte modeIndex = uartParse();
    phCalibration(modeIndex);
    EEPROM_read(SlopeValueAddress, slopeValue);
    EEPROM_read(InterceptValueAddress, interceptValue);
  }

  static unsigned long sampleTimepoint = millis();
  if (millis() - sampleTimepoint > 40U) { // Enviar datos cada 30 minutos
    sampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(SensorPin) / 1024.0 * VREF;
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
      analogBufferIndex = 0;
    averageVoltage = getMedianNum(analogBuffer, SCOUNT);
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 1000U) {
    printTimepoint = millis();
    if (enterCalibrationFlag) {
      Serial.print("Voltage: ");
      Serial.print(averageVoltage);
      Serial.println("mV");
    } else {
      Serial.print("pH: ");
      Serial.println(averageVoltage / 1000.0 * slopeValue + interceptValue);
    }
  }
}

boolean serialDataAvailable(void) {
  char receivedChar;
  static unsigned long receivedTimeOut = millis();
  while (Serial.available() > 0) {
    if (millis() - receivedTimeOut > 1000U) {
      receivedBufferIndex = 0;
      memset(receivedBuffer, 0, (ReceivedBufferLength + 1));
    }
    receivedTimeOut = millis();
    receivedChar = Serial.read();
    if (receivedChar == '\n' || receivedBufferIndex == ReceivedBufferLength) {
      receivedBufferIndex = 0;
      strupr(receivedBuffer);
      return true;
    } else {
      receivedBuffer[receivedBufferIndex] = receivedChar;
      receivedBufferIndex++;
    }
  }
  return false;
}

byte uartParse() {
  byte modeIndex = 0;
  if (strstr(receivedBuffer, "CALIBRATION") != NULL)
    modeIndex = 1;
  else if (strstr(receivedBuffer, "EXIT") != NULL)
    modeIndex = 4;
  else if (strstr(receivedBuffer, "ACID:") != NULL)
    modeIndex = 2;
  else if (strstr(receivedBuffer, "ALKALI:") != NULL)
    modeIndex = 3;
  return modeIndex;
}

void phCalibration(byte mode) {
  char *receivedBufferPtr;
  static byte acidCalibrationFinish = 0, alkaliCalibrationFinish = 0;
  static float acidValue, alkaliValue;
  static float acidVoltage, alkaliVoltage;
  float acidValueTemp, alkaliValueTemp, newSlopeValue, newInterceptValue;

  switch (mode) {
    case 0:
      if (enterCalibrationFlag)
        Serial.println(F("Command Error"));
      break;
    case 1:
      receivedBufferPtr = strstr(receivedBuffer, "CALIBRATION");
      enterCalibrationFlag = 1;
      acidCalibrationFinish = 0;
      alkaliCalibrationFinish = 0;
      Serial.println(F("Enter Calibration Mode"));
      break;
    case 2:
      if (enterCalibrationFlag) {
        receivedBufferPtr = strstr(receivedBuffer, "ACID:");
        receivedBufferPtr += strlen("ACID:");
        acidValueTemp = strtod(receivedBufferPtr, NULL);
        if ((acidValueTemp > 3) && (acidValueTemp < 5)) {
          acidValue = acidValueTemp;
          acidVoltage = averageVoltage / 1000.0;
          acidCalibrationFinish = 1;
          Serial.println(F("Acid Calibration Successful"));
        } else {
          acidCalibrationFinish = 0;
          Serial.println(F("Acid Value Error"));
        }
      }
      break;
    case 3:
      if (enterCalibrationFlag) {
        receivedBufferPtr = strstr(receivedBuffer, "ALKALI:");
        receivedBufferPtr += strlen("ALKALI:");
        alkaliValueTemp = strtod(receivedBufferPtr, NULL);
        if ((alkaliValueTemp > 8) && (alkaliValueTemp < 11)) {
          alkaliValue = alkaliValueTemp;
          alkaliVoltage = averageVoltage / 1000.0;
          alkaliCalibrationFinish = 1;
          Serial.println(F("Alkali Calibration Successful"));
        } else {
          alkaliCalibrationFinish = 0;
          Serial.println(F("Alkali Value Error"));
        }
      }
      break;
    case 4:
      if (enterCalibrationFlag) {
        if (acidCalibrationFinish && alkaliCalibrationFinish) {
          newSlopeValue = (acidValue - alkaliValue) / (acidVoltage - alkaliVoltage);
          EEPROM_write(SlopeValueAddress, newSlopeValue);
          newInterceptValue = acidValue - (newSlopeValue * acidVoltage);
          EEPROM_write(InterceptValueAddress, newInterceptValue);
          Serial.print(F("Calibration Successful"));
        } else {
          Serial.print(F("Calibration Failed"));
        }
        Serial.println(F(",Exit Calibration Mode"));
        acidCalibrationFinish = 0;
        alkaliCalibrationFinish = 0;
        enterCalibrationFlag = 0;
      }
      break;
  }
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) {
    bTab[i] = bArray[i];
  }
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}

void readCharacteristicValues() {
  EEPROM_read(SlopeValueAddress, slopeValue);
  EEPROM_read(InterceptValueAddress, interceptValue);
  if (EEPROM.read(SlopeValueAddress) == 0xFF && EEPROM.read(SlopeValueAddress + 1) == 0xFF && EEPROM.read(SlopeValueAddress + 2) == 0xFF && EEPROM.read(SlopeValueAddress + 3) == 0xFF) 
  {
    slopeValue = 3.5;
    EEPROM_write(SlopeValueAddress, slopeValue);
  }
  if (EEPROM.read(InterceptValueAddress) == 0xFF && EEPROM.read(InterceptValueAddress + 1) == 0xFF && EEPROM.read(InterceptValueAddress + 2) == 0xFF && EEPROM.read(InterceptValueAddress + 3) == 0xFF) 
  {
    interceptValue = 0;
    EEPROM_write(InterceptValueAddress, interceptValue);
  }
}