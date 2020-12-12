#include <AccelStepper.h>
#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Servo.h>

#define DEBUG

#ifdef DEBUG
#define DEBUG_BEGIN(x) Serial.begin(x)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTDEC(x) Serial.print(x, DEC)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_BEGIN(x)
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_READ()
#endif

const byte ROWS = 4;
const byte COLS = 4;

char KPKeys[ROWS][COLS] = {{'1', '2', '3', 'A'},
                           {'4', '5', '6', 'B'},
                           {'7', '8', '9', 'C'},
                           {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {11, 10, 9, 8};
byte colPins[COLS] = {7, 6, 5, 4};

const uint8_t totalJobs = 4;
uint8_t selectedJob = 0;

struct stripJob {
  char id;
  uint16_t strips = 0;
  uint16_t length = 0;
};

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
void servoCut(Servo* servo);
void runJob(LiquidCrystal* lcd, AccelStepper* stepper, Servo* servo,
            stripJob job);
uint16_t mmToSteps(uint16_t millimeters);

void setup() {
  DEBUG_BEGIN(9600);

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
  DEBUG_PRINTLN("WCUT V0.2");
  DEBUG_PRINTLN("Starting...");

  delay(250);
  lcd.clear();
}

void loop() {
  static stripJob jobs[totalJobs];
  for (uint8_t i = 0; i < totalJobs; i++) {
    jobs[i].id = 'A' + i;
  }

  lcd.clear();

  lcd.setCursor(15, 0);
  lcd.print(jobs[selectedJob].id);

  // lcd.cursor();
  lcd.blink();

  lcd.setCursor(0, 0);
  lcd.print("Strips:");
  lcd.setCursor(0, 1);
  lcd.print("Length:");
  if (jobs[selectedJob].strips != 0 && jobs[selectedJob].length != 0) {
    lcd.setCursor(7, 0);
    lcd.print(jobs[selectedJob].strips);
    lcd.setCursor(7, 1);
    lcd.print(jobs[selectedJob].length);
  }

  uint16_t strips = getInput(&lcd, 7, 0, 255);
  if (strips == 0xffff) return;
  jobs[selectedJob].strips = strips;
  lcd.setCursor(7, 0);
  lcd.print(jobs[selectedJob].strips);

  uint16_t length = getInput(&lcd, 7, 1, 10000);
  if (length == 0xffff) return;
  jobs[selectedJob].length = length;
  lcd.setCursor(7, 1);
  lcd.print(jobs[selectedJob].length);

  if (selectedJob != totalJobs - 1) {
    selectedJob++;
    return;
  }

  // lcd.noCursor();
  lcd.noBlink();

  if (getInput(&lcd, 16, 2, 0) == 0xffff) return;

  for (uint8_t i = 0; i < totalJobs; i++) {
    runJob(&lcd, &stepper, &servo, jobs[i]);
  }

  selectedJob = 0;
  lcd.clear();
  lcd.print("Done.");
  delay(1000);
}

uint16_t getInput(LiquidCrystal* lcd, const uint8_t lcdRow,
                  const uint8_t lcdCol, const uint16_t maxInput) {
  char key = keypad.getKey();
  uint8_t charsPrinted = 0;
  uint16_t input = 0;

  lcd->setCursor(lcdRow, lcdCol);

  while (key != '#') {
    switch (key) {
      case NO_KEY:
        break;
      case 'A':
        if (selectedJob != 0) {
          selectedJob = 0;
          return 0xffff;
        }
        break;
      case 'B':
        if (selectedJob != 1) {
          selectedJob = 1;
          return 0xffff;
        }
        break;
      case 'C':
        if (selectedJob != 2) {
          selectedJob = 2;
          return 0xffff;
        }
        break;
      case 'D':
        if (selectedJob != 3) {
          selectedJob = 3;
          return 0xffff;
        }
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
        if (!(input * 10 + (key - '0') > maxInput) && charsPrinted < 5) {
          input = input * 10 + (key - '0');
          lcd->print(key);
          charsPrinted++;
          DEBUG_PRINTLN("input: ");
          DEBUG_PRINTLN(input);
        }
        break;
      case '*':
        if (input == 0)
          return 0xffff;
        else {
          input = 0;
          for (uint8_t i = 0; i < charsPrinted; i++) {
            lcd->setCursor(lcdRow + i, lcdCol);
            lcd->print(' ');
          }
          lcd->setCursor(lcdRow, lcdCol);
          charsPrinted = 0;
          DEBUG_PRINTLN("");
        }
        break;
    }
    key = keypad.getKey();
#ifdef DEBUG
    if (Serial.available() > 0) {
      Serial.readBytes(&key, 1);
    }
#endif
  }

  return input;
}

void servoCut(Servo* servo) {
  for (uint8_t pos = 0; pos <= 180; pos++) {
    servo->write(pos);
    delay(15);
  }

  while (digitalRead(servoEndstop)) {
  }
  servo->write(0);
}

void runJob(LiquidCrystal* lcd, AccelStepper* stepper, Servo* servo,
            stripJob job) {
  if (!job.strips || !job.length) return;

  for (uint8_t i = 0; i < job.strips; i++) {
    lcd->clear();
    lcd->setCursor(15, 0);
    lcd->print(job.id);

    lcd->setCursor(0, 0);
    lcd->print(job.length);
    lcd->print("mm");

    lcd->setCursor(0, 1);
    lcd->print(i + 1);
    lcd->print('/');
    lcd->print(job.strips);

    stepper->move(mmToSteps(job.length));

    while (stepper->distanceToGo() != 0) {
      stepper->run();
    }

    servoCut(servo);

    delay(500);
  }
}

uint16_t mmToSteps(uint16_t millimeters) {
  const uint16_t gearDiameterMM = 50;
  const uint8_t motorStepsPerRevolution = 200;

  const float stepsPerMM = motorStepsPerRevolution / (PI * gearDiameterMM);

  return stepsPerMM * millimeters;
}