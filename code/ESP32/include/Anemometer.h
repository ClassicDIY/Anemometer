#pragma once
#include <Arduino.h>

namespace AnemometerNS
{
	class Anemometer
	{
		#define SAMPLESIZE 8
		int _sensorPin; // Defines the pin that the anemometer output is connected to
		#define ADC_Resolution 4095.0
	public:
		Anemometer(int sensorPin);
		~Anemometer();
		float WindSpeed();
	private:
		float AddReading(float val);
		float _rollingSum;
		int _numberOfSummations;
	};
}
