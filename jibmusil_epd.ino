/*
ESP-32 E-ink display jibmusil name tag
board : LilyGo EPD 4.7" display / ESP32-S3 inbuilt
by jayden 2023
*/

#include <Arduino.h>
#include "epd_driver.h"  // LilyGo EPD 4.7" lib: https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
#include <SPI.h>
#include "fonts.h"
#include "long_logo.h"

#include "confidential.h"
#include "configuration.h"

#include <WiFi.h>
#include <time.h>

#include <ArduinoJson.h>  // for weather json lib: https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>
#include <PubSubClient.h>  // mqtt lib: https://github.com/knolleary/pubsubclient/

#define IN_HOUSE 0  // Desk status
#define CHECK_OUT 1
#define EMPTY 2

#define SCREEN_W EPD_WIDTH  // EPD resolution : 960(W)*540(H)
#define SCREEN_H EPD_HEIGHT

const long gmtOffset_sec = 32400;  // Time offset (GMT+9) korea
const int daylightOffset_sec = 0;

String Time_str = "--:--:--";
String Date_str = "-- --- ----";

static struct tm current_time;
static struct tm old_time;

enum alignment { LEFT,
                 RIGHT,
                 CENTER };

#define White 0xFF
#define Black 0x00

uint8_t* framebuffer;

WiFiClient wclient;
PubSubClient client(wclient);

int wifi_signal = 0;
String user_name;

void Init_system() {
  Serial.begin(115200);

  epd_init();  // E-paper initiailizing
  epd_poweron();
  epd_clear();

  framebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);  // frame buffer memory allocate
  if (!framebuffer) Serial.println("Memory allocation failed!");

  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);  // frame buffer clear

  Serial.println("\n Connect WIFI To..." + String(ssid));
  Init_wifi();
}

void Init_wifi() {

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);     //esp32 set to Station mode
  WiFi.setAutoConnect(1);  // auto connect set
  WiFi.setAutoReconnect(1);
  WiFi.begin(ssid, password);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("STA: Failed\n");
    WiFi.disconnect(0);
    delay(500);
    WiFi.begin(ssid, password);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI();  // check wifi strength
    Serial.println("WIFI Connected Success!!");
    Serial.println("Signal strength : ");
    Serial.print(WiFi.RSSI());
    Serial.println("WIFI connected to " + WiFi.localIP().toString());
  } else {
    Serial.println("WIFI connecting Failed");
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {

  String response;

  Serial.print("Message arrived [");
  Serial.print(mqtt_topic_status);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    response = (char)payload[i];
  }

  if ((char)payload[0] == '0') {
    draw_in_house();
    
    for (int i = 1; i < length; i++) {
      user_name += (char)payload[i];
    }
  }

  if ((char)payload[0] == '1') {
    draw_check_out();
  }

  if ((char)payload[0] == '2') {
    draw_empty();
  }

  Serial.println();
}

void re_connect_mqtt() {
  while (!client.connected()) {
    Serial.println("Waiting for MQTT connection");

    if (client.connect(ID)) {
      client.subscribe(mqtt_topic_status);
      Serial.println("MQTT connected");
      Serial.println("Publishing / Subscribing to");
      Serial.println(mqtt_topic_status);
    } else {
      Serial.println("MQTT re-connecting");
      vTaskDelay(1000);
    }
  }
}

void setup_time() {
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");

  char time_output[30], day_output[30], update_time[30];

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 5000)) {
    Serial.println("Failed to get time");
    return;
  } else {
    Serial.print("Current Time : ");
    Serial.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }
  strftime(day_output, sizeof(day_output), "%Y/%b/%d/%a", &timeinfo);  // Creates  "YYYY / MM / DD / mon"
  strftime(update_time, sizeof(update_time), "%H:%M", &timeinfo);      // Creates: "HH:MM"
  sprintf(time_output, "%s", update_time);

  Time_str = time_output;
  Date_str = day_output;

  old_time = timeinfo;
}

void time_update() {
  char time_output[30];

  if (!getLocalTime(&current_time, 1000)) {
    Serial.println("Failed to get current time");
  }

  if (current_time.tm_hour != old_time.tm_hour) {
    strftime(time_output, sizeof(current_time), "%H:%M", &current_time);  // update: "HH:MM"
    drawString(20, 65, time_output, LEFT);
  }
  if (current_time.tm_min != old_time.tm_min) {
    strftime(time_output, sizeof(current_time), "%H:%M", &current_time);  // update: "HH:MM"
    drawString(20, 65, time_output, LEFT);
    old_time = current_time;
    client.publish(mqtt_topic_status, "time changed");
  }
}

void setup() {
  Init_system();
  setup_time();

  drawString(20, 20, Date_str, LEFT);
  drawString(20, 65, Time_str, LEFT);
  drawString(20, 110, String(wifi_signal), LEFT);

  displayLogo();

  if (!client.connected()) {
    re_connect_mqtt();
  }
}

void loop() {
  time_update();
  client.loop();

  vTaskDelay(500);
}

void drawString(int x, int y, String text, alignment align) {
  char* data = const_cast<char*>(text.c_str());
  int x1, y1;  //the bounds of x,y and w and h of the variable 'text' in pixels.
  int w, h;
  int xx = x, yy = y;

  get_text_bounds((GFXfont*)&OpenSans18B, data, &xx, &yy, &x1, &y1, &w, &h, NULL);

  Rect_t area = {
    .x = x,
    .y = y,
    .width = w,
    .height = h,
  };
  if (align == RIGHT) x = x - w;
  if (align == CENTER) x = x - w / 2;
  
  int cursor_y = y + h;

  epd_clear_area_cycles(area, 5, 50);  //Clear old string area

  //epd_fill_rect(x, y, w, h, White, framebuffer);  // Clear old string space
  //epd_draw_grayscale_image(area, framebuffer);

  writeln((GFXfont*)&OpenSans18B, data, &x, &cursor_y, framebuffer);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);

  displayLogo(); 
}

void displayLogo() { //Draw bottom logo
  Rect_t area = {
    .x = 0,
    .y = 460,
    .width = logo_width,
    .height = logo_height,
  };

  epd_draw_grayscale_image(area, (uint8_t*)logo_data);
}

//*****Desk Status*****//
void draw_in_house() {
  drawString(EPD_WIDTH / 2, EPD_HEIGHT / 2, user_name, CENTER);
}

void draw_check_out() {
  drawString(EPD_WIDTH / 2, EPD_HEIGHT / 2, "EMPTY", CENTER);
}

void draw_empty() {
  drawString(EPD_WIDTH / 2, EPD_HEIGHT / 2, "EMPTY", CENTER);
}