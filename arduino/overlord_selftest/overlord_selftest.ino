// SELF-TEST: runs each part one at a time, forever.
// Upload it after wiring each stage and note what does NOT happen.
// The same messages also appear in Serial Monitor (9600 baud).

#include <Servo.h>
#include <LiquidCrystal.h>

const byte PIN_SERVO  = 9;
const byte PIN_R      = 3;
const byte PIN_G      = 5;
const byte PIN_B      = 6;
const byte PIN_BUZZER = 10;
const byte PIN_BUTTON = A0;
const bool COMMON_ANODE = false;        // set true if the colours look inverted
LiquidCrystal lcd(12, 11, 8, 7, 4, 2);  // RS, E, D4, D5, D6, D7
Servo head;

void setRGB(byte r, byte g, byte b) {
    if (COMMON_ANODE) { r = 255 - r; g = 255 - g; b = 255 - b; }
    analogWrite(PIN_R, r);
    analogWrite(PIN_G, g);
    analogWrite(PIN_B, b);
}

void say(const char* what) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(what);
    Serial.println(what);
}

void setup() {
    Serial.begin(9600);
    pinMode(PIN_R, OUTPUT);
    pinMode(PIN_G, OUTPUT);
    pinMode(PIN_B, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    lcd.begin(16, 2);
    lcd.print("SELF TEST");
    head.attach(PIN_SERVO);
    head.write(80);
}

void loop() {
    say("servo");
    head.write(40); delay(700);
    head.write(120); delay(700);
    head.write(80); delay(500);

    say("rgb red");   setRGB(255, 0, 0); delay(600);
    say("rgb green"); setRGB(0, 255, 0); delay(600);
    say("rgb blue");  setRGB(0, 0, 255); delay(600);
    setRGB(0, 0, 0);

    say("buzzer");
    digitalWrite(PIN_BUZZER, HIGH); delay(200);
    digitalWrite(PIN_BUZZER, LOW); delay(300);

    say("press button");
    unsigned long until = millis() + 3000;
    while (millis() < until) {
        lcd.setCursor(13, 1);
        lcd.print(digitalRead(PIN_BUTTON) == LOW ? "YES" : "no ");
    }
    say("--- again ---");
    delay(500);
}
