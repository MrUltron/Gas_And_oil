#define BLYNK_TEMPLATE_ID "TMPL3duOi7u2_"
#define BLYNK_TEMPLATE_NAME "IOTI"
#define BLYNK_AUTH_TOKEN "bgOlPLAvaKsLb6bgnaJGMFpFNI2x5kow"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>


char ssid[] = "ROS";
char pass[] = "12345678";

#define FLOW_S_PIN 14      
#define SERVO_S_PIN 12     
#define MQ2_ANALOG_S_PIN A0 

#define FLOW_CALIBRATION_FACTOR 7.5 
#define GAS_THRESHOLD 100          
#define PULSES_MIN 0
#define PULSES_MAX 225
#define PRESSURE_MIN 0.1
#define PRESSURE_MAX 2.0

Servo sg90;
volatile int flowPulses = 0;
float flowRate = 0.0;
float pressure = 0.0;
int fluxGeneration = 0;
unsigned long lastCalcTime = 0;
bool manualServoControl = false;
int lastServoAngle = 0;
bool lastGasState = false;

BlynkTimer timer;

ICACHE_RAM_ATTR void flowPulseCounter() {
  flowPulses++;
}

BLYNK_WRITE(V0) {
  int value = param.asInt();
  int angle = (value == 1) ? 180 : 0;
  sg90.write(angle);
  lastServoAngle = angle;
  manualServoControl = true;
  Serial.print("Servo manually switched to: ");
  Serial.println(value ? "On (180°)" : "Off (0°)");
}

void sendDataToBlynk() {
  int mq2Value = analogRead(MQ2_ANALOG_S_PIN);
  Blynk.virtualWrite(V1, flowRate);      
  Blynk.virtualWrite(V2, pressure);       
  Blynk.virtualWrite(V3, fluxGeneration); 
  Blynk.virtualWrite(V4, mq2Value);     
}

void setup() {
  Serial.begin(115200);
  delay(100);

  sg90.attach(SERVO_S_PIN);
  sg90.write(0);

  pinMode(MQ2_ANALOG_S_PIN, INPUT);
  pinMode(FLOW_S_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_S_PIN), flowPulseCounter, FALLING);

  WiFi.begin(ssid, pass);
  Serial.print("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();
  Serial.println("Connected to Blynk!");

  timer.setInterval(1000L, sendDataToBlynk);

  Serial.println("Warming up MQ2 sensor...");
  delay(20000);
  Serial.println("Setup complete.");
}

void loop() {
  Blynk.run();
  timer.run();

  int mq2Value = analogRead(MQ2_ANALOG_S_PIN);
  bool gasDetected = (mq2Value > 500);

  if (manualServoControl && gasDetected != lastGasState) {
    manualServoControl = false; 
  }

  if (!manualServoControl) {
    if (gasDetected) {
      sg90.write(180);
      lastServoAngle = 180;
    } else {
      sg90.write(0);
      lastServoAngle = 0;
    }
  }

  if (millis() - lastCalcTime >= 1000) {
    noInterrupts();
    int pulses = flowPulses;
    flowPulses = 0;
    interrupts();

    fluxGeneration = pulses;

    float rawFlowRate = (pulses / FLOW_CALIBRATION_FACTOR) * 60.0;
    flowRate = map(pulses, PULSES_MIN, PULSES_MAX, 0, 300);
    flowRate = flowRate / 10.0;

    if (pulses <= PULSES_MIN) {
      pressure = PRESSURE_MIN;
    } else if (pulses >= PULSES_MAX) {
      pressure = PRESSURE_MAX;
    } else {
      pressure = PRESSURE_MIN + ((float)(pulses - PULSES_MIN) / (PULSES_MAX - PULSES_MIN)) * (PRESSURE_MAX - PRESSURE_MIN);
    }

    Serial.print("Flux: "); Serial.print(fluxGeneration); Serial.print(" pulses/s | ");
    Serial.print("Mapped Flow: "); Serial.print(flowRate); Serial.print(" L/min | ");
    Serial.print("Raw Flow: "); Serial.print(rawFlowRate); Serial.print(" L/min | ");
    Serial.print("Pressure: "); Serial.print(pressure); Serial.print(" bars | ");
    Serial.print("MQ2: "); Serial.print(mq2Value); Serial.print(" | ");
    Serial.print("Gas: "); Serial.print(gasDetected ? "Yes" : "No"); Serial.print(" | ");
    Serial.print("Servo: "); Serial.print(lastServoAngle); Serial.println("°");

    if (pulses == 0) {
      Serial.println("No flow detected! Check wiring or fluid supply.");
    }

    lastGasState = gasDetected;
    lastCalcTime = millis();
  }

  delay(100);
}
