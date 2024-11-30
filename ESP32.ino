#define RXp2 16
#define TXp2 17
#define START_PIN 4

#include "Arduino.h"
//#include "soc/soc.h"           // Disable brownout problems
//#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
//#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <WiFi.h>
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//Replace with your network credentials
#define Firebase_host ""
#define Firebase_Auth ""

#define USER_EMAIL ""
#define USER_PASSWORD ""

const char* ssid ="";
const char* password = "";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "freshnessinfruit-a56b6.appspot.com"

// Photo File Name to save in LittleFS
#define FILE_PHOTO "/photo.txt"
#define BUCKET_PHOTO "/data/photo.txt"

bool takeNewPhoto = true;
String data1;
String data2;

void initWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
}

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
}

void capturePhotoSaveSpiffs(){
  File file = SPIFFS.open("/photo.txt","w");
  while(!file){
    Serial.println("Failed to open file");
    File file = SPIFFS.open("/photo.txt","w");}
  Serial.println("File opened");
//  digitalWrite(START_PIN,HIGH);
//  delay(10);
  int i=0;
  int index;
  if(Serial2.readStringUntil('\n') == "START FRAME");
  else while(Serial2.readStringUntil('\n') != "START FRAME");
  Serial.println("Frame Started");
  data2 = Serial2.readStringUntil('\n');
  while(data2 != "STOP FRAME" && data2!= ">>>>>>>"){
      if(data2.substring(0,5) == "Count"){
        index = data2.indexOf(':');
        if(i == data2.substring(5,index).toInt()){
          data1 = data2.substring(index+1);
//          Serial.println(data1);
          file.println(data1);
          i++;
        }
      }
      data2 = Serial2.readStringUntil('\n');
  }
  file.close();
  Serial.println("File closed");
  return;
  }

void setup() {
  // put your setup code here, to run once:
Serial.begin(250000);
Serial2.begin(500000,SERIAL_8N1,RXp2,TXp2);

  initWiFi();
  initSPIFFS();
  pinMode(START_PIN,OUTPUT);
  digitalWrite(START_PIN,LOW);

  // Assign the api key
  config.api_key = Firebase_Auth;

  /* Assign the RTDB URL (required) */
  config.database_url = Firebase_host;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // put your main code here, to run repeatedly:
  while(Serial2.readStringUntil('\n') != ">>>>>>>")Serial.println("Starting to read frame");
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  if (Firebase.ready()){

    Serial.print("Uploading picture... ");

    //MIME type should be valid to avoid the download problem.
    //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, BUCKET_PHOTO /* path of remote file stored in the bucket */, "text/plain" /* mime type */,NULL)){
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
          while(!SPIFFS.remove("/photo.txt"));
          takeNewPhoto = true;
    }
    else{
      Serial.println(fbdo.errorReason());
    }
  }}
