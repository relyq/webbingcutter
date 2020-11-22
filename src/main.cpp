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

uint16_t getInput(LiquidCrystal* lcd, const uint8_t lcdRow,
                  const uint8_t lcdCol, const uint16_t maxInput);
void servoCut();
void runJob(AccelStepper* stepper, LiquidCrystal* lcd, uint8_t strips, uint16_t length);
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

  lcd.print("WCUT V0.2");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  Serial.println("WCUT V0.2");
  Serial.println("Starting...");
  delay(250);
  lcd.clear();
}

void loop() {
  lcd.clear();

  lcd.print("Strips:");
  lcd.setCursor(0, 1);
  lcd.print("Length:");
  uint16_t strips = getInput(&lcd, 7, 0, 255);
  uint16_t length = getInput(&lcd, 7, 1, 10000);

  runJob(&stepper, &lcd, strips, length);

  lcd.clear();
  delay(500);
}

uint16_t getInput(LiquidCrystal* lcd, const uint8_t lcdRow,
                  const uint8_t lcdCol, const uint16_t maxInput) {
  char key = keypad.getKey();
  uint8_t charsPrinted;
  uint16_t input = 0;

  lcd->setCursor(lcdRow, lcdCol);

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
        if (!(input * 10 + (key - '0') > maxInput)) {
          input = input * 10 + (key - '0');
          lcd->print(key);
          charsPrinted++;
          Serial.println(input);
        }
        break;

      case '*':
        input = 0;
        for (uint8_t i = 0; i < charsPrinted; i++) {
          lcd->setCursor(lcdRow + i, lcdCol);
          lcd->print(' ');
        }
        lcd->setCursor(lcdRow, lcdCol);
        charsPrinted = 0;
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
    delay(15);
  }

  while (digitalRead(servoEndstop)) {
  }
  servo.write(0);
}

void runJob(AccelStepper* stepper, LiquidCrystal* lcd, uint8_t strips, uint16_t length) {
  for (uint8_t i = 0; i < strips; i++) {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(i+1);
    lcd->print("/");
    lcd->print(strips);

    stepper->move(mmToSteps(length));

    while (stepper->distanceToGo() != 0) {
      stepper->run();
    }

    servoCut();

    delay(500);
  }
}

uint16_t mmToSteps(uint16_t millimetres) {
  const uint16_t gearDiameterMM = 50;
  const uint8_t motorStepsPerRevolution = 200;

  const float stepsPerMM = motorStepsPerRevolution / (PI * gearDiameterMM);

  return stepsPerMM * millimetres;
}