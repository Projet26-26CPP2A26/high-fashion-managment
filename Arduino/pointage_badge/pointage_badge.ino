#define SIMULATION 0

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#if !SIMULATION
#include <SPI.h>
#include <MFRC522.h>
#endif

const byte PIN_SS = 10;
const byte PIN_RST = 9;
const byte PIN_BUZZER = 8;
const byte PIN_SERVO = 6;
const byte PIN_BOUTONS[3] = {2, 3, 4};
const char *BADGES_SIMULES[3] = {"04A1B2C3", "04D4E5F6", "0BADBAD0"};

const unsigned long DELAI_REPONSE = 3000;
const unsigned long DUREE_MESSAGE = 3000;
const int PORTE_FERMEE = 0;
const int PORTE_OUVERTE = 90;

class Note {
public:
  unsigned int frequence;
  unsigned int duree;
  unsigned int suivante;
};

const Note SON_ARRIVEE[] = {{659, 45, 75}, {988, 70, 0}};
const Note SON_DEPART[] = {{988, 45, 75}, {659, 70, 0}};
const Note SON_REFUS[] = {{330, 60, 110}, {330, 80, 0}};
const Note SON_DEJA[] = {{659, 45, 0}};

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo porte;
#if !SIMULATION
MFRC522 lecteur(PIN_SS, PIN_RST);
#else
bool etatPrecedent[3] = {false, false, false};
#endif

char ligne[48];
byte longueur = 0;
bool attente = false;
bool messageAffiche = false;
bool porteOuverte = false;
unsigned long debutAttente = 0;
unsigned long debutMessage = 0;
const Note *melodie = NULL;
byte notesRestantes = 0;
unsigned long prochaineNote = 0;

void afficher(const char *haut, const char *bas) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(haut);
  lcd.setCursor(0, 1);
  lcd.print(bas);
}

void accueil() {
  afficher("AUREA Atelier", "Passez le badge");
  messageAffiche = false;
}

void fermer() {
  if (!porteOuverte) return;
  porte.write(PORTE_FERMEE);
  porteOuverte = false;
}

void ouvrir() {
  porte.write(PORTE_OUVERTE);
  porteOuverte = true;
}

void message(const char *haut, const char *bas) {
  afficher(haut, bas);
  messageAffiche = true;
  debutMessage = millis();
}

void jouer(const Note *notes, byte nombre) {
  melodie = notes;
  notesRestantes = nombre;
  prochaineNote = millis();
}

void jouerNotes() {
  if (notesRestantes == 0 || (long)(millis() - prochaineNote) < 0) return;
  tone(PIN_BUZZER, melodie->frequence, melodie->duree);
  prochaineNote = millis() + melodie->suivante;
  melodie++;
  notesRestantes--;
}

void refuser(const char *haut, const char *bas) {
  fermer();
  jouer(SON_REFUS, 2);
  message(haut, bas);
}

void traiter(char *texte) {
  char *type = strtok(texte, ";");
  char *nom = strtok(NULL, ";");
  char *info = strtok(NULL, ";");
  if (type == NULL || !attente) return;
  char haut[17];
  char bas[17];
  if (strcmp(type, "IN") == 0) {
    snprintf(haut, sizeof(haut), "Bonjour %s", nom ? nom : "");
    snprintf(bas, sizeof(bas), "Arrivee %s", info ? info : "");
    jouer(SON_ARRIVEE, 2);
    ouvrir();
    message(haut, bas);
  } else if (strcmp(type, "OUT") == 0) {
    snprintf(haut, sizeof(haut), "A bientot %s", nom ? nom : "");
    snprintf(bas, sizeof(bas), "Travail %s", info ? info : "");
    jouer(SON_DEPART, 2);
    ouvrir();
    message(haut, bas);
  } else if (strcmp(type, "WAIT") == 0) {
    snprintf(haut, sizeof(haut), "%s", nom ? nom : "");
    jouer(SON_DEJA, 1);
    message(haut, "Deja enregistre");
  } else if (strcmp(type, "NO") == 0) {
    snprintf(bas, sizeof(bas), "%s", nom ? nom : "");
    refuser("Acces refuse", bas);
  } else if (strcmp(type, "ERR") == 0) {
    refuser("Base de donnees", "indisponible");
  } else {
    return;
  }
  attente = false;
}

void envoyer(const char *uid) {
  Serial.print("BADGE:");
  Serial.println(uid);
  attente = true;
  debutAttente = millis();
  messageAffiche = false;
  afficher("Verification...", uid);
}

void lireOrdinateur() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      ligne[longueur] = '\0';
      if (longueur > 0) traiter(ligne);
      longueur = 0;
    } else if (longueur < sizeof(ligne) - 1) {
      ligne[longueur++] = c;
    }
  }
}

void lireBadge() {
  if (attente || messageAffiche) return;
#if SIMULATION
  for (byte i = 0; i < 3; i++) {
    bool appuye = digitalRead(PIN_BOUTONS[i]) == LOW;
    if (appuye && !etatPrecedent[i]) {
      etatPrecedent[i] = true;
      envoyer(BADGES_SIMULES[i]);
      return;
    }
    etatPrecedent[i] = appuye;
  }
#else
  if (!lecteur.PICC_IsNewCardPresent() || !lecteur.PICC_ReadCardSerial()) return;
  char uid[21];
  byte n = 0;
  for (byte i = 0; i < lecteur.uid.size && n < 20; i++) {
    snprintf(uid + n, 3, "%02X", lecteur.uid.uidByte[i]);
    n += 2;
  }
  uid[n] = '\0';
  lecteur.PICC_HaltA();
  lecteur.PCD_StopCrypto1();
  envoyer(uid);
#endif
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUZZER, OUTPUT);
  porte.attach(PIN_SERVO);
  porte.write(PORTE_FERMEE);
  lcd.init();
  lcd.backlight();
#if SIMULATION
  for (byte i = 0; i < 3; i++) pinMode(PIN_BOUTONS[i], INPUT_PULLUP);
#else
  SPI.begin();
  lecteur.PCD_Init();
#endif
  Serial.println("HELLO");
  accueil();
}

void loop() {
  jouerNotes();
  lireOrdinateur();
  if (attente && millis() - debutAttente >= DELAI_REPONSE) {
    attente = false;
    refuser("PC non connecte", "Reessayez");
  }
  if (messageAffiche && millis() - debutMessage >= DUREE_MESSAGE) {
    fermer();
    accueil();
  }
  lireBadge();
}
