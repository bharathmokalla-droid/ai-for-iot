#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ------------------- WiFi -------------------
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ------------------- ThingSpeak -------------------
const char* server = "api.thingspeak.com";
String apiKey = "0QU59PGOZRR6I2SJ";

WiFiClient client;

// ------------------- DHT -------------------
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ------------------- Pins -------------------
#define PIR_PIN 27
#define LED_PIN 2
#define BUZZER_PIN 26
#define ANALOG_PIN 34

// ------------------- OLED -------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ------------------- Logic -------------------
int noMotionCount = 0;
const int graceLimit = 3;

// ------------------- TinyML Model -------------------

float w_co2 = 0.003;   
float w_temp = 0.25;
float w_hum = 0.06;
float bias = -1.6;

// Approx means
float co2_mean = 800.0;
float temp_mean = 23.0;
float hum_mean = 40.0;

// ------------------- ML Function -------------------
int predictOccupancy(float temp, float hum, int analogVal) {

  float co2 = analogVal - co2_mean;
  float t = temp - temp_mean;
  float h = hum - hum_mean;

  float score = (w_co2 * co2) +
                (w_temp * t) +
                (w_hum * h) +
                bias;

  float probability = 1.0 / (1.0 + exp(-score));

  if (probability > 0.5)
    return 1;
  else
    return 0;
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // WiFi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
}

// ------------------- Loop -------------------
void loop() {

  // -------- Read Sensors --------
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int motion = digitalRead(PIR_PIN);
  int analogVal = analogRead(ANALOG_PIN);

  if (isnan(temp) || isnan(hum)) {
    Serial.println("DHT Error");
    return;
  }

  // -------- ML Prediction --------
  int mlPrediction = predictOccupancy(temp, hum, analogVal);

  // -------- Hybrid Logic --------
  int occupancy = 0;

  if (motion == 1) {
    occupancy = 1;
    noMotionCount = 0;
  } else {
    noMotionCount++;

    if (noMotionCount < graceLimit) {
      occupancy = 1;   
    } else {
      occupancy = mlPrediction;  
    }
  }

  // -------- OUTPUT CONTROL --------
  int ledState = occupancy;
  int buzzerState = occupancy;

  digitalWrite(LED_PIN, ledState);
  digitalWrite(BUZZER_PIN, buzzerState);

  // -------- SERIAL OUTPUT --------
  Serial.println("------ Sensor Data ------");
  Serial.print("Temp: "); Serial.println(temp);
  Serial.print("Humidity: "); Serial.println(hum);
  Serial.print("Analog: "); Serial.println(analogVal);
  Serial.print("Motion: "); Serial.println(motion);
  Serial.print("NoMotionCount: "); Serial.println(noMotionCount);
  Serial.print("ML Prediction: "); Serial.println(mlPrediction);
  Serial.print("Final Occupancy: "); Serial.println(occupancy);
  Serial.print("LED: "); Serial.println(ledState);
  Serial.print("Buzzer: "); Serial.println(buzzerState);

  // -------- OLED DISPLAY --------
  display.clearDisplay();
  display.setCursor(0, 0);

  display.print("Temp: "); display.println(temp);
  display.print("Hum: "); display.println(hum);
  display.print("Motion: "); display.println(motion);
  display.print("ML: "); display.println(mlPrediction);
  display.print("Occ: ");
  display.println(occupancy ? "YES" : "NO");

  display.display();

  // -------- CLOUD UPLOAD --------
  sendToThingSpeak(temp, hum, analogVal, occupancy, ledState, buzzerState);

  delay(15000);  // ThingSpeak limit
}

// ------------------- ThingSpeak -------------------
void sendToThingSpeak(float t, float h, int a, int occ, int led, int buzzer) {

  if (client.connect(server, 80)) {

    String url = "/update?api_key=" + apiKey +
                 "&field1=" + String(t) +
                 "&field2=" + String(h) +
                 "&field3=" + String(a) +
                 "&field4=" + String(occ) +
                 "&field5=" + String(led) +
                 "&field6=" + String(buzzer);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + server + "\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.println("Data sent to ThingSpeak");
  } else {
    Serial.println("ThingSpeak Error");
  }
}