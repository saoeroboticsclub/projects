#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

//  FIXED PIN ASSIGNMENT - SPI Compatible
#define RFID_SS    16   // D0 (GPIO15) - **CORRECT SPI SS**
#define RFID_RST   2    // D4 (GPIO2)
#define MOTOR_IN1  5    // D1 (GPIO5)
#define MOTOR_IN2  4    // D2 (GPIO4)  **CHANGED from GPIO0**
#define MOTOR_ENA  15   // D8 (GPIO13) **CHANGED - PWM safe**

MFRC522 rfid(RFID_SS, RFID_RST);
ESP8266WebServer server(80);

const char* ssid = "iQOO Z7 Pro 5G";
const char* password = "shiv1212";
String authorizedUID = "105DD959";

float fuelPerSecond = 0.05;
bool pumpActive = false;
unsigned long fuelStartTime = 0;
float selectedLiters = 0;
float dispensedLiters = 0;
const unsigned long FUEL_TIMEOUT = 45000; // 45 sec safety

void setup() {
  Serial.begin(115200);
  Serial.println("🔧 FIXED FUEL MACHINE");
  
  // RFID
  SPI.begin();
  pinMode(RFID_RST, OUTPUT);
  digitalWrite(RFID_RST, HIGH);
  rfid.PCD_Init();
  
  Serial.print("RFID: 0x");
  Serial.println(rfid.PCD_ReadRegister(rfid.VersionReg), HEX);
  
  // MOTOR - SAFE INITIALIZATION
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
  
  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());
  
  server.on("/", handleRoot);
  server.on("/set", handleSetLiters);
  server.on("/status", handleStatus);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.begin();
  
  Serial.println(" READY - Test motor with web START button");
}

void loop() {
  // RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getUID();
    Serial.println("RFID: " + uid);
    
    if (uid == authorizedUID && selectedLiters > 0) {
      Serial.println(" AUTO START!");
      startFueling();
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  
  // Web
  server.handleClient();
  
  // **FIXED FUEL CONTROL**
  if (pumpActive) {
    float runtime = (millis() - fuelStartTime) / 1000.0;
    dispensedLiters = runtime * fuelPerSecond;
    
    // Stop conditions
    if (dispensedLiters >= selectedLiters || runtime > 45) {
      stopFueling();
    }
  }
  
  delay(50);
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void startFueling() {
  if (pumpActive) return;
  
  pumpActive = true;
  fuelStartTime = millis();
  dispensedLiters = 0;
  
  // **MOTOR ON - TESTED SEQUENCE**
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 800);  // **HIGHER PWM**
  
  Serial.println(" MOTOR ON! " + String(selectedLiters,1) + "L");
}

void stopFueling() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
  pumpActive = false;
  Serial.println(" MOTOR OFF - " + String(dispensedLiters,2) + "L");
}

// WEB INTERFACE
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html><head><title>FUEL</title>
<meta name=viewport content=width=device-width,initial-scale=1>
<style>body{font-family:Arial;text-align:center;padding:30px;background:#e8f5e8}
h1{font-size:2.5em;color:#2e7d32}
.status{font-size:24px;padding:20px;margin:20px;border-radius:15px}
.active{background:#c8e6c9;color:#2e7d32}
input{padding:15px;font-size:20px;width:130px;border-radius:10px}
button{padding:18px 35px;font-size:18px;margin:10px;border-radius:12px;border:none;cursor:pointer}
.start{background:#4caf50;color:white;font-weight:bold}.stop{background:#f44336;color:white;font-weight:bold}</style>
<body>
<h1> FUEL STATION</h1>
<div>Fuel(L):<input id=l type=number step=0.1 value=1 min=0.1 max=20><br><br>
<button onclick=setL()> SET FUEL</button></div>
<div id=s class=status>Tap RFID or use START</div>
<p>Target: <span id=t style=color:#1976d2;font-size:22px;font-weight:bold>0</span>L</p>
<button onclick=r()> STATUS</button>
<button class=start onclick=startP()> START PUMP</button>
<button class=stop onclick=stopP()> STOP</button>
<p>IP: )" + WiFi.localIP().toString() + R"(</p>
<script>
function setL(){let v=document.getElementById('l').value*1;fetch('/set?liters='+v);r();}
function r(){fetch('/status').then(res=>res.json()).then(d=>{
  document.getElementById('t').innerHTML=d.target.toFixed(1);
  let s=document.getElementById('s');
  if(d.pump){s.innerHTML=' PUMPING '+d.dispensed.toFixed(2)+'/'+d.target.toFixed(1)+'L';s.className='status active';}
  else if(d.target>0){s.innerHTML=d.target.toFixed(1)+'L ready - Tap RFID';s.className='status active';}
  else{s.innerHTML='Set fuel → Tap RFID / START';s.className='status';}
});}
function startP(){fetch('/start',{method:'POST'});r();}
function stopP(){fetch('/stop',{method:'POST'});r();}
setInterval(r,1500);r();
</script></body></html>)";
  server.send(200, "text/html", html);
}

void handleSetLiters() {
  selectedLiters = server.arg("liters").toFloat();
  Serial.println("SET: " + String(selectedLiters) + "L");
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleStatus() {
  server.send(200, "application/json", 
    "{\"pump\":" + String(pumpActive) + 
    ",\"target\":" + String(selectedLiters, 1) + 
    ",\"dispensed\":" + String(dispensedLiters, 2) + "}"
  );
}

void handleStart() {
  if (!pumpActive && selectedLiters > 0) {
    startFueling();
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleStop() {
  stopFueling();
  server.sendHeader("Location", "/");
  server.send(302);
}