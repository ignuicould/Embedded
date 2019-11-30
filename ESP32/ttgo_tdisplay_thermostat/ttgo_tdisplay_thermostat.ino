#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Button2.h>
#include "esp_adc_cal.h"
#include "bmp.h"

// PERIPHERALS ========================
#define DHT_11          17
#define TFT_BL          4
#define BUTTON_1        35
#define BUTTON_2        0
#define RELAYPIN        27

// DHT11 SENSOR =======================
#include "DHTesp.h"
DHTesp dht;
TempAndHumidity tempValues;
ComfortState cf;
String comfortStatus;
float heatIndex;
float dewPoint;
float cr;
int tempGoal = 60;
int tempCurrent = 60;

// BLUETOOTH ==========================
#include "BluetoothSerial.h"
BluetoothSerial ESP_BT;
String bt_command;

// SCREEN AND BUTTONS =================
TFT_eSPI tft = TFT_eSPI(135, 240);
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

// misc
long previousMillis = 0;
long currentMillis  = 0;
long interval       = 5000;
int  boilerTime     = 0;
bool boilerRun      = false;

// BEGIN GLOBAL ROUTINES ========================

void writeBT(String stringData)
{
  for (int i = 0; i < stringData.length(); i++)
  {
    ESP_BT.write(stringData[i]);
  }
}

String timeToStr(int seconds)
{
  int _hours   = 0;
  int _minutes = 0;
  int _seconds = 0;
  String timeFormatted = "0:00";

  if (seconds >= 3600) {
    timeFormatted = "hours";
  } else if (seconds >= 60) {
    _minutes = floor(seconds / 60);
    _seconds = seconds - (_minutes * 60);
    timeFormatted = String(_minutes)+":"+String(_seconds); 
  } else {
    timeFormatted = "0:"+String(seconds);
  }
  
  return timeFormatted;
}

boolean isValidNumber(String str)
{
  bool isnum = true;
  for(byte i=0;i<str.length();i++)
  {
    if(!isDigit(str.charAt(i))) isnum = false;
  }
  return isnum;
}

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{
  esp_sleep_enable_timer_wakeup(ms * 1000);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_light_sleep_start();
}

void button_init()
{
  btn1.setPressedHandler([](Button2 & b) {
    tempGoal += 1;
    previousMillis = currentMillis;
    updatescreen();
  });

  btn2.setPressedHandler([](Button2 & b) {
    tempGoal -= 1;
    previousMillis = currentMillis;
    updatescreen();
  });
}

void button_loop()
{
  btn1.loop();
  btn2.loop();
}

bool getTemperature() {
  // Reading temperature for humidity takes about 250 milliseconds!
  tempValues = dht.getTempAndHumidity();
  if (tempValues.temperature < 60) tempCurrent = round((tempValues.temperature * 9 / 5)+32);
  
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  heatIndex = dht.computeHeatIndex(tempValues.temperature, tempValues.humidity);
  dewPoint = dht.computeDewPoint(tempValues.temperature, tempValues.humidity);
  cr = dht.getComfortRatio(cf, tempValues.temperature, tempValues.humidity);

  switch(cf) {
    case Comfort_OK:
      comfortStatus = "Perfect Comfort";
      break;
    case Comfort_TooHot:
      comfortStatus = "Too Hot";
      break;
    case Comfort_TooCold:
      comfortStatus = "Too Cold";
      break;
    case Comfort_TooDry:
      comfortStatus = "Too Dry";
      break;
    case Comfort_TooHumid:
      comfortStatus = "Too Humid";
      break;
    case Comfort_HotAndHumid:
      comfortStatus = "Too Hot + Humid";
      break;
    case Comfort_HotAndDry:
      comfortStatus = "Too Hot + Dry";
      break;
    case Comfort_ColdAndHumid:
      comfortStatus = "Cold + Humid";
      break;
    case Comfort_ColdAndDry:
      comfortStatus = "Too Cold + Dry";
      break;
    default:
      comfortStatus = "Unknown Comfort";
      break;
  };
  
  updatescreen();
  return true;
}

void bt_processCommands(String bt_command)
{
  if (bt_command == "up") {
    tempGoal += 1;
    writeBT(String(tempGoal)+"°");
    updatescreen();
  } else if (bt_command == "down") {
    tempGoal -= 1;
    writeBT(String(tempGoal)+"°");
    updatescreen();
  } else if (bt_command == "temp") {
      writeBT(String(tempCurrent)+"°");
  } else if (bt_command == "tempG") {
      writeBT(String(tempGoal)+"°");
  } else if (bt_command == "humidity") {
      writeBT(String(tempValues.humidity)+"%");
  } else if (bt_command == "dewpoint") {
      writeBT(String(dewPoint)+"°");
  } else if (bt_command == "heatindex") {
      writeBT(String(heatIndex));
  } else if (bt_command == "comfort") {
    writeBT(String(comfortStatus));
  } else if (bt_command == "bstatus") {
    if (boilerRun == true) {
      writeBT("BOILER ON");
    } else {
      writeBT("BOILER OFF");
    }
  } else if (bt_command == "bruntime") {
    writeBT(String(boilerTime));
  } else {
    if (isValidNumber(bt_command)) {
      tempGoal = bt_command.toInt();
      writeBT("Set temp to "+bt_command);
    } else {
      writeBT("Unknown command: "+bt_command);
    }
  }
}

void updatescreen ()
{
  // BOILER STATUS ====================
  tft.setTextSize(2);
  if (tempGoal > tempCurrent) {
    boilerRun = true;
    digitalWrite(RELAYPIN, HIGH);
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("BOILER ON!",  tft.width() / 2, tft.height() - 5);
  } else {
    boilerRun = false;
    digitalWrite(RELAYPIN, LOW);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(timeToStr(boilerTime),  tft.width() / 2, tft.height() - 5);
  }

  // COMFORT STATUS ===================
  tft.drawString(comfortStatus,  tft.width() / 2, 10);
  
  // DESIRED TEMPERATURE ==============
  tft.setTextSize(9);
  tft.drawString(String(tempGoal),  tft.width() - 25, (tft.height() / 2));
  
  // CURRENT TEMPERATURE ==============
  tft.setTextSize(4);
  tft.drawString(String(tempCurrent),  35, (tft.height() / 2)+15);
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.println("CURRENTLY");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  // BLUETOOTH =========================
  ESP_BT.begin("Bobby's Thermostat");
  
  // SCREEN ============================
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0,  240, 135, ttgo);
  espDelay(1000);
  
  // BUTTONS ===========================
  button_init();

  // RELAY =============================
  pinMode(RELAYPIN, OUTPUT);

  // DHT11 SENSOR ======================
  dht.setup(DHT_11, DHTesp::DHT11);
  getTemperature();
}

void loop() {
  // BUTTONS ===========================
  button_loop();

  // BLUETOOTH =========================
  if (ESP_BT.available()) {
    bt_command = ESP_BT.readString();
    bt_processCommands(bt_command);
  }
  
  // DHT11 SENSOR ======================
  currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    if (tempGoal > tempCurrent) {
      boilerTime += 5;
    }
    getTemperature();
  }
}
