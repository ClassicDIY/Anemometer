#include "Anemometer.h"

namespace AnemometerNS
{
Anemometer::Anemometer(int sensorPin)
{
	_sensorPin = sensorPin;
}

Anemometer::~Anemometer()
{
}

// Wind speed in kmh
float Anemometer::WindSpeed()
{
	float rVal = 0;
	// Get a value between 0 and ADC_Resolution from the analog pin connected to the anemometer
	double reading = analogRead(_sensorPin);
	if (reading >= 1 && reading <= ADC_Resolution)
	{
		// The constants used in this calculation are taken from
		// https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function
		// and improves the default ADC reading accuracy to within 1%.
		double sensorVoltage = -0.000000000000016 * pow(reading, 4) +
							   0.000000000118171 * pow(reading, 3) - 0.000000301211691 * pow(reading, 2) +
							   0.001109019271794 * reading + 0.034143524634089;

		// Convert voltage value to wind speed using range of max and min voltages and wind speed for the anemometer
		if (sensorVoltage > AnemometerVoltageMin)
		{
			// For voltages above minimum value, use the linear relationship to calculate wind speed.
			rVal = (sensorVoltage - AnemometerVoltageMin) * AnemometerWindSpeedMax / (AnemometerVoltageMax - AnemometerVoltageMin);
		}
	}
	float ws = AddReading(rVal);
	ws = roundf(ws * 3.6 * 10); // convert to km/h and round to 1 decimal place
	return ws / 10;
}

float Anemometer::AddReading(float val)
{
	float currentAvg = 0.0;
	if (_numberOfSummations > 0)
	{
		currentAvg = _rollingSum / _numberOfSummations;
	}
	if (_numberOfSummations < SAMPLESIZE)
	{
		_numberOfSummations++;
	}
	else
	{
		_rollingSum -= currentAvg;
	}
	_rollingSum += val;
	return _rollingSum / _numberOfSummations;
}
} // namespace AnemometerNS