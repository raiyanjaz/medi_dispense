// === Button Input Pins ===
#define I3 8
#define I2 12
#define I1 7
#define I0 13
#define enterButton 5
#define interuptPin 2

// === State Indicator LEDs ===
#define LED1 3
#define LED2 4

// === Medicine LEDs (10 to 13) ===
#define MED_LED_COUNT 4
const int medLEDs[MED_LED_COUNT] = {6, 9, 10, 11};

// === Debounce & Timer Variables ===
volatile unsigned long timerCount = 0;
volatile uint16_t lastTimerCount = 0;
const unsigned long debounceDurationTime = 30; // 10 x 16ms x 2

// === State Tracking ===
volatile int patientNumber = 0;
volatile int medicine1 = 0;
volatile int medicine2 = 0;
volatile bool enterPressed = false;
volatile int inputStage = 0; // 0: patient number, 1: medicine 1, 2: medicine 2
bool promptPrinted = false;

// === Medication Dispense LED Timing ===
const unsigned long dispenseDurations[MED_LED_COUNT] = {400, 450, 500, 550};
unsigned long medDispenseStart[MED_LED_COUNT] = {0};
bool medLEDActive[MED_LED_COUNT] = {false};

// === Inactivity Timeout ===
unsigned long lastInputTime = 0;
const unsigned long inactivityTimeout = 625; // ~10 seconds

// === Post-Dispensing Delay ===
bool postDoneDelayActive = false;
unsigned long postDoneStartTick = 0;
const unsigned long postDoneDelayTicks = 300;

bool dispensingInProgress = false;

// === Timer1 ISR ===
ISR(TIMER1_COMPA_vect) { timerCount++; }

// === Timer1 Setup ===
void setupTimer1() {
  noInterrupts();
  TCCR1A = 0;   // Set entire TCCR0A register to 0
  TCCR1B = 0;   // Set entire TCCR1B register to 0
  TCNT1 = 0;    // Initialize counter value to 0
  OCR1A = 249; // ~16ms

  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A); // Enable Timer1 Compare Match A interrupt
  interrupts();
}

void setup() {
  Serial.begin(9600);
  while (!Serial); // Waits for serial monitor to load

  // Initializes input pins
  int inputPins[] = {I3, I2, I1, I0, enterButton, interuptPin};
  for (int pin : inputPins) pinMode(pin, INPUT);

  pinMode(LED1, OUTPUT); digitalWrite(LED1, LOW);
  pinMode(LED2, OUTPUT); digitalWrite(LED2, LOW);

  // Medicine LEDs
  for (int i = 0; i < MED_LED_COUNT; i++) {
    pinMode(medLEDs[i], OUTPUT);
    digitalWrite(medLEDs[i], LOW);
  }

  setupTimer1();

  // Causes interrupt
  attachInterrupt(digitalPinToInterrupt(interuptPin), handleButtonPress, RISING);
}

void sendComProtocol(const String& type, const String& patient, const String& meds, const String& status) {
  Serial.print("@"); Serial.print(type); Serial.print("|");
  Serial.print(patient); Serial.print("|");
  Serial.print(meds); Serial.print("|");
  Serial.println(status);
}

// === Main Loop ===
void loop() {
  manageLEDs();
  checkDispensingStatus();
  showPromptIfReady();
  updateStageLEDs();
  if (enterPressed) { handleEnter(); enterPressed = false; }
  checkInactivity();
}

// Mnages the 4 Medicine LEDs, just checks if we are dispensing, if we are dispensing dont do anything. If done dispensing, turn of the LED.
void manageLEDs() {
  for (int i = 0; i < MED_LED_COUNT; i++) {
    if (medLEDActive[i] && (timerCount - medDispenseStart[i] >= dispenseDurations[i])) {
      digitalWrite(medLEDs[i], LOW);
      medLEDActive[i] = false;
    }
  }
}

void checkDispensingStatus() {
  bool anyDispensing = false;
  for (bool active : medLEDActive) if (active) { anyDispensing = true; break; }

  if (anyDispensing && !dispensingInProgress) {
    sendComProtocol("DISPENSE", String(patientNumber), String(medicine1) + "," + String(medicine2), "IN_PROGRESS");
    dispensingInProgress = true;
  } else if (!anyDispensing && dispensingInProgress) {
    sendComProtocol("DONE", String(patientNumber), String(medicine1) + "," + String(medicine2), "COMPLETE");
    dispensingInProgress = false;
    postDoneDelayActive = true;
    postDoneStartTick = timerCount;
    resetInput();
  }
}

void showPromptIfReady() {
  if (inputStage == 0 && !promptPrinted && !dispensingInProgress) {
    if (!postDoneDelayActive || (timerCount - postDoneStartTick >= postDoneDelayTicks)) {
      sendComProtocol("INFO", "--", "--", "ENTER_PATIENT_NUMBER");
      promptPrinted = true;
      postDoneDelayActive = false;
    }
  }
}

void updateStageLEDs() {
  digitalWrite(LED1, inputStage == 1);
  digitalWrite(LED2, inputStage == 2);
}

void checkInactivity() {
  if (inputStage != 0 && (timerCount - lastInputTime > inactivityTimeout)) {
    sendComProtocol("ERR", "--", "--", "TIMEOUT");
    resetInput();
  }
}

void resetInput() {
  inputStage = 0;
  patientNumber = medicine1 = medicine2 = 0;
  promptPrinted = false;
}

void handleEnter() {
  switch (inputStage) {
    case 0:
      sendComProtocol("INFO", String(patientNumber), "--", "PATIENT_SELECTED");
      inputStage = 1;
      break;
    case 1:
      sendComProtocol("INFO", String(patientNumber), String(medicine1), "MED1_SELECTED");
      inputStage = 2;
      break;
    case 2:
      sendComProtocol("INFO", String(patientNumber), String(medicine1) + "," + String(medicine2), "MED2_SELECTED");
      activateMedLED(medicine1);
      activateMedLED(medicine2);
      break;
  }
  promptPrinted = false;
}

void activateMedLED(int index) {
  if (index >= 0 && index < MED_LED_COUNT) {
    digitalWrite(medLEDs[index], HIGH);
    medDispenseStart[index] = timerCount;
    medLEDActive[index] = true;
  }
}

void handleButtonPress() {
  if ((timerCount - lastTimerCount) <= debounceDurationTime || dispensingInProgress) return;
  lastTimerCount = timerCount;
  lastInputTime = timerCount;

  int value = 0;
  if (digitalRead(I0) == HIGH) value += 1;
  if (digitalRead(I1) == HIGH) value += 2;

  switch (inputStage) {
    case 0:
      if (digitalRead(I3) == HIGH) patientNumber += 8;
      if (digitalRead(I2) == HIGH) patientNumber += 4;
      patientNumber += value;
      if (patientNumber > 15) patientNumber = 15;
      break;
    case 1:
      medicine1 += value;
      if (medicine1 > 3) medicine1 = 3;
      break;
    case 2:
      medicine2 += value;
      if (medicine2 > 3) medicine2 = 3;
      break;
  }

  if (digitalRead(enterButton) == HIGH) enterPressed = true;
}