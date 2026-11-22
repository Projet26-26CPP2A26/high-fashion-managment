const int LED_ROUGE = 8;
const int LED_VERTE = 7;
const int BUZZER    = 9;
const int BOUTON    = 2;

void setup() {
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_VERTE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BOUTON, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println("Pret");
  digitalWrite(LED_VERTE, HIGH);   // au départ : tout est à jour
}

void loop() {
  // 1) Réception du nombre de retards (envoyé par Qt, terminé par un retour à la ligne)
  if (Serial.available()) {
    int retards = Serial.readStringUntil('\n').toInt();
    if (retards > 0) {
      digitalWrite(LED_ROUGE, HIGH);
      digitalWrite(LED_VERTE, LOW);
      tone(BUZZER, 1000, 500);     // bip de 0,5 s
    } else {
      digitalWrite(LED_ROUGE, LOW);
      digitalWrite(LED_VERTE, HIGH);
      noTone(BUZZER);
    }
  }

  // 2) Bouton : « commande livrée »
  if (digitalRead(BOUTON) == LOW) {
    Serial.println("LIVRE");
    delay(300);                    // anti-rebond
  }
}