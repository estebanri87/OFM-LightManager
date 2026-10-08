# OFM-LightManagerModule

OpenKNX Function Module für **Human Centric Lighting (HCL)** – eigenständige Sollwertquelle für Farbtemperatur und Helligkeit, nutzbar durch beliebige Consumer-Module (Hue Gateway, DALI-Gateway, MQTT-Bridge, LED-Controller u. a.).

## Status

🧪 **Beta**

### Bekannte Einschränkungen

- Adaptive Helligkeit im Closed-Loop-Modus ist noch nicht mit allen Sensor-DPTs getestet.
- Astronomische HCL-Kurve setzt eine korrekt synchronisierte Systemuhr voraus.

## Features

### HCL-Master-Engine
- Tageszeit-abhängige Farbtemperatur- und Helligkeitssteuerung via **Stützpunkttabellen** (SP0–SP9) mit konfigurierbarer Interpolation
- **Astronomische HCL-Kurve** (Sonnenfenster-Modus): Farbtemperatur und Helligkeit anhand von Sonnenaufgang, Sonnenuntergang und konfigurierbaren Offsets
- Bis zu **N unabhängige HCL-Kanäle** parallel betreibbar (Anzahl per OAM vorgegeben)

### Saison-Profile
Jeder Lichtmanager unterstützt vier Modi:

| Modus | Beschreibung |
|---|---|
| Aus | Immer Winter-Stützpunkte aktiv (kein Sommer-Modus) |
| Automatisch (Sommer/Winterzeit) | Nutzt System-DST-Flag; optionaler Offset-Parameter für abweichende Regionen |
| Festes Datum | Konfigurierbares Sommerfenster (Monat+Tag); unterstützt Jahreswechsel-Übergang |
| Per Kommunikationsobjekt | K04 schaltet Sommer/Winter zur Laufzeit; Zustand wird im Flash persistiert |

Sommer-Stützpunkttabellen (`SP0–SP9 Sommer`) parallel zu Winter-Tabellen; Interpolation nutzt automatisch das aktive Profil.

### Adaptive Helligkeit
Modus je Lichtmanager: `Aus` / `Tageslicht-Kompensation (Open-Loop)` / `Konstantlichtregelung (Closed-Loop)`

- **Open-Loop**: Helligkeit wird anhand eines Lux-Sensors skaliert (konfigurierbar: Skalierungsmaximum, Kompensationsstärke)
- **Closed-Loop**: P-Regler mit Totband; Istwert via Lux-Sensor (konfigurierbar: P-Faktor, Totband, Mindestschrittgröße)
- Aktivierungszeitraum: immer / tagsüber / nach Uhrzeit / per Kommunikationsobjekt
- Sensor-Timeout konfigurierbar

### Multi-Profil-Selektor (F7)
Bis zu **4 Profil-Slots je Kanal** mit Wochentag- / Saison- / Urlaub- / Feiertag-Maske und Default-Fallback. Der Selektor wählt automatisch das spezifischste passende Profil für den aktuellen Tag.

### HCL-Achsen & Zeitfenster (F1)
- **HCL-Achsen** (`Aus` / `Helligkeit+CT` / `Nur CT` / `Nur Helligkeit`) pro Kanal konfigurierbar
- **Zeitfenster** (`Immer` / `Nur tagsüber` / `Nur nachts`) — UND-verknüpft mit Achsen-Filter

### Vorausschau & Tages-Fortschritt (F3/F4)
- **K11** Minuten bis nächstem Stützpunkt (DPT 7.006)
- **K12/K13** Nächste Farbtemperatur / Nächste Helligkeit
- **K14** Tagesfortschritt 0–100 % (DPT 5.001)
- **K15** Tagesphase 0–5 (vor Aufgang … Nacht)

### Externe Quellen (F12)
Override oder Fallback für Helligkeit und Farbtemperatur aus KNX:
- Helligkeit per Prozent (K16, DPT 5.001) oder Lux-Skalierung (K17, DPT 9.004)
- Farbtemperatur per Kelvin (K18, DPT 7.600) oder Skalar (K19, DPT 5.001)
- Per-Stützpunkt Mix-Modus und Mix-Prozent

### Slew-Rate Tag/Nacht (F5)
Per-Kanal getrennte Slew-Raten für Tag und Nacht mit interner Astro-Quelle oder per KO.

### Sperr-System
- **Globale Sperre** (alle Kanäle), **Master-Sperre** (ein Master), **Kanal-Sperre** (ein Kanal)
- **3-Wege-Kanal-Sperre**: `Nein` / `Vollsperre` (K02) / `Getrennt` (K09 LockColor + K10 LockBrightness)
- 6 Rückfall-Strategien: Definierte Zeit / Freie Dauer / Freie Uhrzeit / Dauer oder Uhrzeit / Nur extern / Deaktiviert

### Partial-Sink-Vertrag (F11)
`ILightManagerOutput::onLightManagerPartial()` liefert eine achsenweise Gültigkeitsmaske — Consumer-Module können nicht-valide Achsen aus eigenem Cache auffüllen.

## Kommunikationsobjekte (je Kanal)

| KO | Nr. | Richtung | DPT | Funktion | Bedingung |
|---|---|---|---|---|---|
| Status Soll-Helligkeit | K00 | Ausgang | DPT 5.001 | Aktueller Helligkeitssollwert | HclAxes ≠ Nur CT |
| Status Soll-Farbtemperatur | K01 | Ausgang | DPT 7.600 | Aktueller Farbtemperatursollwert | HclAxes ≠ Nur Helligkeit |
| Sperre (kanal-spezifisch) | K02 | Eingang | DPST-1-3 | Vollsperre Ein/Aus | UseLock ≠ Nein |
| Status Sperre | K03 | Ausgang | DPST-1-11 | Status der Kanalsperre | UseLock ≠ Nein |
| SeasonInput | K04 | Eingang | DPT 1.001 | Sommer/Winter umschalten | SeasonSource = Per KO |
| Tag/Nacht | K06 | Eingang | DPT 1.001 | Tag/Nacht-Quelle für Slew / Adaptive | DayNightSource = KO |
| Status Tunable White kombiniert | K08 | Ausgang | DPST-249-600 | Helligkeit + CT + Überblendzeit | StatusKoOutput inkl. Kombiniert |
| LockColor | K09 | Eingang | DPST-1-3 | Farbtemperatur-Achse sperren | UseLock = Getrennt |
| LockBrightness | K10 | Eingang | DPST-1-3 | Helligkeits-Achse sperren | UseLock = Getrennt |
| Minuten bis nächstem SP | K11 | Ausgang | DPT 7.006 | Vorausschau: Minuten | PreviewEnable = Ja |
| Nächste Farbtemperatur | K12 | Ausgang | DPT 7.600 | Vorausschau: CT des nächsten SP | PreviewEnable = Ja |
| Nächste Helligkeit | K13 | Ausgang | DPT 5.001 | Vorausschau: Helligkeit des nächsten SP | PreviewEnable = Ja |
| Tagesfortschritt | K14 | Ausgang | DPT 5.001 | 0–100 % des Tagesverlaufs | ProgressEnable = Ja |
| Tagesphase | K15 | Ausgang | DPT 5.010 | 0=vor Aufgang … 5=Nacht | ProgressEnable = Ja |
| Externe Helligkeit (%) | K16 | Eingang | DPT 5.001 | Externe Helligkeitsquelle | ExtBrightnessDpt = Prozent |
| Externe Helligkeit (lx) | K17 | Eingang | DPT 9.004 | Externe Helligkeit, Lux-skaliert | ExtBrightnessDpt = Lux |
| Externe Farbtemperatur (K) | K18 | Eingang | DPT 7.600 | Externe CT-Quelle | ExtColorTempDpt = Kelvin |
| Externe Farbtemperatur (Skalar) | K19 | Eingang | DPT 5.001 | Externe CT, skaliert | ExtColorTempDpt = Skalar |

### Globale und Master-KOs

| KO | Richtung | DPT | Funktion |
|---|---|---|---|
| Sperre (global) | Eingang | DPST-1-3 | Alle Lichtmanager-Kanäle sperren |
| Status Sperre (global) | Ausgang | DPST-1-11 | Status der globalen Sperre |
| Entsperren Trigger | Eingang | DPT 1.001 | Alle Sperren sofort aufheben |
| Sperre LM x | Eingang | DPST-1-3 | Master-spezifische Sperre |
| Status Sperre LM x | Ausgang | DPST-1-11 | Status der Master-Sperre |
| Helligkeitssensor (Lux) | Eingang | DPT 9.004 | Lux-Istwert für Adaptive Helligkeit |
| Adaptive Helligkeit aktiv | Ausgang | DPT 1.011 | Status Adaptive Helligkeit |

## Integration in Consumer-Module

```cpp
#include <HCL/LightManagerApi.h>
```

### Wert eines Masters lesen (Pull)

```cpp
// Nur in der Kanalauswahl aktivierte Master existieren (sonst nullptr)
if (HCL::masterManager.getMaster(masterNum) == nullptr) return;
HCL::Value val = HCL::masterManager.getCurrentValue(masterNum); // masterNum: 1-based
// val.kelvin, val.brightness (0-100)
```

### Push-Updates empfangen

Klasse von `ILightManagerOutput` ableiten und registrieren. Ab 0.3.0 ist `onLightManagerPartial()` die primäre Methode — sie liefert eine achsenweise Gültigkeitsmaske:

```cpp
class MyConsumer : public ILightManagerOutput {
    void onLightManagerPartial(uint8_t masterNum, uint16_t kelvin,
                               uint8_t brightness, uint8_t validMask,
                               uint8_t fadeDuration) override {
        // validMask: Bit 0 = Kelvin gültig, Bit 1 = Brightness gültig
        // Nicht-valide Achsen aus eigenem Cache auffüllen
        if (validMask & 0x01) _lastKelvin = kelvin;
        if (validMask & 0x02) _lastBrightness = brightness;
        applyToDevice(_lastKelvin, _lastBrightness, fadeDuration);
    }
    // Fallback: onLightManagerValue() wird aufgerufen wenn onLightManagerPartial() nicht überschrieben wird
};

// Einmalig beim Setup:
LightManagerModule::instance().registerOutput(&myConsumer);
```

Push wird nur ausgelöst, wenn der Kanal **nicht** gesperrt ist. Beim Übergang gesperrt → entsperrt erfolgt ein sofortiger Push.

Weitere Details: [doc/integration.md](doc/integration.md)

## ETS-Konfiguration

- Bis zu N Lichtmanager (Anzahl im OAM vorgegeben), einzeln aktivierbar im Tab **Kanalauswahl**; je Lichtmanager Beschreibung und „Suspendiert"
- Stützpunkte SP0–SP9 (Winter + Sommer) mit Uhrzeit, Helligkeit (%) und Farbtemperatur (K)
- Astronomische Parameter: Sonnenaufgang-/Sonnenuntergang-Offset (min), Helligkeit/CT Min/Max
- Suffix-Anzeige in ETS: `%`, `K`, `K/min`, `lx`, `min`

## Development

```bash
git clone https://github.com/OpenKNX/OFM-LightManager
cd OFM-LightManager
pio run
```

## Abhängigkeiten

- [OGM-Common](https://github.com/OpenKNX/OGM-Common)

## Dokumentation

- [Applikationsbeschreibung](doc/Applikationsbeschreibung-LightManager.md)
- [Integrationsleitfaden für Consumer-Module](doc/integration.md)
- [Changelog](CHANGELOG.md)
