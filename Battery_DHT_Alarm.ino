#include "IP.h"

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <LCD_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>



#define LED_PIN 14
#define LED_COUNT 3
#define BRIGHTNESS 50

#define BUTTON 4
#define SIREN 0

#define DHTPIN 12
#define DHTTYPE DHT11

bool TempOK = false;
bool HumidOK = false;

float t = 0.0;
float h = 0.0;

AsyncWebServer server(80);

int SirenState = LOW;
const long SirenTimer = 200;
int ledState = LOW;
unsigned long OldTime = 0;
unsigned long OldIPTime = 0;
const long IPtime = 10000;
const long LedTimer = 1000;

int brightness=0;

const char *ssid = "Tip-jar";
const char *password = "PASSWORD1234LOL";

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

DHT_Unified dht(DHTPIN, DHTTYPE);

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 46800, 60000);  //43200 Winter   or 46800 Summer
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

LCD_I2C lcd(0x26, 16, 2);


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Home Temps</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
  <br><br><br><br><br><br><br><br><br><br><br><br><br><br>
  <footer>Made By MacaDaca88</footer>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 1000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 1000 ) ;
</script>
</html>)rawliteral";

String processor(const String &var) {
  //Serial.println(var);
  if (var == "TEMPERATURE") {
    return String(t);
  } else if (var == "HUMIDITY") {
    return String(h);
  }
  return String();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT);
  pinMode(SIREN, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(1, 0);
    lcd.print(".");
  }
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to Weemos8266-[MAC]
  // ArduinoOTA.setHostname("Weemos8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Updating");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(progress / (total / 100));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  timeClient.begin();
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(t).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(h).c_str());
  });
  server.begin();
  strip.begin();
  strip.setBrightness(brightness);
  lcd.begin();
  lcd.backlight();
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 0));
  }
  strip.show();
  delay(500);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  }
  strip.show();
  delay(500);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 255));
  }
  strip.show();
  delay(500);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  delay(500);
  digitalWrite(SIREN, HIGH);
  delay(500);
  digitalWrite(SIREN, LOW);
  dht.begin();
  Serial.println(F("DHTxx Unified Sensor Example"));
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print(F("Sensor Type: "));
  Serial.println(sensor.name);
  Serial.print(F("Driver Ver:  "));
  Serial.println(sensor.version);
  Serial.print(F("Unique ID:   "));
  Serial.println(sensor.sensor_id);
  Serial.print(F("Max Value:   "));
  Serial.print(sensor.max_value);
  Serial.println(F("°C"));
  Serial.print(F("Min Value:   "));
  Serial.print(sensor.min_value);
  Serial.println(F("°C"));
  Serial.print(F("Resolution:  "));
  Serial.print(sensor.resolution);
  Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print(F("Sensor Type: "));
  Serial.println(sensor.name);
  Serial.print(F("Driver Ver:  "));
  Serial.println(sensor.version);
  Serial.print(F("Unique ID:   "));
  Serial.println(sensor.sensor_id);
  Serial.print(F("Max Value:   "));
  Serial.print(sensor.max_value);
  Serial.println(F("%"));
  Serial.print(F("Min Value:   "));
  Serial.print(sensor.min_value);
  Serial.println(F("%"));
  Serial.print(F("Resolution:  "));
  Serial.print(sensor.resolution);
  Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
}

void loop() {
 int brightness = BRIGHTNESS;
  ArduinoOTA.handle();  // upload via wifi
  Serial.println(WiFi.localIP());
  unsigned long TimeIRL = millis();
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  float temp = event.temperature - 0.0;
  t = event.temperature;
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error reading temperature!");
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
  } else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    lcd.setCursor(8, 0);
    lcd.print("T:");
    lcd.print(temp);
    lcd.print("C");
    if (event.temperature <= 20 && event.temperature >= 0) {
      for (int i = 0; i < LED_COUNT; i++) {  //
        strip.setPixelColor(0, strip.Color(0, 0, 255));
      }
      strip.show();
    } else if (event.temperature <= 30 && event.temperature >= 20) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(0, strip.Color(0, 255, 0));
      }
      strip.show();
    } else if (event.temperature <= 40 && event.temperature >= 30) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(0, strip.Color(255, 0, 0));
      }
      strip.show();
    }
  }
  dht.humidity().getEvent(&event);
  h = event.relative_humidity;
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Error reading humidity!");
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(2, strip.Color(255, 0, 0));
    }
    strip.show();
  } else {
    Serial.print(F("Humidity: "));
    Serial.print((int)event.relative_humidity);
    Serial.println(F("%"));
    lcd.setCursor(11, 1);
    lcd.print("H:");
    lcd.print(event.relative_humidity);
    lcd.print("%");
    if (h <= 30 && h >= 0) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(255, 0, 0));  // orange
      }
      strip.show();
    } else if (h <= 60 && h >= 30) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(0, 255, 0));  // green
      }
      strip.show();
    } else if (h <= 80 && h >= 60) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(0, 165, 255));  // orange
      }
      strip.show();
      digitalWrite(SIREN, HIGH);
    } else if (h <= 100 && h >= 80) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(0, 0, 255));  // red
      }
      strip.show();
    }
  }

  if (TimeIRL - OldIPTime >= IPtime) {
    OldIPTime = TimeIRL;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0, 12);
    lcd.print(brightness);
    delay(1000);
  }
  if (TimeIRL - OldTime >= LedTimer) {
    OldTime = TimeIRL;
    if (ledState == LOW) {
      ledState = HIGH;
      strip.setPixelColor(1, strip.Color(0, 0, 0));  // Off
      strip.show();
    } else {
      ledState = LOW;
      strip.setPixelColor(1, strip.Color(0, 0, 255));  // Blue
      strip.show();
    }
    digitalWrite(BUILTIN_LED, ledState);
  }
  timeClient.update();
  lcd.setCursor(0, 0);
  lcd.print(daysOfTheWeek[timeClient.getDay()]);
  lcd.setCursor(0, 1);
  int hours = timeClient.getHours();
  bool isPM = hours >= 12;
  if (hours >= 8 && hours <= 18) {
    brightness = 255;
  } else {
    brightness = BRIGHTNESS;
  }
  if (hours > 12) {
    hours -= 12;
  }
  if (hours == 0) {
    hours = 12;
  }
  lcd.print(hours);
  lcd.print(":");
  if (timeClient.getMinutes() < 10) {
    lcd.print("0");  // Add leading zero for single-digit minutes
  }
  lcd.print(timeClient.getMinutes());
  lcd.print(":");
  if (timeClient.getSeconds() < 10) {
    lcd.print("0");  // Add leading zero for single-digit seconds
  }
  lcd.print(timeClient.getSeconds());
  lcd.print(" ");
  lcd.print(isPM ? "PM" : "AM");
  Serial.println(daysOfTheWeek[timeClient.getDay()]);
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.println(timeClient.getSeconds());
  delay(1000);
  lcd.clear();
}
