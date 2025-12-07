#include <ESP32Servo.h>

// Pin definitions
#define SERVO_PIN 13
#define GREEN_LED_PIN 26  //door closed indicator
#define RED_LED_PIN 27    //door open indicator
#define BUZZER_PIN 14     // Alarm buzzer - the alarm is a passive alarm

//servo motor positions
#define DOOR_CLOSED 0     //0 degrees = closed
#define DOOR_OPEN 90      //90 degrees = open


Servo doorServo;

//buzzer state for beeping pattern
bool buzzerState = false;
unsigned long lastBuzzerToggle = 0;
const unsigned long BEEP_INTERVAL = 300;  //beeps every 300ms (adjustable)
bool alarmActive = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Door System with LED Indicators Starting...");
  
  //initialises LED pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  
  //initialises buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  // no need to initialise passive buzzer, noTone handles this
  
  
  doorServo.attach(SERVO_PIN);
  
  //starts with door closed
  closeDoor();
  
  Serial.println("System ready!");
}

void loop() {
  // Handle buzzer beeping pattern when alarm is active
  if (alarmActive) {
    if (millis() - lastBuzzerToggle >= BEEP_INTERVAL) {
      lastBuzzerToggle = millis();
      buzzerState = !buzzerState;
      
      if (buzzerState) {
        tone(BUZZER_PIN, 1000);  // Beep ON
      } else {
        noTone(BUZZER_PIN);      // Beep OFF
      }
    }
  }
  
  // Door control with non-blocking timing
  static unsigned long lastDoorAction = 0;
  static bool doorIsOpen = false;
  
  if (millis() - lastDoorAction >= 3000) {
    lastDoorAction = millis();
    
    if (!doorIsOpen) {
      Serial.println("\n--- Opening Door ---");
      openDoor();
      doorIsOpen = true;
    } else {
      Serial.println("\n--- Closing Door ---");
      closeDoor();
      doorIsOpen = false;
    }
  }
}

void openDoor() {
  Serial.println("Door Status: OPEN");
  doorServo.write(DOOR_OPEN);  
  
  // Update LEDs: RED on, GREEN off
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  // Activate alarm (buzzer will beep in loop() function)
  alarmActive = true;
  
  Serial.println("Red LED ON and Green LED OFF");
  Serial.println("BUZZER BEEPING -> ALARM!");
}

void closeDoor() {
  Serial.println("Door Status: CLOSED");
  doorServo.write(DOOR_CLOSED);  
  
  // Update LEDs: GREEN on, RED off
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  
  //deactivate alarm and turn passive buzzer OFF
  alarmActive = false;
  noTone(BUZZER_PIN);
  
  Serial.println("Green LED ON and Red LED OFF");
  Serial.println("Buzzer OFF -> System Normal");
}