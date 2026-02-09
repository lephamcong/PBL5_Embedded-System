#include <Arduino.h>

#ifndef NTU_PIN
#define NTU_PIN 3
#endif

float turbidity;

float getNTU() 
{
    int val = analogRead(NTU_PIN);
    turbidity = map(val, 300, 2666, 1000, 0);
    if (turbidity < 0) 
    {
        turbidity = 0;
    } else if (turbidity > 1000) 
    {
        turbidity = 1000;
    }
    //Serial.println(text);
    return turbidity;
}