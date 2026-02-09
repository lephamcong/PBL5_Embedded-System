#include <Arduino.h>

#ifndef PH_PIN
#define PH_PIN 2
#endif 

#define Offset 0.70            //deviation compensate Bù TRỪ PH
unsigned long int avgValue;     //Store the average value of the sensor feedback

float getPH()
{
  int buf[10];                //buffer for read analog
  for(int i=0;i<10;i++)       //Get 10 sample value from the sensor for smooth the value
  { 
    buf[i]=analogRead(PH_PIN);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        int temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  avgValue=0;
  for(int i=2;i<8;i++)                      //take the average value of 6 center sample
    avgValue+=buf[i];
  float phValue=(float)avgValue*5.0/4096/6; //convert the analog into millivolt
  phValue=2.0*phValue+Offset;                      //convert the millivolt into pH value  chuyển đổi milivolt thành giá trị pH
  //Serial.print("    pH:"); 

  return phValue; 
  //Serial.print(phValue,2); 
}