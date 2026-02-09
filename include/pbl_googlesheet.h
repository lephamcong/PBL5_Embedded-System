#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include <ESP_Google_Sheet_Client.h>
#include <Wire.h>
// For SD/SD_MMC mounting helper
#include <GS_SDHelper.h>

// Google Project ID
#define PROJECT_ID "wqms-datalogging"

// Service Account's client email
#define CLIENT_EMAIL "wqms-datalogging@wqms-datalogging.iam.gserviceaccount.com"

// Service Account's private key
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCPbfF1FmLIM+bN\n1i4l7LUso4mnH4QfeaQYDg7+g0bXbyyi90HcKOHF9RoKBOOQQbrdLwuGWXTlUD0Q\nOVk7poja5dsUUOn5p4KwoB43O5B6Z6CPsWsCZQdU8R7jTlPXaOHgh52uu9tuuWQT\nbi4WVuYsN7NpnH0Vkxis/F/62JaJUQNWABqnez2Ct74ai+BzkHw7CuJ/6SebROSo\nP0NhvaPh8fHi4X0hCcEmlJPJA20hITp6irat4Bs9VY9BmhJgYJOkuKkHE3es2qah\nro/tVUbywYi0DTMa+XRV+qDWOi1yKeI4VPH2eQMKl8Or5Q+JCq2pltpp4zpbcwyA\nx53Y3sfrAgMBAAECggEAFJ77nRg9LDLSFyaY90ON/2n6a3hg2DRL6a1zwxcqreSX\np/fL6emddDGQryoUHEm8TPqWOe2jRyjUbqsKxO8PiVviCzdgPFA0vkzMVf05VhqP\nVLlu/NF7pAm6pXbodPWTrA1eF/YoTptW47hD5gWpDWhrYglpWRsPQXz+Fk7iGGxs\nkvhUlAALRBxM8bLeYVoUih3XR6cClh8l+GAm/NJcwSQGeAjUnVGzg9gukk7uKv6g\n2+LTMYgvS8Oa9LOFgDQX72kuuhPXPXuEEiRRPuu61ZhUZEahTy06w+wwXG6Euu8v\nE36bRLihQ2NUT5zjxxovKNr8/a3P48zeNgw5Q1aKUQKBgQDAwnMUI1faqVNVAJOJ\n025J+g3Hg4YChn71HggG2O161qvxmoARPgHwBqikuOpHV+fT5w6ZXC/+uWWlR1Rc\nM/tmv6MADNF0inQoHHxYEbtHSVS2BdoA17I3D/BIEdwaYft3S198U3TmcCuBaXIw\nirMSLWOaxqWtFy69PkVlqU6zqQKBgQC+fFd38ujR0HJ2+dxeqWUZf0KH4st4HdiP\n+sjh2ji2A0qmDR2iT/QYqk3XZNzKjYwlExl1vv+mdM5y4qrLdGDTEWBx445QS8bu\nTgy2uUvUY93id7TZ8vf/H6/5Kv1rTCM93oW3xHk4wmZ4iWaiWffDr5UuP8mPEgTX\nY+pTE89bcwKBgQC+/23b5HFm/yTNMzb9+Hxt3NHOgapF0lL2On2lm0kk0JWrXnXL\nn+93kIGGRpwNeTMDKw2yNEByW+416QnUGsXet8ChihH5Mj+Khz9GXLt/FVBU1FOB\nRjkWijqCjv/TPIdZW1wP6voIY9kX8A3vv19UgZkSggckDbaIWa2V4D9VkQKBgCnP\nfx97+PB80XMd+unoQqGrFj5fYIvX+T35LPit/n6tEEiMXnHPHOQBFZ7uq6vmD9nf\nbFo090ZhIyOCbzzDKbGKgeHdsdIcH+kUxqOg3m0bEovv/IlOVHLyUJzfe8p+Zsng\nuvcQMA9uVDGm7xk5qDGSq6sAI0y9BsUEUF213nDTAoGBAJ0BhEhZESpk+X4t95Bv\nz9wDz017ejeRbtFIxtPFLQglqifL6kULZ1b8PjrPs5S2UHaA2EXsxTPjmuuVylbH\n9VFxIsZ9QTYwIwALEgwviw16X9ej7udqkY9PYEMMtzuYCY3SzblxT8MIdrGCC+6E\nTbC80xM3tNvfw/oVzByKCGz8\n-----END PRIVATE KEY-----\n";

// The ID of the spreadsheet where you'll publish the data
const char spreadsheetId[] = "1a0r71QXox47Uq7B5bjzHlSt0WoSMAvK_dq2coZj-MQ0";

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 0;

// Token Callback function
void tokenStatusCallback(TokenInfo info) {
    if (info.status == token_status_error) {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}


bool setupGooglesheet() {
    Serial.begin(115200);
    Serial.println();
    Serial.println();

    // Configure time
    // configTime(7 * 3600, 0, ntpServer); // Adjust for Vietnam timezone (GMT+7)
    try{
        GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);

        // Set the callback for Google API access token generation status (for debug only)
        GSheet.setTokenCallback(tokenStatusCallback);

        // Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
        GSheet.setPrerefreshSeconds(10 * 60);

        // Begin the access token generation for Google API authentication
        GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
        return true;
    } catch (...) {
        return false;
    }
}

bool sendDataToGoogleSheet(String dateTime, float tds, float ph, float ntu, float temp) {
    // Call ready() repeatedly in loop for authentication checking and processing
    bool ready = GSheet.ready();

    if (ready && millis() - lastTime > timerDelay) {
        try {
            lastTime = millis();

            FirebaseJson response;

            // Serial.println("\nAppend spreadsheet values...");
            // Serial.println("----------------------------");

            FirebaseJson valueRange;

            valueRange.add("majorDimension", "COLUMNS");
            valueRange.set("values/[0]/[0]", dateTime);
            if (tds == -1000) {
                valueRange.set("values/[1]/[0]", "N/A");
            }
            else {
                valueRange.set("values/[1]/[0]", String(tds));
            }
            if (ph == -1000) {
                valueRange.set("values/[2]/[0]", "N/A");
            }
            else {
                valueRange.set("values/[2]/[0]", String(ph));
            }
            if (ntu == -1000) {
                valueRange.set("values/[3]/[0]", "N/A");
            }
            else {
                valueRange.set("values/[3]/[0]", String(ntu));
            }
            if (temp == -1000) {
                valueRange.set("values/[4]/[0]", "N/A");
            }
            else {
                valueRange.set("values/[4]/[0]", String(temp));
            }

            // Append values to the spreadsheet
            bool success = GSheet.values.append(&response, spreadsheetId, "Sheet1!A2", &valueRange);
            if (success) {
                response.toString(Serial, true);
                valueRange.clear();
            }
            else {
                Serial.println(GSheet.errorReason());
            }
            // Serial.println();
            // Serial.println(ESP.getFreeHeap());
            return true;
        } catch (...) {
            return false;
        }
    }else {
        return false;
    }
}