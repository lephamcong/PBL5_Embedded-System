#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // Library for I2C LCD

#ifndef SCL_PIN
#define SCL_PIN 19
#endif

#ifndef SDA_PIN
#define SDA_PIN 18
#endif
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16 columns and 2 rows

typedef struct
{
  int col;
  int row;
  String text;
} LCDmatrix;

bool setupLCD() {
  // Khởi tạo màn hình LCD
  try{
    Wire.begin(SDA_PIN,SCL_PIN);  // Create a Wire object with SDA and SCL pins for I2C communication
    lcd.init();
    lcd.backlight();  // Turn on the backlight
    lcd.setCursor(0, 0);
    return true;
  } catch (...) {
    return false;
  }
}

void printLCD(LCDmatrix lcdMatrix[4]) {
  lcd.clear();  
  for(int i = 0; i < 4; i++) {
    lcd.setCursor(lcdMatrix[i].col, lcdMatrix[i].row);
    lcd.print(lcdMatrix[i].text);
  }
}
