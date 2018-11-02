// ##############################################################################################
//          Define section
// ##############################################################################################
#ifndef CODE
#define CODE    (uint32_t) 999999  // Initial value
#endif

#ifndef COUNT
#define COUNT   (byte)  2         // Amount of used segments
#endif

// PINs definition
#define STROBE  7
#define CLOCK   6
#define DATA    5
#define OUT_A   4
#define OUT_B   3
#define BUTTON  2

// Left-most segment location
#define SEGMENT 0xce

// LED digits
#define DIGIT_1 0x06
#define DIGIT_2 0x5b
#define DIGIT_3 0x4f
#define DIGIT_4 0x66
#define DIGIT_5 0x6d
#define DIGIT_6 0x7d
#define DIGIT_7 0x07
#define DIGIT_8 0x7f
#define DIGIT_9 0x6f
#define DIGIT_0 0x3f
#define BLANK   0x00

#define MAX             (uint32_t) (power(10, count) - 1) // Maximal value that can be displayed

// State flags definition
#define STATE_COUNTDOWN (byte) 1   // Countdown has started
#define STATE_END       (byte) 2   // Countdown has ended 
#define STATE_BLINK     (byte) 4   // Bit used for determining if digit should be visible
#define STATE_EDIT      (byte) 8   // Edit mode
#define STATE_SEGMENT   (byte) 16  // Segment edit mode

#define BLINK_TIME      (int32_t) 200        // Number determining blink duration
#define MAX_SPEED       (int32_t) MAX / 6   // Amount of ticks from MAX to 0 in 60s (used in countdown)
#define MIN_SPEED       (int32_t) 10        // Second boundary 1 tick per second    (used in countdown)

#define LONG_PRESS_TIME 500       // Number of milliseconds for long encoder press

// ##############################################################################################
//          Global variables section
// ##############################################################################################

byte rotation_state;    // Encoder rotation state
byte button_state;      // Encoder button state
byte state;             // Counter state (see STATE_* flags above)
byte selected_segment;  // Which digit is currently selected, default is 0
byte count;             // Amount of used LED segments

byte decimal;
uint32_t number;      // Number to display (first digit is treated as first decimal digit)
uint32_t blink_time;
uint32_t prev_time;   // Variable to save previous time, used for debouncing

byte position;

int16_t step_decimal;
int32_t step;         // Step at which counter will decrement / "Speed"

// ##############################################################################################
//          Utility functions section
// ##############################################################################################

uint32_t power(uint8_t base, uint8_t exponent) {
  uint32_t ret_val = 1;
  while (exponent) {
    ret_val *= 10;
    exponent--;
  }
  return ret_val;
}

// Mode setting function
void sendCommand(byte value)
{
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, value); // Send 8-bit mode
  digitalWrite(STROBE, HIGH);
}

// Reset whole LED display
void reset()
{
  sendCommand(0x40);                        // Set auto-increment mode
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, SEGMENT); // Set starting address to 0

  for (byte i = 0; i < 16; i++)             // 8 LED segments + 8 LED diods
    shiftOut(DATA, CLOCK, LSBFIRST, BLANK); // Clear

  digitalWrite(STROBE, HIGH);
}

// Set digit on given LED segment
void setLED(byte position, byte data, byte state)
{
  byte visible = (state & STATE_BLINK) ? 0x00 : 0xff;       // Determine if digit should be visible (used for blinking)

  sendCommand(0x44);
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, SEGMENT - position * 2);  // Select which LED should display digit
  shiftOut(DATA, CLOCK, LSBFIRST, data & visible);          // Send digit to display
  digitalWrite(STROBE, HIGH);
}

// Display integer number on LED display
void displayNumber(uint32_t number, byte state)
{
  uint32_t currentNumber = number;
  // Display maximal COUNT of digits of a number
  for (byte i = 0; i < count; i++) {
    setLED(i, getDigit(currentNumber % 10), state);
    currentNumber /= 10; // Move to another digit
  }

}

// Return encoded digit for LED segment
byte getDigit(byte number) {
  switch (number) {
    case 0: return DIGIT_0;
    case 1: return DIGIT_1;
    case 2: return DIGIT_2;
    case 3: return DIGIT_3;
    case 4: return DIGIT_4;
    case 5: return DIGIT_5;
    case 6: return DIGIT_6;
    case 7: return DIGIT_7;
    case 8: return DIGIT_8;
    case 9: return DIGIT_9;
    default: return BLANK;
  }
}

void countdown()
{
  uint32_t current_time = millis();
  // Debounce decrementing
  if ((number > step) && current_time - prev_time > (100 - count / 2))
  {
    decimal -= step_decimal;
    number -= step + decimal / 100;
    if (decimal > 100)
      decimal = 100 - decimal % 100;
    prev_time = current_time;
  }
  else if (number <= step) {
    number = 0;
    decimal = 0;
    state ^= STATE_COUNTDOWN; // Toggle countdown flag
    state |= STATE_END;       // Enable end flag
  }
}

void blink()
{
  uint32_t current_time = millis();

  // Debounce blink
  if (current_time - blink_time > BLINK_TIME)
  {
    blink_time = current_time;
    state ^= STATE_BLINK; // Toggle digit visibility
  }
}

// Button event handling
void handleButton()
{
  // TODO split into smaller functions
  byte current_state = digitalRead(BUTTON);
  bool key_up = (button_state == LOW) && (current_state == HIGH);
  bool key_not_pressed = (button_state == HIGH) && (current_state == HIGH);
  button_state = current_state;

  uint32_t press_start = millis();
  if (!(state & STATE_COUNTDOWN) && !(state & STATE_END)) {
    if (key_not_pressed)
      prev_time = press_start;

    if (key_up && (press_start - prev_time >= LONG_PRESS_TIME)) {
      state ^= STATE_EDIT;                                 // toggle edit mode
      if (state & STATE_SEGMENT) state ^= STATE_SEGMENT;
      if (state & STATE_BLINK) state ^= STATE_BLINK;
      selected_segment = 0;
      return;
    }
  }

  if (key_up)
  {
    if (state & STATE_EDIT) {
      //when in edit mode
      state ^= STATE_SEGMENT;
    }
    else if (state & STATE_END) {
      state = 0;      // Reset all flags
      step = MIN_SPEED / 100;
      step_decimal = MIN_SPEED % 100;
      number = CODE;  // Restart counter
      decimal = 0;
    } else {
      state ^= STATE_COUNTDOWN; // Toggle countdown
    }
  }
}

// Encoder rotation event handling
void handleRotation() {
  byte output_a = digitalRead(OUT_A);
  byte rotating = 0; // 0 - no rotation

  // Disable rotation if counter has ended
  if (!(state & STATE_END)) {
    if (output_a == LOW && rotation_state == HIGH) {
      if (digitalRead(OUT_B) != output_a) {
        rotating = 1; // 1 - clockwise rotation
      } else {
        rotating = 2; // 2 - counter-clockwise rotation
      }
    }

    // If countdown is on, enable counting speed increasing/reducing
    if (state & STATE_COUNTDOWN) {
      // If there is MAX_SPEED boundary, enable to manipulate speed
      if (MAX_SPEED >= MIN_SPEED) {
        if (rotating == 1 && position < 10)
          position += 1;
        if (rotating == 2 && position > 0)
          position -= 1;

        step = MIN_SPEED / 100 + (position * (MAX_SPEED - MIN_SPEED)) / 100;
        step_decimal = MIN_SPEED % 100 + (position * (MAX_SPEED - MIN_SPEED)) % 100;
        Serial.print(step);
        Serial.print(' ');
        Serial.println(step_decimal);
      }
    } else if (state & STATE_EDIT) {
      uint32_t ten_power = (uint32_t) power(10, selected_segment);
      byte digit = (number % (ten_power * 10)) / ten_power;

      if (state & STATE_SEGMENT) {
        // segment editing
        uint32_t left_part = number / (ten_power * 10);
        uint32_t right_part = number % (ten_power);
        if (rotating == 1)
        {
          if (digit == 0) digit = 9;
          else digit--;
        }
        else if (rotating == 2)
        {
          if (digit == 9) digit = 0;
          else digit++;
        }
        number = ((left_part * 10 + digit) * ten_power) + right_part;
      } else {
        // segment selection
        if (rotating != 0) {
          if (state & STATE_BLINK) setLED(selected_segment, getDigit(digit), state ^ STATE_BLINK);
        }
        if (rotating == 1) {
          if (selected_segment == count - 1) selected_segment = 0;
          else selected_segment++;
        }
        else if (rotating == 2) {
          if (selected_segment == 0) selected_segment = count - 1;
          else selected_segment--;
        }
      }
      setLED(selected_segment, getDigit(digit), state);
    }
    rotation_state = output_a;
  }
}

// ##############################################################################################
//          Arduino main section
// ##############################################################################################

void setup()
{
  Serial.begin(9600); // Debugging

  // PINs setting-up
  pinMode(STROBE, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(DATA, OUTPUT);

  pinMode(OUT_A, INPUT);
  pinMode(OUT_B, INPUT);
  pinMode(BUTTON, INPUT);

  // If initial value is bigger, set down to maximal
  if (COUNT > 8)
    count = 8;
  else
    count = COUNT;

  if (CODE > MAX)
    number = MAX;
  else
    number = CODE;
  decimal = 100;

  sendCommand(0x8f);  // Activate and set brightness to max
  reset();            // Reset whole display

  // Initialize global variables
  rotation_state = digitalRead(OUT_A);
  button_state = HIGH;
  step = MIN_SPEED / 100;
  step_decimal = MIN_SPEED % 100;
  prev_time = millis();
  blink_time = millis();
  displayNumber(number, state);
  position = 0;
}

void loop()
{
  handleButton();   // Handle encoder pressing events
  handleRotation(); // Handle encoder rotation events

  if (state & STATE_EDIT) {
    uint32_t current_time = millis();
    if (current_time - blink_time > BLINK_TIME)
    {
      blink_time = current_time;
      state ^= STATE_BLINK; // Toggle digit visibility
    }
  } else {
    displayNumber(number, state); // Display number without first decimal place digit
  }
  if (state & STATE_COUNTDOWN) countdown();
  if (state & STATE_END) blink();
}
