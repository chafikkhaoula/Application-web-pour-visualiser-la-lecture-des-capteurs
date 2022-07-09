#include <SoftwareSerial.h>
#include <DHT.h>
#include <Firebase.h>
#include <FirebaseFS.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define DHT_PIN D2
#define rxPin D5
#define txPin D6
#define sensor_pin A0  /* Connect Soil moisture analog sensor pin to A0 of NodeMCU */

// Insert your network credentials
#define WIFI_SSID "Khaoulac"
#define WIFI_PASSWORD "lola1999"

// Insert Firebase project API Key
#define API_KEY "AIzaSyBfqNycFwWuA0Ez8-a19xf5mfL7mSqMw80"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "khchafik99@gmail.com"
#define USER_PASSWORD "Chafik1999"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "iot-last-version-default-rtdb.firebaseio.com"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;

// Database child nodes
String tempPath = "/temperature";
String humPath = "/humidity";
String soilPath = "/humidityofsoil";
String timePath = "/timestamp";


float t,h,humS;

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 18000;

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  
  while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
  }
  
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Parent Node (to be updated in every loop)
String parentPath;

// Variable to save current epoch time
int timestamp;

FirebaseJson json;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Function that gets current epoch time
unsigned long getTime() {
    timeClient.update();
    unsigned long now = timeClient.getEpochTime();
    return now;
}

#define DHTTYPE DHT22

 DHT dht(DHT_PIN, DHTTYPE);

SoftwareSerial sim800(rxPin, txPin);

String smsStatus, senderNumber, receivedDate, msg;
const String PHONE = "+212620111267";

void setup() {

  Serial.begin(115200);
  Serial.println("Esp8266 serial initialize");
  dht.begin();
  Serial.println("initialize DHT22");
  
  initWiFi();
  timeClient.begin();

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }

  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";

   sim800.begin(9600);
  sim800.println("AT+CMGF=1");
  delay(2000);
}

void loop()
{

  //Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  //Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  
  float humS = 100.00 - ( (analogRead(sensor_pin) / 1023.00) * 100.00 ) ;

    // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    //Get current timestamp
    timestamp = getTime();
    Serial.print ("time: ");
    Serial.println (timestamp);
    parentPath = databasePath + "/" + String(timestamp);
    if (isnan(t) || isnan(h) || isnan(humS))                                   // Checking sensor working
    {
      Serial.println(F("Failed to read from DHT sensor and Soil Moisture Sensor!"));
      return;
    } else {
      json.set(tempPath.c_str(), String(t));
      json.set(humPath.c_str(), String(h));
      json.set(soilPath.c_str(), String(humS));
      json.set(timePath, String(timestamp));
      Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    }
  }
  
  String buff;  
  
  //°C
  String humidity = "Humidity: " + String(h) + "%";
  String temperature = "Temperature: " + String(t) + " Celsius";
  String hum_soil = "Humidty of soil: " + String(humS) + " %";
  String result = humidity + "\n" + temperature + "\n" + hum_soil;
  
  while (sim800.available()) {
    
    buff = sim800.readString();
    Serial.println(buff);
    unsigned int len, index;

    //Remove sent "AT Command" from the response string.
    index = buff.indexOf("\r");
    buff.remove(0, index + 2);
    buff.trim();
    
    //--------------------------------------------------------------------
    if (buff != "OK") {
      index = buff.indexOf(":");
      String cmd = buff.substring(0, index);
      cmd.trim();
      buff.remove(0, index + 2);

          //____________________________________________________________
          if (cmd == "+CMTI") {
            //get newly arrived memory location and store it in temp
            index = buff.indexOf(",");
            String temp = buff.substring(index + 1, buff.length());
            temp = "AT+CMGR=" + temp + "\r";
            //get the message stored at memory location "temp"
            sim800.println(temp);
             }
          //____________________________________________________________
          else if (cmd == "+CMGR") {
            extractSms(buff);
            Serial.println(msg);
            Serial.println(senderNumber);

                    if (senderNumber == PHONE && msg == "get temperature") {
                      Reply(result);
                    }
             }
      //____________________________________________________________
        }
 }
   
 //--------------------------------------
  while (Serial.available())  {
    sim800.println(Serial.readString());
  }

}

void Reply(String text)

{
  if (text == "") {
    return;
  }

  sim800.print("AT+CMGF=1\r");
  delay(1000);
  sim800.print("AT+CMGS=\"" + PHONE + "\"\r");
  delay(1000);
  sim800.print(text);
  delay(100);
  sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
  delay(1000);
  Serial.println("SMS Sent Successfully.");
}

void extractSms(String buff) {
  unsigned int index;

  index = buff.indexOf(",");
  smsStatus = buff.substring(1, index - 1);
  buff.remove(0, index + 2);

  senderNumber = buff.substring(0, 13);
  buff.remove(0, 19);

  receivedDate = buff.substring(0, 20);
  buff.remove(0, buff.indexOf("\r"));
  buff.trim();

  index = buff.indexOf("\n\r");
  buff = buff.substring(0, index);
  buff.trim();
  msg = buff;
  buff = "";
  msg.toLowerCase();
}
