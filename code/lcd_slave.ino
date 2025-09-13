#include <LiquidCrystal.h> // Includes the LCD library to interface with a character LCD

// === LCD and Serial ===
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // Initialize the LCD with the corresponding Arduino pins

const char* drugNames[4] = {"NOR", "FEN", "MID", "HEP"}; // Array storing drug abbreviations: Norepinephrine, Fentanyl, Midazolam, Heparin

// === Scroll Management ===
String displayMessage = "";   // String to hold the message currently being displayed
int scrollIndex = 0;          // Index used to scroll the message
bool scrolling = false;       // Flag to indicate if scrolling is active

// === Timer1 Tick-Based Scrolling ===
volatile unsigned long timerTicks = 0;       // Counter incremented by Timer1 ISR
const unsigned long scrollDelayTicks = 50;   // Number of ticks between scroll steps (~800 ms)
unsigned long lastScrollTick = 0;            // Last tick value when scroll was updated

// === Timer1 ISR ===
ISR(TIMER1_COMPA_vect) {
  timerTicks++; // Increment the tick counter on each Timer1 compare match interrupt
}

// === Timer1 Setup ===
void setupTimer1() {
  noInterrupts(); // Disable interrupts during setup
  TCCR1A = 0;     // Clear Timer/Counter1 Control Register A
  TCCR1B = 0;     // Clear Timer/Counter1 Control Register B
  TCNT1 = 0;      // Initialize counter value to 0
  OCR1A = 249;    // Set compare match register for 16ms (prescaler 1024 and 16 MHz clock)
  TCCR1B |= (1 << WGM12);              // Enable CTC mode (Clear Timer on Compare Match)
  TCCR1B |= (1 << CS12) | (1 << CS10); // Set prescaler to 1024
  TIMSK1 |= (1 << OCIE1A);             // Enable Timer1 compare interrupt
  interrupts();  // Re-enable interrupts
}

// === Protocol Message Class ===
class ProtocolMessage {
public:
  String type;     // Message type (e.g., DISPENSE, INFO, DONE, etc.)
  String patient;  // Patient ID
  String meds;     // Medication index(es)
  String status;   // Status or additional info
  bool valid;      // Validity flag for the parsed message

  // Constructor: parse incoming message string
  ProtocolMessage(String raw) {
    valid = false; // Start as invalid

    if (!raw.startsWith("@")) return; // Must start with '@'
    raw = raw.substring(1); // Remove '@'

    // Find the positions of the separators
    int i1 = raw.indexOf('|');
    int i2 = raw.indexOf('|', i1 + 1);
    int i3 = raw.indexOf('|', i2 + 1);

    if (i1 == -1 || i2 == -1 || i3 == -1) return; // If any part is missing, parsing fails

    // Extract parts
    type = raw.substring(0, i1);
    patient = raw.substring(i1 + 1, i2);
    meds = raw.substring(i2 + 1, i3);
    status = raw.substring(i3 + 1);
    status.trim(); // Clean up any leading/trailing spaces
    valid = true;  // Message is valid
  }

  // Converts the parsed protocol message to a string suitable for LCD display
  String toLCDText() {
    if (!valid) return "Parse error";

    if (type == "DISPENSE" || type == "DONE") {
      // Handle 1 or 2 medication indices
      int commaIndex = meds.indexOf(',');
      String medStr;
      if (commaIndex != -1) {
        int m1 = meds.substring(0, commaIndex).toInt();
        int m2 = meds.substring(commaIndex + 1).toInt();
        medStr = String(drugNames[m1]) + "+" + String(drugNames[m2]);
      } else {
        medStr = String(drugNames[meds.toInt()]);
      }
      // Return appropriate message for DISPENSE or DONE
      return (type == "DISPENSE" ? "Dispensing " : "P" + patient + " got ") + medStr + (type == "DISPENSE" ? " for P" + patient : "");
    } 
    else if (type == "ERR") {
      // Handle error messages
      if (status == "TIMEOUT") return "Timeout. Restarting.";
      return "Error: " + status;
    } 
    else if (type == "INFO") {
      // Handle step-by-step user instructions
      if (status == "ENTER_PATIENT_NUMBER") return "Enter Patient ID";
      if (status == "PATIENT_SELECTED") return "P" + patient + " selected. Select MED1";
      if (status == "MED1_SELECTED") return "MED1: " + String(drugNames[meds.toInt()]) + " selected. Select MED2";
      if (status == "MED2_SELECTED") {
        int commaIdx = meds.indexOf(',');
        int med2Index = (commaIdx != -1) ? meds.substring(commaIdx + 1).toInt() : meds.toInt();
        return "MED2: " + String(drugNames[med2Index]) + " selected";
      }
      return "P" + patient + ": " + status;
    }
    return "Unknown message"; // Default for unrecognized types
  }
};

// === Arduino Setup ===
void setup() {
  lcd.begin(16, 2);         // Initialize LCD with 16 columns and 2 rows
  Serial.begin(9600);       // Start serial communication
  lcd.print("Waiting...");  // Initial LCD message
  setupTimer1();            // Start Timer1 for scrolling
}

// === Arduino Loop ===
void loop() {
  // === Read structured protocol message from Serial ===
  if (Serial.available()) {
    ProtocolMessage msg(Serial.readStringUntil('\n')); // Read message line from Serial
    displayMessage = msg.toLCDText(); // Convert parsed message to LCD string
    lcd.clear();
    lcd.setCursor(0, 0);

    if (displayMessage.length() <= 16) {
      // If message fits in one line, print directly
      lcd.print(displayMessage);
      scrolling = false;
    } else {
      // If message is too long, scroll it
      lcd.print(displayMessage.substring(0, 16));
      scrollIndex = 0;
      lastScrollTick = timerTicks;
      scrolling = true;
    }
  }

  // === Scroll the message horizontally ===
  if (scrolling && (timerTicks - lastScrollTick >= scrollDelayTicks)) {
    scrollIndex++; // Move scroll index

    // If there's enough text left to scroll forward
    if (scrollIndex + 16 <= displayMessage.length()) {
      lcd.setCursor(0, 0);
      lcd.print(displayMessage.substring(scrollIndex, scrollIndex + 16));
    } else {
      // Restart scrolling from beginning
      scrollIndex = 0;
      lcd.setCursor(0, 0);
      lcd.print(displayMessage.substring(0, 16));
    }
    lastScrollTick = timerTicks; // Update last scroll tick
  }
}