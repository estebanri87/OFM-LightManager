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
- Bis zu **8 unabhängige HCL-Master** parallel betreibbar

### Saison-Profile
Jeder Lichtmanager unterstützt vier Modi:

| Modus | Beschreibung |
|---|---|
| Standard | Immer Winter-Profil aktiv |
| Automatisch (Sommer/Winterzeit) | Nutzt System-DST-Flag; optionaler Offset-Parameter für abweichende Regionen |
| Festes Datum | Konfigurierbares Sommerfenster (Monat+Tag); unterstützt Jahreswechsel-Übergang |
| Per Kommunikationsobjekt | KO „Saison" schaltet Sommer/Winter zur Laufzeit |

Sommer-Stützpunkttabellen (`SP0–SP9 Sommer`) parallel zu Winter-Tabellen; Interpolation nutzt automatisch das aktive Profil.

### Adaptive Helligkeit
Modus je Lichtmanager: `Aus` / `Tageslicht-Kompensation (Open-Loop)` / `Konstantlichtregelung (Closed-Loop)`

- **Open-Loop**: Helligkeit wird anhand eines Lux-Sensors skaliert (konfigurierbar: Skalierungsmaximum, Kompensationsstärke)
- **Closed-Loop**: P-Regler mit Totband; Istwert via Lux-Sensor (konfigurierbar: P-Faktor, Totband, Mindestschrittgröße)
- Aktivierungszeitraum: immer / tagsüber / nach Uhrzeit / per Kommunikationsobjekt
- Sensor-Timeout konfigurierbar

### Manager-spezifischer Lock
- Sperre per ETS-Parameter, Kommunikationsobjekt oder Programm-API
- Konfigurierbarer Rückfall nach Sperrdauer
- Slew-Rate (K/min) begrenzt die Farbtemperaturänderungsgeschwindigkeit

## Kommunikationsobjekte (je Master)

| KO | Richtung | DPT | Funktion |
|---|---|---|---|
| HCL-Sperre | Eingang | DPST-1-3 | HCL-Lock aktivieren/deaktivieren |
| HCL-Sperre Status | Ausgang | DPST-1-11 | Status der Sperre |
| Saison | Eingang | DPT 1.001 | Sommer/Winter umschalten (Modus „Per KO") |
| Helligkeitssensor | Eingang | DPT 9.004 | Lux-Istwert für Adaptive Helligkeit |
| Tag/Nacht | Eingang | DPT 1.001 | Aktivierungssteuerung Adaptive Helligkeit |
| Adaptive Helligkeit aktiv | Ausgang | DPT 1.011 | Status Adaptive Helligkeit |
| Status Soll-Helligkeit | Ausgang | DPT 5.001 | Aktueller HCL-Helligkeitssollwert |
| Status Soll-Farbtemperatur | Ausgang | DPT 7.600 | Aktueller HCL-Farbtemperatursollwert |

## Integration in Consumer-Module

```cpp
#include <HCL/LightManagerApi.h>
```

### Wert eines Masters lesen (Pull)

```cpp
uint8_t count = HCL::masterManager.getMasterCount();
HCL::Value val = HCL::masterManager.getCurrentValue(masterNum); // masterNum: 1-based
// val.kelvin, val.brightness (0-100)
```

### Push-Updates empfangen

Klasse von `ILightManagerOutput` ableiten und registrieren:

```cpp
class MyConsumer : public ILightManagerOutput {
    void onLightManagerValue(uint8_t masterNum, uint16_t kelvin,
                             uint8_t brightness, uint8_t fadeDuration) override {
        // Wert anwenden
    }
};

// Einmalig beim Setup:
LightManagerModule::instance().registerOutput(&myConsumer);
```

Push wird nur ausgelöst, wenn der Master **nicht** gesperrt ist. Beim Übergang gesperrt → entsperrt erfolgt ein sofortiger Push.

Weitere Details: [doc/integration.md](doc/integration.md)

## ETS-Konfiguration

- Bis zu 16 Lichtmanager konfigurierbar
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
