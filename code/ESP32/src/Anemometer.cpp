#include "Anemometer.h"


Anemometer::Anemometer(int sensorPin, float voltageConversionConstant)
{
	_sensorPin = sensorPin;
	_voltageConversionConstant = voltageConversionConstant;
}

Anemometer::~Anemometer()
{
}

float Anemometer::WindSpeed()
{
	int sensorValue = analogRead(_sensorPin); //Get a value between 0 and 1023 from the analog pin connected to the anemometer

	float sensorVoltage = sensorValue * _voltageConversionConstant; //Convert sensor value to actual voltage

																   //Convert voltage value to wind speed using range of max and min voltages and wind speed for the anemometer
	if (sensorVoltage <= voltageMin) {
		return 0; //Check if voltage is below minimum value. If so, set wind speed to zero.
	}
	else {
		return (sensorVoltage - voltageMin)*windSpeedMax / (voltageMax - voltageMin); //For voltages above minimum value, use the linear relationship to calculate wind speed.
	}
}

float Anemometer::SensorVoltage()
{
	int sensorValue = analogRead(_sensorPin); //Get a value between 0 and 1023 from the analog pin connected to the anemometer
	return sensorValue * _voltageConversionConstant; //Convert sensor value to actual voltage
}

