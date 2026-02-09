#include <Arduino.h>

#ifndef TDS_PIN
#define TDS_PIN 0
#endif 

#define SCOUNT  30            // Number of samples of the array for the median filter algorithm
#define VREF 3.3              // ADC reference voltage

int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;       // current temperature for compensation

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  /*
    Median filtering algorithm for filtering the analog value read from the ADC pin
    Args:
      bArray: array to be filtered
      iFilterLen: length of the array
    Returns:
      Median value of the array
  */
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
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
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

float getTDS() {
  /*
    Function to read the analog value from the TDS sensor and convert it to TDS value
    Returns:
      TDS value in ppm
  */
  static unsigned long analogSampleTimepoint = millis();
  static unsigned long printTimepoint = millis();

  if (millis() - analogSampleTimepoint > 40U) {     // For every 40 milliseconds, gather 30 samples
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TDS_PIN);    // Read the analog value from the TDS sensor
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) { 
      analogBufferIndex = 0;
    }
  }

  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();

    // Copy the analog data to the temporary array for sorting and filtering
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    }

    // Calculate the average voltage value
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;

    // Calculate the compensation coefficient based on the current temperature
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage = averageVoltage / compensationCoefficient;

    // Convert the voltage value to TDS value
    tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage 
                - 255.86 * compensationVoltage * compensationVoltage 
                + 857.39 * compensationVoltage) * 0.5;

    return tdsValue;
  }
  return -1;  // If the function is called before the first reading, return -1
}


