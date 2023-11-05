#include <NTPClient.h>    // Clock Sync
#include <ESP8266WiFi.h>  // ESP8266 Wifi Lib
#include <ESP8266mDNS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>  // ESP WebServerr Lib
#include <WiFiUdp.h>
#include <LCD_I2C.h>            // I2C LIB 16x2 20x4 DotMatrix
#include <Adafruit_NeoPixel.h>  // WS218B Led Lib
#include <Adafruit_Sensor.h>    // Base Sensor Lib
#include <DHT.h>                // DHT
#include <DHT_U.h>              // Unified Dht
#include <ArduinoOTA.h>         // Remote Upload Lib



#define LED_PIN 14             // Built in Led Weemos D1 R2 Mini
#define LED_COUNT 3            // on board leds Ws218b
#define BRIGHTNESS_DEFAULT 20  // Default brightness for day time
#define BRIGHTNESS_NIGHT 5     // Night led brightness settings

#define BUTTON 4  //  Button Pin
#define SIREN 13  // Siren Pin
bool AlarmON = true;
bool AlarmOFF = false;

#define DHTPIN 12      // Dht Pin
#define DHTTYPE DHT11  // Dht Type

bool TempOK = false;   // State to compare if Temps are Optimal
bool HumidOK = false;  // State to compare if Humidity is Optimal

float temp = 0.0;   // Float incoming Temps
float humid = 0.0;  // Float incoming Humidity

AsyncWebServer server(80);

int SirenState = LOW;  // intiate siren in the off postion
int ledState = LOW;    // intiate Led in off postion

unsigned long OldSirenTime = 0;
unsigned long OldTime = 0;          // time hold for led blink
unsigned long OldIPTime = 0;        // time hold for IP Info Flash to LCD
unsigned long OldLcdClearTime = 0;  // time hold for LCD Clear

const long SirenTime = 200;      // timer for Siren to Pulse
const long IPtime = 10000;       // how often IP Shows on LCD 10000ms
const long LedTimer = 1000;      // Blink Led every 1000ms to count time
const long LcdClearTime = 1000;  // delay before lcd clears

int brightness = BRIGHTNESS_DEFAULT;  // intiate brightness as Default settings

const char *ssid = "Tip-jar";              // Your Wifi Network
const char *password = "PASSWORD1234LOL";  //Your Password

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };  // Display Day as Words

DHT_Unified dht(DHTPIN, DHTTYPE);  // Connecting Dht pin and type

WiFiUDP ntpUDP;  //????????

NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 6800, 3600);  //  Clock Sync Server  Adj 43200 Winter   or 46800 Summer for DST +12GMT Auck NZ
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);  // connect WS218B Pin,LedCount,Type

LCD_I2C lcd(0x26, 16, 2);  // What Size Dotmatrix used 16x2 , 20,4  (SCL,SDA)
 
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();

// Logic For WebServer Page--------------------------------------------------------------------------------------------

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


// End Of Web Page Logic-------------------------------------------------------------------------------------

String processor(const String &var) {
  //Serial.println(var);
  if (var == "TEMPERATURE") {
    return String(temp);
  } else if (var == "HUMIDITY") {
    return String(humid);
  }
  return String();
}

void setup() {
  Serial.begin(115200);          // Start Serial Moniter
  pinMode(BUTTON, INPUT);        // Define Button as INPUT
  pinMode(SIREN, OUTPUT);        // Define Siren as OUTPUT
  pinMode(BUILTIN_LED, OUTPUT);  //Define Builtin_led as OUTPUT
  WiFi.mode(WIFI_STA);           // Set Wifi Mode To Station
  WiFi.begin(ssid, password);    // connect ssid and password

  while (WiFi.status() != WL_CONNECTED) {  // check if  NOT connected
    delay(500);                            // Give Life a chance to catch up
    Serial.print(".");
    lcd.setCursor(1, 0);
    lcd.print(".");  // Prints on LCD . Until Connected
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
  ArduinoOTA.onStart([]() {  // Starts wifi update
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Updating");  //Print on LCD Updating
  });
  ArduinoOTA.onEnd([]() {  // Upload Complete
    Serial.println("\nEnd");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Up-Date Completed");  // Print on LCD End
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {  // Progress during update
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(progress / (total / 100));  // Prints on LCD The % of Upload Out of 100
  });
  ArduinoOTA.onError([](ota_error_t error) {  // If Upload FAILS
    Serial.printf("Error[%u]: ", error);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(error);  // Print On LCD Active Error type
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
  ArduinoOTA.begin();  // Begin the upload
  timeClient.begin();  // Start Clock Sync Server
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(temp).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(humid).c_str());
  });
  server.begin();            // Begin The server Active Webpage
  strip.begin();             // Intiate the led strip
  strip.setBrightness(255);  // Set the led strip Brightness for startup
  lcd.begin();               // Initiate the LCD
  lcd.backlight();           // Initate the LCD Backlight
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 0));  //Test  Orange
  }
  strip.show();
  delay(500);  // Give Life a chance to catch up
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));  //Test Green
  }
  strip.show();
  delay(500);  // Give Life a chance to catch up
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 255));  //Test Blue
  }
  strip.show();
  delay(500);  // Give Life a chance to catch up
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));  // Test OFF/BLACK
  }
  strip.show();
  delay(500);  // Give Life a chance to catch up
  digitalWrite(SIREN, HIGH);
  delay(500);  // Give Life a chance to catch up
  digitalWrite(SIREN, LOW);
  dht.begin();  //Intiate DHT
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
// Setup Completed --------------------------------------------------------------------

void updateBrightness() {  // checks if time has reached to dim lights to save power

  // Check if it's daytime (between 8 AM and 6 PM)
  if (hours >= 8 && hours <= 17) {
    brightness = BRIGHTNESS_DEFAULT;  //Day
    strip.setBrightness(brightness);  // Set the led strip Brightness for DAY
  } else {
    brightness = BRIGHTNESS_NIGHT;    //Night
    strip.setBrightness(brightness);  // Set the led strip Brightness for NIGHT
  }
  if ((hours >= 1) && (hours <= 7)) {
    strip.setPixelColor(1, strip.Color(0, 0, 0));  // OFF
    brightness == 0;
    Serial.println("off");
  }
}

void Alarm() {




  if ((hours >= 7) && (minutes >= 30)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("    Ayla and Locky  ");
    lcd.setCursor(1, 0);
    lcd.print("are retards :0 :P");

  //  digitalWrite(SIREN, HIGH);
    delay(500);
    digitalWrite(SIREN, LOW);
    delay(500);
  } else if (hours >= 8) {
  //  digitalWrite(SIREN, HIGH);
    delay(200);
    digitalWrite(SIREN, LOW);
    delay(200);
  } else if ((hours >= 8) && (minutes >= 30)) {
    digitalWrite(SIREN, HIGH);
  } else if (hours >= 11) {
    digitalWrite(SIREN, LOW);
  }
  if (BUTTON == HIGH) {
    AlarmOFF = true;
  } else {
    AlarmOFF = false;
  }
}
void loop() {

  ArduinoOTA.handle();  // upload via wifi

  Serial.println(WiFi.localIP());  // Serial prints the active IP address
  Alarm();
  updateBrightness();  // updates Strip Brightness to time of day

  int BATT = analogRead(A0);
  float volts = BATT * (5.00 / 1023.00);
  Serial.print("Volts = ");
  Serial.println(volts);

  unsigned long TimeIRL = millis();  //Time
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  temp = event.temperature;



  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error reading temperature!");
    TempOK = false;  // sets Temps As failed intiating Fail logic
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
  } else {
    TempOK = true;  // sets temps  as in optimal range
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    lcd.setCursor(8, 0);
    lcd.print("T:");
    lcd.print(temp);
    lcd.print("C");
    if (temp <= 20 && temp >= 0) {
      for (int i = 0; i < LED_COUNT; i++) {  //
        strip.setPixelColor(0, strip.Color(0, 0, 255));
      }
      strip.show();
    } else if (temp <= 30 && temp >= 20) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(0, strip.Color(0, 255, 0));
      }
      strip.show();
    } else if (temp <= 40 && temp >= 30) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(0, strip.Color(255, 0, 0));
      }
      strip.show();
    }
  }
  dht.humidity().getEvent(&event);
  humid = event.relative_humidity;
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Error reading humidity!");
    HumidOK = false;
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
    lcd.print(humid);
    lcd.print("%");
    if (humid <= 30 && humid >= 0) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(255, 0, 0));  // orange
      }
      strip.show();
    } else if (humid <= 60 && humid >= 30) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(0, 255, 0));  // green
      }
      strip.show();
    } else if (humid <= 80 && humid >= 60) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(0, 165, 255));  // orange
      }
      lcd.clear();
      digitalWrite(SIREN, HIGH);
      lcd.setCursor(0, 12);
      lcd.print("Siren");
      if (TimeIRL - OldSirenTime >= SirenTime) {
        OldSirenTime = TimeIRL;
        digitalWrite(SIREN, LOW);
        lcd.setCursor(0, 12);
        lcd.print("Siren pulse");
      }
      strip.show();
    } else if (humid <= 100 && humid >= 80) {
      digitalWrite(SIREN, HIGH);
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(2, strip.Color(0, 0, 255));  // red
      }
      strip.show();
    } else {
      digitalWrite(SIREN, LOW);
    }
  }

  if (TimeIRL - OldIPTime >= IPtime) {
    OldIPTime = TimeIRL;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0, 1);
    lcd.print(brightness);
    lcd.setCursor(4, 1);
    lcd.print("Volts =");
    lcd.print(volts);
    lcd.print("V");

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
  timeClient.update();  // Update the Clock
  lcd.setCursor(0, 0);
  lcd.print(daysOfTheWeek[timeClient.getDay()]);
  lcd.setCursor(0, 1);

  bool isPM = hours >= 12;

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
  if (TimeIRL - OldLcdClearTime >= LcdClearTime) {  // Check if time to clear LCD
    OldLcdClearTime = TimeIRL;
    lcd.clear();
  }
}
