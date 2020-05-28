// 1) Importeer libraries
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "WiFi.h"
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"

TFT_eSPI tft = TFT_eSPI();       // Constructor for the TFT library

#define STEP_PIN 2
#define DIR_PIN 15

#define LEFT_BUTTON 0
#define RIGHT_BUTTON 35

#define TRIGGER 17
#define ECHO 22

#define MAXLEN 1327
#define MINLEN 175
#define GOAL_MARGIN 1
#define MAXCM 20.0
#define POSBUFFER_SIZE 100

#define SSID "Kergroaz"
#define PASSWORD "Sauvage36"

const char* ssid     = "Kergroaz";
const char* password = "Sauvage36";

#define IO_USERNAME  "Pieter_JanLavaerts"
#define IO_KEY       "aio_Qhrn75VkwCF0sVoMDuY3vtXsU2oG"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, SSID, PASSWORD);

AdafruitIO_Feed *distanceFeed = io.feed("plankske_distance");
AdafruitIO_Feed *leftButtonFeed = io.feed("left_button");
AdafruitIO_Feed *rightButtonFeed = io.feed("right_button");
AdafruitIO_Feed *moveFeed = io.feed("move_plankske");

void printTitel() {
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
	tft.setCursor(0, 0, 4);
	tft.setTextColor(TFT_BLACK, TFT_YELLOW);
	tft.println("Project Delek");
}

int adafruitLeft = 0;
void handleMessageLeft(AdafruitIO_Data *data) {
	if (adafruitLeft) {
		adafruitLeft = 0;
	}
	else {
		adafruitLeft = 1;
	}
}

int adafruitRight = 0;
void handleMessageRight(AdafruitIO_Data *data) {
	if (adafruitRight) {
		adafruitRight = 0;
	}
	else {
		adafruitRight = 1;
	}
}

void setup_adafruit() {
	io.connect();
	Serial.print("\nWaiting for adafruit...");
	while (io.status() < AIO_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.println("");
	tft.setCursor(20, 30, 2);
	tft.println(WiFi.localIP());    
	tft.setCursor(20, 50, 2); 
	tft.println("Connect to adafruit!");

	leftButtonFeed->onMessage(handleMessageLeft);
	rightButtonFeed->onMessage(handleMessageRight);
}

void setup() {
	Serial.begin(9600);

	printTitel();
	setup_adafruit();

	pinMode(STEP_PIN, OUTPUT);
	pinMode(DIR_PIN, OUTPUT);

	pinMode(LEFT_BUTTON, INPUT);
	pinMode(RIGHT_BUTTON, INPUT);

	pinMode(TRIGGER, OUTPUT);
	pinMode(ECHO, INPUT);

	xTaskCreate(
			measureTask,
			"Measure task",
			10000,
			NULL,
			1,
			NULL);

	xTaskCreate(
			buttonsTask,
			"buttons task",
			10000,
			NULL,
			1,
			NULL);
}

void loop() {
	io.run();
}

int positionIndex = 0;
double positionBuffer[POSBUFFER_SIZE];
void printBuffer() {
	for (int i = 0; i < POSBUFFER_SIZE; i++) {
		char message[100];
		sprintf(message, "%f ", positionBuffer[i]);
		Serial.print(message);
	}
	Serial.println("");
}
double bufferAverage() {
	double total = 0;
	for (int i = 0; i < POSBUFFER_SIZE; i++) {
		total += positionBuffer[i];
	}
	return total/POSBUFFER_SIZE;
}
double last_average = 0;
unsigned long last_save = 0;
void measureTask(void *parameters) {
	while(true) {
		digitalWrite(TRIGGER, LOW);
		delayMicroseconds(2);
		digitalWrite(TRIGGER, HIGH);
		delayMicroseconds(10);
		digitalWrite(TRIGGER, LOW);

		double current_position = pulseIn(ECHO, HIGH);
		double positioncm = mapd(current_position, MINLEN, MAXLEN, 0, MAXCM);
   
    if (positioncm <= 30 && positioncm > -3) {
      positionBuffer[positionIndex] = positioncm;
      positionIndex = (positionIndex + 1)%POSBUFFER_SIZE;
      
      double new_average = bufferAverage();
      if ((millis() - last_save > 2000) && (abs(new_average - last_average) > 1) && new_average <= MAXCM && new_average > 0) {
        distanceFeed->save(new_average);
        last_save = millis();
        last_average = new_average;
      }

		  //screen

		  char message[50];
		  sprintf(message, " Position: %4.2f cm \r", positioncm);
		  tft.setCursor(20, 70, 4); 
		  tft.println(message); 
    }
    
    vTaskDelay(2 / portTICK_PERIOD_MS);
	}
}

unsigned long lastMoveRead = 0;
void buttonsTask(void *parameters) {
	while (true) {
		if (millis() - lastMoveRead > 10000) {
			lastMoveRead = millis();
     
			moveFeed->get();
			int move = moveFeed->data->toInt();
      Serial.println("Move : "+String(move));
			if (move != 0) {
				moveFeed->save(0);
				if (move < 0) {
					digitalWrite(DIR_PIN, HIGH);
				}
				else {
					digitalWrite(DIR_PIN, LOW);
				}
				for (int i = 0; i < abs(((double) move)*250); i++) {
					turn();
				}
			}
		}

		else if ((!digitalRead(LEFT_BUTTON) && digitalRead(RIGHT_BUTTON)) || (adafruitLeft && !adafruitRight)) {
			digitalWrite(DIR_PIN, HIGH);
			turn();  
		}
		else if ((digitalRead(LEFT_BUTTON) && !digitalRead(RIGHT_BUTTON)) || (!adafruitLeft && adafruitRight)) {
			digitalWrite(DIR_PIN, LOW);
			turn();
		}
	}
}

double mapd(double x, long in_min, long in_max, long out_min, long out_max)
{
	return ((double)((x - in_min) * (out_max - out_min))) / ((double)((in_max - in_min) + out_min));
}

void turn() {
	digitalWrite(STEP_PIN, HIGH);
	delayMicroseconds(500);
	digitalWrite(STEP_PIN, LOW);
	delayMicroseconds(500);
}
