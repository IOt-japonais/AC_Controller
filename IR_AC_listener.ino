#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>

const uint16_t kRecvPin = 14;     
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50;

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results;

const char *names[] = {
  "ON20",
  "OFF",
  "ON22",
  "ON24",
  "ON26",
};

const int NUM_COMMANDS = sizeof(names) / sizeof(names[0]);
int step = 0;

void askNext() {
  Serial.println();
  Serial.println("===========================================");
  Serial.print("Press remote button for: ");
  Serial.println(names[step]);
  Serial.println("===========================================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  irrecv.enableIRIn();

  Serial.println();
  Serial.println("Daikin312 Capture Assistant");
  askNext();
}

void loop() {

  if (!irrecv.decode(&results))
    return;

  if (results.decode_type != DAIKIN312) {
    Serial.println("Not a DAIKIN312 packet. Try again.");
    irrecv.resume();
    return;
  }

  Serial.println();
  Serial.println("//----------------------------------------------------");
String src = resultToSourceCode(&results);

// Find where the state array begins.
int pos = src.indexOf("uint8_t state");
if (pos >= 0) {
  src = src.substring(pos);
  src.replace("uint8_t state[39]",
              String("const uint8_t ") + names[step] + "[39]");
}

Serial.println(src);

  Serial.println("//----------------------------------------------------");

  step++;

  if (step >= NUM_COMMANDS){
    Serial.println();
    Serial.println("*************************************");
    Serial.println("ALL COMMANDS CAPTURED SUCCESSFULLY");
    Serial.println("Copy everything into DaikinCodes.h");
    Serial.println("*************************************");

    while (true)
      delay(1000);
  }

  askNext();

  irrecv.resume();
}