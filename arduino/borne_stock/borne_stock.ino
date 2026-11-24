// AUREA - Borne de stock (Arduino Uno + clavier 4x4 + LCD I2C 16x2 + buzzer)
// Clavier : ID de la matiere, puis C = consulter
//           ID, A = entree ou B = sortie, quantite, # = valider      * ou D = annuler
// Vers Qt : C6   E6:10   S6:2          Depuis Qt : ligne1|ligne2|etat
// etat : 0 = normal, 1 = sous le seuil (buzzer), 2 = erreur
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIN_BUZZER 10
LiquidCrystal_I2C lcd(0x27, 16, 2);   // si l'ecran reste vide : essayer 0x3F

const char touches[4][4] = {
  {'1', '2', '3', 'A'}, {'4', '5', '6', 'B'}, {'7', '8', '9', 'C'}, {'*', '0', '#', 'D'}};
const byte lignes[4] = {9, 8, 7, 6};
const byte colonnes[4] = {5, 4, 3, 2};

char derniere = 0;          // derniere touche lue (anti-rebond)
String id = "", quantite = "", reponse = "";
char operation = 0;         // 'E' entree, 'S' sortie, 0 = saisie de l'ID
bool attente = false;       // en attente de la reponse de Qt
unsigned long debut = 0;

char lireClavier()
{
  char lue = 0;
  for (byte l = 0; l < 4; l++) {
    digitalWrite(lignes[l], LOW);
    for (byte c = 0; c < 4; c++)
      if (digitalRead(colonnes[c]) == LOW) lue = touches[l][c];
    digitalWrite(lignes[l], HIGH);
  }
  if (lue == derniere) return 0;
  delay(25);
  derniere = lue;
  return lue;
}

void afficher(String l1, String l2)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(l2.substring(0, 16));
}

void accueil()
{
  id = "";
  quantite = "";
  operation = 0;
  attente = false;
  afficher("AUREA  Stock", "ID matiere ?");
}

void envoyer(String message)
{
  Serial.println(message);
  afficher("Recherche...", "");
  attente = true;
  debut = millis();
}

void traiterReponse()
{
  int a = reponse.indexOf('|'), b = reponse.lastIndexOf('|');
  if (a < 0 || a == b) return;
  char etat = reponse.charAt(b + 1);
  afficher(reponse.substring(0, a), reponse.substring(a + 1, b));
  byte bips = etat == '1' ? 3 : etat == '2' ? 1 : 0;
  for (byte i = 0; i < bips; i++) {
    tone(PIN_BUZZER, etat == '2' ? 300 : 1000, 200);
    delay(350);
  }
  id = "";
  quantite = "";
  operation = 0;
  attente = false;           // le resultat reste affiche jusqu'a la prochaine touche
}

void traiterTouche(char t)
{
  if (t == '*' || t == 'D') { accueil(); return; }
  if (attente) return;
  if (t >= '0' && t <= '9') {
    if (operation == 0 && id.length() < 6) id += t;
    else if (operation != 0 && quantite.length() < 5) quantite += t;
  }
  else if (operation == 0 && id.length() > 0 && t == 'C') { envoyer("C" + id); return; }
  else if (operation == 0 && id.length() > 0 && (t == 'A' || t == 'B')) operation = t == 'A' ? 'E' : 'S';
  else if (t == '#' && operation != 0 && quantite.length() > 0) {
    envoyer(String(operation) + id + ":" + quantite);
    return;
  }
  if (operation == 0) afficher("ID : " + id, "A+  B-  C=voir");
  else afficher(String(operation == 'E' ? "Entree ID " : "Sortie ID ") + id, "Qte : " + quantite + "  #=OK");
}

void setup()
{
  Serial.begin(9600);
  pinMode(PIN_BUZZER, OUTPUT);
  for (byte i = 0; i < 4; i++) {
    pinMode(lignes[i], OUTPUT);
    digitalWrite(lignes[i], HIGH);
    pinMode(colonnes[i], INPUT_PULLUP);
  }
  lcd.init();
  lcd.backlight();
  accueil();
}

void loop()
{
  char t = lireClavier();
  if (t) traiterTouche(t);

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      traiterReponse();
      reponse = "";
    }
    else if (c != '\r' && reponse.length() < 60) reponse += c;
  }
  if (attente && millis() - debut > 5000) {
    accueil();
    afficher("AUREA ferme ?", "Pas de reponse");
  }
}
