
#include <Arduino.h>
#include <ctype.h>
#include <map>

// #include <EEPROM.h>
// #include <BlynkSimpleEsp8266.h>

#define BLYNK_DEVICE_NAME "Quickstart Template"

#define BLYNK_AUTH_TOKEN ""
#define BLYNK_TEMPLATE_ID ""
char ssid[] = "";
char pass[] = "";

char auth[] = BLYNK_AUTH_TOKEN;

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

using profitDict = std::map<String, float>;
using tubingDict = std::map<String, float>;

// ========= Config options ========= 
// float options are decimals!
// int options are whole numbers!
// char options are alphanumeric enclosed in quotes!

// How long the pump runs per ml in general. 
// Values set here affect everything below.
int DURATION_PER_ML_MS = 200;
int DURATION_REVERSE_MS = 250; // TODO: change to ml // Amount of time to reverse after a fill to prevent drips

int PUMP_VOLUME_ML = 10;

// TODO: Maybe combine all this below...
// Tweak pump times according to tubing and cologne type
// This value must match an option from the left side of MULTIPLIERS below
String MULTIPLIER_OPTION = "item0-6mm";

tubingDict MULTIPLIERS = {
  {"item0-6mm", 1.0},
  {"item0-4mm", 0.5},
  {"item1-6mm", 0.98},
  {"item0-4mm", 0.44}
};

// Price/ml, used for calculating profit display.
// Update both 'INVENTORY_NAMES' and 'PROFIT_PER_ML' so the items in INVENTORY_NAMES match with the left side of PROFIT_PER_ML
String inventoryNames[] = {"item0", "item1", "item2"};
profitDict PROFIT_PER_ML = {
  {"item0", 3.0},
  {"item1", 2.5},
  {"item3", 4}
};

// ========= END Config options ========= 

// TODO: Probably not needed, but noted
// The amount of time it takes for the pump to get up to full speed,
// and the amount of fluid that pumps during that slower time.
// float SPINUP_MS = 100; 
// float SPINUP_ML = 0.1; // This amount is factored into the fill time

float IR_LOW_MULT = 0.5;
int IR_LOW = 0;

int PIN_ONBOARD_LED = 2;
// PIN_SENSOR_LED @ A0

int PIN_IR_LED = 4; // D2

int PIN_DIR = 0; // D3
int PIN_PUMP = 2; // D4 + LED

int PIN_PUMP_BUTTON = 14; // D5
int PIN_DIR_SWITCH = 12; // D6

// a0, 2 3 4 5(IN) 6(IN) G VIN 3V

/*
pin translation esp8266
SD3 - 10
SD2 - 9
D0 - 16 NodeMCU LED
D1 - 5
D2 - 4
D3 - 0
D4 - 2 ESP12 LED
D5 - 14
D6 - 12
D7 - 13
D8 - 15
RX - 3
TX - 1
*/

int PROFIT = 0;

// WidgetTerminal terminal(V0);
// BlynkTimer timer;

// void profitUpdateEvent()
// {
//   PROFIT++;
//   // Please don't send more that 10 values per second.
//   Blynk.virtualWrite(V5, PROFIT);

//   // TODO:
//   // Profit per inventory type

//   // Total profit
// }

// BLYNK_WRITE(V0)
// {
//   String input = param.asStr();

//   terminal.println(input);
//   terminal.flush();
// }

bool isIrTriggered() {
  return analogRead(A0) <= IR_LOW;
}

bool isLinePrimed = true;
void pump(int ms, bool isReverse, bool isWatchIr = true) {
  Serial.print("Pumping ");
  if (isReverse) {
    Serial.print("reverse ");
    digitalWrite(PIN_DIR, HIGH);
    delay(100);
  } else {
    digitalWrite(PIN_DIR, LOW);    
  }
  Serial.print(String(ms) + " ms ");

  pressOnce();

  if (isWatchIr) {
    int i = 0;
    while(i < ms) {
      i++;
      if (i % 100 == 0) {
        Serial.print(".");
      }

      if (!isIrTriggered()) {
        digitalWrite(PIN_DIR, LOW);
        Serial.print("ABORTED");
        break;
      }
      delay(1);
    }
  } else {
    digitalWrite(PIN_PUMP, HIGH);
    delay(ms);
  }

  pressOnce();
  Serial.println();

  // Reset dir no matter what
  digitalWrite(PIN_DIR, LOW);

  // If we just finished a reversal pump, mark the line as unprimed
  if (isReverse) 
    isLinePrimed = false;
}

bool isPumpInProgress = false;
void fillSeq(int forwardMs, int reverseMs) {
  isPumpInProgress = true;
  if (isIrTriggered())
    pump(forwardMs, false);
  if (isIrTriggered()){
    pump(reverseMs, true);
  }
  isPumpInProgress = false;

  Serial.println("Fill complete."); // i.e. remove the bottle
}

void toggleReverse() {
  // todo
}

// Short press of the pump button
void pressOnce() {
  digitalWrite(PIN_PUMP, HIGH);
  delay(100);
  digitalWrite(PIN_PUMP, LOW);  
}

uint8_t initDirSwitchState = 0;
void setup()
{  
  Serial.begin(9600);
  Serial.println("\n\n=== CologneBot ===");

  pinMode(PIN_IR_LED, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_PUMP_BUTTON, INPUT_PULLUP);
  pinMode(PIN_DIR_SWITCH, INPUT_PULLUP);

  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_IR_LED, HIGH);

  // // Turn pump off since it starts ON
  // delay(100);
  // pressOnce();

  // EEPROM.begin(512);

  Serial.print("Calibrating IR sensor...");
  int avgIrReading = 0;
  for (int i = 0; i<10; i++) {
    int irRead = analogRead(A0);
    avgIrReading += irRead;
    Serial.print(".");
    delay(100);
  }
  avgIrReading = avgIrReading / 10;
  IR_LOW = avgIrReading * IR_LOW_MULT;
  Serial.println(" Done. IR low: " + String(IR_LOW));

  Serial.print("Reverse toggle state: ");
  initDirSwitchState = digitalRead(PIN_DIR_SWITCH);
  Serial.println(initDirSwitchState);

  Serial.print("Initializing Blynk...");
  // Blynk.begin(auth, ssid, pass);
  // timer.setInterval(500L, profitUpdateEvent);
  Serial.println(" Done");

  Serial.println("=== Init Complete ===\n");
}

bool isReadyToFill = true;
bool isLastManualReverse = false;

bool isManualPumping = false;
bool isLastManualPumping = false;

uint8_t lastButtonState = HIGH; // internal pullup reverses readings
uint8_t buttonState = HIGH;

unsigned long lastDebounceMs = 0;
unsigned long debounceDelay = 100;

void loop()
{
  float selectedMultiplier = MULTIPLIERS.find(MULTIPLIER_OPTION)->second;
  int fillMs = selectedMultiplier * PUMP_VOLUME_ML * DURATION_PER_ML_MS; 

  bool isBottleIn = isIrTriggered();
  if (!isBottleIn && !isReadyToFill && !isPumpInProgress) {
    isReadyToFill = true;
  }
  
  if (isBottleIn && isReadyToFill && !isPumpInProgress) {
    isReadyToFill = false;
    if (isLinePrimed)
      // No need to compensate for reversed volume if line is primed
      fillSeq(fillMs, DURATION_REVERSE_MS);
    else {
      // Line is missing some volume from the previous fill's reversal volume, so add it back.
      fillSeq(fillMs + DURATION_REVERSE_MS, DURATION_REVERSE_MS);
    }
  }

  // Manual control
  if (!isBottleIn) {
    // Pass along manual dir switch input.
    // No need to debounce because of the delays required.
    bool isManualReverse = digitalRead(PIN_DIR_SWITCH) != initDirSwitchState; // Determine current state based on the initial state when powered on
    if (isLastManualReverse != isManualReverse) {
      if (digitalRead(PIN_DIR_SWITCH) != initDirSwitchState) {
        Serial.println("Manual reverse ON");
        isLastManualReverse = true;
        digitalWrite(PIN_DIR, HIGH);
        delay(100);
      } else {
        Serial.println("Manual reverse OFF");
        isLastManualReverse = false;
        digitalWrite(PIN_DIR, LOW);
        delay(100);
      }
    }
    
    // Pass along manual pump input. 
    uint8_t rawButtonState = digitalRead(PIN_PUMP_BUTTON);
    digitalWrite(PIN_PUMP, !rawButtonState);

    // Reduce logging of manual pump input.
    // Since this is a momentary button, it needs to be debounced

    // Reading changed due to noise or sustained press
    if (rawButtonState != lastButtonState) {
      lastDebounceMs = millis();
    }


    if ((millis() - lastDebounceMs) > debounceDelay && rawButtonState != buttonState) {
      buttonState = rawButtonState;

      // We only care about the beginning of down presses
      if (buttonState == LOW) {
        isManualPumping = !isManualPumping;
      }
    }

    if (isManualPumping != isLastManualPumping) {
      Serial.print("Manual ");
      if (isLastManualReverse){
        Serial.print("REVERSE ");
      }
      if (isLastManualPumping){
        Serial.print("pumping STOPPED");
      }
      else {
        Serial.print("pumping STARTED");
      }
      Serial.println();

      isLastManualPumping = isManualPumping;
    }
      
    // Save debounced reading
    lastButtonState = rawButtonState;
  }

  // Blynk.run();
}
