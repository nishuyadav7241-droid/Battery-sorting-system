/*******************************************************
 * BATTERY SORTING SYSTEM
 * FILE 1 : battery_sorter_esp32.ino
 * ESP32 MASTER CONTROLLER
 *******************************************************/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <DHT.h>

// ========================================================
//  PINS
// ========================================================
#define DHTPIN        4
#define DHTTYPE       DHT11

#define TRIG_PIN      5
#define ECHO_PIN      18

#define RXD2          26
#define TXD2          27

#define SERVO_18650   14
#define SERVO_21700   13
#define SERVO_9V      15
#define SERVO_COIN    17
#define SERVO_DCELL   19
#define SERVO_LEAD    16

// ========================================================
//  TIMING
// ========================================================
const unsigned long IR_TO_SERVO1  = 1200UL;
const unsigned long SERVO_SPACING = 1900UL;

// ========================================================
//  OBJECTS
// ========================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT               dht(DHTPIN, DHTTYPE);
HardwareSerial    ArduinoSerial(2);

Servo servo18650;
Servo servo21700;
Servo servo9V;
Servo servoCoin;
Servo servoDCell;
Servo servoLead;

// ========================================================
//  QUEUE
// ========================================================
#define MAX_QUEUE 20

struct Battery
{
    char          type[12];
    bool          active;
    unsigned long servoTime;
};

Battery bQueue[MAX_QUEUE];
int qFront = 0;
int qRear  = 0;

bool qEmpty() { return qFront == qRear; }
bool qFull()  { return ((qRear + 1) % MAX_QUEUE) == qFront; }

void qEnqueue(const char* t)
{
    if (qFull()) { Serial.println("Queue full"); return; }
    strncpy(bQueue[qRear].type, t, 11);
    bQueue[qRear].type[11] = '\0';
    bQueue[qRear].active    = false;
    bQueue[qRear].servoTime = 0;
    qRear = (qRear + 1) % MAX_QUEUE;
    Serial.print("Queued: ");
    Serial.println(t);
}

void qDequeue()
{
    if (!qEmpty()) qFront = (qFront + 1) % MAX_QUEUE;
}

int qSize()
{
    return (qRear - qFront + MAX_QUEUE) % MAX_QUEUE;
}

// ========================================================
//  SENSOR GLOBALS
// ========================================================
float tempC    = 0;
float humidity = 0;
long  distCM   = 0;

// ========================================================
//  LCD CONTROL
// ========================================================
unsigned long lcdTimer = 0;
bool          lcdPage  = false;

// ========================================================
//  ULTRASONIC
// ========================================================
long readDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
    if (dur == 0) return -1;
    return (long)(dur * 0.034 / 2);
}

// ========================================================
//  DHT11
// ========================================================
void readDHT()
{
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && t > -10 && t < 80)   tempC    = t;
    if (!isnan(h) && h >= 0  && h <= 100) humidity = h;
}

// ========================================================
//  LCD
// ========================================================
void updateLCD()
{
    if (millis() - lcdTimer < 2000) return;
    lcdTimer = millis();

    lcd.clear();

    if (!lcdPage)
    {
        lcd.setCursor(0, 0);
        lcd.print("Bat:");
        lcd.print(qEmpty() ? "WAITING" : bQueue[qFront].type);

        lcd.setCursor(0, 1);
        distCM = readDistance();
        lcd.print("D:");
        if (distCM < 0) lcd.print("---");
        else            lcd.print(distCM);
        lcd.print("cm Q:");
        lcd.print(qSize());
    }
    else
    {
        lcd.setCursor(0, 0);
        lcd.print("T:");
        lcd.print((int)tempC);
        lcd.print((char)223);
        lcd.print("C H:");
        lcd.print((int)humidity);
        lcd.print("%");

        lcd.setCursor(0, 1);
        lcd.print("Queue:");
        lcd.print(qSize());
    }

    lcdPage = !lcdPage;
}

// ========================================================
//  SERVOS
// ========================================================
void resetServos()
{
    servo18650.write(0); servo21700.write(0); servo9V.write(0);
    servoCoin.write(0);  servoDCell.write(0); servoLead.write(0);
}

void fireServo(const char* type)
{
    Serial.print(">>> Firing servo: ");
    Serial.println(type);

    if      (strcmp(type, "18650")    == 0) servo18650.write(90);
    else if (strcmp(type, "21700")    == 0) servo21700.write(90);
    else if (strcmp(type, "9V")       == 0) servo9V.write(90);
    else if (strcmp(type, "CoinCell") == 0) servoCoin.write(90);
    else if (strcmp(type, "DCell")    == 0) servoDCell.write(90);
    else if (strcmp(type, "LeadAcid") == 0) servoLead.write(90);
    else
    {
        Serial.print("!!! Unknown type: ");
        Serial.println(type);
        return;
    }

    delay(500);
    resetServos();
    Serial.println(">>> Servo done");
}

// ========================================================
//  SETUP
// ========================================================
void setup()
{
    Serial.begin(9600);
    ArduinoSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();

    dht.begin();

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    servo18650.setPeriodHertz(50); servo18650.attach(SERVO_18650, 500, 2400);
    servo21700.setPeriodHertz(50); servo21700.attach(SERVO_21700, 500, 2400);
    servo9V   .setPeriodHertz(50); servo9V   .attach(SERVO_9V,    500, 2400);
    servoCoin .setPeriodHertz(50); servoCoin .attach(SERVO_COIN,   500, 2400);
    servoDCell.setPeriodHertz(50); servoDCell.attach(SERVO_DCELL,  500, 2400);
    servoLead .setPeriodHertz(50); servoLead .attach(SERVO_LEAD,   500, 2400);
    resetServos();

    lcd.setCursor(0, 0); lcd.print("Battery Sorter");
    lcd.setCursor(0, 1); lcd.print("System Ready");
    delay(2000);
    lcd.clear();

    // Delay before sending GO so Arduino has time to boot
    delay(3000);
    ArduinoSerial.println("GO");
    Serial.println("ESP32 Ready - GO sent to Arduino");
}

// ========================================================
//  LOOP
// ========================================================
void loop()
{
    readDHT();

    // ── Python -> ESP32 ──
    if (Serial.available())
    {
        String s = Serial.readStringUntil('\n');
        s.trim();

        // Strip garbage characters
        String clean = "";
        for (int i = 0; i < s.length(); i++)
        {
            char c = s[i];
            if (c >= 32 && c < 127) clean += c;
        }

        if (clean.length() > 0 && clean.length() < 12)
        {
            Serial.print("Python sent: [");
            Serial.print(clean);
            Serial.println("]");
            qEnqueue(clean.c_str());
        }
    }

    // ── Arduino -> ESP32 ──
    if (ArduinoSerial.available())
    {
        String msg = ArduinoSerial.readStringUntil('\n');
        msg.trim();

        // Strip garbage
        String clean = "";
        for (int i = 0; i < msg.length(); i++)
        {
            char c = msg[i];
            if (c >= 32 && c < 127) clean += c;
        }

        Serial.print("Arduino msg: [");
        Serial.print(clean);
        Serial.println("]");

        if (clean == "READY")
        {
            Serial.println("READY received!");

            if (!qEmpty())
            {
                bQueue[qFront].active    = true;
                bQueue[qFront].servoTime = millis() + IR_TO_SERVO1;
                Serial.print("Timer set for: ");
                Serial.println(bQueue[qFront].type);
            }
            else
            {
                Serial.println("Queue empty - no battery to sort");
                // Restart conveyor anyway
                ArduinoSerial.println("NEXT");
            }
        }
    }

    // ── Servo scheduler ──
    if (!qEmpty() && bQueue[qFront].active)
    {
        if (millis() >= bQueue[qFront].servoTime)
        {
            char buf[12];
            strncpy(buf, bQueue[qFront].type, 11);
            buf[11] = '\0';

            fireServo(buf);
            qDequeue();

            Serial.println("Sending NEXT to Arduino");
            ArduinoSerial.println("NEXT");
        }
    }

    updateLCD();
}
