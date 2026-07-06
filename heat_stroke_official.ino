// ============================================================
//  HEAT STROKE MONITOR & DAIKIN AC CONTROLLER
//  Hardware : ESP32 Dev Board
//
//  TFT Wiring:
//    VCC  → 3.3V    GND  → GND
//    CS   → GPIO5   RES  → GPIO16
//    DC   → GPIO17  SDA  → GPIO23
//    SCK  → GPIO18  LEDA → 3.3V
//
//  DHT22 Wiring:
//    VCC  → 3.3V    GND → GND
//    DATA → GPIO4   + 10kΩ from DATA to 3.3V
//
//  IR LED Wiring:
//    GPIO13 → 100Ω → 2N2222 Base
//    2N2222 Collector → IR LED Anode (+)
//    2N2222 Emitter   → GND
//    IR LED Cathode   → GND
// ============================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <DHT.h>
#include <IRsend.h>
#include <IRremoteESP8266.h>
#include "DaikinCodes.h"


// ── Pin Definitions ─────────────────────────────────────────
#define TFT_CS    5
#define TFT_DC    17
#define TFT_RST   16
#define TFT_MOSI  23
#define TFT_SCLK  18
#define IR_PIN    13
#define DHTPIN    4
#define LED_PIN   2

// ── DHT22 ───────────────────────────────────────────────────
#define DHTTYPE   DHT22
DHT dht(DHTPIN, DHTTYPE);

// ── TFT ─────────────────────────────────────────────────────
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ── Daikin IR ────────────────────────────────────────────────
IRsend irsend(IR_PIN);

// ── Colors ──────────────────────────────────────────────────
#define BLACK     ST77XX_BLACK
#define WHITE     ST77XX_WHITE
#define RED       ST77XX_RED
#define GREEN     ST77XX_GREEN
#define BLUE      ST77XX_BLUE
#define YELLOW    ST77XX_YELLOW
#define ORANGE    0xFC00
#define CYAN      ST77XX_CYAN
#define GREY      0x8410

// ── Thresholds ──────────────────────────────────────────────
#define HI_DANGER   40.0   // °C — AC ON at 20°C
#define HI_HIGH     36.0   // °C — AC ON at 22°C
#define HI_MEDIUM   32.0   // °C — AC ON at 24°C
#define HI_LOW      27.0   // °C — AC ON at 26°C
#define HI_SAFE     26.0   // °C — AC OFF

// ── Hysteresis / debounce ────────────────────────────────────
#define HI_MARGIN          0.5     // °C dead-zone band below HI_SAFE
#define MIN_CMD_INTERVAL   300000  // 5 min minimum between AC IR commands
#define STATE_REASSERT_MS  1800000 // 30 min: re-send current state to correct drift

// ── Timing ──────────────────────────────────────────────────
#define CHECK_INTERVAL  30000   // 30 seconds between readings

// ── State ───────────────────────────────────────────────────
bool acIsOn               = false;
uint8_t acTemp             = 26;
unsigned long lastCheck    = 0;
unsigned long lastAcCommand = 0;

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== HEAT MONITOR STARTING ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ── Init TFT ──────────────────────────────────────────────
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  showSplash();
  delay(2000);

  // ── Init DHT22 ────────────────────────────────────────────
  dht.begin();
  Serial.println("DHT22 ready.");

  // ── Init Daikin IR ────────────────────────────────────────
  irsend.begin();
  Serial.println("Daikin IR ready.");

  Serial.println("=== MONITORING STARTED ===");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  unsigned long now = millis();
  if (now - lastCheck < CHECK_INTERVAL) return;
  lastCheck = now;

  // ── Read DHT22 ────────────────────────────────────────────
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("DHT22 read error!");
    showError("Sensor Error", "Check DHT22");
    return;
  }

  float hi = dht.computeHeatIndex(temp, hum, false);

  Serial.printf("Temp: %.1f C | Hum: %.1f%% | HI: %.1f C | AC: %s @ %d C\n",
                temp, hum, hi, acIsOn ? "ON" : "OFF", acTemp);

  // ── AC Control Logic ──────────────────────────────────────
  controlAC(hi);

  // ── Update Display ────────────────────────────────────────
  showReadings(temp, hum, hi);
}

// ============================================================
//  AC CONTROL
// ============================================================
void controlAC(float hi) {
  uint8_t targetTemp = 26;
  bool shouldBeOn = true;

  if (hi >= HI_DANGER) {
    targetTemp = 20;
  } else if (hi >= HI_HIGH) {
    targetTemp = 22;
  } else if (hi >= HI_MEDIUM) {
    targetTemp = 24;
  } else if (hi >= HI_LOW) {
    targetTemp = 26;
  } else if (hi >= HI_SAFE - HI_MARGIN) {
    // Dead zone: hi is between (HI_SAFE - margin) and HI_LOW.
    // Don't make a new decision here — just hold whatever state we're already in.
    // This prevents rapid ON/OFF toggling when HI hovers near HI_SAFE.
    maybeReassertState();
    return;
  } else {
    shouldBeOn = false;
  }

  unsigned long now = millis();

  // Debounce: don't send IR more often than MIN_CMD_INTERVAL,
  // unless this is the very first command (lastAcCommand == 0).
  if (lastAcCommand != 0 && (now - lastAcCommand < MIN_CMD_INTERVAL)) {
    return;
  }

  if (shouldBeOn) {
    if (!acIsOn || targetTemp != acTemp) {
      acTemp = targetTemp;
      acIsOn = true;
      sendDaikinOn(acTemp);
      lastAcCommand = now;
      Serial.printf("AC ON -> %d C\n", acTemp);
      blinkLED(2);
    }
  } else {
    if (acIsOn) {
      acIsOn = false;
      sendDaikinOff();
      lastAcCommand = now;
      Serial.println("AC OFF");
      blinkLED(1);
    }
  }
}

// Periodically re-send the currently believed AC state, in case the
// physical remote was used or the AC unit lost power and reset —
// this keeps the real AC state from silently drifting away from
// what this controller thinks it is.
void maybeReassertState() {
  unsigned long now = millis();
  if (lastAcCommand == 0 || (now - lastAcCommand < STATE_REASSERT_MS)) {
    return;
  }

  if (acIsOn) {
    sendDaikinOn(acTemp);
  } else {
    sendDaikinOff();
  }
  lastAcCommand = now;
  Serial.println("Re-asserted AC state (drift correction)");
}

// ============================================================
//  DAIKIN IR COMMANDS
// ============================================================

void sendDaikinOn(uint8_t temp)
{
    const uint8_t *cmd = ON26;
    size_t len = sizeof(ON26);

    switch(temp)
    {
        case 20:
            cmd = ON20;
            len = sizeof(ON20);
            break;

        case 22:
            cmd = ON22;
            len = sizeof(ON22);
            break;

        case 24:
            cmd = ON24;
            len = sizeof(ON24);
            break;

        case 26:
        default:
            cmd = ON26;
            len = sizeof(ON26);
            break;
    }

    irsend.sendDaikin312(cmd, len);

    Serial.printf("Sent Daikin ON %d°C\n", temp);
}

void sendDaikinOff()
{
    irsend.sendDaikin312(OFF, sizeof(OFF));

    Serial.println("Sent Daikin OFF");
}

// ============================================================
//  DISPLAY FUNCTIONS
// ============================================================

void showSplash() {
  tft.fillScreen(BLACK);
  tft.drawRect(0, 0, 160, 128, CYAN);

  tft.setTextColor(CYAN);
  tft.setTextSize(1);
  tft.setCursor(25, 10);
  tft.println("HEAT MONITOR");

  tft.drawLine(0, 22, 160, 22, CYAN);

  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(15, 35);
  tft.println("Daikin AC Control");
  tft.setCursor(25, 50);
  tft.println("ESP32 + DHT22");
  tft.setCursor(30, 65);
  tft.println("ST7735 TFT");

  tft.setTextColor(YELLOW);
  tft.setCursor(20, 90);
  tft.println("Initializing...");
}

void showReadings(float temp, float hum, float hi) {
  tft.fillScreen(BLACK);

  // ── Header ──────────────────────────────────────────────
  tft.fillRect(0, 0, 160, 18, CYAN);
  tft.setTextColor(BLACK);
  tft.setTextSize(1);
  tft.setCursor(28, 5);
  tft.println("HEAT MONITOR");

  // ── Readings ────────────────────────────────────────────
  tft.setTextSize(1);

  // Temp
  tft.setTextColor(WHITE);
  tft.setCursor(5, 24);
  tft.print("Temp :");
  tft.setTextColor(YELLOW);
  tft.setCursor(60, 24);
  tft.printf("%.1f C", temp);

  // Humidity
  tft.setTextColor(WHITE);
  tft.setCursor(5, 38);
  tft.print("Hum  :");
  tft.setTextColor(YELLOW);
  tft.setCursor(60, 38);
  tft.printf("%.0f %%", hum);

  // Heat Index
  tft.setTextColor(WHITE);
  tft.setCursor(5, 52);
  tft.print("HI   :");
  tft.setTextColor(hiColor(hi));
  tft.setCursor(60, 52);
  tft.printf("%.1f C", hi);

  // ── Divider ─────────────────────────────────────────────
  tft.drawLine(0, 66, 160, 66, GREY);

  // ── Risk Level ──────────────────────────────────────────
  uint16_t rColor = hiColor(hi);
  tft.fillRect(0, 70, 160, 20, rColor);
  tft.setTextColor(BLACK);
  tft.setTextSize(1);
  tft.setCursor(45, 75);
  tft.println(riskLabel(hi));

  // ── AC Status ───────────────────────────────────────────
  tft.drawLine(0, 94, 160, 94, GREY);

  tft.setTextSize(1);
  tft.setCursor(5, 100);
  tft.setTextColor(WHITE);
  tft.print("AC :");

  if (acIsOn) {
    tft.fillRect(40, 97, 115, 16, GREEN);
    tft.setTextColor(BLACK);
    tft.setCursor(45, 100);
    tft.printf("ON  @ %d C Cool", acTemp);
  } else {
    tft.fillRect(40, 97, 115, 16, GREY);
    tft.setTextColor(WHITE);
    tft.setCursor(45, 100);
    tft.println("OFF");
  }

  // ── Heat Index Bar ──────────────────────────────────────
  tft.setCursor(5, 118);
  tft.setTextColor(GREY);
  tft.print("HI:");
  int barWidth = constrain((int)((hi - 20.0) * 3.5), 0, 130);
  tft.fillRect(28, 119, barWidth, 6, rColor);
  tft.drawRect(28, 119, 130, 6, WHITE);
}

void showError(const char* line1, const char* line2) {
  tft.fillScreen(BLACK);
  tft.drawRect(0, 0, 160, 128, RED);

  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(30, 15);
  tft.println("ERROR");

  tft.drawLine(0, 38, 160, 38, RED);

  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 50);
  tft.println(line1);
  tft.setCursor(5, 65);
  tft.println(line2);

  tft.setTextColor(YELLOW);
  tft.setCursor(5, 90);
  tft.println("Retrying in 30s...");
}

// ============================================================
//  HELPERS
// ============================================================

uint16_t hiColor(float hi) {
  if (hi < HI_SAFE)    return GREEN;
  if (hi < HI_LOW)     return CYAN;
  if (hi < HI_MEDIUM)  return YELLOW;
  if (hi < HI_HIGH)    return ORANGE;
  return RED;
}

const char* riskLabel(float hi) {
  if (hi < HI_SAFE)    return "  SAFE   ";
  if (hi < HI_LOW)     return " CAUTION ";
  if (hi < HI_MEDIUM)  return " WARNING ";
  if (hi < HI_HIGH)    return " DANGER  ";
  return "  EXTREME";
}

void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
}
