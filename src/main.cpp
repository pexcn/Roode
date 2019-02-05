/*  RooDe - Room Presence Detection
Author: Kai Bepperling, kai.bepperling@gmail.com
License: GPLv3
*/
#include <Arduino.h> //need to be included, cause the file is moved to a .cpp file
#include <Config.h>
#include <Wire.h>
#ifdef USE_MQTT
#include <ESP8266WiFi.h>
#include <MQTTTransmitter.h>
MQTTTransmitter transmitter;
const char *topic_Domoticz_IN = "domoticz/in";
const char *topic_Domoticz_OUT = "domoticz/out";
#endif
#ifdef USE_MYSENSORS
#include <MySensors.h> // include the MySensors library
#include <MySensorsTransmitter.h>
MySensorsTransmitter transmitter;
#endif

#include <OptionChecker.h>
#include <MotionSensor.h> //MotionSensorLib
#include <Calibration.h>
#include <VL53L0XSensor.h>
#include <PeopleCounter.h>

// battery setup
#ifdef USE_BATTERY
#include <BatteryMeter.h>                           //Include and Set Up BatteryMeter Library
BatteryMeter battery(BATTERY_METER_PIN);            //BatteryMeter instance
MyMessage voltage_msg(CHILD_ID_BATTERY, V_VOLTAGE); //MySensors battery voltage message instance
#endif

VL53L0XSensor ROOM_SENSOR(ROOM_XSHUT, ROOM_SENSOR_newAddress);
VL53L0XSensor CORRIDOR_SENSOR(CORRIDOR_XSHUT, CORRIDOR_SENSOR_newAddress);

void setup()
{
#ifdef USE_MQTT
  Wire.begin(D6, D5);
  Serial.begin(115200);
  // Connect to WiFi access point.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(transmitter.ssid);

  // connect to WiFi Access Point
  // ESP.wdtDisable(); //Disable soft watch dog
  WiFi.mode(WIFI_STA);
  WiFi.begin(transmitter.ssid, transmitter.password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection to the main WiFi Failed!");
    delay(2000);
    if (transmitter.WiFi_AP == 1)
    {
      transmitter.WiFi_AP = 2;
      Serial.println("Trying to connect to the alternate WiFi...");
      WiFi.begin(transmitter.ssid2, transmitter.password2);
    }
    else
    {
      transmitter.WiFi_AP = 1;
      Serial.println("Trying to connect to the main WiFi...");
      WiFi.begin(transmitter.ssid, transmitter.password);
    }
  }

  //MQTT
  void callback(char *topic, byte *payload, unsigned int length);
  client.setServer(transmitter.mqtt_server, 1883);
  client.setCallback(callback);

  // say we are now ready and give configuration items
  Serial.println("Ready");
  Serial.print("Connected to ");
  if (transmitter.WiFi_AP == 1)
    Serial.println(transmitter.ssid);
  else
    Serial.println(transmitter.ssid2);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  if (!client.connected())
  { // MQTT connection
    transmitter.reconnect();
  }
#endif
#ifdef USE_MYSENSORS
  Wire.begin();
#endif

#ifdef USE_OLED
  oled.init();
  oled.sendCommand(BRIGHTNESS_CTRL);
  oled.sendCommand(BRIGHTNESS);
  oled.clear();
  oled.setTextSize(1);
  oled.setCursor(0, 25);
  oled.println("### Roode ###");
  oled.setCursor(10, 0);
  oled.print("RooDe: ");
  oled.print(ROODE_VERSION);
  oled.print("\n");
  oled.print("MySensors: ");
  oled.println(MYSENSORS_LIBRARY_VERSION);
  delay(2000);
#endif

  Serial.println("##### RooDe Presence Detection System #####");

  //Motion Sensor
  pinMode(DIGITAL_INPUT_SENSOR, INPUT); // declare motionsensor as input

  //DistanceSensors
  ROOM_SENSOR.init();
  CORRIDOR_SENSOR.init();

#ifdef CALIBRATION
#ifdef USE_OLED
  oled.clear();
  oled.setCursor(0, 0);
  oled.setTextSize(1, 1);
  oled.println("# Calibrate Motion #");
#endif
  motion.Setup(MOTION_INIT_TIME);
  // Serial.println("#### motion sensor initialized ####");

  Serial.println("#### calibrate the ir sensors ####");
  ROOM_SENSOR.calibration();
  CORRIDOR_SENSOR.calibration();
  // calibration(ROOM_SENSOR, CORRIDOR_SENSOR);
#endif

  Serial.println("#### Setting the PresenceCounter and Status to OUT (0) ####");
#ifdef USE_MQTT
  if (!client.connected())
  { // MQTT connection
    transmitter.reconnect();
  }
#endif
  transmitter.transmit(transmitter.devices.room_switch, 0, "Off");
  transmitter.transmit(transmitter.devices.peoplecounter, 0);

#ifdef USE_OLED
  oled.clear();
  oled.setCursor(10, 0);
  oled.println("Setting Counter to 0");
  delay(1000);
#endif
} // end of setup()
#ifdef USE_MYSENSORS
void presentation()
{
  transmitter.presentation();
}
#endif

int lastState = LOW;
int newState = LOW;

void loop()
{
#ifdef USE_MQTT
  if (!client.connected())
  { // MQTT connection
    transmitter.reconnect();
  }
#endif

  Serial.print("Room Sensor Threshold: ");
  Serial.println(ROOM_SENSOR.getThreshold());
  Serial.print("Room Sensor Threshold: ");
  Serial.println(ROOM_SENSOR.threshold);

  Serial.print("Corridor Sensor Threshold: ");
  Serial.println(CORRIDOR_SENSOR.getThreshold());
  Serial.print("Corridor Sensor Threshold: ");
  Serial.println(CORRIDOR_SENSOR.threshold);
  // transmitter.transmit(transmitter.devices.room_switch, 0);
  // delay(1000);
  // transmitter.transmit(transmitter.devices.room_switch, 1);
  if (ROOM_SENSOR.timeoutOccurred() || CORRIDOR_SENSOR.timeoutOccurred())
  {
#ifdef USE_OLED
    oled.clear();
    oled.setCursor(5, 0);
    oled.setTextSize(2, 1);
    oled.print("Timeout occured!");
#endif
    // reportToController(65535);
    Serial.println("Timeout occured. Restart the System");

    // calibration(ROOM_SENSOR, CORRIDOR_SENSOR);
    char buf[20];
    const char *room_threhsold = itoa(ROOM_SENSOR.calibration(), buf, 10);
    const char *corridor_threhsold = itoa(CORRIDOR_SENSOR.calibration(), buf, 10);

    
    strcpy(buf, "");
    strcat(buf, room_threhsold);
    strcat(buf, corridor_threhsold);
    transmitter.transmit(transmitter.devices.threshold, 0, buf);
  }

  //   // Sleep until interrupt comes in on motion sensor. Send never an update
  if (motion.checkMotion() == LOW)
  {
#ifdef MY_DEBUG
    Serial.println("1. Motion sensor is off");
#endif
#ifdef USE_OLED
    oled.clear();
#endif
#ifdef USE_ENEGERY_SAVING
    if (lastState == HIGH)
    {
#ifdef MY_DEBUG
      Serial.println("2. Motion sensor is off. Last readloop");
#endif
      peoplecounting(ROOM_SENSOR, CORRIDOR_SENSOR, transmitter);

#ifdef MY_DEBUG
      Serial.println("3. Shutting down sensors");
#endif
      ROOM_SENSOR.stopContinuous();
      CORRIDOR_SENSOR.stopContinuous();
      lastState = LOW;
    }
#else
    peoplecounting(ROOM_SENSOR, CORRIDOR_SENSOR, transmitter);
#endif
#ifdef USE_BATTERY
    smartSleep(digitalPinToInterrupt(DIGITAL_INPUT_SENSOR), RISING, SLEEP_TIME); //sleep function only in battery mode needed
    peoplecounting(ROOM_SENSOR, CORRIDOR_SENSOR, transmitter);
#ifdef USE_OLED
    oled.clear();
    oled.setCursor(5, 0);
    oled.setTextSize(2, 1);
    oled.print("Counter: ");
    oled.println(peopleCount);
#endif

    while (motion.checkMotion() != LOW)
    {
      yield();
#ifdef MY_DEBUG
      Serial.println("4. Motion sensor is on. Start counting");
#endif
      peoplecounting(ROOM_SENSOR, CORRIDOR_SENSOR, transmitter);
    }
#endif
  }
  else
  {
    if (lastState == LOW)
    {
#ifdef USE_ENEGERY_SAVING
#ifdef MY_DEBUG
      Serial.println("5. Starting continuous mode again");
#endif
#ifdef MY_DEBUG
      Serial.println("6. Start Sensors");
#endif
      ROOM_SENSOR.startContinuous();
      CORRIDOR_SENSOR.startContinuous();
      delay(10);
#endif
    }
    lastState = HIGH;
#ifdef USE_OLED
    oled.clear();
    oled.setCursor(5, 0);
    oled.setTextSize(2, 1);
    oled.print("Counter: ");
    oled.println(peopleCount);
#endif
    while (motion.checkMotion() != LOW)
    {
#ifdef MY_DEBUG
      Serial.println("7. Motion sensor is on. Start counting");
#endif
      peoplecounting(ROOM_SENSOR, CORRIDOR_SENSOR, transmitter);
    }
  }
#ifdef USE_MQTT
  client.loop();
#endif
}

#ifdef USE_MQTT
// !! Needs to be implemented !!
void callback(char *topic, byte *payload, unsigned int length)
{ // ****************

  DynamicJsonDocument root(MQTT_MAX_PACKET_SIZE);
  String messageReceived = "";

  // Affiche le topic entrant - display incoming Topic
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // decode payload message
  for (int i = 0; i < length; i++)
  {
    messageReceived += ((char)payload[i]);
  }
  // display incoming message
  Serial.print(messageReceived);

  // if domoticz message
  if (strcmp(topic, topic_Domoticz_OUT) == 0)
  {
    //JsonObject& root = jsonBuffer.parseObject(messageReceived);
    DeserializationError error = deserializeJson(root, messageReceived);
    if (error)
      return;

    const char *idxChar = root["idx"];
    String idx = String(idxChar);
    /*
        if ( idx == LIGHT_SWITCH_IDX[0] ) {      
           const char* cmde = root["nvalue"];
           if( strcmp(cmde, "0") == 0 ) {  // 0 means we have to switch OFF the lamps
                if( LIGHT_ACTIVE[0] == "On" ) { digitalWrite(Relay1, LOW); LIGHT_ACTIVE[0] = "Off"; } 
           } else if( LIGHT_ACTIVE[0] == "Off" ) { digitalWrite(Relay1, HIGH); LIGHT_ACTIVE[0] = "On"; }           
           Serial.print("Lighting "); Serial.print(LIGHTING[0]); Serial.print(" is now : "); Serial.println(LIGHT_ACTIVE[0]);
        }  // if ( idx == LIGHT_SWITCH_IDX[0] ) {

        if ( idx == LIGHT_SWITCH_IDX[1] ) {      
           const char* cmde = root["nvalue"];
           if( strcmp(cmde, "0") == 0 ) { 
                if( LIGHT_ACTIVE[1] == "On" ) { digitalWrite(Relay2, LOW); LIGHT_ACTIVE[1] = "Off"; } 
           } else if( LIGHT_ACTIVE[1] == "Off" ) { digitalWrite(Relay2, HIGH); LIGHT_ACTIVE[1] = "On"; }    
           Serial.print("Lighting "); Serial.print(LIGHTING[1]); Serial.print(" is now : "); Serial.println(LIGHT_ACTIVE[1]);
        }  // if ( idx == LIGHT_SWITCH_IDX[0] ) {
            */
  } // if domoticz message

  delay(15);
} // void callback(char* to   ****************

#endif