# Applikationsbeschreibung OFM-LightManager

OpenKNX Function-Module zur tageszeitabhängigen Steuerung von Helligkeit und Farbtemperatur (Human Centric Lighting).
Bis zu 16 unabhängige Lichtmanager liefern Sollwerte, die von kompatiblen Ausgabemodulen (z. B. OFM-HueGatewayModule) konsumiert werden.

<!-- DOC HelpContext="Allgemein" -->
### Allgemein

(c) OpenKNX, Steffen Rittmeier 2026

Die vollständige Projektdokumentation ist unter https://github.com/OpenKNX/OFM-LightManager verfügbar.

Das Modul übernimmt die Berechnung der HCL-Sollwerte:

`Stützpunkte / Astro / Sensor` → `OFM-LightManager` → `Status-KOs / Konsumenten-Modul`

**Wichtig:**
- Das Modul stellt nur Sollwerte bereit. Die eigentliche Ausgabe an Leuchten erfolgt in den jeweiligen Ziel-Modulen oder GA´s).
- Parameter und KOs müssen in ETS konsistent projektiert werden.
- Logikfunktionen (Saison-Logik, Zentralfunktionen) können in dedizierten Logikmodulen umgesetzt werden.
<!-- DOCEND -->

## Inhaltsverzeichnis

- [Human Centric Lighting](#human-centric-lighting)
- [Lichtmanager Auswahl](#lichtmanager-auswahl)
- [Einstellungen](#einstellungen)
- [Sperre (global)](#sperre-global)
- [Rückfallstrategie nach Sperre](#rückfallstrategie-nach-sperre)
- [Status-KOs je Lichtmanager](#status-kos-je-lichtmanager)
- [Lichtmanager 1..16](#lichtmanager-116)
- [Saison-Profil](#saison-profil)
- [Adaptive Helligkeit](#adaptive-helligkeit)
- [Kommunikationsobjekte](#kommunikationsobjekte)
- [Häufige Fehler und Lösungen](#häufige-fehler-und-lösungen)

<!-- DOC -->
## Human Centric Lighting

Aktiviert zeitabhängige Sollwerte für Helligkeit und Farbtemperatur.
Bis zu 16 Lichtmanager können parallel definiert werden.

<!-- DOC HelpContext="Lichtmanager" -->
### Lichtmanager

Bis zu 16 unabhängige Lichtmanager berechnen Helligkeits- und Farbtemperatur-Sollwerte über den Tagesverlauf.
Jeder Lichtmanager besitzt eine eigene Kurvenkonfiguration, optionale Saison-Profile und optionale adaptive Helligkeitsregelung.
<!-- DOCEND -->

<!-- DOC HelpContext="Lichtmanager-Auswahl" -->
### Lichtmanager Auswahl

Legt die Anzahl sichtbarer Lichtmanager-Seiten (1..16) fest.
Nur die hier aktivierten Lichtmanager werden als eigene ETS-Reiter eingeblendet.
<!-- DOCEND -->

<!-- DOC -->
### Einstellungen

- **Aktualisierungsintervall (Sekunden)**
- **Überblendzeit (Sekunden)**

Sollwerte werden aus der Lichtmanager-Kurve berechnet und bei Wertänderung als Status-KO übertragen.

<!-- DOC -->
### Sperre (global)

Sperrt die automatische Ausgabe aller Lichtmanager.

### Sperr-Hierarchie

Die Sperren werden mit folgender Priorität ausgewertet:

```text
Kanal-Sperre
	> Manager-Sperre
		> Globale Sperre
```

Eine aktive Kanal-Sperre übersteuert also immer die managerbezogene und die globale Sperre. Das ist gewollt, damit einzelne Kanäle nach einer Szene oder einem Sonderbetrieb gezielt aus der HCL-Führung herausgenommen werden können, ohne andere Kanäle desselben Lichtmanagers zu beeinflussen.

<!-- DOC HelpContext="HCL-Sperre-global" -->
Sperrt die automatische Ausgabe aller Lichtmanager (HCL-Bereich).

Optionen:
- **Rückfallzeit nach Sperre** (inkl. Tageswechsel, `kein Rückfall` möglich)
- **Rückfallstrategie nach Sperre**: wirkt für globale, manager-spezifische und kanal-spezifische Sperren

KOs:
- `Sperre (global)` (Eingang)
- `Status Sperre` (Ausgang)
<!-- DOCEND -->

<!-- DOC HelpContext="HCL-Sperre-global-Status-HCL-Sperre" -->
Globale HCL-Sperre inkl. Statusrückmeldung.

- KO `Sperre (global)` (Eingang): Setzt die globale HCL-Sperre für alle Lichtmanager.
- KO `Status Sperre` (Ausgang): Gibt den aktuellen HCL-Sperrstatus zurück.
<!-- DOCEND -->

<!-- DOC -->
### Rückfallstrategie nach Sperre

Zusätzlich zur Rückfallzeit gibt es eine zentrale Strategie, wie Sperren wieder aufgehoben werden.
Diese Vorgabe gilt für globale, manager-spezifische und kanal-spezifische Sperren.

Verfügbare Strategien:
1. **Definierte Rückfallzeit**: verwendet ausschließlich die gewählte Rückfallzeit aus der Dropdown-Liste.
2. **Freie Dauer**: verwendet den Parameter **Freie Rückfalldauer** in Sekunden.
3. **Freie Uhrzeit**: verwendet den Parameter **Rückfall-Uhrzeit (HH:MM)**.
4. **Dauer oder Uhrzeit**: hebt die Sperre auf, sobald entweder die freie Dauer abgelaufen ist oder die Rückfall-Uhrzeit erreicht wird.
5. **Nur externes Entsperren**: es erfolgt keine automatische Freigabe; die Sperre muss über ein KO aufgehoben werden.

Ergänzende Parameter:
- **Freie Rückfalldauer**: Bereich `0..65535 s`, Standard `1800 s`
- **Rückfall-Uhrzeit (HH:MM)**: Standard `03:00`

Hinweis:
- Für das externe Entsperren steht zusätzlich das globale KO `Entsperren Trigger` zur Verfügung.

<!-- DOC -->
### Status-KOs je Lichtmanager

Aktiviert pro Manager die Ausgabe:
- `Status Helligkeit Soll` (`1 Byte`, `0..100 %`)
- `Status Farbtemperatur Soll` (`2 Byte`, `2000..6500 K`)

Hinweise:
- Die Ausgabe erfolgt zyklisch gemäß **Aktualisierungsintervall** des Lichtmanager-Bereichs.
- Die KOs liefern die vom Lichtmanager berechneten Sollwerte, unabhängig davon, wie viele Konsumenten diesem zugeordnet sind.

<!-- DOC -->
### Lichtmanager 1..16

<!-- DOC HelpContext="HCL-Manager-18" -->
Jeder Lichtmanager 1..16 besitzt identischen Aufbau (HCL-Konfiguration):

- **Bezeichnung**: Freie ETS-Bezeichnung des Lichtmanagers.
- **Lichtmanager Sperre (spezifisch)**: Sperrt nur den jeweiligen Manager.
- **Erweiterte Kurve**: Kurventyp `FixedTime`, `SunPosition`, `Manual` oder `Astronomischer Sonnenstand`.
- **Stützpunkte**: Bis zu 10 Stützpunkte je Manager (bei `FixedTime` oder `SunPosition`).
- **Saison-Profil**: Optionale Sommer-/Winter-Stützpunkte (Modus `Standard`, `Auto-DST`, `Festes Datum` oder `Per Objekt`).
- **Adaptive Helligkeit**: Optionale Tageslicht-Kompensation (Open-Loop) oder Konstantlichtregelung (Closed-Loop) per Helligkeitssensor.
<!-- DOCEND -->

Jeder Manager besitzt identischen Aufbau:

#### Bezeichnung
Freie ETS-Bezeichnung des Lichtmanagers.

#### Lichtmanager Sperre (spezifisch)
Sperrt nur den jeweiligen Lichtmanager.
Alle Konsumenten-Kanäle, die diesem Lichtmanager zugeordnet sind, erhalten während der Sperre keine automatischen Sollwerte mehr.

Optionen je Manager:
- **Rückfallzeit nach Sperre**
- **Rückfallstrategie nach Sperre**: zentrale Vorgabe, siehe Abschnitt [Rückfallstrategie nach Sperre](#rückfallstrategie-nach-sperre)

KOs je Lichtmanager:
- `Sperre Lichtmanager x` (Eingang)
- `Status Sperre Lichtmanager x` (Ausgang)

#### Erweiterte Kurve
Kurventyp:
- **FixedTime**
- **SunPosition**
- **Manual**
- **Astronomischer Sonnenstand**

Erweiterte Parameter je Manager:
- **Slew-Rate (K/min)**: begrenzt die Kelvin-Änderung pro Minute (`0` = keine Begrenzung).
- **Manuelle Farbtemperatur**: fixer Kelvin-Sollwert bei Kurventyp `Manual` (Bereich `2000..6500 K`).
- **Sonnenaufgang/Sonnenuntergang** und **Offsets (min)**: relevant für Kurventyp `SunPosition`.
- **Astro Min/Max Kelvin** und **Astro Min/Max Helligkeit**: relevant für Kurventyp `Astronomischer Sonnenstand`.

Kurventypen im Detail:
1. **FixedTime**
	Lineare Interpolation zwischen klassischen Stützpunkten aus Zeit, Helligkeit und Farbtemperatur.
2. **SunPosition**
	Nutzt ebenfalls Stützpunkte, richtet die Tagesform aber an Sonnenaufgang und Sonnenuntergang mit konfigurierbaren Offsets aus.
	Die Minimal- und Maximalwerte werden aus den gesetzten Stützpunkten abgeleitet.
3. **Manual**
	Verwendet eine feste Farbtemperatur aus dem Parameter **Manuelle Farbtemperatur**.
	Optionale Stützpunkte beeinflussen in diesem Modus nur den Helligkeitsverlauf.
4. **Astronomischer Sonnenstand**
	Verwendet keine Stützpunkte.
	Helligkeit und Farbtemperatur werden direkt aus dem Sonnenstand berechnet und zwischen den Astro-Min-/Max-Werten skaliert.
	Grundlage sind die OpenKNX-Basisparameter für Standort und Zeitzone.

#### Stützpunkte
Bis zu 10 Stützpunkte je Manager bei Kurventyp `FixedTime` oder `SunPosition`.

Hinweise:
- Bei `FixedTime` und `SunPosition` sind mindestens 2 gültige Zeit-Stützpunkte erforderlich.
- Nicht alle 10 Stützpunkte müssen belegt werden; unbenutzte Einträge werden ignoriert.
- Bei `Manual` sind Stützpunkte optional; wenn sie gesetzt werden, definieren sie Zeit + Helligkeit, die Farbtemperatur kommt aus dem Parameter **Manuelle Farbtemperatur**.
- Bei `Manual` ohne Stützpunkte bleibt die Helligkeit konstant auf `100 %`, die Farbtemperatur auf dem konfigurierten manuellen Kelvin-Wert.
- Bei `Astronomischer Sonnenstand` werden keine Stützpunkte verwendet; stattdessen werden Minimal- und Maximalwerte für Kelvin und Helligkeit genutzt.
- Die letzte Zeit eines Tages gilt bis zum ersten Stützpunkt des nächsten Tages.

Beispiel:
- SP1 `06:00 / 3000K / 30%`
- SP2 `12:00 / 5000K / 90%`
- SP3 `20:00 / 2400K / 35%`

Praxisregel:
- `Aktualisierungsintervall`, `Überblendzeit` und `Slew-Rate` gemeinsam abstimmen, damit Übergänge ruhig bleiben.

### Saison-Profil

Das Saison-Profil ermöglicht es, für jeden Lichtmanager zwei voneinander unabhängige Stützpunkt-Sätze zu hinterlegen: einen für **Sommer** und einen für **Winter**. Der Lichtmanager wechselt automatisch oder auf KNX-Befehl zwischen den beiden Profilen.

**Hintergrund**: Im Sommer steht die Sonne bei Sonnenuntergang (z. B. 19:00 Uhr) noch hoch, der Himmel ist hell und das Auge nimmt warmes Licht als „zu gelb" wahr. Im Winter hingegen ist die Dämmerung um dieselbe Uhrzeit längst abgeschlossen — warmes Licht fühlt sich natürlicher an. Mit dem Saison-Profil können beide Situationen optimal parametriert werden, ohne zwei separate Lichtmanager anlegen zu müssen.

#### Saison-Modus

Der Parameter **Saison-Modus** legt fest, wie der Wechsel zwischen Sommer- und Winter-Stützpunkten ausgelöst wird:

| Modus | Beschreibung |
|---|---|
| **Standard** | Immer Winter-Stützpunkte aktiv. Sommer-Stützpunkte werden ignoriert. |
| **Auto-DST** | Automatischer Wechsel anhand der mitteleuropäischen Sommerzeit (MESZ). Sommer = letzter Sonntag März bis letzter Sonntag Oktober. Kein ETS-Eingriff nötig. |
| **Festes Datum** | Sommer gilt zwischen zwei konfigurierbaren Daten (Tag+Monat). Ermöglicht individuelle Anpassung an lokale Verhältnisse oder persönliche Präferenzen. |
| **Per Objekt** | Das KNX-Kommunikationsobjekt **LM x: Sommer aktiv** steuert den Wechsel. Der Zustand wird im Flash persistiert und bleibt nach Neuprogrammierung erhalten. |

Bei Modus **Standard** sind keine Sommer-Stützpunkte erforderlich; die Spalten werden in ETS ausgeblendet.

#### Parameter bei Modus „Festes Datum"

- **Sommer Start (Tag)** / **Sommer Start (Monat)**: Beginn des Sommerprofils (inklusiv).
- **Sommer Ende (Tag)** / **Sommer Ende (Monat)**: Ende des Sommerprofils (inklusiv).
- **DST-Offset (Tage)**: Optionaler Vorlauf/Nachlauf in Tagen (Bereich `-30..+30`, Standard `0`).

Beispiel: Start `01.04.`, Ende `31.10.` entspricht grob der MESZ — identisch mit Auto-DST, aber manuell justierbar.

#### Sommer-Stützpunkte

Bei aktivem Saison-Modus (nicht `Standard`) erscheint in ETS für jeden Stützpunkt eine zweite Spalte:

| Spalte | Beschreibung |
|---|---|
| **Sommer Aktiv** | Checkbox: Stützpunkt im Sommer-Profil verwenden. Inaktive Stützpunkte werden ignoriert. |
| **Sommer Kelvin** | Farbtemperatur für diesen Stützpunkt im Sommer (2000–6500 K). |
| **Sommer Helligkeit** | Helligkeit für diesen Stützpunkt im Sommer (0–100 %). |

Zeit und Sichtbarkeit des Stützpunkts bleiben für beide Profile gleich — nur Kelvin und Helligkeit werden saisonal überschrieben.

Hinweise:
- Nicht alle Stützpunkte müssen einen Sommer-Wert haben. Stützpunkte mit deaktiviertem **Sommer Aktiv** werden im Sommer-Profil übersprungen.
- Im Sommer-Profil müssen mindestens 2 aktive Stützpunkte vorhanden sein (bei Kurventyp `FixedTime` oder `SunPosition`), sonst fällt der Manager auf das Winter-Profil zurück.

#### KO: LM x: Sommer aktiv (nur Modus „Per Objekt")

<!-- DOC HelpContext="HCL-Saison-KO" -->
Kommunikationsobjekt **LM x: Sommer aktiv** (Eingang, 1 Bit, DPT 1.001).

Nur sichtbar wenn der Saison-Modus des Lichtmanagers auf **Per Objekt** eingestellt ist.

- Wert `1` = Sommer-Stützpunkte aktiv
- Wert `0` = Winter-Stützpunkte aktiv (Standard)

Der zuletzt empfangene Zustand wird im Flash gespeichert und nach einem Neustart bzw. nach einer Neuprogrammierung automatisch wiederhergestellt.

Bei den anderen Saison-Modi (Standard, Auto-DST, Festes Datum) wechselt der Manager automatisch; dieses KO ist dann nicht sichtbar.
<!-- DOCEND -->

### Adaptive Helligkeit

<!-- DOC HelpContext="HCL-Adaptive-Helligkeit" -->
Passt die Soll-Helligkeit automatisch an das Umgebungslicht an. Voraussetzung ist ein Helligkeitssensor, der seinen Messwert per KO sendet.

Verfügbare Modi:
- **Aus**: Keine adaptive Regelung — Lichtmanager arbeitet nur nach HCL-Kurve.
- **Tageslicht-Kompensation (Open-Loop)**: Misst das aktuelle Umgebungslicht und zieht es vom HCL-Sollwert ab. Je heller es draußen ist, desto weniger Kunstlicht wird eingeschaltet. Einfach und stabil, empfohlen für die meisten Räume.
- **Konstantlichtregelung (Closed-Loop)**: Vergleicht den Sensorwert fortlaufend mit dem HCL-Sollwert und korrigiert die Leuchten laufend nach bis beide übereinstimmen. Geeignet wenn eine sehr genaue Beleuchtungsstärke erforderlich ist (z. B. Arbeitsplatz nach DIN EN 12464).
<!-- DOCEND -->

#### Adaptive Helligkeit (Modus)

<!-- DOC HelpContext="HCL-Adaptive-Modus" -->
Wählt den Betriebsmodus der adaptiven Helligkeitsregelung:

- **Deaktiviert**: Keine adaptive Regelung — Lichtmanager arbeitet nur nach HCL-Kurve.
- **Tageslicht-Kompensation (Open-Loop)**: Der Sensor misst das aktuelle Umgebungslicht (Lux). Dieses wird vom HCL-Sollwert abgezogen: viel Tageslicht → Kunstlicht wird reduziert, wenig Tageslicht → Kunstlicht bleibt hoch. Es gibt keine Rückkopplung — der berechnete Wert wird direkt ausgegeben, ohne zu prüfen ob das Ergebnis wirklich stimmt. Das macht den Modus einfach, stabil und für die meisten Räume ausreichend.
- **Konstantlichtregelung (Closed-Loop)**: Der Regler vergleicht den aktuellen Sensorwert laufend mit dem HCL-Sollwert und passt die Leuchten so lange nach, bis beide übereinstimmen. Im Gegensatz zu Open-Loop wird also nicht einmalig berechnet sondern fortlaufend korrigiert. Das ergibt eine präzisere Regelung, erfordert aber sorgfältige Parametrierung (Kp, Totband) damit der Regler nicht schwingt. Empfohlen für Bereiche mit genauer Beleuchtungsanforderung.
<!-- DOCEND -->

#### Aktivierung

<!-- DOC HelpContext="HCL-Adaptive-Aktivierung" -->
Steuert, wann die adaptive Regelung aktiv ist:

- **Immer aktiv**: Regelung läuft unabhängig von Tageszeit.
- **Nur tagsüber (per KO)**: Regelung ist nur aktiv, wenn das KO „Tag/Nacht" den Tageswert meldet. Polarität konfigurierbar mit „Tag/Nacht-Polarität".
- **Nach Uhrzeit**: Regelung ist nur innerhalb des konfigurierten Zeitfensters aktiv (Startzeit / Endzeit).
<!-- DOCEND -->

#### Tag/Nacht-Polarität

<!-- DOC HelpContext="HCL-Adaptive-Polaritaet" -->
Legt fest, welcher KO-Wert „Tag" bedeutet. Nur sichtbar bei Aktivierung = „Nur tagsüber (per KO)".

- **1 = Tag, 0 = Nacht** (Standard): KO-Wert `1` aktiviert die Regelung.
- **0 = Tag, 1 = Nacht**: KO-Wert `0` aktiviert die Regelung.

Passend zur Polarität des sendenden Gerätes einstellen (z. B. Präsenzmelder, Zeitschaltuhr, Logikbaustein).
<!-- DOCEND -->

#### Startzeit / Endzeit

<!-- DOC HelpContext="HCL-Adaptive-Zeitfenster" -->
Definiert das Zeitfenster, in dem die adaptive Regelung aktiv ist. Nur sichtbar bei Aktivierung = „Nach Uhrzeit".

- **Startzeit**: Beginn der aktiven Phase (HH:MM).
- **Endzeit**: Ende der aktiven Phase (HH:MM).

Außerhalb des Zeitfensters ist die adaptive Regelung pausiert; der Lichtmanager folgt nur der HCL-Kurve.
<!-- DOCEND -->

#### Skalierungsmaximum

<!-- DOC HelpContext="HCL-Adaptive-Skalierungsmaximum" -->
Gibt den maximalen Lux-Wert des Sensors an, bei dem die Regelung vollständig ausgesteuert ist. Dieser Parameter muss auf den tatsächlichen Messbereich des verwendeten Sensors abgestimmt werden — er ist kein Raumtyp-Richtwert, sondern ein Sensor-Kennwert.

**Wie der Wert verwendet wird:**

- *Open-Loop*: Bei Sensorwert ≥ Skalierungsmaximum wird die Helligkeit maximal reduziert (auf Mindesthelligkeit). Dazwischen wird linear skaliert.
- *Closed-Loop*: Das Skalierungsmaximum rechnet den HCL-Sollwert (%) in einen Lux-Zielwert um: `Ziel-Lux = HCL-Sollwert% × Skalierungsmaximum`. Der Regler arbeitet auf diesen Zielwert hin.

**Beispiel mit einem Innensensor (max. 1000 Lux):**

Skalierungsmaximum = 1000 Lux, HCL-Sollwert = 80 %, Sensorwert = 500 Lux, Stärke = 80 %:
- Open-Loop: `80% × (1 − 500/1000 × 0,8) = 80% × 0,6 = 48%`
- Closed-Loop: Ziel = 800 Lux, Ist = 500 Lux → Regler erhöht die Helligkeit

**Beispiel mit einem Außensensor (max. 50.000 Lux):**

Wird ein Außensensor verwendet, muss das Skalierungsmaximum entsprechend hoch eingestellt werden.
Skalierungsmaximum = 1000 Lux, Sensorwert = 50.000 Lux → Sensorwert weit über Maximum → Helligkeit fällt sofort auf Mindesthelligkeit, völlig unabhängig vom HCL-Profil. Das ist in diesem Fall falsch.
Korrekt wäre Skalierungsmaximum = 50.000 Lux, damit der Sensor seinen vollen Bereich nutzt.

**Orientierungswerte je Sensortyp:**

- Einfacher Innensensor (bis 1.000 Lux): 500–1.000 Lux
- Hochwertiger Innensensor (bis 10.000 Lux): 1.000–5.000 Lux
- Außensensor / Dachsensor (bis 100.000 Lux): 20.000–65.000 Lux
<!-- DOCEND -->

#### Kompensationsstärke

<!-- DOC HelpContext="HCL-Adaptive-Staerke" -->
*(Nur bei Tageslicht-Kompensation, Open-Loop)*

Prozentualer Anteil, mit dem die Helligkeitsreduktion auf den HCL-Sollwert angewendet wird.

- **100 %**: Volle Kompensation — bei Skalierungsmaximum wird auf die Mindesthelligkeit abgesenkt.
- **50 %**: Halbe Kompensation — sanfterer Eingriff ins HCL-Profil.

Bei 0 % ist die Regelung wirkungslos; in dem Fall stattdessen Modus „Aus" verwenden.
<!-- DOCEND -->

#### Auf HCL-Wert begrenzen

<!-- DOC HelpContext="HCL-Adaptive-CeilToHCL" -->
*(Nur bei Konstantlichtregelung, Closed-Loop)*

- **Ja**: Die geregelte Helligkeit überschreitet nie den aktuellen HCL-Sollwert. Tageslicht „ersetzt" das Kunstlicht, die Leuchte wird nie heller als der HCL-Sollwert.
- **Nein**: Regelung kann auch über den HCL-Sollwert hinausgehen (z. B. für reine Lux-Regelung ohne HCL-Bezug).

Empfehlung: **Ja**, um ungewolltes Aufhellen bei schlechten Lichtverhältnissen zu verhindern.
<!-- DOCEND -->

#### P-Faktor (Kp)

<!-- DOC HelpContext="HCL-Adaptive-Kp" -->
*(Nur bei Konstantlichtregelung, Closed-Loop)*

Proportionalverstärkung des Reglers. Höhere Werte reagieren schneller, können aber bei trägen Sensoren instabil werden:

- **0.5**: Langsame, sehr stabile Regelung
- **1.0**: Standardwert, ausgewogen
- **1.5**: Schnellere Reaktion
- **2.0**: Aggressiv, nur bei stabilen Sensoren empfohlen

Empfehlung: Mit `1.0` beginnen und bei Bedarf anpassen.
<!-- DOCEND -->

#### Totband

<!-- DOC HelpContext="HCL-Adaptive-Totband" -->
*(Nur bei Konstantlichtregelung, Closed-Loop)*

Minimale Abweichung in Lux zwischen Soll- und Istwert, ab der der Regler eingreift. Verhindert ständiges Nachregeln bei kleinen Schwankungen des Sensors.

Empfehlung: 30–80 Lux je nach Sensorgenauigkeit. Bei sehr empfindlichen Sensoren eher höher wählen.
<!-- DOCEND -->

#### Mindesthelligkeit

<!-- DOC HelpContext="HCL-Adaptive-Mindesthelligkeit" -->
Untergrenze der Helligkeitsreduktion durch die adaptive Regelung. Die Helligkeit wird nie unter diesen Wert gesenkt, auch wenn das Umgebungslicht das Skalierungsmaximum überschreitet.

Verhindert, dass der Raum bei sehr hellem Tageslicht vollständig dunkel geregelt wird.
<!-- DOCEND -->

#### Sensor-Timeout (0=aus)

<!-- DOC HelpContext="HCL-Adaptive-Timeout" -->
Maximale Zeit in Minuten ohne neuen Lux-Messwert, bevor der Sensor als ausgefallen gilt und die adaptive Regelung pausiert wird. Nach Ablauf folgt der Lichtmanager wieder nur der HCL-Kurve.

- `0` = kein Timeout (nicht empfohlen — bei Sensorausfall bleibt die letzte Reduktion dauerhaft aktiv).
- Empfehlung: 5–15 Minuten, abhängig vom Sendeintervall des Sensors.
<!-- DOCEND -->

#### Mindestschrittgröße

<!-- DOC HelpContext="HCL-Adaptive-Mindestschritt" -->
Minimale Helligkeitsänderung in Prozent, die der Regler tatsächlich ausführen muss. Berechnete Korrekturen unterhalb dieses Schwellwerts werden ignoriert.

Verhindert Flackern bei sehr kleinen, rauschbedingten Korrekturen. Empfehlung: 1–3 %.
<!-- DOCEND -->

#### Kommunikationsobjekte der adaptiven Regelung

<!-- DOC HelpContext="HCL-Adaptive-KOs" -->
Kommunikationsobjekte für die adaptive Helligkeit je Lichtmanager:

- **Helligkeitssensor (Lux)** (Eingang, DPT 9.004): Umgebungslichtstärke vom Sensor
- **Tag/Nacht** (Eingang, DPT 1.001): Aktivierungssignal (Polarität konfigurierbar)
- **Adaptive Helligkeit aktiv** (Ausgang, DPT 1.011): Status: Regelung aktuell aktiv

Das KO „Tag/Nacht" ist nur sichtbar, wenn Aktivierung = „Nur tagsüber (per KO)".
Das KO „Adaptive Helligkeit aktiv" meldet, ob die Regelung gerade eingreift (z. B. für Logiken oder Visualisierung).
<!-- DOCEND -->

<!-- DOC -->
## Kommunikationsobjekte

### Globale Kommunikationsobjekte

#### Sperre (global) / Status Sperre
Globale Sperre inkl. Statusrückmeldung.

#### Entsperren Trigger
1-Bit Triggerobjekt zum gleichzeitigen Aufheben aller globalen, manager-spezifischen und kanal-spezifischen Sperren.

#### Sperre Lichtmanager 1..16 / Status
Lichtmanager-spezifische Sperrobjekte inkl. Statusrückmeldung.
Sichtbarkeit abhängig von der konfigurierten Anzahl Lichtmanager.

#### Lichtmanager Status Helligkeit Soll / Farbtemperatur Soll
Je Lichtmanager zwei Sollwert-KOs; Sichtbarkeit abhängig von Option **Status-KOs je Lichtmanager**.

### Pro-Lichtmanager Kommunikationsobjekte

#### LM x: Sommer aktiv
1-Bit Eingang (DPT 1.001). Nur sichtbar bei Saison-Modus **Per Objekt**. Zustand wird im Flash persistiert.

#### Helligkeitssensor (Lux)
2-Byte Eingang (DPT 9.004). Eingang für die adaptive Helligkeitsregelung.

#### Tag/Nacht
1-Bit Eingang (DPT 1.001). Nur sichtbar bei Aktivierung der adaptiven Regelung = „Nur tagsüber (per KO)".

#### Adaptive Helligkeit aktiv
1-Bit Ausgang (DPT 1.011). Status der adaptiven Regelung.

<!-- DOC -->
## Häufige Fehler und Lösungen

### Lichtmanager-Parameter oder HCL-KOs fehlen
- Lichtmanager global aktiviert?
- Anzahl Lichtmanager in **Lichtmanager Auswahl** ausreichend?
- Erst nach Aktivierung des globalen Lichtmanagers werden Zuordnung und Sperrparameter sichtbar.

### Lichtmanager wirkt nicht
- Lichtmanager global aktiviert?
- Manager im Konsumenten-Modul zugewiesen?
- Bei `FixedTime`/`SunPosition`: mind. 2 gültige Stützpunkte?
- Bei `Manual`: gewünschte manuelle Farbtemperatur gesetzt und optionaler Helligkeitsverlauf passend parametriert?
- Bei `Astronomischer Sonnenstand`: sinnvolle Astro-Min/Max-Werte gesetzt?
- Globale/spezifische Sperre aktiv?

### Saison-Profil schaltet nicht um
- Saison-Modus ist `Standard`? → dann sind Sommer-Stützpunkte absichtlich deaktiviert.
- Bei Modus `Festes Datum`: Start- und Ende-Datum korrekt eingetragen? Datum liegt im aktiven Bereich?
- Bei Modus `Auto-DST`: Systemzeit korrekt? DST-Erkennung setzt korrekte Uhrzeit voraus.
- Bei Modus `Per Objekt`: KO `LM x: Sommer aktiv` mit GA verbunden und Wert `1` gesendet?
- Im Sommer-Profil mindestens 2 Stützpunkte mit **Sommer Aktiv = Ja** vorhanden (bei `FixedTime`/`SunPosition`)?

<!-- DOC -->
## Lizenz und Haftung

Open-Source-Modul im OpenKNX-Umfeld.
Keine Gewährleistung; Nutzung in eigener Verantwortung.
