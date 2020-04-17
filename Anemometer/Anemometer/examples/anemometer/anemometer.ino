
#include "Anemometer.h"

Anemometer _anemometer(A0);


float _recordedWindSpeedAtLastEvent = 0;
uint32_t _lastWindEvent = 0;

void setup()
{
  Serial.begin(115200); 
  while (!Serial) {
    ; // wait for serial port to connect.
  }
}

void loop()
{
  float windSpeed = _anemometer.WindSpeed();
  Serial.print("Wind speed: ");
  Serial.print(windSpeed);
  Serial.println(" Kmh");
  delay(1000);
}