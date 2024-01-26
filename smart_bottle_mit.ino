#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

// WiFi credentials
// const char* ssid = "your_wifi_ssid";
// const char* password = "your_wifi_password";

// MQTT credentials
const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_user = "root";
const char* mqtt_password = "Quench@it";
const int mqtt_port = 1883;

// MQTT topics
const char* todayIntakeTopic = "todayIntake";
const char* weeklyIntakeTopic = "weeklyIntake";
const char* monthlyIntakeTopic = "monthlyIntake";
const char* remainderTopic = "reminder";
const char* waterlevelTopic = "waterlevel";

// WiFiManager
WiFiManager wm;

// Water level sensor setup
const int trigPin = 5;  // D4 pin for ultrasonic sensor trig
const int echoPin = 18;  // D5 pin for ultrasonic sensor echo

// Water intake tracking
float todayIntake = 0.0;
float weeklyIntake = 0.0;
float monthlyIntake = 0.0;

// Time tracking
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 2 * 60 * 60 * 1000; // 2 hours in milliseconds

// Day, week, and month tracking
unsigned long lastDayReset = 0;
unsigned long lastWeekReset = 0;
unsigned long lastMonthReset = 0;

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  WiFiManager wm;
  bool res;
   res = wm.autoConnect("AutoConnectAP", "password"); // password protected AP

  if (!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  } else {
    Serial.println("connected...yeey");
  }

  // Connect to MQTT broker
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  connectToMQTT();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // Read water level sensor
  float waterLevel = getWaterLevel();

  static float prevWaterLevel = waterLevel; // Store previous water level

  // Calculate water intake only if water level decreases
  if (waterLevel < prevWaterLevel && waterLevel >= 0 && waterLevel <= 21.5) {
    unsigned long currentTime = millis();
    // Calculate change in water level (negative change indicates consumption)
    float waterChange = prevWaterLevel - waterLevel;

    // Update previous water level
    prevWaterLevel = waterLevel;

    // Check if it's a new day, and if so, reset daily intake
    if (currentTime - lastDayReset >= 86400000) { // 86400000 milliseconds in a day
      todayIntake = 0.0;
      lastDayReset = currentTime;
    }

    // Check if it's a new week, and if so, reset weekly intake
    if (currentTime - lastWeekReset >= 604800000) { // 604800000 milliseconds in a week
      weeklyIntake = 0.0;
      lastWeekReset = currentTime;
    }

    // Check if it's a new month, and if so, reset monthly intake
    if (currentTime - lastMonthReset >= 2628000000) { // Roughly 30.44 days in a month
      monthlyIntake = 0.0;
      lastMonthReset = currentTime;
    }

    // Update water intake values
    todayIntake += waterChange * 33.4; // Assuming 33.4 ml per cm
    weeklyIntake += waterChange * 33.4;
    monthlyIntake += waterChange * 33.4;

    // Publish water intake data to MQTT
    publishWaterIntake(todayIntake, weeklyIntake, monthlyIntake);

    // Print intake details
    Serial.print("Water consumed: ");
    Serial.print(waterChange); // ml
    Serial.print(" ml | Time: ");
    Serial.println(currentTime);
  }

  // Check if more than 2 hours have passed without water consumption
  if ((millis() - lastCheckTime) > (2 * 60 * 60 * 1000)) {
    // Send reminder to drink water
    Serial.println("Reminder: Drink water!");
    // Publish reminder to MQTT
    char reminderData[50];
    snprintf(reminderData, sizeof(reminderData), "{\"message\":\"Drink water!\"}");
    client.publish(remainderTopic, reminderData);
  }

  // Update intake statistics every 2 hours
  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    Serial.println("Intake Stats:");
    Serial.print("Today: ");
    Serial.print(todayIntake);
    Serial.println(" ml");
    Serial.print("This Week: ");
    Serial.print(weeklyIntake);
    Serial.println(" ml");
    Serial.print("This Month: ");
    Serial.print(monthlyIntake);
    Serial.println(" ml");

    // Publish water level to MQTT
    char waterLevelData[50];
    snprintf(waterLevelData, sizeof(waterLevelData), "{\"water_level\":%.2f}", waterLevel);
    client.publish(waterlevelTopic, waterLevelData);

    lastCheckTime = millis();
  }

  delay(6000); // Delay for stability
}

// Function to get water level using ultrasonic sensor
float getWaterLevel() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * 0.0343 / 2;
  float waterLevel = 21.5 - distanceCm;
  return waterLevel;
}

// Function to publish water intake data to MQTT
void publishWaterIntake(float today, float week, float month) {
  char todayData[50];
  char weekData[50];
  char monthData[50];

  snprintf(todayData, sizeof(todayData), "{\"today\":%.2f}", today);
  snprintf(weekData, sizeof(weekData), "{\"week\":%.2f}", week);
  snprintf(monthData, sizeof(monthData), "{\"month\":%.2f}", month);

  client.publish(todayIntakeTopic, todayData);
  client.publish(weeklyIntakeTopic, weekData);
  client.publish(monthlyIntakeTopic, monthData);
}

// MQTT connection function
void connectToMQTT() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retry in 5 seconds");
      delay(5000);
    }
  }
}

// MQTT callback function
void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming MQTT messages here
}