# Tumbler26

Zweirädriger Balancierroboter auf der eigenen Platine **tumlerV2**
(ESP32-WROOM-32E, TB6612FNG, MPU6050, Radencoder, 8× WS2812).

Das Repository enthält **zwei Firmwares für denselben Roboter**, damit sich
zwei Lösungswege derselben Aufgabe nebeneinander vergleichen lassen:

| Zweig | Was es ist | Winkelfehler |
|---|---|---|
| **`master`** *(hier)* | Portierung von [OE3KUW/TumbllerVSC](https://github.com/OE3KUW/TumbllerVSC) — Parallelsumme, Kalman, Originalwerte | 0,69° |
| [`abgestimmte-fassung`](../../tree/abgestimmte-fassung) | eigenständig entwickelt — Kaskade, Selbstabstimmung, Aufschwingen, Stoßreaktion | 0,566° |

Beide Werte sind am selben Aufbau gemessen (Effektivwert des Winkelfehlers im
freien Stand). Bemerkenswert daran: die Vorlage steht mit ihren *unveränderten*
Verstärkungen vom Yahboom-Fahrzeug nur rund 20 % unruhiger als die über Stunden
eingemessene Eigenentwicklung.

---

# Diese Fassung: die Portierung

Die Herkunft und Zuordnung der übernommenen Dateien ist in
[ATTRIBUTION.md](ATTRIBUTION.md) dokumentiert.

Die Regelung ist Zeile für Zeile die des Originals — gleiche Struktur, gleiche
Verstärkungen, gleiche Grenzen. Geändert wurde nur, was auf dem ESP32 anders
sein *muss*; jede dieser Stellen ist im Quelltext einzeln vermerkt.

```
 Winkel  ──[ kp_balance, kd_balance ]──┐
 Tempo   ──[ kp_speed, ki_speed ]──────┼─► pwm_links  = balance − tempo − drehung
 Drehung ──[ kp_turn, kd_turn ]────────┴─► pwm_rechts = balance − tempo + drehung
```

Alle drei Regler erzeugen direkt PWM und werden addiert — anders als in der
`abgestimmte-fassung`, wo die äußeren Regler den *Sollwinkel* verschieben.

## Inbetriebnahme

### 1. WLAN eintragen

```bash
cp src/secrets.h.example src/secrets.h     # SSID und Passwort eintragen
```

Die Datei steht in `.gitignore`. Der ESP32 kann nur **2,4 GHz**.

### 2. Aufspielen

Beim ersten Mal über Kabel, danach über WLAN:

```bash
pio run -t upload                       # Kabel
pio run -e ota -t upload                # danach kabellos
pio device monitor                      # 115200 Baud
```

### 3. Starten

Aufrecht hinstellen, dann **START** auf der Weboberfläche, `5` in der Konsole
oder den Taster drücken.

> Der Selbststart der Vorlage (zwei Sekunden nach dem Einschalten) ist hier
> **abgeschaltet**, siehe `AUTO_START` in `src/main.cpp`. Grund: dieser Roboter
> meldet liegend nur 17,7°, das liegt innerhalb des Balancierfensters. Die
> Vorlage hielte ihn für aufrecht und würde mit voller Stellgröße losfahren,
> obwohl er am Boden liegt.

## Bedienung

**Weboberfläche** unter `http://tumbler-orig.local/` — Winkel, beide
Stellgrößen, Betriebszustand, Laufzeit eines Regelschritts, Verlauf über 24 s,
und alle sechs Reglerwerte als Schieber mit dem Originalwert daneben.
Änderungen wirken sofort, werden aber nicht gespeichert.

**Konsole über WLAN**, falls kein Browser zur Hand ist:

```bash
nc tumbler-orig.local 23
```

Dieselben Zeichen wie über die serielle Konsole:

| Taste | Wirkung |
|---|---|
| `5` | starten |
| `s` | Stillstand (balanciert auf der Stelle) |
| `x` | anhalten — bleibt aus, bis START oder STANDBY kommt |
| `f` / `b` | vorwärts / rückwärts |
| `l` / `i` | links / rechts |
| `z` | nullen — aktuelle Lage als Senkrechte übernehmen |

Der **Taster an GPIO2** ist ein Wechselschalter: steht er, geht er los — läuft
er, hält er an. Das Original schickt dort `s` (also *STANDBY*, „auf der Stelle
balancieren") und konnte den Roboter damit gar nicht anhalten.

`x` und `z` gibt es im Original nicht.

**`x` (STOP)** hält wirklich an. Die Vorlage kennt `STOP` nur als kurzen
Zwischenzustand — eine Sekunde später schaltet `setMotionState()` von selbst
nach `STANDBY` zurück, sobald der Roboter aufrecht steht:

```c
case STOP:
    if (millis() - start_prev_time > 1000)
      if (in range) motion_mode = STANDBY;     // fängt von allein wieder an
```

Das ist sinnvoll, damit er sich nach einem Sturz selbst wieder fängt — es
bedeutet aber, dass es *gar keinen* bewussten Stillstand gibt und man den
Nullpunkt nie messen kann. Ein Flag unterscheidet jetzt beides: von Hand
angehalten bleibt angehalten, ein Sturz nicht.

**`z` (ZERO)** füllt zwei Variablen, die im Original zwar im Regelgesetz
stehen, aber nirgends gesetzt werden — `angle_zero` und
`angular_velocity_zero` bleiben dort auf null. Damit regelt die Vorlage auf die
Senkrechte des *Sensors*, nicht auf die Lage, in der der Schwerpunkt wirklich
über der Achse steht; die beiden unterscheiden sich um den Einbauwinkel des
GY-521 und um die Gewichtsverteilung, und die Folge ist ein dauerhaftes
Wegdriften.

`z` mittelt eine halbe Sekunde und übernimmt das Ergebnis — ohne Warteschleife,
der Regeltakt zählt selbst mit. Es wird **abgelehnt**, wenn er nicht im
Stillstand oder nicht ruhig und aufrecht ist. Beides ist am eigenen Fehlversuch
gelernt: einmal wurde die *liegende* Lage zur Senkrechten erklärt, und einmal
erwischte die Mittelung im laufenden Betrieb einen Punkt der Pendelbewegung —
−6,7° und −69°/s, was den D-Anteil dauerhaft verschoben und den Regler
unbrauchbar gemacht hätte.

## Als App aufs Handy

Die Seite lässt sich wie eine App auf den Startbildschirm legen — eigenes
Symbol, Vollbild, keine Browserleiste:

1. `http://tumbler-orig.local/` in **Chrome** öffnen
2. Menü ⋮ → **Zum Startbildschirm hinzufügen**

Dahinter steckt kein zweites Programm, sondern ein `manifest.webmanifest` und
ein Symbol, die der Roboter selbst ausliefert. Das Symbol ist ein SVG von rund
400 Byte statt eines PNG-Satzes in mehreren Größen — bei 85 % belegtem
Programmspeicher zählt jedes Kilobyte.

Dazu ein paar Handgriffe fürs Handy: kein Doppeltipp-Zoom auf den Knöpfen, kein
Überziehen zum Neuladen, und die Statusleiste färbt sich passend ein.

Der Roboter muss dafür im selben WLAN sein. Eine echte App, die über Bluetooth
statt WLAN spricht, wäre ein eigenes Android-Projekt — dafür braucht es das
Android-SDK und einen Sideload.

## Bluetooth

Der ESP32-WROOM-32E beherrscht Bluetooth Classic — das Original braucht dafür
ein HC-05-Modul an der seriellen Schnittstelle. Dieselben Tastenzeichen
funktionieren damit auch vom Handy aus, mit jeder „Serial Bluetooth
Terminal"-App unter Android. (iPhones können SPP nicht, dort bräuchte es BLE.)

1. Auf der Seite **Bluetooth → ein**
2. Koppeln als **`Tumbller26`**
3. Mit der Terminal-App verbinden — der Roboter schickt Messwerte zurück

**Es startet bewusst nicht von selbst.** WLAN und Bluetooth teilen sich eine
Antenne, und mit dauerhaft laufendem Bluetooth wurde am eigenen Aufbau
gemessen:

| | ohne Bluetooth | mit Bluetooth |
|---|---|---|
| Ping | ~5 ms | 41–84 ms |
| OTA | zuverlässig | bricht bei 8 % ab |

Beim ersten Anlauf lief es dauerhaft mit — damit war die Aktualisierung über
Funk unbrauchbar, und ohne Kabel kam man nicht mehr an den Roboter heran, um es
zurückzunehmen. Vor dem nächsten Flashen also **Bluetooth → aus**.

## Warum OTA bei großen Abbildern abbrach

Mit Bluetooth im Programm wächst das Abbild von 0,9 auf 1,7 MB, und OTA brach
reproduzierbar bei 11 % ab — auch bei ausgeschaltetem Bluetooth. Ursache war
nicht der Funk, sondern die Prioritätsverteilung: die Regelung läuft alle 5 ms
mit Priorität 5, das Update im Hauptprogramm mit Priorität 1. Bei 0,9 MB ging
sich das aus, bei 1,7 MB verhungerte das Update.

`ArduinoOTA.onStart()` legt die Regelung deshalb schlafen (`vTaskSuspend`) —
die Motoren sind dort ohnehin schon abgeschaltet, und gleich darauf wird neu
gestartet. Danach läuft OTA wieder durch.

## Winkelfilter umschaltbar

Auf der Seite lässt sich zwischen **Kalman** und **Komplementärfilter**
umschalten. Beide rechnen immer mit, umgeschaltet wird nur, welcher den Regler
speist — ein Vergleich ist damit im laufenden Betrieb möglich.

Auch das füllt eine Lücke der Vorlage: `KalmanFilter::Yiorderfilter()` ist dort
deklariert, implementiert **und wird nirgends aufgerufen**. Sogar der Parameter
`K1 = 0.05` wird bis in `Angle()` durchgereicht und dann ignoriert. Es ist ein
Komplementärfilter, also genau das Gegenstück zum Kalman.

In der `abgestimmte-fassung` war der Komplementärfilter messbar ruhiger —
0,566° gegen 0,633° Effektivfehler, dreimal im Wechsel gemessen. Der Grund ist
plausibel: mit `R_angle = 0.5` glaubt der Kalman dem Beschleunigungssensor
stark, und der ist auf einem Balancierer genau dann verfälscht, wenn der
Roboter beschleunigt. Ob das hier genauso ausgeht, lässt sich mit dem Umschalter
direkt nachprüfen.

## Statusanzeige

| Anzeige | Bedeutung |
|---|---|
| wandernder Punkt | balanciert — eine Wasserwaage, grün unter 1°, gelb bis 3°, darüber rot |
| blaues Blinken | Fahrbefehl, auf der jeweiligen Hälfte der Reihe |
| alles rot | `STOP` oder umgefallen |

Die `RGB`-Klasse der Vorlage ist auf genau **vier** LEDs an den Ecken
geschrieben: `setColorNew()` nimmt vier Farbtripel, `flashBlueColorFront()`
setzt davon Nummer 2 und 3. Diese Platine hat **acht in einer Reihe**, ein
„vorne" gibt es dort nicht. Die Methodennamen sind deshalb beibehalten, damit
die Aufrufstellen dieselben bleiben — die Darstellung dahinter behandelt die
Reihe als Balken, wie die älteren Projekte auf dieser Platine.

---

# Was gegenüber dem Original anders ist

## Musste anders sein

| Original | hier | warum |
|---|---|---|
| `MsTimer2` (Timer-Interrupt, 5 ms) | FreeRTOS-Task, 5 ms, Kern 1 | `balanceCar()` liest den MPU6050 über I2C. Der Wire-Treiber des ESP32 benutzt selbst Interrupts und Mutexe — aus einer ISR heraus stürzt das ab. Die Task hält denselben Takt und dieselbe Garantie, nicht vom Hauptprogramm aufgehalten zu werden. |
| `analogWrite` | LEDC, 20 kHz | Der ESP32 hat kein `analogWrite`. 20 kHz statt der rund 1 kHz des AVR, sonst pfeifen die Motoren hörbar. |
| `EnableInterrupt` | `attachInterrupt` | Die Bibliothek ist reiner AVR-Registercode. |
| `char balance_angle_min` | `int8_t` | siehe unten — der Fehler, der am längsten gekostet hat |
| Motoren gleichsinnig | Motor rechts umgekehrt | Auf dieser Platine sitzen die Motoren spiegelbildlich; ohne die Umkehr dreht er sich im Kreis. |
| Taster aktiv LOW | aktiv HIGH, 30 ms entprellt | GPIO2 ist Strapping-Pin mit externem Pulldown. |
| Balancierfenster ±22° | ±14° (`BALANCE_WINDOW`) | Die ±22° setzen voraus, dass ein umgefallenes Fahrzeug ~90° meldet. Dieser hier meldet liegend 17,7°. |
| Selbststart nach 2 s | aus (`AUTO_START`) | dieselbe Ursache |
| `Serial.begin(9600)` | 115200 | schnellere Telemetrie |
| Pinbelegung ATmega328 | `src/Pins.h` | andere Platine |

Dazu zwei Kompatibilitätsblöcke in `src/I2Cdev.cpp`, beide im Quelltext
markiert:

* `BUFFER_LENGTH` ist ein Makro der AVR-Wire-Bibliothek; auf dem ESP32 heißt es
  `I2C_BUFFER_LENGTH`, und `min()` ist dort eine Schablone, die beide Argumente
  im selben Typ braucht.
* Vor jedem `requestFrom` stand ein verwaistes `Wire.beginTransmission(devAddr)`
  ohne abschließendes `endTransmission`. Auf dem AVR folgenlos — der
  ESP32-Treiber wartet danach auf das passende `endTransmission` und **bleibt
  stehen**. Sechs Stellen betroffen.

## Kam dazu

WLAN mit Aktualisierung über Funk (`Ota.h`), Konsole über TCP (`Console.h`),
Weboberfläche (`WebUi.h`), Statusanzeige (`Rgb.h`) und das Halte-Kommando `x`.
Der ATmega328 des Originals kann davon nichts. **An der Regelung ändert das
nichts** — WLAN und Webserver laufen auf Kern 0, die Regelung in ihrer eigenen
Task auf Kern 1.

Eine Ergänzung betrifft die Regelung doch, und sie füllt eine Lücke der
Vorlage: **`kp_turn` wird dort deklariert und nirgends benutzt.** Gedämpft wird
nur die Dreh*rate* (`kd_turn * Gyro_z`) — das bremst das Verdrehen, holt den
Roboter aber nie zurück, weil nirgends steht, wo er einmal hingeschaut hat.
Hier wird `Gyro_z` zur Blickrichtung aufintegriert und mit `kp_turn`
zurückgeholt; der Wert leckt mit 30 s Zeitkonstante gegen null, sonst würde die
Integrationsdrift eine eingebildete Verdrehung real wegdrehen.

## Blieb absichtlich stehen

Damit es die Vorlage bleibt:

* **Encoder einspurig.** Gezählt wird nur eine Spur je Rad, die Richtung aus
  dem Vorzeichen des PWM *geraten*:
  ```c
  encoder_left_pulse_num_speed += pwm_left < 0 ? -encoder_count_left_a : encoder_count_left_a;
  ```
  Wird ein Rad entgegen der Ansteuerung geschoben, zählt es falsch herum. Die
  Platine könnte echte Quadratur über die PCNT-Hardware — der Zweig
  `abgestimmte-fassung` nutzt sie.
* **Gyro-Nullpunkt fest einprogrammiert:** `Gyro_x = (gx - 128.1) / 131` im
  Kalman-Filter ist der Wert *eines bestimmten* Sensorexemplars.

---

# Der Fehler, der am längsten gekostet hat

Der Roboter regelte sauber — und schaltete nach zwei Sekunden ab, bei
kerzengeradem Winkel. `STANDBY`, der eigentliche Balancierzustand, wurde nie
erreicht.

Die Ursache steht in einer einzigen Zeile der Vorlage:

```c
char balance_angle_min = -22;
```

**`char` ist auf dem AVR vorzeichenbehaftet, auf dem Xtensa des ESP32
vorzeichenlos.** Aus `−22` wird `234`. Die Prüfung `234 <= Winkel` ist für
jeden normalen Winkel falsch, also landete jeder Startversuch im `STOP`-Zweig.
Die Sturzprüfung war genauso betroffen: `Winkel < 234` ist immer wahr.

Gefunden wurde es erst, als der Abschaltgrund mitprotokolliert wurde
(`Grund: Startpruefung bei 0.18 Grad`) statt ihn aus dem Verlauf zu erraten.
Die Vermutungen davor — Tasterstörung, zu enges Fenster, zu hohe Verstärkung —
waren alle falsch.

---

# Erste Inbetriebnahme am eigenen Aufbau

Fährt er in Fallrichtung *weg* statt dagegen, ist die Wirkrichtung verkehrt.
Einen Schalter dafür gibt es im Original nicht: dann in `BalanceCar.h` die
beiden `digitalWrite(AIN1, …)` in `balanceCar()` tauschen (`0` ↔ `1`), für
`BIN1` über `MOTOR_RIGHT_INVERT` in `src/Pins.h`. **Das sieht man nur beim
Hinschauen** — aus der Telemetrie geht die Drehrichtung des Rades gegenüber der
Kipprichtung nicht hervor.

`kp_balance = 55` und `kd_balance = 0.75` stammen vom Yahboom-Fahrzeug. Auf
diesem Aufbau funktionieren sie, sind aber nicht zwingend optimal — über die
Weboberfläche lassen sie sich im Betrieb nachziehen.

## Dateien

| Datei | Inhalt |
|---|---|
| `src/main.cpp` | Zustandsmaschine, Tasten, Telemetrie |
| `src/BalanceCar.h` | die Regelung — Kern der Portierung |
| `src/KalmanFilter.*` | Winkelschätzung, unverändert übernommen |
| `src/I2Cdev.*`, `src/MPU6050.*` | Sensortreiber, zwei markierte ESP32-Anpassungen |
| `src/Pins.h` | Pinbelegung tumlerV2 |
| `src/Rgb.h` | Statusanzeige auf acht LEDs |
| `src/WebUi.h`, `src/Console.h`, `src/Ota.h` | Bedienung und Aktualisierung über WLAN |

## Sicherheit

Ab ±14° gehen die Motoren aus. Im Übergangszustand `START` ist diese Prüfung
abgeschaltet — das ist im Original so und der Grund, warum der Roboter dort
zwei Sekunden lang auch aus einer Schräglage heraus regelt.
