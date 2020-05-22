/*
 shellyEMmockup.ino - Simple mockup alternative to shelly EM Energy Meter firmware
  Created by APR
  May 21, 2020

  Released into the public domain.


  Features:
  - press button for 2 seconds to toggle relay.
  - LED follows Relay status
  - Communcate with ADE7953 and PCF85363A via I2C
  - Publish energy readings to emoncms server every 10s
  - Publish RTC time and energy readings to MQTT every 1s
  - WebUpdater.ino code included for easy updates OTA - http://<IP_address>/update

  Caveats:
  - Calibration method should be vastly improved. 
    AN-1118.pdf doc below is too much for me atm.

  Disclaimer & instructions:
  - AC mains can kill you. You wont be able to undo any mistake after you're dead.
  - You CANNOT connect shelly to mains AC and to the programming pins at the same time.
    Disconnect the device from any mains and program it with this firmware as a starting
    point. Check serial data for IP address, check if Emoncms and MQTT is working,
    disable the features you don't want on the code. After everything is tested disconnect
    the programming pins and connect to AC mains and test again. From there you can update 
    the ESP8266 using the webupdater.

    

  Credits:
  https://github.com/esp8266/Arduino
  https://github.com/CalPlug/ADE7953-Wattmeter
  https://github.com/arendst/Tasmota
  https://github.com/MacWyznawca/ADE7953_ESP8266
  https://tronixstuff.com/2013/08/13/tutorial-arduino-and-pcf8563-real-time-clock-ic/

  Datasheets:
  https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf
  https://www.nxp.com/docs/en/data-sheet/PCF85363A.pdf
  https://www.analog.com/media/en/technical-documentation/data-sheets/ADE7953.pdf
  https://www.analog.com/media/en/technical-documentation/application-notes/AN-1118.pdf

*/

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h> 
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <Wire.h>

// ******************************************************************* APR
#define WIFI_SSID   "YOUR_WIFI_SSID"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"

// comment line below to disable MQTT or emoncms
#define MQTT_ENABLE
#define EMONCMS_ENABLE
// -- end

#ifdef MQTT_ENABLE
#define MQTT_SERVER        "MQTT_IP_ADDRESS"
#define MQTT_PORT          1883
#define MQTT_ID            "SHELLYAPR"
#define MQTT_PUB_TOPIC     "shellyAPR/data"
#define MQTT_PUB_RTC_TOPIC "shellyAPR/RTC"
#endif

#ifdef EMONCMS_ENABLE
#define EMONCMS_URL   "http://192.168.0.1/emoncms"
#define EMONCMS_API   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define EMONCMS_NODE  "shellyAPR"
#endif

// ***********************************************************************

/* ***********************************************************************
How to Calibrate ADE7953_UREF, ADE7953_IREF and ADE7953_PREF:

NEW_REF = (meter_reading * OLD_REF) / (correct_reading)

Example for voltage:
meter_reading=240,90V; correct_reading=240,74V; OLD_REF = 26182

NEW_REF = 240,90 * 26182 / 240,74 = 26199
************************************************************************ */
#define ADE7953_UREF            26199
#define ADE7953_IREF            8716
#define ADE7953_PREF            1338


// shelly EM pinout
#define LED     2
#define BUTTON  4
#define RELAY   15
#define SDA_PIN 12
#define SCL_PIN 14

#define I2C_MASTER       0x42
#define PCF8563_address  0x51
#define ADE7953_address  0x38

#define VRMS_32  0x31C //VRMS, (R) Default: 0x000000, Unsigned, VRMS register (32 bit)
#define IRMSA_32 0x31A //IRMSA, (R) Default: 0x000000, Unsigned,IRMS register (Current Channel A)(32 bit)
#define IRMSB_32 0x31B //IRMSB, (R) Default: 0x000000, Unsigned,IRMS register (Current Channel B)(32 bit)
#define AWATT_32 0x312 //AWATT, (R) Default: 0x000000, Signed,Instantaneous active power (Current Channel A)(32 bit)
#define BWATT_32 0x313 //BWATT, (R) Default: 0x000000, Signed,Instantaneous active power (Current Channel B)(32 bit)

// RTC functions
byte bcd2dec(byte value);
byte dec2bcd(byte value);
void getPCF8563(char str[], int str_size );
void setPCF8563(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
// interesting unused feature: 64 byte RAM from 0x40 to 0x7F

// button functions
bool button_state = 1;
uint8_t button_timeout = 2;
void handleButton();

// ADE7953 functions
uint32_t read_ADE7953(uint16_t reg);
void write_ADE7953(uint16_t reg, uint32_t val);
int Ade7953RegSize(uint16_t reg);
void getADE7953(char str[], int str_size );


unsigned long everysecond = 0;

#ifdef EMONCMS_ENABLE
uint8_t publishEmoncms = 10;  // trigger emoncms update ever 10 seconds
#endif

#ifdef MQTT_ENABLE
WiFiClient espClient;
PubSubClient client(espClient);
#endif 

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;



void setup()
{
  Serial.begin(115200);
  Serial.println("");

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while ( WiFi.status() != WL_CONNECTED ) delay(500);
  Serial.printf("IP address: %s", WiFi.localIP().toString().c_str() );

#ifdef MQTT_ENABLE
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.connect(MQTT_ID);
  while (!client.connected()) delay(500);
#endif 

  httpUpdater.setup(&httpServer);
  httpServer.begin();
  Wire.begin(SDA_PIN, SCL_PIN, I2C_MASTER);

  // setup_ADE7953
  write_ADE7953(0x102,0x0004);  
  write_ADE7953(0x0FE,0x00AD);
  write_ADE7953(0x120,0x0030);

}

void loop() {

  client.loop();
  httpServer.handleClient();
  
  unsigned long now = millis();
  if (now - everysecond > 1000) 
  {
    everysecond = now;
  
    handleButton();

    char msg[100];
    getPCF8563(msg, 100); // get RTC data
    Serial.println(msg);
#ifdef MQTT_ENABLE
    if (client.connected()) client.publish(MQTT_PUB_RTC_TOPIC, msg);
#endif

    getADE7953(msg, 100); // get Energy Data
    Serial.println(msg);

#ifdef MQTT_ENABLE
    if (client.connected()) client.publish(MQTT_PUB_TOPIC, msg);
#endif

#ifdef EMONCMS_ENABLE
    if(!publishEmoncms) 
    {
      publishEmoncms = 10;

      char url[300] = {0};
      snprintf (url, 300, "%s/input/post.json?apikey=%s&node=%s&json=%s", 
        EMONCMS_URL,
        EMONCMS_API,
        EMONCMS_NODE,
        msg ); 

      HTTPClient http;
      WiFiClient httpclient;                                  
      http.begin(httpclient,url);                                      
      http.GET();                              
      http.end();                                 
    }
    else publishEmoncms--;
#endif

  }
}




void handleButton()
{
  if(button_state != digitalRead(BUTTON) )
  {
    button_state = !button_state;
  }
  if(!button_state)
  {
    Serial.printf("Button...%d\n",button_timeout);

    if(button_timeout) button_timeout--;
    else 
    {
      bool relay_status = digitalRead(RELAY);
      digitalWrite( RELAY, !relay_status );
      relay_status ? digitalWrite( LED, LOW ) : digitalWrite( LED, HIGH );
    }
  }
  else button_timeout = 3;
}


void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

byte bcd2dec(byte value) { return ((value/16)*10 + value % 16); }
byte dec2bcd(byte value) { return (value/10 * 16 + value % 10); }

void getPCF8563(char str[], int str_size )
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  const char* days[] = { "Sunday", "Monday","Tuesday","Wednesday","Thursday","Friday"};

  Wire.beginTransmission(PCF8563_address);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(PCF8563_address,7);
  second     = bcd2dec(Wire.read() & B01111111);
  minute     = bcd2dec(Wire.read() & B01111111);
  hour       = bcd2dec(Wire.read() & B00111111);
  dayOfMonth = bcd2dec(Wire.read() & B00111111);
  dayOfWeek  = bcd2dec(Wire.read() & B00000111);
  month      = bcd2dec(Wire.read() & B00011111);
  year       = bcd2dec(Wire.read() );

  snprintf(str, str_size,  "{\"RTC\":\"%s %d/%d/%d %d:%d:%d\"}" ,
    days[dayOfWeek],
    dayOfMonth,
    month,
    year,
    hour,
    minute,
    second
  );
}

void setPCF8563(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
{
  Wire.beginTransmission(PCF8563_address);
  Wire.write(0x01);
  Wire.write( dec2bcd(second)     );
  Wire.write( dec2bcd(minute)     );
  Wire.write( dec2bcd(hour)       );
  Wire.write( dec2bcd(dayOfMonth) );
  Wire.write( dec2bcd(dayOfWeek)  );
  Wire.write( dec2bcd(month)      );
  Wire.write( dec2bcd(year)       );
  Wire.endTransmission();
}


uint32_t read_ADE7953(uint16_t reg)
{
  uint32_t response = 0;

  int size = Ade7953RegSize(reg);
  if (size) {
    Wire.beginTransmission(ADE7953_address);
    Wire.write((reg >> 8) & 0xFF);
    Wire.write(reg & 0xFF);
    Wire.endTransmission(0);
    Wire.requestFrom(ADE7953_address, size);
    if (size <= Wire.available()) {
      for (int i = 0; i < size; i++) {
        response = response << 8 | Wire.read();   // receive DATA (MSB first)
      }
    }
  }
  return response;
}


void write_ADE7953(uint16_t reg, uint32_t val)
{
  int size = Ade7953RegSize(reg);
  if (size) {
    Wire.beginTransmission(ADE7953_address);
    Wire.write((reg >> 8) & 0xFF);
    Wire.write(reg & 0xFF);
    while (size--) {
      Wire.write((val >> (8 * size)) & 0xFF);  // Write data, MSB first
    }
    Wire.endTransmission();
    delayMicroseconds(5);    // Bus-free time minimum 4.7us
  }
}

int Ade7953RegSize(uint16_t reg)
{
  int size = 0;
  switch ((reg >> 8) & 0x0F) {
    case 0x03:
      size++;
    case 0x02:
      size++;
    case 0x01:
      size++;
    case 0x00:
    case 0x07:
    case 0x08:
      size++;
  }
  return size;
}

void getADE7953(char str[], int str_size )
{
    uint32_t ade_V  = read_ADE7953(VRMS_32);
    uint32_t ade_I1 = read_ADE7953(IRMSA_32);
    int32_t  ade_P1 = (int32_t)read_ADE7953(AWATT_32);
    if(ade_P1 && 0x80000000) ade_P1 = (-1) * (~ade_P1+1);
    uint32_t ade_I2 = read_ADE7953(IRMSB_32);
    int32_t  ade_P2 = (int32_t)read_ADE7953(BWATT_32); 
    if(ade_P2 && 0x80000000) ade_P2 = (-1) * (~ade_P2+1);

    float V  = (float) ade_V/ADE7953_UREF;
    float I1 = (float) ade_I1/(ADE7953_IREF * 10);
    float P1 = (float) ade_P1/(ADE7953_PREF / 10);
    float I2 = (float) ade_I2/(ADE7953_IREF * 10);
    float P2 = (float) ade_P2/(ADE7953_PREF / 10);
    
    snprintf(str, str_size, "{\"V\":%.2f,\"I1\":%.2f,\"P1\":%.2f,\"I2\":%.2f,\"P2\":%.2f}", V, I1, P1, I2, P2);

}
