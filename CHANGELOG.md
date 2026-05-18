# Changelog

Alle wesentlichen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

## [0.1.0] - 2026-05-18

Initiale Version des OFM-LightManagerModuls – HCL-Engine, Stützpunkttabellen, Adaptive Helligkeit und ETS-Konfiguration wurden aus OFM-HueGatewayModule als eigenständiges Modul extrahiert.

### Hinzugefügt

- **HCL-Master-Engine** (Human Centric Lighting):
  - Tageszeit-abhängige Farbtemperatur-/Helligkeitssteuerung via Stützpunkttabellen (SP0–9) mit konfigurierbarer Interpolation
  - Astronomische HCL-Kurve (Sonnenfenster-Modus): Farbtemperatur und Helligkeit anhand von Sonnenaufgang, Sonnenuntergang und konfigurierbaren Offsets
  - Saison-Profil je Lichtmanager mit vier Modi:
    - `Standard` – immer Winter-Profil aktiv
    - `Automatisch (Sommer/Winterzeit)` – nutzt System-DST-Flag; optionaler DST-Offset-Tage-Parameter für abweichende Regionen
    - `Festes Datum` – konfigurierbares Sommerfenster (Sommerstart/Sommerende je Monat+Tag); unterstützt Jahreswechsel-Übergang (Südhalbkugel)
    - `Per Kommunikationsobjekt` – KO „Saison" schaltet Sommer/Winter zur Laufzeit
  - Sommer-Stützpunkttabellen (`_setpointsSummer`) parallel zu Winter-Tabellen; Interpolation und Bereichsberechnung nutzen automatisch das aktive Profil
  - Kanal-spezifischer HCL-Lock: Parameter, Kommunikationsobjekt und Runtime-Verhalten
  - Slew-Rate (K/min): konfigurierbare Begrenzung der Farbtemperaturänderungsgeschwindigkeit
  - Fallback-Policies und Diagnose-Ringpuffer für stabilen Dauerbetrieb

- **Adaptive Helligkeit**:
  - Modus je Lichtmanager: `Aus` / `Tageslicht-Kompensation (Open-Loop)` / `Konstantlichtregelung (Closed-Loop)`
  - KO: **Helligkeitssensor** (DPT 9.004) – Lux-Istwert-Eingang
  - KO: **Tag/Nacht** (DPT 1.001) – Aktivierungssteuerung per Tageszeit
  - KO: **Adaptive Helligkeit aktiv** (DPT 1.011) – Status-Ausgang
  - Konfigurierbar (Open-Loop): Skalierungsmaximum, Kompensationsstärke
  - Konfigurierbar (Closed-Loop): P-Faktor, Totband, Auf-HCL-Wert-begrenzen
  - Konfigurierbar (beide Modi): Mindesthelligkeit, Mindestschrittgröße, Sensor-Timeout, Aktivierungszeitraum (immer / tagsüber / nach Uhrzeit)

- **ETS-Parameter je Lichtmanager**:
  - Name / Bezeichnung, Rückfallzeit nach Sperre, Kurventyp-Auswahl
  - Stützpunkte SP0–9 und Sommer-SP0–9 mit Uhrzeit, Helligkeit und Kelvin
  - Astro-Minimum/Maximum für Helligkeit und Farbtemperatur

### Geändert / Verbessert (ETS-Darstellung)

- Kanal-Bezeichnung wird in die KO-Übersicht übernommen (ComObjectRef `Text` + `TextParameterRefId`)
- TypeTime-Parameter als 16-Bit-Minutenwert (`UIHint="Time_hhmm"`) statt 40-Bit-Textformat
- Suffix-Anzeige in ETS-Parameterlisten ergänzt:
  - Helligkeit: ` %`, Farbtemperatur: ` K` (alle Stützpunkte SP0–9, Sommer-SP0–9, Astro Min/Max, manuelle Kelvin-Eingabe)
  - Slew-Rate: ` K/min`
  - Sonnenaufgang- / Sonnenuntergang-Offset: ` min`
  - Skalierungsmaximum / Totband: ` lx`; Kompensationsstärke / Mindesthelligkeit / Mindestschrittgröße: ` %`; Sensor-Timeout: ` min`
- Bezeichnungen übersetzt: `Sunrise` → `Sonnenaufgang`, `Sunset` → `Sonnenuntergang`
- HCL-Master-Auswahl referenziert gemeinsamen `PT-LMGMasterSelect` (kein lokales Duplikat in OFM-HueGatewayModule mehr)

### Behoben

- HCL-Manager 5–8: ETS-Zuweisungen wurden zur Laufzeit nicht korrekt angewendet.
