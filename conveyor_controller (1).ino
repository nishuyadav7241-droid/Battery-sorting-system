/*******************************************************
 * BATTERY SORTING SYSTEM
 * FILE 2 : conveyor_controller.ino
 * ARDUINO UNO  -  CONVEYOR CONTROLLER
 *
 * Flow:
 *   1. Auto-start conveyor on boot
 *   2. IR detects battery → stop → send READY
 *   3. ESP32 replies RUN → restart conveyor
 *   4. ESP32 replies STOP → stop conveyor (battery at servo)
 *   5. After servo fires ESP32 sends NEXT → restart conveyor
 *******************************************************/

#include <SoftwareSerial.h>

#define IR_PIN    2
#define ENA       5
#define IN1       8
#define IN2       9
#define ESP_RX    10
#define ESP_TX    11

const int MOTOR_SPEED = 150;

SoftwareSerial espSerial(ESP_RX, ESP_TX);

bool conveyorRunning = false;
bool irTriggered     = false;
bool waitingForStop  = false;

unsigned long irDebounce   = 0;
unsigned long startupTimer = 0;
bool          startupDone  = false;

const unsigned long DEBOUNCE_MS = 300;
const unsigned long STARTUP_MS  = 3000;

void startConveyor()
{
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, MOTOR_SPEED);
    conveyorRunning = true;
    Serial.println("Conveyor: ON");
}

void stopConveyor()
{
    analogWrite(ENA, 0);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    conveyorRunning = false;
    Serial.println("Conveyor: OFF");
}

void setup()
{
    Serial.begin(9600);
    espSerial.begin(9600);

    pinMode(IR_PIN, INPUT);
    pinMode(ENA,    OUTPUT);
    pinMode(IN1,    OUTPUT);
    pinMode(IN2,    OUTPUT);

    stopConveyor();
    startupTimer = millis();
    Serial.println("Arduino Ready");
}

void loop()
{
    // Auto-start after 3 seconds
    if (!startupDone && (millis() - startupTimer > STARTUP_MS))
    {
        startupDone = true;
        if (!conveyorRunning)
        {
            Serial.println("Auto-start conveyor");
            irTriggered    = false;
            waitingForStop = false;
            startConveyor();
        }
    }

    // Receive commands from ESP32
    if (espSerial.available())
    {
        String cmd = espSerial.readStringUntil('\n');
        cmd.trim();

        // Strip garbage
        String clean = "";
        for (int i = 0; i < cmd.length(); i++)
        {
            char c = cmd[i];
            if (c >= 32 && c < 127) clean += c;
        }

        Serial.print("ESP32 cmd: [");
        Serial.print(clean);
        Serial.println("]");

        if (clean == "GO" || clean == "NEXT")
        {
            // Start conveyor for next battery
            irTriggered    = false;
            waitingForStop = false;
            startConveyor();
        }
        else if (clean == "RUN")
        {
            // ESP32 says restart after IR — battery traveling to servo
            waitingForStop = true;
            irTriggered    = false;
            startConveyor();
        }
        else if (clean == "STOP")
        {
            // ESP32 says stop — battery is now at servo position
            waitingForStop = false;
            stopConveyor();
            Serial.println("At servo position");
        }
    }

    // IR sensor — only triggers when conveyor running and not in stop phase
    if (conveyorRunning && !waitingForStop)
    {
        int ir = digitalRead(IR_PIN);

        // LOW = battery detected (change to HIGH if your sensor is opposite)
        if (ir == LOW && !irTriggered)
        {
            unsigned long now = millis();
            if (now - irDebounce > DEBOUNCE_MS)
            {
                irDebounce  = now;
                irTriggered = true;

                stopConveyor();

                espSerial.println("READY");
                Serial.println("IR triggered -> READY sent");
            }
        }
    }
}
