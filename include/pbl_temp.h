#include <OneWire.h> 
#include <DallasTemperature.h>

#ifndef TEMP_PIN
#define TEMP_PIN 1
#endif

OneWire oneWire(TEMP_PIN); 
DallasTemperature tempSensors(&oneWire);

bool setupTemp(void) 
{ 
 tempSensors.begin(); 
 if (tempSensors.getDeviceCount() == 0) 
 { 
   return false; 
 } else { 
   return true;
 }
}

float getTEMP() 
{ 
 //Serial.print(" Requesting temperatures..."); 
 tempSensors.requestTemperatures();
 //Serial.println("DONE"); 
 //Serial.print("Temperature is: "); 
 //Serial.print(tempSensors.getTempCByIndex(0));
 //delay(1000); 
 return tempSensors.getTempCByIndex(0);
}
