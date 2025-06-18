// https://github.com/FastLED/FastLED/wiki/ESP8266-notes#pin-definitions
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#include <FastLED.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <IPAddress.h>

#define NUM_LEDS 84
#define DATA_PIN 1

CRGB leds[NUM_LEDS];

typedef uint16_t timer;

unsigned long last_ts = 0;
const timer TICKS_PER_SECOND = 40;
const unsigned long TICK_INTERVAL = 1000000 / TICKS_PER_SECOND;

const timer PULSE_TICKS = TICKS_PER_SECOND * 3;
const timer PULSE_MIDPOINT = PULSE_TICKS / 2;
const float PULSE_VALUE_MULTIPLIER = 1.7f;
const timer TRANSITION_TICKS = TICKS_PER_SECOND * 2;
const timer TRANSITION_MIDPOINT = TRANSITION_TICKS / 2;
const uint8_t BASE_SATURATION = 220;
bool direct_transition = false;
float base_value = 0.15f;
float hue_from = 0.0f;
float hue_to = 0.8f;
CHSV current;
timer transition_timer = TRANSITION_TICKS;
timer pulse_timer = PULSE_TICKS;

const char* SSID = "********";
const char* PASSWORD = "********";

WiFiUDP Udp;
unsigned int port = 8888;
char incoming[256];

IPAddress listenerAddr;
uint16_t listenerPort;

void udpResponsePong()
{
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(0);
  Udp.endPacket();
}

void udpResponseResult(bool result)
{
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(1);
  Udp.write(result);
  Udp.endPacket();
}

void udpSendColor(const CRGB &rgb)
{
  if (!listenerAddr.isSet())
  {
    return;
  }

  Udp.beginPacket(listenerAddr, listenerPort);
  Udp.write(255);
  Udp.write(rgb[0]);
  Udp.write(rgb[1]);
  Udp.write(rgb[2]);
  Udp.endPacket();
}

void setStripColor()
{
  CRGB rgb = hsv2rgb_spectrum(current);
  udpSendColor(rgb);

  fill_solid(leds, NUM_LEDS, rgb);
  FastLED.show();
}

float lerp(float a, float b, float t)
{
  return a + (b - a) * t;
}

float easeInOutCubic(float x)
{
  return x < 0.5f ? (4.0f * x * x * x) : (1.0f - powf(2.0f - 2.0f * x, 3.0f) / 2.0f);
}

float easeInOutCubicDouble(float x)
{
  return x < 0.5f ? (1.0f - easeInOutCubic(x * 2.0f)) : easeInOutCubic(2.0f * x - 1.0f);
}

fract8 floatToFract(float x)
{
  return static_cast<uint8_t>(fminf(fmaxf(x, 0.0f), 1.0f) * 255.0f);
}

void tick() {
  float hue = hue_to;
  float value = base_value;

  if (transition_timer < TRANSITION_TICKS)
  {
    transition_timer++;

    if (transition_timer < TRANSITION_MIDPOINT)
    {
      hue = hue_from;
    }

    float value_mult = easeInOutCubicDouble(static_cast<float>(transition_timer) / static_cast<float>(TRANSITION_TICKS));
    value = lerp(0, base_value, value_mult);
  }

  if (pulse_timer < PULSE_TICKS)
  {
    pulse_timer++;

    float peak_value = fminf(base_value * PULSE_VALUE_MULTIPLIER, 1.0f);
    float t = easeInOutCubicDouble(static_cast<float>(pulse_timer) / static_cast<float>(PULSE_TICKS));
    value = lerp(peak_value, base_value, t);
  }

  current = CHSV(floatToFract(hue), BASE_SATURATION, floatToFract(value));

  setStripColor();
}

void setup()
{
  Serial.begin(115200);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");

  Udp.begin(port);

  Serial.printf("Listening at %s:%d\n", WiFi.localIP().toString().c_str(), port);
}

void loop()
{
  int packet_size = Udp.parsePacket();
  if (packet_size)
  {
    Serial.printf("Received %d bytes\n", packet_size);
    int len = Udp.read(incoming, 255);
    incoming[len] = 0;

    switch (incoming[0])
    {
      case 0: // Ping
        udpResponsePong();
        break;
      case 1: // SetHue
        if (len < 5)
        {
          return udpResponseResult(false);
        }

        transition_timer = 0;

        hue_from = hue_to;
        memcpy(&hue_to, &incoming[1], sizeof(hue_to));

        udpResponseResult(true);
        break;
      case 2: // SetBaseValue
        if (len < 5)
        {
          return udpResponseResult(false);
        }

        memcpy(&base_value, &incoming[1], sizeof(base_value));
      case 3: // Pulse
        if (pulse_timer == PULSE_TICKS)
        {
          pulse_timer = 0;
        }
        else if (pulse_timer > PULSE_MIDPOINT)
        {
          pulse_timer = PULSE_TICKS - pulse_timer;
        }

        udpResponseResult(true);
        break;
      case 255: // Listen
        listenerAddr = Udp.remoteIP();
        listenerPort = Udp.remotePort();
        udpResponseResult(true);
        break;
      default:
        udpResponseResult(false);
    }
  }

  unsigned long ts = micros();

  if (ts - last_ts >= TICK_INTERVAL)
  {
    last_ts = ts;
    tick();
  }
}
