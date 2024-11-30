#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include <WiFi.h>
#include <DHT.h>
#include <DHT_U.h>
#include <MQUnifiedsensor.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define Firebase_host ""
#define Firebase_Auth ""
#define STORAGE_BUCKET_ID ""

#define USER_EMAIL ""
#define USER_PASSWORD ""

const char* ssid ="";
const char* password = "";

FirebaseData firebasedata;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long long int date;
int count = 1,total = 0;
bool node=false;
String t,t1,t2;

// Initialize DHT sensor
#define DHTPIN 5 // Pin where the DHT sensor is connected
#define DHTTYPE DHT11 // Change to DHT11 if you're using that sensor
//DHT dht (DHTPIN, DHTTYPE);
// Initialize MQ sensors
#define mq2Pin    32  // Pin where MQ4 is connected (A0)
#define mq135Pin  33 // Pin where MQ135 is connected (A0)
#define mq4Pin    34  // Pin where MQ4 is connected (A0)
#define mq9Pin    35   // Pin where MQ9 is connected (A0)

// Initialize PIR sensor
#define pirPin    36   // Pin where the PIR sensor is connected 
#define tilt_pin 27

DHT_Unified dht(DHTPIN, DHTTYPE);

String pirvalue = "No Motion",tilt_value="No Tilt";
float co2,co,methane,NH4,Alcohol_135,Alcohol_4,Alcohol_8,Propane;

//Definitions
#define         Board                   ("ESP-32")
/***********************Software Related Macros************************************/
#define         Voltage_Resolution      (5)
#define         ADC_Bit_Resolution      (12) 

#define         Type135 "MQ-135" //MQ135
#define         RatioMQ135CleanAir (3.6)//RS / R0 = 3.6 ppm  
#define         Type9                    ("MQ-9") //MQ9
#define         RatioMQ9CleanAir        (9.6) //RS / R0 = 60 ppm 
#define         Type4                    ("MQ-4") //MQ4
#define         RatioMQ4CleanAir        (4.4) //RS / R0 = 60 ppm 
#define         Type2                    ("MQ-2") //MQ2
#define         RatioMQ2CleanAir        (9.83) //RS / R0 = 9.83 ppm 

//Declare Sensor
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, mq135Pin, Type135);
MQUnifiedsensor MQ9(Board, Voltage_Resolution, ADC_Bit_Resolution, mq9Pin  , Type9);
MQUnifiedsensor MQ4(Board, Voltage_Resolution, ADC_Bit_Resolution, mq4Pin , Type4);
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, mq2Pin, Type2);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define error_pin 39

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(error_pin, OUTPUT); 
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);}
    
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Staring new process...");
  display.display();
  //// Connect to Wi-Fi
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi");
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println("Connecting to WiFi");
    display.display();
  }
  display.clearDisplay();
  
  Serial.println("Connected to WiFi");
  Serial.print ("IP address: ");
  Serial.println (WiFi.localIP());

  /* Assign the api key (required) */
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
    display.println(config.signer.signupError.message.c_str());
    display.display();
  }
  display.clearDisplay();
  
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize DHT sensor
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  // Initialize PIR sensor
  pinMode (pirPin, INPUT);
  pinMode (tilt_pin,INPUT);
  
  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
//  MQ135.setA(110.47); MQ135.setB(-2.862); 
  MQ135.init(); 
  MQ9.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ9.setA(599.65); MQ9.setB(-2.244);
  MQ9.init(); 
  MQ4.setRegressionMethod(1); //_PPM =  a*ratio^b
//  MQ4.setA(1012.7); MQ4.setB(-2.786); 
  MQ4.init();
  MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ2.setA(658.71); MQ2.setB(-2.168);    
  MQ2.init(); 

  Serial.print("Calibrating please wait.");
//  delay(300000);
  float calcR0_135 = 0;
  float calcR0_9 = 0;
  float calcR0_4 = 0;
  float calcR0_2 = 0;
  
  for(int i = 1; i<=10; i++)
  {
    MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0_135 += MQ135.calibrate(RatioMQ135CleanAir);
    MQ9.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0_9 += MQ9.calibrate(RatioMQ9CleanAir);
    MQ4.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0_4 += MQ4.calibrate(RatioMQ4CleanAir);
    MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0_2 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0_135/10);
  MQ9.setR0(calcR0_9/10);
  MQ4.setR0(calcR0_4/10);
  MQ2.setR0(calcR0_2/10);
  Serial.println("  done!.");
  
  if(isinf(calcR0_135)) {Serial.println("Warning: Conection issue, R0_135 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0_135 == 0){Serial.println("Warning: Conection issue found, R0_135 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  if(isinf(calcR0_9)) {Serial.println("Warning: Conection issue, R0_9 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0_9 == 0){Serial.println("Warning: Conection issue found, R0_9 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  if(isinf(calcR0_4)) {Serial.println("Warning: Conection issue, R0_4 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0_4 == 0){Serial.println("Warning: Conection issue found, R0_4 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  if(isinf(calcR0_2)) {Serial.println("Warning: Conection issue, R0_2 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0_2 == 0){Serial.println("Warning: Conection issue found, R0_2 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  /*****************************  MQ CAlibration ********************************************/ 
  MQ135.serialDebug(true);
  MQ9.serialDebug(true);
  MQ4.serialDebug(true);
  MQ2.serialDebug(true);

//  delay(600000);
}

void loop() {

// Read temperature and humidity from DHT sensor
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  float temperature = event.temperature;
  dht.humidity().getEvent(&event);
  float humidity = event.relative_humidity;
  if (isnan (temperature) | isnan (humidity)) {
  Serial.println("Failed to read from DHT sensor!");
  delay(2000);
  return;
  }
  
  MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
  MQ135.setA(110.47); MQ135.setB(-2.862); 
  co2 = MQ135.readSensor(); //Configure the equation to calculate CO2 concentration value
  MQ135.setA(102.2 ); MQ135.setB(-2.473); // Configure the equation to calculate NH4 concentration value
  NH4 = MQ135.readSensor();
  MQ135.setA(77.255); MQ135.setB(-3.18); //Configure the equation to calculate Alcohol concentration value
  Alcohol_135 = MQ135.readSensor();
  MQ135.serialDebug(); // Will print the table on the serial port
  
  MQ9.update(); // Update data, the arduino will read the voltage from the analog pin
  co = MQ9.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  MQ9.serialDebug(); // Will print the table on the serial port
  
  MQ4.update(); // Update data, the arduino will read the voltage from the analog pin
  MQ4.setA(1012.7); MQ4.setB(-2.786);
  methane = MQ4.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  MQ4.setA(60000000000); MQ4.setB(-14.01);
  Alcohol_4 = MQ4.readSensor();
  MQ4.serialDebug(); // Will print the table on the serial port
  
  MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
  Propane = MQ2.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  MQ2.serialDebug();

  if(digitalRead(pirPin) == HIGH){
      pirvalue = "Motion Detected";
    }
  else{
      pirvalue = "No Motion";
  }
  if(digitalRead(tilt_pin) == 1){
      tilt_value = "Tilt Detected";
    }
  else{
      tilt_value = "No Tilt";
  }

    display.println("Reading Data...");
    display.display();
    delay(30000);
    display.clearDisplay();
    display.setCursor(0, 10);
    
    Serial.print("Temperature:");Serial.println(temperature);   
    Serial.print("Humidity:");Serial.println(humidity);
    Serial.print("CO2:");Serial.println(co2);
    Serial.print("CO:");Serial.println(co);
    Serial.print("NH4:");Serial.println(NH4);
    Serial.print("Alcohol_135:");Serial.println(Alcohol_135);
    Serial.print("Alcohol_4:");Serial.println(Alcohol_4);
    Serial.print("Methane:");Serial.println(methane);
    Serial.print("Propane:");Serial.println(Propane);
    Serial.print("Motion_Detected:");Serial.println(pirvalue);
    Serial.print("Tilt:");Serial.println(tilt_value);
  
  if(Firebase.ready()){
      count=1;
      Firebase.RTDB.setTimestamp(&firebasedata,"Date");
      if(Firebase.RTDB.getDouble(&firebasedata,"Date")){
        if(firebasedata.dataType()=="double"){
        date = uint64_t(firebasedata.doubleData());
        if (!node){
          t = "Fruit_data_" + String(date);
          t2=t+"/Restart";
          Firebase.RTDB.setString(&firebasedata,t2.c_str(),"No Restart");
          node = true;
        }}} 
        
    if(Firebase.RTDB.getString(&firebasedata,t2.c_str())){
      if(firebasedata.dataType() == "string"){
        if(firebasedata.stringData() == "Restart"){
          display.println("Restarting.......");
          display.display();
          delay(30000);
          abort();}}}

    t1 = String(date);
    FirebaseJson json;
    json.add(t1+"_"+"Temperature:",temperature);
    json.add(t1+"_"+"Humidity:",humidity);
    json.add(t1+"_" +"CO2 in ppm:",co2);
    json.add(t1+"_"+"CO in ppm:",co);
    json.add(t1+"_"+"NH4 in ppm:",NH4);
    json.add(t1+"_"+"Methane:",methane);
    json.add(t1+"_"+"Propane:",Propane);
    json.add(t1+"_"+"Alcohol_4:",Alcohol_4);
    json.add(t1+"_"+"Alcohol_135:",Alcohol_135);
    json.add(t1+"_"+"Motion_Detected:",pirvalue);
    json.add(t1+"_"+"Tilt:",tilt_value);
    
    if (Firebase.RTDB.updateNode(&firebasedata, t.c_str(), &json)){
        Serial.println("PASSED");
        total = total+1;
        display.print("PASSED-");
        display.println(total);
        display.display();
      }
      else {
        Serial.println("FAILED REASON: " + firebasedata.errorReason());
        display.println(firebasedata.errorReason());
        display.println(total);
        display.println("Restarting.....");
        display.display();
        digitalWrite(error_pin,HIGH);
        delay(10000);
        digitalWrite(error_pin,LOW);
        abort();
      }}
    else{
      if(count%5 == 0)abort();
      display.println("Firebase is not ready");
      display.display();
      count++;
    }
    delay(30000);
    display.clearDisplay();
    display.println("Wait for 1 Minute");
    display.display();
    delay(30000);
    display.clearDisplay();
    display.setCursor(0, 10);
 
//Delay before reading again
  
}
