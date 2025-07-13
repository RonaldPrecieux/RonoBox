#include "ACS712_ESP8266.h"

ACS712 sensor(ACS712_30A, A0); // Type 5A, lecture sur A0

void setup() {
  Serial.begin(115200);
  delay(1000);
  sensor.calibrate(); // Repos sans courant
}

void loop() {
  float current = sensor.getCurrentAC(); // ou getCurrentDC()
  Serial.print("Courant : ");
  Serial.print(current, 3);
  Serial.println(" A");
  delay(1000);
}
