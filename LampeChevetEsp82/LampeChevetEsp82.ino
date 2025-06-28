const int soundPin = 13; // D0 du capteur branché sur D5 (GPIO14)
const int ledPin = 2;    // LED embarquée

void setup() {
  pinMode(soundPin, INPUT);
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Éteint la LED
}

void loop() {
  int detection = digitalRead(soundPin);
  Serial.println(detection);
  if (detection == LOW) {
    digitalWrite(ledPin, LOW); // Allume la LED (inversé sur ESP8266)
  } else {
    digitalWrite(ledPin, HIGH); // Éteint
  }
}
