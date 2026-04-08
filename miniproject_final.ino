
// secrets.h - Keep this file private!

#define BLYNK_TEMPLATE_ID   "TMPLxxxxxx"
#define BLYNK_TEMPLATE_NAME "Cable Fault Monitor"
#define BLYNK_AUTH_TOKEN    "YourAuthTokenHere"

// Include WiFi and Blynk libraries
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

char ssid[] = "Your_WiFi_Name";
char pass[] = "Your_WiFi_Password";          


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define ADC_PIN 34

#define GREEN_LED 18
#define RED_LED 19
#define YELLOW_LED 17

#define BUZZER 23
#define RELAY 5

int adcValue = 0;

int normal_min = 1500;
int normal_max = 2500;

int short_threshold = 800;
int earth_threshold = 1200;

float total_length = 3.0;

unsigned long previousMillis = 0;
const long interval = 500;
bool buzzerState = false;

// Variable to track state changes to prevent spamming notifications
String lastStatus = "";

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY, OUTPUT);

  digitalWrite(RELAY, HIGH);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Connecting WiFi...");
  display.display();

  // Initialize Blynk and WiFi
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  display.clearDisplay();
  display.setCursor(20, 20);
  display.println("System Ready!");
  display.display();
  delay(1000);
}

void loop() {
  // Required for Blynk to process data
  Blynk.run();

  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(ADC_PIN);
    delay(5);
  }
  adcValue = sum / 10;

  Serial.println(adcValue);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);

  bool fault = false;
  String status = "";
  float distance = 0;

  // Determine Fault State
  if (adcValue >= normal_min && adcValue <= normal_max) {
    status = "NORMAL";
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RELAY, HIGH);
    fault = false;
  }
  else if (adcValue < short_threshold) {
    status = "SHORT";
    digitalWrite(RED_LED, HIGH);
    digitalWrite(RELAY, LOW);
    fault = true;
  }
  else if (adcValue < earth_threshold) {
    status = "EARTH";
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RELAY, LOW);
    fault = true;
  }
  else {
    status = "OPEN";
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RELAY, LOW);
    fault = true;
  }

  // Calculate Distance
  if (fault) {
    distance = ((float)adcValue / 4095.0) * total_length;
  } else {
    distance = 0.0;
  }

  // --- BLYNK NOTIFICATION & DATA UPDATE LOGIC ---
  // Only send data to Blynk if the status has CHANGED.
  // This prevents sending thousands of requests to the cloud per minute.
  if (status != lastStatus) {
    Blynk.virtualWrite(V0, status);
    Blynk.virtualWrite(V1, distance);
    
    // If the new status is a fault, log the event to trigger the notification
    if (fault) {
      // Create a message string combining fault type and distance
      String alertMsg = status + " FAULT detected at " + String(distance, 2) + " km!";
      
      // Send the event to Blynk. "fault_alert" must match the Event Name in your template exactly.
      Blynk.logEvent("fault_alert", alertMsg);
      Serial.println("Notification sent: " + alertMsg);
    }
    
    // Update last status so we don't trigger again until it changes
    lastStatus = status;
  }

  // Buzzer Logic
  if (fault) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      buzzerState = !buzzerState;
      digitalWrite(BUZZER, buzzerState);
    }
  } else {
    digitalWrite(BUZZER, LOW);
  }

  // OLED Display Logic
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("CABLE MONITOR");

  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 15);
  display.print("ADC: ");
  display.println(adcValue);

  display.setCursor(0, 30);
  display.print("Status: ");
  display.println(status);

  display.setCursor(0, 45);
  display.print("Dist: ");
  if (fault) {
    display.print(distance, 2);
    display.print(" km");
  } else {
    display.print("--");
  }

  display.display();

  // Standard delay (kept as you had it, but be careful not to make this too high or it will disconnect Blynk)
  delay(200); 
}

// This function is called every time the widget on V2 changes state
BLYNK_WRITE(V2) {
  int pinValue = param.asInt(); // Get the value (1 if pressed, 0 if released)

  if (pinValue == 1) {
    Serial.println("Remote Reset Triggered!");
    
    // 1. Clear the software states
    lastStatus = ""; 
    
    // 2. Clear the OLED for a "Refreshing" look
    display.clearDisplay();
    display.setCursor(20, 20);
    display.println("RESETTING...");
    display.display();
    
    // 3. Reset the Relay/Buzzer immediately
    digitalWrite(RELAY, HIGH);
    digitalWrite(BUZZER, LOW);
    
    delay(500); 
    
    // Optional: You can even trigger a full hardware restart 
    // ESP.restart(); 
  }
}
