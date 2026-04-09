/************** SENSOR + ALARM VARIABLES ***************/
const int IR_SENSOR_PIN = 5;      // D1
const int BUZZER_PIN    = 4;      // D2
const int MQ3_PIN       = A0;     // MQ3 analog output

unsigned long sleepStartTime = 0;
const unsigned long SLEEP_THRESHOLD = 3000;  
bool alarmActive = false;

unsigned long buzzerLastTime = 0;
bool buzzerState = false;

// NEW — track both alarms separately
bool sleepAlarm = false;
bool alcoholAlarm = false;


/************** BLYNK CONFIG ***************/


#define BLYNK_TEMPLATE_ID "TMPL3agHp6cUA"
#define BLYNK_TEMPLATE_NAME "Project"
#define BLYNK_AUTH_TOKEN "nExNvwoFBNQmBi1-vQjVcvAWSICIut7g"

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "OppoA16";  
char pass[] = "ayush jha";  

BlynkTimer timer;


/************** FUNCTIONS ***************/

// Non-blocking alarm
void handleAlarm() {
  if (alarmActive) {
    unsigned long now = millis();

    if (now - buzzerLastTime >= 150) {  
      buzzerLastTime = now;
      buzzerState = !buzzerState;

      if (buzzerState) tone(BUZZER_PIN, 1000);
      else noTone(BUZZER_PIN);
    }
  }
  else {
    noTone(BUZZER_PIN);
  }
}

void activateAlarm() {
  if (!alarmActive) {
    alarmActive = true;
    Serial.println("*** ALERT ACTIVATED ***");
    Blynk.virtualWrite(V1, "ALERT");
  }
}

void deactivateAlarm() {
  if (alarmActive) {
    alarmActive = false;
    noTone(BUZZER_PIN);
    Serial.println("Alarm stopped");
    Blynk.virtualWrite(V1, "NORMAL");
  }
}

// NEW — stop alarm only when BOTH alerts are false
void checkIfAlarmShouldStop() {
  if (!sleepAlarm && !alcoholAlarm) {
    deactivateAlarm();
  }
}


// Sensor monitoring (runs 10x/second)
void checkSensors() {

  int irState = digitalRead(IR_SENSOR_PIN);
  int alcohol = analogRead(MQ3_PIN);

  Serial.print("mq3:");
  Serial.println(alcohol);


  // -------------- Alcohol Detection ---------------
  if (alcohol >= 500) {
    Serial.println("Alcohol Detected!");
    alcoholAlarm = true;

    activateAlarm();
    Blynk.virtualWrite(V3, 1);
  } 
  else {
    alcoholAlarm = false;
    Blynk.virtualWrite(V3, 0);
  }


  // -------------- Sleep Detection -----------------
  if (irState == LOW) {
    if (sleepStartTime == 0) {
      sleepStartTime = millis();
      Serial.println("Sleep detection started...");
    }

    if (millis() - sleepStartTime >= SLEEP_THRESHOLD) {
      sleepAlarm = true;

      activateAlarm();
      Blynk.logEvent("Sleep", "Driver appears sleepy!");

      Blynk.virtualWrite(V4, 1);
    }

  } 
  else {
    sleepStartTime = 0;
    sleepAlarm = false;
    Blynk.virtualWrite(V4, 0);
  }


  // Buzzer handler
  handleAlarm();

  // NEW — check if alarm must be stopped
  checkIfAlarmShouldStop();
}



// Blynk uptime sender
void myTimerEvent() {
  Blynk.virtualWrite(V2, millis() / 1000);
}



/************** SETUP ***************/
void setup() {
  Serial.begin(115200);

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(MQ3_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  Serial.println("=== Sleep + Alcohol Alert System ===");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(100L, checkSensors);
  timer.setInterval(1000L, myTimerEvent);
}



/************** LOOP ***************/
void loop() {
  Blynk.run();
  timer.run();
}