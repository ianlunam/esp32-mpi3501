// Button widget demo, requires SPI display with touch screen

// Requires widget library here:
// https://github.com/Bodmer/TFT_eWidget
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>

#include <FS.h>
#include "Free_Fonts.h"  // Include the header file attached to this sketch

#include <TFT_eSPI.h>     // Hardware-specific library
#include <TFT_eWidget.h>  // Widget library

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

ButtonWidget btnR = ButtonWidget(&tft);

#define BUTTON_W 100
#define BUTTON_H 50

// Create an array of button instances to use in for() loops
// This is more useful where large numbers of buttons are employed
ButtonWidget *btn[] = { &btnR };
;
uint8_t buttonCount = sizeof(btn) / sizeof(btn[0]);


// MQTT Broker
const char *mqtt_broker = "mqtt.local";
const char *topic = "fairylights/toggle";
const char *state_topic = "homeassistant/switch/sonoff_1001ffea20_1/state";
byte on_state[] = {'o','n'};
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char *topic, byte *payload, unsigned int length) {
  if (memcmp(payload, on_state, sizeof(on_state)) == 0) {
    Serial.println("New state is on");
    btnR.drawSmoothButton(true, 3, TFT_BLACK, "ON");
  } else {
    Serial.println("New state is off");
    btnR.drawSmoothButton(false, 3, TFT_BLACK, "OFF");
  }
}

void btnR_pressAction(void) {
  if (btnR.justPressed()) {
    Serial.println("Button toggled");
    client.publish(topic, "Light toggle.");
    btnR.setPressTime(millis());
  }
}

void btnR_releaseAction(void) {
  // Not action
}

void initButtons() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(5, 30);
  tft.print("Fairy Lights");
  
  // uint16_t x = (tft.width() - BUTTON_W) / 2;
  // uint16_t y = tft.height() / 2 - BUTTON_H - 10;
  uint16_t x = 15;
  uint16_t y = (BUTTON_H / 2) + 20;
  btnR.initButtonUL(x, y, BUTTON_W, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_GREEN, "OFF", 1);
  btnR.setPressAction(btnR_pressAction);
  //btnR.setReleaseAction(btnR_releaseAction);
  btnR.drawSmoothButton(false, 3, TFT_BLACK);  // 3 is outline width, TFT_BLACK is the surrounding background colour for anti-aliasing

}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(FF18);


  // Calibrate the touch screen and retrieve the scaling factors
  touch_calibrate();
  initButtons();

  // Get WiFi creds from preferences storage
  Preferences wifiCreds;
  wifiCreds.begin("wifiCreds", true);
  String ssid = wifiCreds.getString("ssid");
  String pwd = wifiCreds.getString("password");
  wifiCreds.end();

  WiFi.begin(ssid.c_str(), pwd.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (!MDNS.begin("mpi3501")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
        delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) { 
      Serial.println("EMQX MQTT broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  // Publish and subscribe
  // client.publish(topic, "Hi, I'm ESP32 ^^");
  client.subscribe(state_topic);
}

void loop() {
  static uint32_t scanTime = millis();
  uint16_t t_x = 9999, t_y = 9999;  // To store the touch coordinates

  // Scan keys every 50ms at most
  if (millis() - scanTime >= 50) {
    // Pressed will be set true if there is a valid touch on the screen
    bool pressed = tft.getTouch(&t_x, &t_y);
    scanTime = millis();
    for (uint8_t b = 0; b < buttonCount; b++) {
      if (pressed) {
        if (btn[b]->contains(t_x, t_y)) {
          btn[b]->press(true);
          btn[b]->pressAction();
        }
      } else {
        btn[b]->press(false);
        btn[b]->releaseAction();
      }
    }
  }
  client.loop();
}

void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!LittleFS.begin()) {
    Serial.println("Formating file system");
    LittleFS.format();
    LittleFS.begin();
  }

  // check if calibration file exists and size is correct
  if (LittleFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      // Delete if we want to re-calibrate
      LittleFS.remove(CALIBRATION_FILE);
    } else {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}
