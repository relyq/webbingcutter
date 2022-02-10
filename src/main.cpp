#include <AccelStepper.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Servo.h>

#define VERSION "V0.4"
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
#endif

const byte ROWS = 4;
const byte COLS = 4;

char KPKeys[ROWS][COLS] = {{'1', '2', '3', 'A'},
                           {'4', '5', '6', 'B'},
                           {'7', '8', '9', 'C'},
                           {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {11, 10, 9, 8};
byte colPins[COLS] = {7, 6, 5, 4};

const uint8_t totalScreens = 4;
uint8_t selectedScreen = 0;

struct stripJob {
  char id;
  uint16_t strips = 0;
  uint16_t length = 0;
};

struct stepperConfig {
  uint16_t maxSpeed;
  uint16_t speed;
  uint16_t accel;
  bool dir;
};

const uint8_t servoPin = 12;
const uint8_t servoEndstop = 13;

const uint8_t rs = PIN_A5, en = PIN_A4, d4 = PIN_A3, d5 = PIN_A2, d6 = PIN_A1,
              d7 = PIN_A0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

Keypad keypad = Keypad(makeKeymap(KPKeys), rowPins, colPins, ROWS, COLS);

AccelStepper stepper(AccelStepper::DRIVER, 2, 3);  // step, dir

Servo servo;

uint8_t intDigits(uint16_t n);
uint16_t getInput(LiquidCrystal* lcd, const uint8_t lcdRow,
                  const uint8_t lcdCol, uint16_t prevInput,
                  const uint16_t maxInput);
void servoCut(Servo* servo);
void runJob(LiquidCrystal* lcd, AccelStepper* stepper, Servo* servo,
            stripJob job, stepperConfig stepper_config);
uint16_t mmToSteps(uint16_t millimeters);
uint8_t setJob(LiquidCrystal* lcd, stripJob* job, stepperConfig* stepper_config,
               uint8_t screen);
void printSettings(LiquidCrystal* lcd, stripJob job);
void printSettings(LiquidCrystal* lcd, stepperConfig stepper_config,
                   uint8_t config);

void setup() {
  DEBUG_BEGIN(9600);

  lcd.begin(16, 2);

  pinMode(servoEndstop, INPUT_PULLUP);

  servo.attach(servoPin);
  servo.write(0);

  // first time setup
  uint16_t magic_0_4 = 48343;
  uint16_t magic_buffer;
  EEPROM.get(0, magic_buffer);
  if (magic_0_4 != magic_buffer) {
    DEBUG_PRINTLN("executing first time initialization");
    stripJob emptyJob;
    stepperConfig defaultConfig;
    emptyJob.id = 'A';
    defaultConfig.maxSpeed = 500;
    defaultConfig.speed = 500;
    defaultConfig.accel = 100;
    defaultConfig.dir = 0;
    EEPROM.put(10, emptyJob);
    EEPROM.put(20, defaultConfig);
  }

  // 2500
  stepper.setMaxSpeed(500);
  stepper.setSpeed(500);
  stepper.setAcceleration(100.0);

  keypad.setDebounceTime(100);

  lcd.print("WCUT ");
  lcd.print(VERSION);
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  DEBUG_PRINTLN("WCUT ");
  DEBUG_PRINTLN(VERSION);
  DEBUG_PRINTLN("Starting...");

  delay(500);
  lcd.clear();
}

void loop() {
  stripJob job;
  stepperConfig stepper_config;

  EEPROM.get(10, job);
  EEPROM.get(20, stepper_config);

  while (selectedScreen < totalScreens) {
    if (setJob(&lcd, &job, &stepper_config, selectedScreen)) {
      continue;  // dont go to next job
    }
    selectedScreen++;
  }

  // mostrar las opciones configuradas y confirmarlas o cambiarlas
  uint16_t confirmJobs;
  while (confirmJobs != 0) {
    lcd.clear();
    printSettings(&lcd, job);
    lcd.setCursor(0, 1);
    printSettings(&lcd, stepper_config, 0);

    confirmJobs = getInput(&lcd, 16, 2, 0, 0);
    if (confirmJobs >= 0x7fff && confirmJobs != 0xffff) {
      switch (confirmJobs) {
        case 0x7fff:
          selectedScreen = 0;
          break;
        case 0x8000:
          selectedScreen = 1;
          break;
        case 0x8001:
          selectedScreen = 2;
          break;
        case 0x8002:
          selectedScreen = 3;
          break;
      }
      setJob(&lcd, &job, &stepper_config, selectedScreen);
      continue;
    }

    lcd.clear();
    printSettings(&lcd, stepper_config, 1);
    lcd.setCursor(0, 1);
    printSettings(&lcd, stepper_config, 2);

    confirmJobs = getInput(&lcd, 16, 2, 0, 0);
    if (confirmJobs >= 0x7fff) {
      switch (confirmJobs) {
        case 0x7fff:
          selectedScreen = 0;
          break;
        case 0x8000:
          selectedScreen = 1;
          break;
        case 0x8001:
          selectedScreen = 2;
          break;
        case 0x8002:
          selectedScreen = 3;
          break;
        case 0xffff:
          continue;
          break;
      }
      setJob(&lcd, &job, &stepper_config, selectedScreen);
      continue;
    }
  }

  runJob(&lcd, &stepper, &servo, job, stepper_config);

  EEPROM.put(10, job);
  EEPROM.put(20, stepper_config);

  selectedScreen = 0;
  lcd.clear();
  lcd.print("Done.");
  delay(2000);
}

// lee un stream de numeros hasta presionar #
// * del
// las letras seleccionan la pantalla
uint16_t getInput(LiquidCrystal* lcd, const uint8_t lcdRow,
                  const uint8_t lcdCol, const uint16_t prevInput,
                  const uint16_t maxInput) {
  char key = keypad.getKey();
  uint16_t input = prevInput;
  uint8_t charsPrinted = intDigits(input);

  lcd->setCursor(lcdRow, lcdCol);
  lcd->print(input);

  while (key != '#') {
    switch (key) {
      case NO_KEY:
        break;
      case 'A':
        return 0x7fff;
        break;
      case 'B':
        return 0x8000;
        break;
      case 'C':
        return 0x8001;
        break;
      case 'D':
        return 0x8002;
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
      case '9': {
        // return pressed number
        if (!(input * 10 + (key - '0') > maxInput) &&
            (input * 10 + (key - '0') != 0)) {
          if (input == 0) {
            lcd->setCursor(lcdRow, lcdCol);
            lcd->print(' ');
            lcd->setCursor(lcdRow, lcdCol);
          }
          input = input * 10 + (key - '0');
          lcd->print(key);
          charsPrinted++;
          DEBUG_PRINTLN("input: ");
          DEBUG_PRINTLN(input);
        }
        break;
      }
      case '*': {
        if (input == 0)
          return 0xffff;
        else {
          // del
          input = 0;
          for (uint8_t i = 0; i < charsPrinted; i++) {
            lcd->setCursor(lcdRow + i, lcdCol);
            lcd->print(' ');
          }
          lcd->setCursor(lcdRow, lcdCol);
          lcd->print(input);
          charsPrinted = 1;
          DEBUG_PRINTLN("");
        }
        break;
      }
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
            stripJob job, stepperConfig stepper_config) {
  if (!job.strips || !job.length) return;

  if (stepper_config.dir) {
    stepper->setMaxSpeed((int16_t)stepper_config.maxSpeed * -1);
  } else {
    stepper->setMaxSpeed(stepper_config.maxSpeed);
  }

  stepper->setAcceleration(stepper_config.accel);

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

uint8_t intDigits(uint16_t n) {
  //  if (n == 0) return 0;
  if (n < 10) return 1;
  if (n < 100) return 2;
  if (n < 1000) return 3;
  if (n < 10000) return 4;
  if (n < 100000) return 5;
}

// configura el trabajo
// devuelve 1 para cambiar la pantalla
// devuelve 0 para continuar con la siguiente opcion
uint8_t setJob(LiquidCrystal* lcd, stripJob* job, stepperConfig* stepper_config,
               uint8_t screen) {
  lcd->clear();

  // lcd.cursor();
  lcd->blink();

  // cada caso podria ser una funcion con parametros diferentes
  switch (screen) {
    // strips-length
    case 0: {
      const char* str_strips = "Cantidad: ";
      const char* str_length = "Largo: ";
      const size_t str_strips_len = strlen(str_strips);
      const size_t str_length_len = strlen(str_length);

      lcd->setCursor(0, 0);
      lcd->print(str_strips);
      lcd->setCursor(0, 1);
      lcd->print(str_length);

      if (job->strips != 0 && job->length != 0) {
        lcd->setCursor(str_strips_len, 0);
        lcd->print(job->strips);
        lcd->setCursor(str_length_len, 1);
        lcd->print(job->length);
      }

      uint16_t strips = getInput(lcd, str_strips_len, 0, job->strips, 255);
      if (strips >= 0x7fff) {
        switch (strips) {
          case 0x7fff:
            selectedScreen = 0;
            break;
          case 0x8000:
            selectedScreen = 1;
            break;
          case 0x8001:
            selectedScreen = 2;
            break;
          case 0x8002:
            selectedScreen = 3;
            break;
          case 0xffff:  // return to previous
            job->strips = 0;
            break;
        }
        return 1;
      }
      job->strips = strips;
      lcd->setCursor(str_strips_len, 0);
      lcd->print(job->strips);

      uint16_t length = getInput(lcd, str_length_len, 1, job->length, 10000);
      if (length >= 0x7fff) {
        switch (length) {
          case 0x7fff:
            selectedScreen = 0;
            break;
          case 0x8000:
            selectedScreen = 1;
            break;
          case 0x8001:
            selectedScreen = 2;
            break;
          case 0x8002:
            selectedScreen = 3;
            break;
          case 0xffff:  // return to previous
            job->length = 0;
            break;
        }
        return 1;
      }
      job->length = length;
      lcd->setCursor(str_length_len, 1);
      lcd->print(job->length);

      break;
    }
    case 1: {
      const char* str_maxSpeed = "Vel. Max: ";
      const size_t str_maxSpeed_len = strlen(str_maxSpeed);
      lcd->setCursor(0, 0);
      lcd->print(str_maxSpeed);

      if (stepper_config->maxSpeed != 0) {
        lcd->setCursor(str_maxSpeed_len, 0);
        lcd->print(stepper_config->maxSpeed);
      }

      uint16_t maxSpeed =
          getInput(lcd, str_maxSpeed_len, 0, stepper_config->maxSpeed, 5000);
      if (maxSpeed >= 0x7fff) {
        switch (maxSpeed) {
          case 0x7fff:
            selectedScreen = 0;
            break;
          case 0x8000:
            selectedScreen = 1;
            break;
          case 0x8001:
            selectedScreen = 2;
            break;
          case 0x8002:
            selectedScreen = 3;
            break;
          case 0xffff:  // return to previous
            stepper_config->maxSpeed = 0;
            break;
        }
        return 1;
      }
      stepper_config->maxSpeed = maxSpeed;
      lcd->setCursor(str_maxSpeed_len, 0);
      lcd->print(stepper_config->maxSpeed);

      break;
    }
    case 2: {
      const char* str_accel = "Acel.: ";
      const size_t str_accel_len = strlen(str_accel);
      lcd->setCursor(0, 0);
      lcd->print("Acel.:");

      if (stepper_config->accel != 0) {
        lcd->setCursor(str_accel_len, 0);
        lcd->print(stepper_config->accel);
      }

      uint16_t accel =
          getInput(lcd, str_accel_len, 0, stepper_config->accel, 2000);
      if (accel >= 0x7fff) {
        switch (accel) {
          case 0x7fff:
            selectedScreen = 0;
            break;
          case 0x8000:
            selectedScreen = 1;
            break;
          case 0x8001:
            selectedScreen = 2;
            break;
          case 0x8002:
            selectedScreen = 3;
            break;
          case 0xffff:  // return to previous
            stepper_config->accel = 0;
            break;
        }
        return 1;
      }
      stepper_config->accel = accel;
      lcd->setCursor(str_accel_len, 0);
      lcd->print(stepper_config->accel);

      break;
    }
    case 3: {
      const char* str_dir = "Dir: ";
      const size_t str_dir_len = strlen(str_dir);
      lcd->setCursor(0, 0);
      lcd->print("Dir:");

      if (stepper_config->dir != 0) {
        lcd->setCursor(str_dir_len, 0);
        lcd->print(stepper_config->dir);
      }

      uint16_t dir =
          getInput(lcd, str_dir_len, 0, (uint16_t)stepper_config->dir, 1);
      if (dir >= 0x7fff) {
        switch (dir) {
          case 0x7fff:
            selectedScreen = 0;
            break;
          case 0x8000:
            selectedScreen = 1;
            break;
          case 0x8001:
            selectedScreen = 2;
            break;
          case 0x8002:
            selectedScreen = 3;
            break;
          case 0xffff:  // return to previous
            stepper_config->dir = 0;
            break;
        }
        return 1;
      }
      stepper_config->dir = (bool)dir;
      lcd->setCursor(str_dir_len, 0);
      lcd->print(stepper_config->dir);

      break;
    }
  }

  // lcd.noCursor();
  lcd->noBlink();

  return 0;
}

// mostrar o el trabajo o las opciones de stepper
void printSettings(LiquidCrystal* lcd, stripJob job) {
  lcd->print(job.id);
  lcd->print(": ");
  lcd->print(job.strips);
  lcd->print('x');
  lcd->print(job.length);
  lcd->print("mm");
}

// config - opcion a mostrar
void printSettings(LiquidCrystal* lcd, stepperConfig stepper_config,
                   uint8_t config) {
  switch (config) {
    // maxSpeed
    case 0: {
      lcd->print(stepper_config.maxSpeed);
      lcd->print(" steps/s");
      break;
    }
    // accel
    case 1: {
      lcd->print(stepper_config.accel);
      lcd->print(" steps/s/s");
      break;
    }
    // dir
    case 2: {
      if (stepper_config.dir) {
        lcd->print("Reloj");
      } else {
        lcd->print("Contra Reloj");
      }
      break;
    }
  }
}