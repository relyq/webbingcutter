#include <AccelStepper.h>
#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Servo.h>

const byte ROWS = 4;
const byte COLS = 4;

char KPKeys[ROWS][COLS] = {{'1', '2', '3', 'A'},
                           {'4', '5', '6', 'B'},
                           {'7', '8', '9', 'C'},
                           {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {11, 10, 9, 8};
byte colPins[COLS] = {7, 6, 5, 4};

const uint8_t servoPin = 12;
const uint8_t servoEndstop = 13;

const uint8_t rs = PIN_A5, en = PIN_A4, d4 = PIN_A3, d5 = PIN_A2, d6 = PIN_A1,
              d7 = PIN_A0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

Keypad keypad = Keypad(makeKeymap(KPKeys), rowPins, colPins, ROWS, COLS);

AccelStepper stepper(AccelStepper::DRIVER, 2, 3);  // step, dir

Servo servo;

uint16_t getInput();
void servoCut();
void runJob(uint16_t length);
uint16_t mmToSteps(uint16_t millimetres);

void setup() {
  Serial.begin(9600);

  lcd.begin(16, 2);

  pinMode(servoEndstop, INPUT_PULLUP);

  servo.attach(servoPin);
  servo.write(0);

  // 2500
  stepper.setMaxSpeed(500);
  stepper.setSpeed(500);
  stepper.setAcceleration(100.0);

  keypad.setDebounceTime(100);

  lcd.print("WCUT V0.1");
  Serial.println("WCUT V0.1");
  delay(250);
  lcd.clear();
  lcd.print("Starting...");
  Serial.println("Starting...");
}

void loop() {
  lcd.clear();
  lcd.print("Length:");

  uint16_t input = getInput();

  runJob(input);

  lcd.clear();
  delay(500);
}

uint16_t getInput() {
  char key = keypad.getKey();
  uint16_t input = 0;

  while (key != '#') {
    switch (key) {
      case NO_KEY:
        break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        if (!(input * 10 + (key - '0') > 10000)) {
          input = input * 10 + (key - '0');
          lcd.print(key);
          Serial.println(input);
        }
        break;

      case '*':
        input = 0;
        lcd.clear();
        lcd.print("Length:");
        Serial.println("");
        break;
    }
    key = keypad.getKey();
  }

  return input;
}

void servoCut() {
  for (uint8_t pos = 0; pos <= 180; pos++) {
    servo.write(pos);
    delay(30);
  }

  while (digitalRead(servoEndstop)) {
  }
  servo.write(0);
}

void runJob(uint16_t length) {
  stepper.move(mmToSteps(length));

  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  servoCut();
}

uint16_t mmToSteps(uint16_t millimetres) {
  const uint16_t gearDiameterMM = 50;
  const uint8_t motorStepsPerRevolution = 200;

  const float stepsPerMM = motorStepsPerRevolution / (PI * gearDiameterMM);

  return stepsPerMM * millimetres;
}