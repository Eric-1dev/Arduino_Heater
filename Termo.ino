/*
 Name:		Termo.ino
 Created:	7/10/2022 10:50:49 PM
 Author:	Eric
*/

#include "max6675.h"
#include <AnalogKeypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#define HEATER1_PIN D6
#define HEATER2_PIN D0
#define SO D7
#define CS D8
#define SCK D5
#define DISPLAY_UPDATE_INTERVAL 1000
#define READ_TEMP_INTERVAL 500
#define MIN_TEMP 50
#define MAX_TEMP 280
#define MIN_TIME 10
#define MAX_TIME 120
#define START_TEMP 150
#define START_TIME 90
#define INCREMENT_TEMP 5
#define INCREMENT_TIME 5
//#define SSID "Eric's AP 2.4G"
//#define PASSWORD "19881989"
#define SSID "Heater"
#define PASSWORD "1234567890"
#define KEYPAD_HOLD_TIME_MS 700

uint32_t lastDisplayUpdate = 0;
uint32_t halfSecond = 0;
uint32_t readTempTime = 0;
uint32_t minuteInterval = 0;
int16_t currentTemperature = 0;
int16_t lastCurTemperature = 0;
int16_t targetTemperature = START_TEMP;
int16_t lastTargetTemperature = 0;
int16_t curMinutes = START_TIME;
int16_t lastMinutes = -1;
uint8_t curHeaterCount = 1;
uint8_t lastHeaterCount = 0;
bool showDots = true;
bool powerOn = false;
bool heaterOn = false;
bool lastStateHeaterIcon = false;
const char* ssid = SSID;
const char* password = PASSWORD;

const int KeypadMap[] = { 13, 157, 332, 511, 745 };

AnalogKeypad keypad(A0, KeypadMap, 5, KEYPAD_HOLD_TIME_MS);

DynamicJsonDocument doc(1024);

ESP8266WebServer server(80);

#pragma region Custom chars

byte dergreeChar[8] = {
	0b00110,
	0b01001,
	0b01001,
	0b00110,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

byte tChar[8] = {
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11100,
	0b01000,
	0b01000,
	0b00000
};

byte cChar[8] = {
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b10100,
	0b10100,
	0b11110,
	0b00010
};

byte yaChar[8] = {
	0b00000,
	0b00000,
	0b01110,
	0b10001,
	0b01111,
	0b00101,
	0b01001,
	0b00000
};

byte heaterChar[8] = {
	0b00100,
	0b10101,
	0b01110,
	0b01110,
	0b10101,
	0b00100,
	0b00100,
	0b00100
};

#pragma endregion

MAX6675 module(SCK, CS, SO);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Check I2C address of LCD, normally 0x27 or 0x3F

void setup() {
	powerOn = false;
	pinMode(HEATER1_PIN, OUTPUT);
	pinMode(HEATER2_PIN, OUTPUT);
	digitalWrite(HEATER1_PIN, HIGH);
	digitalWrite(HEATER2_PIN, HIGH);

	// initialize LCD
	lcd.init();
	// turn on LCD backlight                      
	lcd.backlight();
	lcd.createChar(0, dergreeChar);
	lcd.createChar(1, tChar);
	lcd.createChar(2, cChar);
	lcd.createChar(3, yaChar);
	lcd.createChar(4, heaterChar);
	lcd.clear();

	//WiFi.mode(WIFI_STA);
	WiFi.softAP(ssid, password);
	
	
	///////////////////////////////////////////////////////
	//WiFi.begin(ssid, password);
	//Serial.begin(115200);
	//Serial.println("");

	//// Wait for connection
	//while (WiFi.status() != WL_CONNECTED) {
	//	delay(500);
	//	Serial.print(".");
	//}
	//Serial.println("");
	//Serial.print("Connected to ");
	//Serial.println(ssid);
	//Serial.print("IP address: ");
	//Serial.println(WiFi.localIP());
	/////////////////////////////////////////////////////////

	server.on("/", handleRoot);
	server.on("/get", handleGet);
	server.on("/post", HTTP_POST, handlePost);
	server.begin();
}

void loop() {
	ShowCurrentTemp();

	ShowTargetTemp();

	ShowTime();

	ShowHeatIcon();

	ShowHeaterCount();

	ReadTemp();

	HeaterPower();

	if (powerOn)
		PowerOnMode();
	else
		PowerOffMode();

	ReadKey();

	server.handleClient();

	yield();
}

void PowerOffMode()
{
	ShowDots(false);
	heaterOn = false;
}

void PowerOnMode()
{
	ShowDots(true);
	if (millis() - minuteInterval > 60000) // 60000
	{
		minuteInterval = millis();
		curMinutes--;
		if (curMinutes < 1)
		{
			curMinutes = START_TIME;
			powerOn = false;
		}
	}

	if (currentTemperature > targetTemperature + 7)
		heaterOn = false;
	else if (currentTemperature < targetTemperature - 7)
		heaterOn = true;
}

void ReadKey()
{
	keypad.loop(ButtonHandler);
}

void ReadTemp() {
	if (millis() - readTempTime < READ_TEMP_INTERVAL)
		return;

	readTempTime = millis();

	currentTemperature = module.readCelsius();
}

void HeaterPower()
{
	if (heaterOn && powerOn)
	{
		digitalWrite(HEATER1_PIN, LOW);
		if (curHeaterCount == 2)
			digitalWrite(HEATER2_PIN, LOW);
		else
			digitalWrite(HEATER2_PIN, HIGH);
	}
	else
	{
		digitalWrite(HEATER1_PIN, HIGH);
		digitalWrite(HEATER2_PIN, HIGH);
	}
}

#pragma region Display

void ShowHeatIcon() {
	if (heaterOn == lastStateHeaterIcon)
		return;

	lastStateHeaterIcon = heaterOn;

	lcd.setCursor(15, 1);
	if (heaterOn)
		lcd.write((uint8_t)4);
	else
		lcd.print(" ");
}

void ShowDots(bool needToBlynk)
{
	if (millis() - halfSecond < 500)
		return;

	halfSecond = millis();
	lcd.setCursor(7, 1);
	if (showDots || !needToBlynk)
	{
		lcd.print(":");
		showDots = false;
	}
	else
	{
		lcd.print(" ");
		showDots = true;
	}
}

void ShowCurrentTemp()
{
	if (currentTemperature == lastCurTemperature
		|| millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL)
	{
		return;
	}
	
	lastCurTemperature = currentTemperature;
	lastDisplayUpdate = millis();
	
	lcd.setCursor(0, 0);
	lcd.print("T");
	lcd.write((uint8_t)1);
	lcd.printf("=%d", currentTemperature);
	lcd.write((uint8_t)0);

	if (currentTemperature < 100)
		lcd.print(" ");
}

void ShowTargetTemp()
{
	if (targetTemperature == lastTargetTemperature && millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL)
		return;

	lastTargetTemperature = targetTemperature;
	lastDisplayUpdate = millis();

	lcd.setCursor(9, 0);
	lcd.print("T");
	lcd.write((uint8_t)2);
	lcd.printf("=%d", targetTemperature);
	lcd.write((uint8_t)0);

	if (targetTemperature < 100)
		lcd.print(" ");
}

void ShowTime()
{
	if (curMinutes == lastMinutes)
		return;

	lastMinutes = curMinutes;
	lcd.setCursor(0, 1);
	lcd.print("Bpem");
	lcd.write((uint8_t)3);
	lcd.printf("=%d", curMinutes / 60);
	lcd.setCursor(8, 1);
	lcd.printf("%02d", curMinutes % 60);
}

void ShowHeaterCount()
{
	if (curHeaterCount == lastHeaterCount)
		return;

	lcd.setCursor(12, 1);
	lcd.printf("H%d", curHeaterCount);
}

#pragma endregion

#pragma region WebApi

void handleRoot()
{
	server.send(200, "text/html", "Hello");
}

void handleGet()
{
	server.send(200, "application/json", fillGetResult());
}

void handlePost()
{
	if (!server.hasArg("targetTemp") || !server.hasArg("curTime") || !server.hasArg("powerState"))
	{
		server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
		return;
	}

	targetTemperature = server.arg("targetTemp").toInt();
	curMinutes = server.arg("curTime").toInt();

	if (server.arg("powerState") == "true")
		powerOn = true;
	else
		powerOn = false;

	server.send(200, "application/json", FillSetResult(true));
}

String FillSetResult(bool status)
{
	doc.clear();
	doc["result"] = status;
	String message;
	serializeJson(doc, message);
	return message;
}

String fillGetResult()
{
	doc.clear();
	doc["curTemp"] = currentTemperature;
	doc["targetTemp"] = targetTemperature;
	doc["curTime"] = curMinutes;
	doc["powerState"] = powerOn;
	doc["minTemp"] = MIN_TEMP;
	doc["maxTemp"] = MAX_TEMP;
	doc["minTime"] = MIN_TIME;
	doc["maxTime"] = MAX_TIME;
	doc["incrementTemp"] = INCREMENT_TEMP;
	doc["incrementTime"] = INCREMENT_TIME;

	String message;
	serializeJson(doc, message);
	return message;
}

#pragma endregion

#pragma region Buttons

void OnButPressTempUp(bool isLongPress)
{
	int increment = INCREMENT_TEMP;
	if (isLongPress)
		increment *= 4;

	targetTemperature += increment;
	if (targetTemperature > MAX_TEMP)
		targetTemperature = MAX_TEMP;
}

void OnButPressTempDown(bool isLongPress)
{
	int increment = INCREMENT_TEMP;
	if (isLongPress)
		increment *= 4;

	targetTemperature -= increment;
	if (targetTemperature < MIN_TEMP)
		targetTemperature = MIN_TEMP;
}

void OnButPressTimeUp(bool isLongPress)
{
	int increment = INCREMENT_TIME;
	if (isLongPress)
		increment *= 4;

	curMinutes += increment;
	if (curMinutes > MAX_TIME)
		curMinutes = MAX_TIME;
}

void OnButPressTimeDown(bool isLongPress)
{
	int increment = INCREMENT_TIME;
	if (isLongPress)
		increment *= 4;

	curMinutes -= increment;
	if (curMinutes < MIN_TIME)
		curMinutes = MIN_TIME;
}

void OnButPressStart(bool isLongPress)
{
	if (isLongPress)
	{
		powerOn = !powerOn;
		return;
	}

	if (curHeaterCount == 1)
		curHeaterCount = 2;
	else
		curHeaterCount = 1;
}

void ButtonHandler(const ButtonParam& param)
{
	if (param.button == 0 && param.state == ButtonState_Click)
		OnButPressTimeDown(false);
	else if (param.button == 0 && param.state == ButtonState_Hold)
		OnButPressTimeDown(true);
	else if (param.button == 1 && param.state == ButtonState_Click)
		OnButPressTempUp(false);
	else if (param.button == 1 && param.state == ButtonState_Hold)
		OnButPressTempUp(true);
	else if (param.button == 2 && param.state == ButtonState_Click)
		OnButPressTempDown(false);
	else if (param.button == 2 && param.state == ButtonState_Hold)
		OnButPressTempDown(true);
	else if (param.button == 3 && param.state == ButtonState_Click)
		OnButPressTimeUp(false);
	else if (param.button == 3 && param.state == ButtonState_Hold)
		OnButPressTimeUp(true);
	else if (param.button == 4 && param.state == ButtonState_Click)
		OnButPressStart(false);
	else if (param.button == 4 && param.state == ButtonState_Hold)
		OnButPressStart(true);
}

#pragma endregion