// DESK OVERLORD - Arduino side
// Receives lines like "2|YOUTUBE AGAIN?" over USB (9600 baud) and shows a mood.
// Sends "P" back when the button is pressed (a request for a truce).

#include <Servo.h>
#include <LiquidCrystal.h>

// ---------- Pins (must match the wiring section) ----------
const byte PIN_SERVO  = 9;
const byte PIN_R      = 3;   // PWM
const byte PIN_G      = 5;   // PWM
const byte PIN_B      = 6;   // PWM
const byte PIN_BUZZER = 10;
const byte PIN_BUTTON = A0;
LiquidCrystal lcd(12, 11, 8, 7, 4, 2);  // RS, E, D4, D5, D6, D7

// ---------- Tuning: edit these, not the logic ----------
const bool COMMON_ANODE = false;  // true if the LED colours look inverted
const bool SOUND_ON     = true;   // false = silent rehearsal
const int  ANGLE_AWAY   = 40;     // head angle when ignoring you
const int  ANGLE_MID    = 80;     // head angle when watching you
const int  ANGLE_YOU    = 120;    // head angle when glaring at you
const unsigned long LINK_TIMEOUT_MS = 5000;  // silence this long = NO SIGNAL

Servo head;
int level = -1;                  // -1 = no link, 0..3 = mood
char line2[17] = "WAITING FOR PC";
bool lcdDirty = true;
int currentAngle = ANGLE_AWAY;
unsigned long lastMsgMs = 0;
unsigned long lastServoMs = 0;
char buf[24];
byte bufLen = 0;
bool lastButton = HIGH;
unsigned long lastButtonMs = 0;

void setRGB(byte r, byte g, byte b) {
    if (COMMON_ANODE) { r = 255 - r; g = 255 - g; b = 255 - b; }
    analogWrite(PIN_R, r);
    analogWrite(PIN_G, g);
    analogWrite(PIN_B, b);
}

// Write exactly 16 characters to one LCD row (pads with spaces, no flicker).
void lcdLine(byte row, const char* text) {
    lcd.setCursor(0, row);
    byte i = 0;
    while (text[i] != 0 && i < 16) { lcd.write(text[i]); i++; }
    while (i < 16) { lcd.write(' '); i++; }
}

void setLevel(int newLevel, const char* msg) {
    if (newLevel != level || strncmp(line2, msg, 16) != 0) lcdDirty = true;
    level = newLevel;
    strncpy(line2, msg, 16);
    line2[16] = 0;
}

// ---------- Input from the laptop: "<level>|<text>\n" ----------
void readSerial(unsigned long now) {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            buf[bufLen] = 0;
            if (bufLen >= 1 && buf[0] >= '0' && buf[0] <= '3') {
                const char* msg = (bufLen >= 2 && buf[1] == '|') ? buf + 2 : "";
                setLevel(buf[0] - '0', msg);
                lastMsgMs = now;
            }
            bufLen = 0;
        } else if (bufLen < sizeof(buf) - 1) {
            buf[bufLen++] = c;
        }
    }
}

// ---------- Button: send "P" once per press ----------
void checkButton(unsigned long now) {
    bool b = digitalRead(PIN_BUTTON);
    if (b != lastButton && now - lastButtonMs > 40) {
        lastButtonMs = now;
        lastButton = b;
        if (b == LOW) Serial.println("P");
    }
}

// ---------- Outputs ----------
void updateLCD() {
    if (!lcdDirty) return;
    lcdDirty = false;
    const char* top = "NO SIGNAL";
    if (level == 0) top = "ALL IS CALM";
    if (level == 1) top = "I SEE YOU...";
    if (level == 2) top = "CLOSE THAT TAB";
    if (level == 3) top = "ENOUGH!!!";
    lcdLine(0, top);
    lcdLine(1, line2);
}

void updateServo(unsigned long now) {
    if (now - lastServoMs < 20) return;
    lastServoMs = now;
    int target = ANGLE_AWAY;
    int shake = 0;
    if (level == 1) target = ANGLE_MID;
    if (level == 2) { target = ANGLE_YOU; shake = 6; }
    if (level == 3) { target = ANGLE_YOU; shake = 14; }
    if (shake > 0) target += ((now / 120) % 2 == 0) ? shake : -shake;
    target = constrain(target, 10, 170);
    if (currentAngle < target) currentAngle = min(currentAngle + 4, target);
    else if (currentAngle > target) currentAngle = max(currentAngle - 4, target);
    head.write(currentAngle);
}

void updateLED(unsigned long now) {
    if (level == 0) setRGB(0, 40, 0);              // calm: soft green
    else if (level == 1) setRGB(120, 70, 0);       // watching: amber
    else if (level == 2) setRGB(255, 40, 0);       // angry: orange-red
    else if (level == 3) {                         // furious: flashing red
        if ((now / 150) % 2 == 0) setRGB(255, 0, 0); else setRGB(0, 0, 0);
    }
    else setRGB(0, 0, 60);                         // no signal: blue
}

void updateBuzzer(unsigned long now) {
    bool on = false;
    if (SOUND_ON) {
        if (level == 2) on = (now % 2000) < 90;    // short chirp every 2 s
        if (level == 3) on = (now % 400) < 200;    // rapid beeping
    }
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

void setup() {
    Serial.begin(9600);
    pinMode(PIN_R, OUTPUT);
    pinMode(PIN_G, OUTPUT);
    pinMode(PIN_B, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    lcd.begin(16, 2);
    head.attach(PIN_SERVO);
    head.write(currentAngle);
}

void loop() {
    unsigned long now = millis();
    readSerial(now);
    if (level >= 0 && now - lastMsgMs > LINK_TIMEOUT_MS) setLevel(-1, "CHECK PYTHON");
    checkButton(now);
    updateLCD();
    updateServo(now);
    updateLED(now);
    updateBuzzer(now);
}
