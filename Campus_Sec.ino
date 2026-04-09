#include "esp_camera.h" 
#include <WiFi.h>
#include <Arduino.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
String a;

// Insert Firebase project API Key
#define API_KEY "AIzaSyBkoKRwMTCckgOLC4-lUu5drI8WEQlUKM8"

// Insert RTDB URL
#define DATABASE_URL "https://campus-security-3fb14-default-rtdb.firebaseio.com/"

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;

FirebaseConfig configg;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;
#define CAMERA_MODEL_AI_THINKER  // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Anush";
const char *password = "12345678";

void startCameraServer();
void setupLedFlash(int pin);


//*pinDefine*************************
#define PIR_PIN 13
#define flame_pin 15
#define buzz_pin 14
#define ir 2


int countt = 0;


void setup() {
  Serial.begin(115200);

  //+++++++++++++++++++pinMode Setup+++++++++++++++++++++++++
      pinMode(PIR_PIN, INPUT);
      pinMode(flame_pin ,INPUT);
      pinMode(buzz_pin , OUTPUT);
      pinMode(ir , INPUT);

  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  String b = WiFi.localIP().toString();
  a = "http://" + b;

  Serial.println("' to connect");
  /* Assign the api key (required) */
  configg.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  configg.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&configg, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", configg.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  configg.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  Firebase.begin(&configg, &auth);
  Firebase.reconnectWiFi(true);
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    // Write an Int number on the database path test/int
    if (Firebase.RTDB.setString(&fbdo, "test/camera", a)) {
      digitalWrite(buzz_pin, 1);
  delay(500);
  digitalWrite(buzz_pin, 0);
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
  
}

void loop() {

int pir_s = digitalRead(PIR_PIN) ;

int flame_s = digitalRead(flame_pin);
int irs = digitalRead(ir);
Serial.print("ir ");
Serial.println(irs);
Serial.print("pir_s");
Serial.println(pir_s);
Serial.print("flame");
Serial.println(flame_s);







  if ((Firebase.RTDB.setInt(&fbdo, "test/motion", pir_s)) && (Firebase.RTDB.setInt(&fbdo, "test/flame", flame_s))  ) {
      Serial.println("PASSED");


    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }




  

if(irs==0){
  countt++ ;
   if (Firebase.RTDB.setInt(&fbdo, "test/Student_count", countt)) {
      Serial.println("PASSED");
    

    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

}



if(flame_s == 1){
  digitalWrite(buzz_pin, 1);
  Serial.println("fire detected");
}
else{
  digitalWrite(buzz_pin, 0);
}

if(pir_s==1){
  digitalWrite(buzz_pin, 1);
  delay(500);
  digitalWrite(buzz_pin, 0);
}
else{
  delay(100);
}

}