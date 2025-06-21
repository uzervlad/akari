#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <FastLED.h>

#define FASTLED_ESP8266_NODEMCU_PIN_ORDER

#define DATA_PIN 5
#define WIFI_SSID "********"
#define WIFI_PASS "********"

WiFiUDP Udp;
#define UDP_PORT 8888
char incoming[UDP_TX_PACKET_MAX_SIZE];

void beginPacket() {
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
}

void endPacket() {
  Udp.endPacket();
}

#define NUM_LEDS 84

#define MODE_STATIC 0
#define MODE_SCROLLING 1
#define MODE_BREATHING 2

CRGB leds[NUM_LEDS];
CRGB display[NUM_LEDS];
uint mode = MODE_STATIC;
short base_brightness = 15;
bool on = true;

void setup() {
  Serial.begin(115200);
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  Udp.begin(UDP_PORT);

  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Listening on port %d\n", UDP_PORT);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(display, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::MediumPurple);
  FastLED.setBrightness(base_brightness);
}

const char PNG[4] = "PNG"; // ping
const char INF[4] = "INF"; // request info
const char TGL[4] = "TGL"; // toggle on/off
const char SET[4] = "SET"; // set leds
const char BRI[4] = "BRI"; // set brightness
const char MOD[4] = "MOD"; // set mode

#define TICKS_PER_SECOND 15
const unsigned long TICK_INTERVAL = 1000000 / TICKS_PER_SECOND;
unsigned long last_ts = 0;

unsigned long ticks = 0;

void loop() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int len = Udp.read(incoming, UDP_TX_PACKET_MAX_SIZE);

    if (memcmp(incoming, PNG, 3) == 0) {
      beginPacket();
      Udp.write("PNG");
      endPacket();
    } else
    if (memcmp(incoming, INF, 3) == 0) {
      beginPacket();
      Udp.write("INF");
      Udp.write((uint)NUM_LEDS);
      Udp.write((uint)base_brightness);
      Udp.write((uint)on);
      endPacket();
    } else
    if (memcmp(incoming, TGL, 3) == 0) {
      on = !on;

      if (on) {
        FastLED.setBrightness(base_brightness);
      } else {
        FastLED.setBrightness(0);
      }
    } else
    if (memcmp(incoming, SET, 3) == 0) {
      memcpy(leds, &incoming[3], sizeof(CRGB) * NUM_LEDS);
    } else
    if (memcmp(incoming, BRI, 3) == 0) {
      base_brightness = incoming[3];
      FastLED.setBrightness(base_brightness);
    } else
    if (memcmp(incoming, MOD, 3) == 0) {
      mode = incoming[3];
    }
  }

  unsigned long ts = micros();

  if (ts - last_ts >= TICK_INTERVAL) {
    last_ts = ts;
    ticks++;

    switch (mode) {
      case MODE_STATIC: {
        memcpy(display, leds, sizeof(CRGB) * NUM_LEDS);
        break;
      }
      case MODE_SCROLLING: {
        int l = ticks % NUM_LEDS;
        int r = NUM_LEDS - l;
        memcpy(display, &leds[l], sizeof(CRGB) * r);
        memcpy(&display[r], leds, sizeof(CRGB) * l);
        break;
      }
      case MODE_BREATHING: {
        memcpy(display, leds, sizeof(CRGB) * NUM_LEDS);
        float t = 1 - (sin((float)ticks / 12) + 1) / 4;
        float brightness = (float)base_brightness * t;
        if (on) {
          FastLED.setBrightness((short)brightness);
        }
        break;
      }
    }

    FastLED.show();
  }
}
