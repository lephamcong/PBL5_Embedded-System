#include <Arduino.h>
#include <WiFi.h>
#include "pbl_blynk.h"
#include <BlynkSimpleEsp32.h>
#include "pbl_lcd.h"
#include "pbl_ntu.h"
#include "pbl_ph.h"
#include "pbl_sd.h"
#include "pbl_tds.h"
#include "pbl_temp.h"
#include "pbl_googlesheet.h"
#include "time.h"
#include <Wire.h>
#include <HTTPClient.h>

#define TDS 1
#define PH 2
#define NTU 3
#define TEMP 4

const char* ssid = "Ngoc Anh";  // WiFi SSID
const char* password = "999999999";  // WiFi Password

const char* ntpServer = "pool.ntp.org";  // NTP Server 
const long gmtOffset_sec = 7 * 3600;  // Timezone (UTC+7)
const int daylightOffset_sec = 0;  // Daylight saving time (0: not use)



#define TDS_THRESHOLD 500.0 // ppm
#define PH_MIN 6.0 
#define PH_MAX 8.5
#define NTU_THRESHOLD 100 // NTU
#define WIFI_TIMEOUT_MS 20000

#define RESET_HARDWARE 10

QueueHandle_t dataQueue;
TaskHandle_t sendDataTaskHandle;
TaskHandle_t tdsTaskHandle;
TaskHandle_t tempTaskHandle;
TaskHandle_t phTaskHandle;
TaskHandle_t ntuTaskHandle;

SemaphoreHandle_t resetHardwareSemaphore;
SemaphoreHandle_t BlynkSemaphore;
SemaphoreHandle_t GoogleSheetSemaphore;
SemaphoreHandle_t SDcardSemaphore;
SemaphoreHandle_t LCDSemaphore;

volatile unsigned long lastInterruptTime = 0;  // Lưu thời gian ISR cuối cùng
const unsigned long debounceTime = 1000;        // Thời gian debounce (ms)

/*
  Struct xData: Data struct to store value and source of data
  ucValue: Value of data 
  ucSource: Source of data (from TDS_Task, PH_Task, NTU_Task, TEMP_Task)
*/
typedef struct {
  float ucValue;
  unsigned char ucSource;
} xData;

String getFormattedTime() {
    /*
      Function getFormattedTime: Get current time in format: dd/mm/yyyy hh:mm:ss from NTP server
      Args: None
      Return: String
    */
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "--/--/---- --:--:--";
    }
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
    return String(timeStr);
}

bool isInternetAvailable() {
    HTTPClient http;
    http.begin("http://www.google.com");  // URL kiểm tra
    int httpCode = http.GET();            // Gửi yêu cầu GET

    if (httpCode > 0) {
        Serial.printf("HTTP Status: %d\n", httpCode);
        http.end();
        return true;  // Có kết nối Internet
    } else {
        Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;  // Không có Internet
    }
}

void keepWifiAlive(void * parameters){ 
  for(;;){
    //if connected do nothing and quit
    if(WiFi.status() == WL_CONNECTED){
      if (isInternetAvailable()) {
        Serial.print("WiFi still connected: "); Serial.println(WiFi.localIP().toString().c_str());
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        continue;
      } else {
        Serial.println("No Internet Connection");
        WiFi.disconnect();
      }
    }
    //initiate connection
    Serial.println("Wifi Connecting");
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();

    //Indicate to the user that we are not currently connected but are trying to connect.
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS){
      Serial.print(".");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    //Indicate to the user the outcome of our attempt to connect.
    if(WiFi.status() == WL_CONNECTED){
      if (isInternetAvailable()) {
        Serial.print("[WIFI] Connected: "); Serial.println(WiFi.localIP().toString().c_str());
        Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
        if (Blynk.connected()) {
          Blynk.virtualWrite(Led, 1);
          Serial.println("Connected to Blynk");
          xSemaphoreGive(BlynkSemaphore);
        } else {
          Blynk.virtualWrite(Led, 0);
          Serial.println("Failed to connect to Blynk");
        }

        if (setupGooglesheet()) {
          Serial.println("Google Sheet OK");
          xSemaphoreGive(GoogleSheetSemaphore);
        } else {
          Serial.println("Google Sheet Failed");
        }
      } else {
        Serial.println("No Internet Connection");
        WiFi.disconnect();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
      }
    } else {
      Serial.println("[WIFI] Failed to Connect before timeout");
    }
  }
}  

void resetHardware(void * parameter) {
  /*
    Function resetHardware: Reset hardware using a button
    Args: None
    Return: None
  */
  for (;;) {
    xSemaphoreTake(resetHardwareSemaphore, portMAX_DELAY);
    Serial.println("Resetting hardware...");
    if (setupSD()) {
      Serial.println("SD Card OK");
      xSemaphoreGive(SDcardSemaphore);
    } else {
      Serial.println("SD Card Failed");
    }

    if (setupLCD()) {
      Serial.println("LCD OK");
      xSemaphoreGive(LCDSemaphore);
    } else {
      Serial.println("LCD Failed");
    }

    if (setupTemp()) {
      Serial.println("Temperature sensor OK");
    } else {
      Serial.println("Temperature sensor Failed");
    }
  }
}

static void IRAM_ATTR resetHardwareISR() {
  /*
    Function resetHardwareISR: ISR function to reset hardware
    Args: None
    Return: None
  */
  unsigned long currentInterruptTime = millis();
  
  if (currentInterruptTime - lastInterruptTime > debounceTime) {
      lastInterruptTime = currentInterruptTime;

      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xSemaphoreGiveFromISR(resetHardwareSemaphore, (BaseType_t *)&xHigherPriorityTaskWoken);
      if (xHigherPriorityTaskWoken == pdTRUE) {
          portYIELD_FROM_ISR();
      }
  }
}

void TDS_task(void * parameter) {
  /*
    Task function to get TDS value and send to dataQueue
    Using xTaskNotifyGive to notify sendData_task.
    Using xTaskNotifyTake to wait for notification from sendData_task
  */
  for (;;) {
    xData tdsToSend = {getTDS(), TDS};
    if (uxQueueSpacesAvailable(dataQueue) > 0) {
      xQueueSend(dataQueue, &tdsToSend, portMAX_DELAY);
    }

    if (uxQueueMessagesWaiting(dataQueue) == 4) {
      xTaskNotifyGive(sendDataTaskHandle);
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

void PH_task(void * parameter) {
  /*
    Task function to get PH value and send to dataQueue
    Using xTaskNotifyGive to notify sendData_task.
    Using xTaskNotifyTake to wait for notification from sendData_task
  */
  for (;;) {
    xData phToSend = {getPH(), PH};
    if (uxQueueSpacesAvailable(dataQueue) > 0) {
      xQueueSend(dataQueue, &phToSend, portMAX_DELAY);
    }

    if (uxQueueMessagesWaiting(dataQueue) == 4) {
      xTaskNotifyGive(sendDataTaskHandle);
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

void NTU_task(void * parameter) {
  /*
    Task function to get NTU value and send to dataQueue
    Using xTaskNotifyGive to notify sendData_task.
    Using xTaskNotifyTake to wait for notification from sendData_task
  */
  for (;;) {
    xData ntuToSend = {getNTU(), NTU};
    if (uxQueueSpacesAvailable(dataQueue) > 0) {
      xQueueSend(dataQueue, &ntuToSend, portMAX_DELAY);
    }

    if (uxQueueMessagesWaiting(dataQueue) == 4) {
      xTaskNotifyGive(sendDataTaskHandle);
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

void TEMP_task(void * parameter) {
  /*
    Task function to get TEMP value and send to dataQueue
    Using xTaskNotifyGive to notify sendData_task.
    Using xTaskNotifyTake to wait for notification from sendData_task
  */
  for (;;) {
    xData tempToSend = {getTEMP(), TEMP};
    if (uxQueueSpacesAvailable(dataQueue) > 0) {
      xQueueSend(dataQueue, &tempToSend, portMAX_DELAY);
    }

    if (uxQueueMessagesWaiting(dataQueue) == 4) {
      xTaskNotifyGive(sendDataTaskHandle);
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

String getInfoString(String dateTime, float tds, float ph, float ntu, float temp) {
  /*
    Function getInfoString: Get information of the system
    Args: None
    Return: String
  */
  String info = "Time: " + dateTime + " ";
  info += tds == -1000 ? "TDS: N/A " : "TDS: " + String(tds) + " ppm ";
  info += ph == -1000 ? "PH: N/A " : "PH: " + String(ph) + " ";
  info += ntu == -1000 ? "NTU: N/A " : "NTU: " + String(ntu) + " ";
  info += temp == -1000 ? "Temperature: N/A " : "Temperature: " + String(temp) + " °C ";
  return info;
}

void sendData_task(void * parameter) {
  /*
    Task function to receive data from dataQueue and send to Blynk, Google Sheet, LCD, SD Card
    Using xTaskNotifyTake to wait for notification from TDS_Task, PH_Task, NTU_Task, TEMP_Task
    Using xTaskNotifyGive to notify TDS_Task, PH_Task, NTU_Task, TEMP_Task after sending data
  */
  xData receivedData;
  float tds, ph, ntu, temp;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));
    tds, ph, ntu, temp = -1000;
    int dataQueueSize = uxQueueMessagesWaiting(dataQueue);
    for (int i = 0; i < dataQueueSize; i++) {
      xQueueReceive(dataQueue, &receivedData, portMAX_DELAY);
      if (receivedData.ucSource == TDS) {
        tds = receivedData.ucValue;
        Blynk.virtualWrite(TDS_BLYNK, tds);
      } else if (receivedData.ucSource == PH) {
        ph = receivedData.ucValue;
        Blynk.virtualWrite(PH_BLYNK, ph);
      } else if (receivedData.ucSource == NTU) {
        ntu = receivedData.ucValue;
        Blynk.virtualWrite(NTU_BLYNK, ntu);
      } else if (receivedData.ucSource == TEMP) {
        temp = receivedData.ucValue;
        Blynk.virtualWrite(TEMP_BLYNK, temp); 
      }
    }
    Serial.println("--------------------------------");
    String dateTime = getFormattedTime();

    if (xSemaphoreTake(SDcardSemaphore, pdMS_TO_TICKS(200))) {
      if (printSD(getInfoString(dateTime,tds, ph, ntu, temp)) == true) {
        xSemaphoreGive(SDcardSemaphore);
      } else {
        Serial.println("Failed to save data to SD Card");
      }
    } else {
      Serial.println("SD Card Not Available");
    }

    if (xSemaphoreTake(GoogleSheetSemaphore, pdMS_TO_TICKS(200))) {
      if (sendDataToGoogleSheet(dateTime, tds, ph, ntu, temp) == true) {
        xSemaphoreGive(GoogleSheetSemaphore);
      } else {
        Serial.println("Failed to send data to Google Sheet");
      }
    } else {
      Serial.println("Google Sheet Not Available");
    }

    Serial.println("\n");
    Serial.println(getInfoString(dateTime,tds, ph, ntu, temp));

    if (xSemaphoreTake(LCDSemaphore, pdMS_TO_TICKS(200))) {
      LCDmatrix lcdMatrix[4] = {
      {0, 0, tds == -1000 ? String("N/A") : String(tds)},
      {8, 0, ph == -1000 ? String("N/A") : String(ph)},
      {0, 1, ntu == -1000 ? String("N/A") : String(ntu)},
      {8, 1, temp == -1000 ? String("N/A") : String(temp)}
      };
      printLCD(lcdMatrix);
      xSemaphoreGive(LCDSemaphore);
    } else {
      Serial.println("LCD Not Available");
    }

    xTaskNotifyGive(tdsTaskHandle);
    xTaskNotifyGive(phTaskHandle);
    xTaskNotifyGive(ntuTaskHandle);
    xTaskNotifyGive(tempTaskHandle);

    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  resetHardwareSemaphore = xSemaphoreCreateBinary();
  BlynkSemaphore = xSemaphoreCreateBinary();
  GoogleSheetSemaphore = xSemaphoreCreateBinary();
  SDcardSemaphore = xSemaphoreCreateBinary();
  LCDSemaphore = xSemaphoreCreateBinary();

  attachInterrupt(RESET_HARDWARE, resetHardwareISR, FALLING);

  if (setupSD()) {
    Serial.println("SD Card OK");
    xSemaphoreGive(SDcardSemaphore);
  } else {
    Serial.println("SD Card Failed");
  }

  if (setupLCD()) {
    Serial.println("LCD OK");
    xSemaphoreGive(LCDSemaphore);
  } else {
    Serial.println("LCD Failed");
  }
  
  if (setupTemp()) {
    Serial.println("Temperature sensor OK");
  } else {
    Serial.println("Temperature sensor Failed");
  }

  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult(10000UL) != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi");
  } else {
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
    if (Blynk.connected()) {
      Blynk.virtualWrite(Led, 1);
      Serial.println("Connected to Blynk");
      xSemaphoreGive(BlynkSemaphore);
    } else {
      Blynk.virtualWrite(Led, 0);
      Serial.println("Failed to connect to Blynk");
    }

    if (setupGooglesheet()) {
      Serial.println("Google Sheet OK");
      xSemaphoreGive(GoogleSheetSemaphore);
    } else {
      Serial.println("Google Sheet Failed");
    }
  }
  
  dataQueue = xQueueCreate(4, sizeof(xData));
  /*
    Create tasks for TDS, PH, NTU, TEMP, sendData
  */
  if (dataQueue != NULL) {
    xTaskCreate(resetHardware, "resetHardware", 2048, NULL, 4, NULL);
    xTaskCreate(keepWifiAlive, "keepWifiAlive", 10000, NULL, 3, NULL);
    xTaskCreate(sendData_task, "sendData_task", 5120, NULL, 2, &sendDataTaskHandle);
    xTaskCreate(TDS_task, "TDS_task", 2048, NULL, 1, &tdsTaskHandle);
    xTaskCreate(PH_task, "PH_task", 2048, NULL, 1, &phTaskHandle);
    xTaskCreate(NTU_task, "NTU_task", 2048, NULL, 1, &ntuTaskHandle);
    xTaskCreate(TEMP_task, "TEMP_task", 2048, NULL, 1, &tempTaskHandle);
  } else {
    Serial.println("Failed to create dataQueue");
  }
}

void loop() {}
