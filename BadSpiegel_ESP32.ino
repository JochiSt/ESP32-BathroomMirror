#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include <NeoPixelBus.h>
#include <NeoPixelBrightnessBus.h>
#include <Artnet.h>

/*****************************************************/
//   WiFi
#include "WiFi_credentials.h"
#include "OTA.h"

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
uint8_t brightness = 127;

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
  if ( artnet_size % 4 == 0) { // make sure, that we have full pixel data
    for (i = 0; i < artnet_size; i += 4) {
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
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

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

  // this resets all the neopixels to an off state
  strip.Begin();

  Serial.println();
  Serial.println("Running...");

  for ( int i = 0; i < PixelCount; i++) {
    strip.SetPixelColor(i, white);
  }

  strip.SetBrightness(brightness);

  strip.Show();

  CTRLmode = MANUAL;
}


#define MAX_TIMER_STEP 6
uint8_t timerStep = 0;

void handleTimer() {
  Serial.println("handle timer");
  Serial.println(timerStep);

  if (timerStep == 0) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, red);
    }
  } else if (timerStep == 1) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, green);
    }
  } else if (timerStep == 2) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, blue);
    }
  } else if (timerStep == 3) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, white);
    }
  } else if (timerStep == 4) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, Dwhite);
    }
  } else if (timerStep == 5) {
    for ( int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, black);
    }
  } else if (timerStep == 6) {
    createRainbow();
  }

  timerStep ++;
  if ( timerStep > MAX_TIMER_STEP) {
    timerStep = 0;
  }

  if (ArtNet_NoRX < 127) { // ensure, that we do not overflow
    ArtNet_NoRX++;
  }

  strip.SetBrightness(brightness);
  strip.Show();
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

  artnet.parse(); // check if artnet packet has come and execute callback
}
