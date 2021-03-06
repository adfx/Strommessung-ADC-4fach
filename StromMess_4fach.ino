// ProgrammName: StromMess
// Kurzbeschreibung: Strommessung, Effektivwertbildung, Datenaufbereitung und Übertragung
// Version: 1.0
// Datum: 04.03.2017
// Autor: Jürgen Stähr

//Fragen: 
//Baudrate 115000 oder 115200?
//Werte ohne angeschlossenen Slave?
//Sensor ohne Strom -> zeigt 32mA an

// Konfiguration
//******* ACHTUNG ********** Hier an die vorhandene Hardware anpassen ******** ACHTUNG ***************
const int ADCAnz = 4;           // Anzahl der angeschlossenen ADC
const int CRundAnz = 4;         // Anzahl der Messungen je Sekunde
const int Korr = -7;             // 0 Abgleich
// Pin Nummern der cs Eingänge der ADC. Erlaubt sind 2 - 10 (digital Pins)
//                                                  14 - 17 (analogPins A0 - A3)
//const int cs[] = {10, 10,10,10,10,10,10,10,10,10};  // das geht auch, macht aber keinen Sinn
const int cs[] = {0, 2, 4, 6}; // Muss mit ADCAnz übereinstimmen oder größer sein
const int Shunt[] = {33, 33, 33, 33}; // Entsprechend dem cs[] Array die Widerstände eintragen
const unsigned long BaudRate = 115200;  // hier die Baudrate des seriellen Interfaces einstellen
//********************************************************************
// Beschreibung
/*--------------------------------------------------------------------------
 * Das Programm läuft auf einem Arduiono (Nano, Mini,...) und soll den Wechselstrom von mehreren Installationsleitungen messen.
 * 
 * Max. können 10 Wandler angeschlossen werden. Bei jedem Wandler wird 4 mal pro Sekunde je eine Periode gemessen und aus den 
 * Messergebnissen der RMS gebildet. Der Arduino schafft 586 Messungen in 20 ms bei Verwendung von unsigned long zum Speichern
 * der Messergebnisse. Bei Verwendung von float können nur 380 - 400 Messungen durchgeführt werden.
 * Die Integer Berechnung begrenzt den max. gemessenen Strom auf 2707 Wandlerzählern, entspricht bei 50 Ohm 33A.
 * 
 * Die Zusammengefassten Ergebnisse werden einmal pro Sekunde über die serielle Schnittstelle übertragen.
 * 
 * Die Messwandler liefern 1mA pro 1A Messstrom, max. jedoch 2,5V. ( wenn das der Effektivwert ist, liegt der Spitzenwert schon zu hoch für den ADC!!!!)
 * Ein 13 Bit ADC MCP 3301 digitalisiert die über einen Messshunt abfallende Spannung.
 * Die Größe des Mess-Shunt ist je ADC wählbar und muss im Array Shunt[] eingetragen werden.
 * Der ADC erhält eine Referenzspannung von 2,5V. Diese geteilt durch die halbe Auflösung=4096 ergibt den
 * Spannungswert eines ADC-Zählers= 0,61035 mV. Die Referenzspannungsquelle muss damit hinreichend genau sein.
 * 
 * Der ADC wird über SPI angesprochen. Er kann mit max 1,7 MHz getaktet werden. Der AVR läuft mit 16 MHz und kann den SPI Takt
 * nur durch 2,4,8,16,.. teilen, so dass der ADC mit 1 MHz getaktet wird. Damit dauert die Übertragung eines
 * Messwertes 16 uS. Weiter 18 uS werden für die Verarbeitung benötigt
 */

//#define debug

const unsigned long CPeriode = 20000; // Microsekunden einer Halbwelle
const unsigned long CRunde = 1000 / CRundAnz; // ms einer Messrunde über alle ADC
// Array zur Aufnanme der aller Messergebnisse einer Sekunde
int Erg[ADCAnz * CRundAnz];
const int Nr;

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3f, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address
#include <SPI.h>                    // SPI Bibliothek
#include <Streaming.h>              // Bibliothek zur besseren Ausgabeformatierung ==> Streaming.h ins Arduino Bibliotheksverzeichni kopieren
#include <MCP3304.h>
MCP3304 adc1(10);

// Funktionsprototypen  
int MessPeriode(int csADC);
void Ausgabe();
// Vorbereitung
void setup() {
  Serial.begin(BaudRate);  // Serielle Schnittstelle mit 115000 Baud starten
  lcd.begin(20,4);
  lcd.clear();
    lcd.setCursor(0,0); //Start at character 2 on line 1
    lcd.print("Strom L1: ");
    lcd.setCursor(0,1); //Start at character 1 on line 2
    lcd.print("Strom L2: ");
    lcd.setCursor(0,2); //Start at character 1 on line 3
    lcd.print("Strom L3: ");
        lcd.setCursor(0,3); //Start at character 1 on line 4
    lcd.print("Strom N:  ");
  SPI.begin();            // SPI Interface starten
  for (int i = 0; i < ADCAnz; i++) {    // cs Pins initialisieren
    pinMode(cs[i], OUTPUT);
    digitalWrite(cs[i], HIGH);
  }
  SPI.beginTransaction(SPISettings(1700000, MSBFIRST, SPI_MODE0)); // SPI Parameter
}



// Endlosschleife
void loop() {
  // Diese Schleife wird jede Sek. einmal durchlaufen
  for (int i = 0; i < ADCAnz * CRundAnz; i++) Erg[i] = 0; // Ergebniise des vorherigen Laufes löschen
  // Schleife je Runde. In jeder Runde werden alle ADC für je 20ms abgefragt
  for (int runde = 0; runde < CRundAnz; runde++) {
    unsigned long ZeitRunde = millis(); // Startwert in ms für eine Messrunde nehmen
    for (int ADCNr = 0; ADCNr < ADCAnz; ADCNr++) {
      int Index = runde * ADCAnz + ADCNr; // Zeigt auf das Ergebnisarray, Reihenfolge: Ergs der ersten Runde, Ergs der 2. Runde, usw.
      Erg[Index] = MessPeriode(cs[ADCNr]); // Messung für einen ADC durchführen und speichern
    }  // Ende, alle ADC einmal durchmessen
    if (runde == CRundAnz - 1) Ausgabe(); // wenn es die letzte Runde war ==> Daten ausgeben
#ifdef debug
    //Serial << "Wartezeit: " << (CRunde - (millis() - ZeitRunde)) << "ms; ";
#endif
    while (millis() - ZeitRunde < CRunde); // warten, bis Rundenzeit abgelaufen
  } // Ende Schleife über alle Mess-Runden

} //Ende loop
/* Function MessPeriode
    Führt mit einem ADC die Messungen über eine Periode durch.
    Der cs-PIN des ADC wird übergeben
    summiert die quadrierten Werte auf
    nach Ablauf wird durch die Anzahl der Messungen geteilt und die Wurzel gezogen ==> Rückgabewert*/
int MessPeriode(int csADC) {
  unsigned long ZeitPeriode = micros();
  int MessAnz = 0; // Anzahl der Messungen
  int ADCdata;  // nimmt den Messwert vom ADC auf
  unsigned long Erg = 0; // nimmt die quadrierten Summen einer Periode auf
     
     Serial.print("Korr: ");
     Serial.println(adc1.readAdc(csADC,0));
     
  do {
     ADCdata=adc1.readAdc(csADC,0)- Korr;

      

    Erg += (unsigned long)ADCdata * (unsigned long)ADCdata;  // Quadrieren
    MessAnz++;                                               // Anzahl der Messungen erhöhen
  } while (micros() - ZeitPeriode < CPeriode);               // Ende MessSchleife über eine Periode
  Erg /= MessAnz;                // Das Ergebnis aller Messungen durch die Anzahl teilen (quadratischer Mittelwert)
#ifdef debug
  Serial << "Anz:" << MessAnz;
  Serial << "M:" << ADCdata << " E:" << (int(sqrt(Erg))) << "; ";
#endif
  return int(sqrt(Erg));        // Rückgabe ist die Wurzel aus dem Mittelwert der Quadrate
}
/* Function Ausgabe
    gibt jede Sekunde die Messergebnisse über die Serielle Schnittstelle aus
*/
const unsigned long Uadc = 610350; // Wert eines Zählers 
unsigned long IuA;                 // Strom je Zähler eines ADC
void Ausgabe() {
  unsigned long SekWert;   // nimmt die Ergebnisse einer Runde eines ADC auf
//  for (int ADCNr = 0; ADCNr < ADCAnz; ADCNr++) { // Schleife über alle ADC

    SekWert = 0;
    for (int Runde = 0; Runde < CRundAnz; Runde++) { // Schleife über alle Runden je ADC
      SekWert += Erg[Runde * ADCAnz + 0]; // Messergebnisse eines ADC aufaddieren
    } // Ende Schleife über Rnden eines ADC
    SekWert /= CRundAnz;    // Mittelwert bilden
    IuA=Uadc/Shunt[1];  // Strom je Zähler in uA berechnen
    SekWert *= IuA;         // mit Strom-Zählerwert multiplizieren z.B. für 50 Ohm(12207 uA)
    SekWert /= 1000;        // auf mA runter rechnen
    // Ausgeben


     
     char buffer[7];   
    lcd.setCursor(10,0); //Start at character 1 on line 3

    sprintf(buffer,"%4imA ",SekWert);
    lcd.print (buffer);
    
Serial.print("3 ");
Serial.println(buffer);

    SekWert = 0;
    for (int Runde = 0; Runde < CRundAnz; Runde++) { // Schleife über alle Runden je ADC
      SekWert += Erg[Runde * ADCAnz + 1]; // Messergebnisse eines ADC aufaddieren
    } // Ende Schleife über Rnden eines ADC
    SekWert /= CRundAnz;    // Mittelwert bilden
    IuA=Uadc/Shunt[0];  // Strom je Zähler in uA berechnen
    SekWert *= IuA;         // mit Strom-Zählerwert multiplizieren z.B. für 50 Ohm(12207 uA)
    SekWert /= 1000;        // auf mA runter rechnen
    // Ausgeben

    
    lcd.setCursor(10,1); //Start at character 1 on line 2

    sprintf(buffer,"%4imA ",SekWert);
    lcd.print (buffer);
    
Serial.print("2 ");
Serial.println(buffer);

    SekWert = 0;
    for (int Runde = 0; Runde < CRundAnz; Runde++) { // Schleife über alle Runden je ADC
      SekWert += Erg[Runde * ADCAnz + 2]; // Messergebnisse eines ADC aufaddieren
    } // Ende Schleife über Rnden eines ADC
    SekWert /= CRundAnz;    // Mittelwert bilden
    IuA=Uadc/Shunt[1];  // Strom je Zähler in uA berechnen
    SekWert *= IuA;         // mit Strom-Zählerwert multiplizieren z.B. für 50 Ohm(12207 uA)
    SekWert /= 1000;        // auf mA runter rechnen
    // Ausgeben
    
    lcd.setCursor(10,2); //Start at character 1 on line 3

    sprintf(buffer,"%4imA ",SekWert);
    lcd.print (buffer);
    
Serial.print("1 ");
Serial.println(buffer);

    SekWert = 0;
    for (int Runde = 0; Runde < CRundAnz; Runde++) { // Schleife über alle Runden je ADC
      SekWert += Erg[Runde * ADCAnz + 3]; // Messergebnisse eines ADC aufaddieren
    } // Ende Schleife über Rnden eines ADC
    SekWert /= CRundAnz;    // Mittelwert bilden
    IuA=Uadc/Shunt[2];  // Strom je Zähler in uA berechnen
    SekWert *= IuA;         // mit Strom-Zählerwert multiplizieren z.B. für 50 Ohm(12207 uA)
    SekWert /= 1000;        // auf mA runter rechnen
    // Ausgeben

    lcd.setCursor(10,3); //Start at character 1 on line 4

    sprintf(buffer,"%4imA ",SekWert);
    lcd.print (buffer);

Serial.print("0 ");
Serial.println(buffer);

//  }  // Ende Schleife über alle ADC

} // Ende Function Ausgabe

