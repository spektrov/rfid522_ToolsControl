#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "ArduinoJson.h"
#include <string.h>
#include <time.h>


const String apiUrl = "http://toolscontrol-001-site1.htempurl.com/api/";

#define SS_PIN D8
#define RST_PIN D3

WiFiManager wifiManager;
WiFiClient wifiClient;
HTTPClient http;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// Init array that will store new NUID
byte nuidPICC[4];

// If equipment is available.
bool isFree = true;

time_t startTime;
time_t finishTime;


void setup() {
  Serial.begin(115200);
  SPI.begin();     
  rfid.PCD_Init();

  Serial.println("Connecting to Wi-Fi...");
  wifiManager.autoConnect("CONNECT Wi-Fi");
  Serial.println("Connected to Wi-Fi.");

  timeClient.begin();
  timeClient.setTimeOffset(7200);

  lcd.init();
  lcd.backlight();

  void printScanMessageOnDisplay();
}


void loop() {
  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  lcd.clear();
  // If new card read.
  if (!cardsAreEqual(rfid.uid.uidByte, nuidPICC)) {
    // Already in usage.
    if (!isFree) {
      lcd.print(" Not available. ");
      delay(2000);
      lcd.clear();
      lcd.print(" In use of:     ");
      printCardNumberOnDisplay();
    } else {
      // Store NUID into nuidPICC array
      for (byte i = 0; i < 4; i++) {
        nuidPICC[i] = rfid.uid.uidByte[i];
      }

      if (WiFi.status() == WL_CONNECTED) {
        String cardN = getCardNumberString();
        String hasAccess = checkAccess(cardN);
                
        // If access granted
        if (hasAccess == "true") {
          isFree = false;
          timeClient.update();
          startTime = timeClient.getEpochTime();
          lcd.print(" Start usage! ");
          printCardNumberOnDisplay();
          delay(2000);
          lcd.setCursor(0, 0);
          lcd.print(" In use of:     ");
          setAvailability(false);
        } 
        else {
          lcd.print(" Not allowed.");
          delay(2000);
          printScanMessageOnDisplay();
        }
      }
    }
  } 
  else {
    // End of usage.
    if (!isFree) {
      finishTime = timeClient.getEpochTime();
      isFree = true;
      nuidPICC[0] = 0;
      lcd.clear();
      lcd.print(" Usage finished. ");
      printTimeDifferenceOnDisplay(finishTime - startTime);
      setAvailability(true);
      sendUsage();
    } 
    else {
      lcd.print(" Not allowed.");
    }

    delay(2000);
    printScanMessageOnDisplay();
  }

  rfid.PICC_HaltA(); // Halt PICC
  rfid.PCD_StopCrypto1(); // Stop encryption on PCD
}


bool cardsAreEqual(byte a[4], byte b[4]) {
  for (int i = 0; i < 4; i++) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}


void printScanMessageOnDisplay() {
  lcd.clear();
  lcd.print(" Access Control ");
  lcd.setCursor(0, 1);
  lcd.print("Scan Your Card>>");
}


void printCardNumberOnDisplay() {
  lcd.setCursor(0, 1);
  lcd.print(" No:");
  for (byte i = 0; i < 4; i++) 
  {
    lcd.print(nuidPICC[i], DEC);
  }
}


void printTimeDifferenceOnDisplay(int time) {
  lcd.setCursor(2, 1);
  int hours = time / 3600;
  int minutes = (time - 3600 * hours) / 60;
  int seconds = (time - 3600 * hours - 60 * minutes);
  if (hours < 10)
    lcd.print("0");
  lcd.print(hours);
  lcd.print(":");
  if (minutes < 10)
    lcd.print("0");
  lcd.print(minutes);
  lcd.print(":");
  if (seconds < 10)
    lcd.print("0");
  lcd.print(seconds);
}


void printDateOnDisplay() {
  lcd.clear();
  time_t epochTime = timeClient.getEpochTime();
  struct tm* ptm = gmtime((time_t*)&epochTime);
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  int currentYear = ptm->tm_year + 1900;
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  lcd.print(currentDate);
  lcd.setCursor(0, 1);
  String formattedTime = timeClient.getFormattedTime();
  lcd.print(formattedTime);
}


void setAvailability(bool value) {
  String request = apiUrl;
  request += "equipments/47011f31-f992-4641-7723-08dae284943f/available?isAvailable=";
  request += value;

  http.begin(wifiClient, request); 
  int httpCode = http.PUT("");
  Serial.println(request);
  Serial.println(httpCode);
  if (httpCode > 0) {
    String messageResponse = http.getString();
  }
  http.end();
}


void sendUsage() {
   String cardN = getCardNumberString();
      String sT = epochTimeToDateString(startTime);
      String fT = epochTimeToDateString(finishTime);

      String request = apiUrl;
      request += "usages?equipmentId=47011f31-f992-4641-7723-08dae284943f";
      request += "&workerCard=";
      request += cardN;
      request += "&start=";
      request += sT;
      request += "&finish=";
      request += fT;

      http.begin(wifiClient, request);
      int httpCode = http.POST("");
      Serial.println(request);
      Serial.println(httpCode);
      if (httpCode > 0) {
        String messageResponse = http.getString();
        Serial.println(messageResponse);
      }
      http.end();
}


String checkAccess(String cardN) {
  String hasAccess;
  String request = apiUrl;
  request += "equipments/47011f31-f992-4641-7723-08dae284943f/verify?cardNumber=";
  request += cardN;

  http.begin(wifiClient, request); 
  int httpCode = http.GET();       
  Serial.println(request);
  Serial.println(httpCode);
  if (httpCode > 0) { 
    hasAccess = http.getString();
  }
  http.end(); 
  return hasAccess;
}


String epochTimeToDateString(time_t time) {
  struct tm ts;
  char buf[80];
  ts = *localtime(&time);
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &ts);
  return buf;
}


String getCardNumberString() {
  String cardN = "";
  for (byte i = 0; i < 4; i++) {
    cardN += rfid.uid.uidByte[i];
  }
  return cardN;
}
