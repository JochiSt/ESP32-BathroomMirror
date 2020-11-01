#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

// use WiFiAutoSelector from https://gist.github.com/AndiSHFR/e9c46890af7cddff6cb5ea7d4f1c5c49
#include "WiFiAutoSelector.h"

#define WIFI_CONNECT_TIMEOUT 8000
WiFiAutoSelector wifiAutoSelector(WIFI_CONNECT_TIMEOUT);

// install NeoPixelBus by Makuna
#include <NeoPixelBus.h>
#include <NeoPixelBrightnessBus.h>

// install ArtNet by hideakitai
#include <Artnet.h>

/*****************************************************/
//   WiFi
#include "WiFi_credentials.h"
#include "OTA.h"

// primary WiFi
const char* ssid = mySSID;
const char* password = myPASSWORD;

// alternative WiFi
const char* ssid2 = mySSID2;
const char* password2 = myPASSWORD2;

/*****************************************************/
//   Timer -> create a hardware timer
hw_timer_t* timer = NULL;
volatile bool isTimer = false;
void IRAM_ATTR onTimer() {
  isTimer = true;
}

/*****************************************************/
//   NeoPixel
// TOP 35
// Left + Right = 38
const uint16_t PixelCount = 38 + 35 + 38; // this example assumes 4 pixels, making it smaller will cause a failure

#define colorSaturation 255

const uint8_t PixelPin = 26;  // used on ESP32
// see https://github.com/Makuna/NeoPixelBus/wiki/
// NeoPixelBus<NeoGrbwFeature, NeoEsp32I2s1800KbpsMethod> strip(PixelCount, PixelPin);
NeoPixelBrightnessBus<NeoGrbwFeature, NeoEsp32I2s1800KbpsMethod> strip(PixelCount, PixelPin);

// define some colors
const RgbwColor red     (colorSaturation,               0,               0,               0);
const RgbwColor green   (0              , colorSaturation,               0,               0);
const RgbwColor blue    (0              ,               0, colorSaturation,               0);
const RgbwColor white   (0              ,               0,               0, colorSaturation);
const RgbwColor Dwhite  (colorSaturation, colorSaturation, colorSaturation, colorSaturation);
const RgbwColor black   (0);

void createRainbow(int startPixel = 0) {
  for ( int i = 0; i < PixelCount; i++) {
    float hue = (float)i / (float)PixelCount;
    strip.SetPixelColor(i, HsbColor( hue, 1, 1) );
  }
  strip.Show();
}

/// global brightness level
volatile uint8_t brightness = 127;

/// Mode definitions

#define MANUAL 0  // Control Pixel via Buttons
#define ARTNET 1  // Control Pixel via ArtNet
uint8_t CTRLmode = MANUAL;

/*****************************************************
  ArtNet
  receive all pixel in R - G - B - W format
*/

/// counter, how long did we not receive ArtNet
uint8_t ArtNet_NoRX = 0;

ArtnetWiFiReceiver artnet;
void ArtNetCallback_Universe_1(uint8_t* artnet_data, uint16_t artnet_size) {
  uint16_t i;
  if ( (artnet_size % 4 == 0) || (artnet_size >= 4 * PixelCount) ) { // make sure, that we have full pixel data
    for (i = 0; (i < artnet_size) && (i < 4 * PixelCount); i += 4) {
      strip.SetPixelColor( i / 4,
                           RgbwColor(
                             artnet_data[i + 0],
                             artnet_data[i + 1],
                             artnet_data[i + 2],
                             artnet_data[i + 3]
                           ) );
    }
    // finally update all pixel
    strip.Show();
    ArtNet_NoRX = 0;
  }
  // Debug ArtNet
  // Serial.println(artnet_data[0]);
  // Serial.println(artnet_data[1]);
}

const int PIN_BUTTON_1 = 25;
const int PIN_BUTTON_2 = 33;
const int PIN_BUTTON_3 = 32;

const int PIN_LED = 27;

const uint8_t brightness_step = 8;

void toggleLED() {
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

void IRAM_ATTR button_1() {
  if ( brightness < 255 - brightness_step)
    brightness += brightness_step;
  else
    brightness = 255;

  toggleLED();
}

void IRAM_ATTR button_2() {
  if ( brightness >= brightness_step)
    brightness -= brightness_step;
  else
    brightness = 0;

  toggleLED();
}

void IRAM_ATTR button_3() {

  toggleLED();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  Serial.println("Ports...");
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  pinMode(PIN_BUTTON_1, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_1), button_1, FALLING);

  pinMode(PIN_BUTTON_2, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_2), button_2, FALLING);

  pinMode(PIN_BUTTON_3, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_3), button_3, FALLING);


  Serial.println("LED Pixel...");
  // this resets all the neopixels to an off state
  strip.Begin();

  // first set LED Pixel and do network startup later
  for ( int i = 0; i < PixelCount; i++) {
    strip.SetPixelColor(i, white);
  }

  strip.SetBrightness(brightness);

  strip.Show();

  Serial.println("WiFi...");

  wifiAutoSelector.add(ssid , password );
  wifiAutoSelector.add(ssid2, password2);

  WiFi.mode(WIFI_STA);

  Serial.print("Connecting...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting wifi ");
    if (-1 < wifiAutoSelector.scanAndConnect()) {
      int connectedIndex = wifiAutoSelector.getConnectedIndex();
      Serial.print("to '");
      Serial.print(wifiAutoSelector.getSSID(connectedIndex));
      Serial.println("'. Done.");
    } else {
      Serial.println("failed.");
    }
  }

  digitalWrite(PIN_LED, HIGH);

  Serial.println("OTA...");

  setupOTA("BadSpiegel");

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /************************************************/
  /* Use 1st timer of 4 */
  /* 1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up */
  timer = timerBegin(0, 80, true);

  /* Set alarm to call onTimer function every second 1 tick is 1us
    => 1 second is 1000000us */
  /* Repeat the alarm (third parameter) */
  timerAlarmWrite(timer, 1000000, true);

  timerAttachInterrupt(timer, &onTimer, true);

  /* Start an alarm */
  timerAlarmEnable(timer);

  /************************************************/

  Serial.println();
  Serial.println("Initializing...");
  Serial.flush();

  artnet.begin();
  artnet.subscribe(1, ArtNetCallback_Universe_1);



  Serial.println();
  Serial.println("Running...");



  CTRLmode = MANUAL;
  ArtNet_NoRX = 127;  // we have not received ArtNet
}


#define MAX_TIMER_STEP 6
uint8_t timerStep = 0;

void handleTimer() {
  timerStep ++;
  if ( timerStep > MAX_TIMER_STEP) {
    timerStep = 0;
  }

  if (ArtNet_NoRX < 127) { // ensure, that we do not overflow
    ArtNet_NoRX++;
  }

  if ( CTRLmode == ARTNET) {
    Serial.println("ArtNet Mode");
  } else {
    Serial.println(".");
  }

  ArduinoOTA.handle();

  Serial.print("Brightness: ");
  Serial.println(brightness);

  if ( CTRLmode == MANUAL) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, white);
    }
    strip.SetBrightness(brightness);
    strip.Show();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting wifi ");
    if (-1 < wifiAutoSelector.scanAndConnect()) {
      int connectedIndex = wifiAutoSelector.getConnectedIndex();
      Serial.print("to '");
      Serial.print(wifiAutoSelector.getSSID(connectedIndex));
      Serial.println("'. Done.");
    } else {
      Serial.println("failed.");
    }
  }

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}

void loop() {
  // handle timer
  if (isTimer) {
    isTimer = false;
    handleTimer();
  }

  // we have not received ArtNet since 30s
  if ( ArtNet_NoRX > 30 ) {
    CTRLmode = MANUAL;
  } else {
    CTRLmode = ARTNET;
  }

  if ( CTRLmode == MANUAL ) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, white);
    }
    strip.SetBrightness(brightness);
    strip.Show();
  }

  artnet.parse(); // check if artnet packet has come and execute callback
}
