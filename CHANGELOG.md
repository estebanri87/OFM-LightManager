# Changelog

Alle wesentlichen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

## Geplantes folgendes Release [0.4.0]

> **Breaking (Kanalauswahl):** Die Lichtmanager werden nicht mehr über „Verfügbare Kanäle" freigeschaltet, sondern einzeln im neuen Tab **Kanalauswahl**. Nach dem ETS-Update sind alle Lichtmanager deaktiviert und müssen dort wieder aktiviert werden; die Einstellungen der Kanäle bleiben erhalten.

> **Breaking (API):** `getMasterCount()` / `providerMasterCount()` liefern jetzt die höchste mögliche Master-Nummer (16) statt der Anzahl. Nicht aktivierte oder suspendierte Lichtmanager existieren zur Laufzeit nicht: `channel(n)` bzw. `getMaster(n)` liefern `nullptr`. Consumer-Module müssen die Zuordnung darüber prüfen (OFM-HueGatewayModule ab 0.8.0).

* feat(ets): Kanalauswahl nach OpenKNX-Standard — eigener Tab „Kanalauswahl" unter „Allgemein" mit einer Zeile je Lichtmanager (Kanal, Kanalaktivität Deaktiviert/Aktiviert, Beschreibung). Nur aktivierte Lichtmanager erscheinen im Baum. Der Regler „Verfügbare Kanäle" entfällt (bleibt als versteckte Konstante erhalten).
* feat(ets): Neuer Parameter „Suspendiert" im Kanalkopf — der Lichtmanager bleibt projektiert, wird aber nicht ausgeführt; im Baum wird er markiert.
* feat(ets): Der Freitext „Name Lichtmanager" heißt jetzt „Beschreibung" (Parameter und Speicher unverändert).
* feat(channel): Die Firmware legt nur aktivierte, nicht suspendierte Lichtmanager an; neue Methoden `channelConfigured()` und `activeChannelCount()`.
* fix(channel): always allow bus output in Extern integration mode regardless of BusStatusEnable
* fix(ets): Im globalen Block „Lichtmanager Sperre (global)" waren die Sichtbarkeitsregeln der Rückfallstrategie gegenüber dem Enum um eins verschoben — „Freie Uhrzeit" blendete das Sekundenfeld (`LMGHCLFallbackDurationSec`) statt des Uhrzeit-Pickers ein, „Dauer oder Uhrzeit" zeigte keine Dauer, für „Freie Dauer" fehlte die Regel ganz und „Rückfallzeit nach Sperre" (`LMGHCLLockFallback`) stand außerhalb des `choose` und war dadurch immer sichtbar. Der Block ist jetzt deckungsgleich mit dem korrekten Kanal-Block. Die Firmware-Auswertung war nie betroffen.

## [0.3.0] - 2026-05-22 — HCL-Profil-Release

> **Breaking:** Das HCL-Datenmodell wurde komplett auf ProfileV2 umgestellt. ETS-Projekte aus 0.2.x sind nicht migrationsfähig; HCL-Konfiguration je Lichtmanager muss neu projektiert werden.
 
> **Breaking (KO-Block):** `LMG_KoBlockSize` wächst von 12 auf **22** je Kanal (+10 KOs). Alle Module mit nachgelagerten KO-Offsets (insb. OFM-HueGatewayModule) müssen ihre Offsets entsprechend anpassen.

### Hinzugefügt

- **ProfileV2-HCL-Engine** — neue Module `HCLProfile`, `HCLProfileResolver`, `HCLProfileSelector`, `HCLSeasonDayNight`, `HCLTypes`. Jeder Master besitzt 4 Profil-Slots × 10 Stützpunkte (SetpointV2) mit Saison- und Tag/Nacht-Auswahl.
- **F1 HCL-Achsen + Zeitfenster** (Per-Kanal): `CHHclAxes` (Aus/KH/NurFarbtemperatur/NurHelligkeit) und `CHHclTimeWindow` (Immer/NurTagsueber/NurNachts) gaten K00/K01/K08/K11/K12/K13/K14/K15 sowie den partial-Sink-Aufruf. Filter sind UND-verknüpft. Bei `DayNightSource=Aus + HclTimeWindow≠Immer` greift AstroIntern-Fallback.
- **F2 Per-Kanal-Lock 3-Wege**: neuer Parameter `CHUseLock` (Nein/Vollsperre/Getrennt). Getrennt-Modus aktiviert achsenspezifische Sperr-KOs **K09 LockColor** und **K10 LockBrightness** (DPT-1-3 disable/enable). K02 dominiert über K09/K10. Pro Kanal eigene `FallbackPolicy` (6 Werte: Definierte Rückfallzeit / Freie Dauer / Freie Uhrzeit / Dauer oder Uhrzeit / Nur externes Entsperren / Deaktiviert).
- **F3 Vorausschau-KOs** (Per-Kanal, gated über `PreviewEnable` + `LookAheadMinutes`):
  - **K11 MinutesToNext** (DPT-7-6) — Minuten bis zum nächsten Stützpunkt
  - **K12 NextKelvin** (DPT-7-600) — Farbtemperatur des nächsten SP
  - **K13 NextBrightness** (DPT-5-1) — Helligkeit des nächsten SP
  - Δ-Schwellen: ΔK ≥ 50, ΔBrightness ≥ 1 %, ΔMinutes ≥ 1.
- **F4 Tages-Fortschritts-KOs** (Per-Kanal, gated über `ProgressEnable`):
  - **K14 DayProgress** (DPT-5-1) — 0…100 % zwischen erstem und letztem aktiven SP des Tages
  - **K15 DayPhase** (DPT-5-10) — 0=vor Sonnenaufgang, 1=Vormittag, 2=Mittag, 3=Nachmittag, 4=Abend, 5=Nacht
- **F5 Per-Kanal-Slew Tag/Nacht** (#1, kein Master-Aggregat): `UseDayNightSlew`, `SlewRateDay`, `SlewRateNight`, `DayNightSource` (Aus/KO/AstroIntern). Bei `Aus`-Quelle Fallback auf `SlewRateDay`.
- **F6 Astro-Engine** (intern): NOAA-Sonnenstand-Approximation mit `latitude/longitude/tzOffsetMin` aus Master-Params; `_lastSunriseMin`/`_lastSunsetMin`/`_lastSolarNoonMin` werden in `tickAstro()` aktualisiert; DST-Cache vermeidet Anker-Re-Resolves außerhalb von Datums-/DST-Wechseln (#7).
- **F7 Multi-Profil-Selektor + Saison**: 4 Profile pro Master mit `WeekdayMask` (16 Bit: Mo–So + Urlaub + Feiertag + Sommer/Winter + Default-Fallback). Selektor wertet Spezifität aus (Default-Fallback Bit 9 wirkt nur wenn kein spezifischeres Profil matcht; Bit 9 ist exklusiv).
- **F7-Hybrid Saison-Quelle**: `SeasonSource` (Aus/Automatisch-DST/FestesDatum/PerKO) mit `SeasonOffsetDays`, `SummerStart`, `SummerEnd`, **K04 SeasonInput** (PerKO-Modus). Initialisierung `SummerActiveInit` (Nichts/ReadFromBus/Winter/Sommer); Persistenz `SummerActiveSavePower=Ja` legt Per-Master `{magic:0x03, summerActive:bool}` im OpenKNX::Flash ab.
- **F8 Anker-Stützpunkte**: `AnchorType` je SP (FixedTime/Sunrise±/Sunset±/CivilDawn±/CivilDusk±/SolarNoon±) mit `AnchorOffsetMin` und `ClampMode` (Frei / Nicht vor / Nicht nach / Festklemmen).
- **F11 Partial-Sink-Vertrag**: `ILightManagerOutput::onLightManagerPartial(masterNum, kelvin, brightness, validMask, fade)`. Bit 0 = Kelvin valid, Bit 1 = Brightness valid. Default-Impl ruft `onLightManagerValue()` mit befüllten Werten. `LightManagerModule::loop()` ruft `onLightManagerPartial()` mit achsen-spezifischer Maske gemäß `HclAxes`/`StatusKoOutput` auf.
- **F12 Externe Quellen** (Per-Kanal):
  - **K16 ExtBrightnessPercent** (DPT-5-1) bzw. **K17 ExtBrightnessLux** (DPT-9-4, mit Skalierung `pct = lux*100/ExtLuxMax`, L6)
  - **K18 ExtColorTempKelvin** (DPT-7-600) bzw. **K19 ExtColorTempScalar** (DPT-5-1, mit Skalierung `k = kMin + scalar*(kMax-kMin)/255`, L6)
  - Parameter: `ExtColorTempSource`/`ExtBrightnessSource` (Aus/Override/Fallback), `ExtColorTempDpt`/`ExtBrightnessDpt`, `ExtKelvinMin`/`Max` (Defaults 2700/6500), `ExtLuxMax` (Default 500), `ExtFallbackTimeoutSec`.
  - Per-SP `ExtColorTempMode` (Off/Always/OnlyGreater/OnlySmaller) + `ExtMixPercent` für gezielte Mischung.
- **L1 Variante-E-Dispatch** (Per-Kanal, ersetzt 0.2.0 `StatusKoEnable`/`Dpt`):
  - `IntegrationMode` (Intern/Extern/Intern+Extern), `BusStatusEnable` (nur bei Intern sichtbar), `StatusKoOutput` (BrightnessAndKelvin ⭐ / BrightnessOnly / KelvinOnly / CombinedOnly / All).
  - Bei `Extern` wird `onLightManagerPartial()` nicht aufgerufen (Hue stumm).
- **#14 `AdaptiveDayNightPolarity`** (Per-Kanal, ETS-exponiert): Heller bei Nacht ⭐ / Dunkler bei Nacht.
- **L7 Preview/Progress Achsen-Bindung**: Helper `_shouldSendAxis(kelvin|brightness)` gated K12/K13 anhand `HclAxes`.

### Geändert

- **Live-Pipeline statt Bus-Trigger:** `pushIfChanged()` durchläuft jetzt vollständig `ProfileV2-Resolve → Slew → Adaptive → Ext-Quellen-Mix → KO/Sink-Push`. Der bisherige `HCL::masterManager.forceUpdate()`-Trigger im Lux-KO-Handler wurde entfernt — Adaptive wirkt automatisch beim nächsten Loop.
- **Hue-Sink-Routing:** `LightManagerModule::loop()` übergibt registrierten `ILightManagerOutput`-Sinks (z. B. Hue-Lampen) nun die geslewten + adaptierten + ext-quellen-gemischten Werte (`appliedKelvin()` / `effectiveBrightness()`) statt des rohen HCL-Sollwerts. Bus- und Lampen-Helligkeit sind damit konsistent.
- **Globale Sperre (Modul-KO 2/3/4)** dominiert über Per-Kanal-Locks und über `UseLock=Nein` (Kanal trotz UseLock=Nein eingefroren). Per-Kanal- und globale Fallback-Mechaniken laufen unabhängig.

### Entfernt

- **62 Legacy-Stützpunkt-Parameter** (Per-Channel SP-Slots aus der V1-Engine) inklusive zugehöriger `ParameterRefs` und Dynamic-Block-Choose-Verzweigungen.
- **Per-Kanal-Parameter `Wirkungsbereich`** (`CH%C%AdaptiveScope`, ID 014) inkl. `PT-LMGAdaptiveScope`, Dynamic-Block-Eintrag, Doku-Sektion und Help-Baggage `LMG-HCL-Adaptive-Wirkungsbereich` — nicht plan-konform; Routing läuft jetzt ausschließlich über Variante E (`IntegrationMode` + `BusStatusEnable` + `StatusKoOutput`). Adaptierte Helligkeit wird unbedingt auf Bus und interne Senken weitergereicht.
- **F9 `CurveType` komplett entfernt** — keine Parameter, keine ETS-Sichtbarkeit, alle Profile sind SP-basiert.
- ParameterTypes ohne Konsumenten: `PT-LMGStatusKoEnable`, `PT-LMGStatusKoDpt`, `PT-LMGHCLCurveType`, `PT-LMGHCLSeasonMode`, `PT-LMGHCLSummerDay`, `PT-LMGHCLSummerMonth`. Beibehalten: `PT-LMGHCLDSTOffsetDays` (weiterhin referenziert durch CC-019).

### Behoben

- **Slew-Rate-Regression im Tag-/Nacht-Fallback:** `applySlew()` referenzierte den entfernten `ParamLMG_CHSlewRate`; Fallback bei `!UseDayNightSlew` nutzt nun `ParamLMG_CHSlewRateDay`. Der Bug war durch den PlatformIO-Build-Cache verdeckt und wurde erst nach knxprod.h-Regenerierung sichtbar.

### ETS-UI

- **Stunden- und Minuten-Felder** aller HCL-Stützpunkt-Zeitparameter (`PT-LMGHCLHour` / `PT-LMGHCLMinute`) als `UIHint="DropDown"` mit je 24 (00–23) bzw. 60 (00–59) Einträgen — einheitlich mit den Zeitschaltuhren des OFM-LogicModuls.
- **HCL-Profil-Tabellen** (alle 4 Profil-Slots): beschreibende Spaltenüberschriften (Typ / Std / Min / Offset / Nicht vor/nach / Std / Min / Helligkeit / Farbtemperatur / Externe Farbtemperatur / Mix) statt generischer Bezeichner; Spaltenbreiten auf ETS-Darstellung optimiert (30/10/10/16/22/10/10/18/26/24/14 %).
- **Adaptive Helligkeit – Zeitfenster**: wird nur noch eingeblendet wenn Modus = „Nach Uhrzeit" gewählt ist (vorher dauerhaft sichtbar).
- **Tag/Nacht-Polarität** (`AdaptiveDayNightPolarity`) aus dem Regelverhalten-Block der Adaptiven Helligkeit auf die LM-Kanal-Hauptseite verschoben — direkt unterhalb von „Tag/Nacht-Quelle" eingeblendet wenn `DayNightSource ≠ Aus`.
- **„Gilt für"-Tabellen** (alle 4 Profil-Slots): grauer Spalten-Kopfbalken (Mo / Di / Mi / Do / Fr / Sa / So / Urlaub / Feiertag) wie in den Stützpunkt-Tabellen; Spaltenbreiten angepasst (Mo–So je 8 %, Urlaub 15 %, Feiertag 26 %) damit „Feiertag" vollständig sichtbar ist.
- **SeasonFilter-Dropdown** pro HCL-Profil (Sommer+Winter / Sommer / Winter / Fallback): Saison-Zuordnung je Profil direkt in den Profil-Einstellungen wählbar; nur sichtbar wenn `SeasonSource ≠ Aus`. Neuer ParameterType `PT-LMGSeasonFilter` (2 Bit), Parameter P760–P763.
- **Profil-Parameter ausgeblendet** wenn „Profil X aktiv = Nein": alle Unterparameter (Gilt für, Saison-Filter, Stützpunkte) werden per `choose/when` versteckt — weniger ETS-Rauschen für nicht verwendete Profile.
- **Hinweistexte als blaue Infobox** (`UIHint="Information"`): Sektion „Saisonale Umschaltung" erhält Info „Die Saison-Zuordnung je Profil wird in den Profil-Einstellungen konfiguriert."; Allgemein-Seite erhält globalen Hinweis mit Verweis auf Kanal-Konfiguration.
- **Sektion „Sommer-Hybrid (global)"** umbenannt zu **„Saisonale Umschaltung (global)"**.
- **„Saisonalen Zustand speichern"** von Checkbox auf Ja/Nein-Radio (`PT-OnOffYesNo`) umgestellt; Init-Dropdown „Falls Vorbelegung nicht möglich, vorbelegen mit" nur noch sichtbar wenn Ja gewählt.

### Behoben

- **Status-KO-Ausgabe** war immer sichtbar wenn `BusStatusEnable=0` (Modus ≠ Extern): Sichtbarkeit wird nun korrekt per `choose/when` auf `BusStatusEnable=1` konditioniert.

### Intern

- **NVS-Magic auf 0x03 gebumpt** (`LMG_SUMMER_FLASH_VERSION = 0x03`): bei Mismatch (z. B. alter 0x02-Block) wird der Per-Master-Slot ignoriert, `applySummerActiveInit()` greift, neuer Block mit Magic 0x03 wird geschrieben. Bei `SavePower=Nein` wird der Slot mit 3×0x00 überschrieben.
- `KoBlockSize` 12 → **22** (Reserve K20/K21 für zukünftige Erweiterungen).
- **`LMG_LOAD_PROFILE`-Makro** um SeasonFilter-Auswertung erweitert: `ParamLMG_CHP##P##_SeasonFilter` setzt `Summer` (Bit 10), `Winter` (Bit 11) oder `Fallback` (Bit 9) in `weekdayMask`; Wert 0 (Sommer+Winter) setzt keines der Bits.
- Skript `scripts/d3-remove-obsolete.py` (balanced choose-tracking + inline `018!=2` Unwrap) für reproducible XML-Cleanup hinzugefügt.

## [0.2.0] - 2026-05-19

### Hinzugefügt

- **Neues Kommunikationsobjekt `Status Tunable White kombiniert` (DPT 249.600, 6 Byte)** je Kanal mit Helligkeit, Farbtemperatur und Überblendzeit in einem Telegramm.
- **Per-Kanal-Parameter** für die Status-Ausgabe:
  - `Ausgabe verwenden` (Intern / Extern / Beides)
  - `Status-KOs zusätzlich aktivieren` (nur bei `Intern` sichtbar)
  - `Datentyp der Status-Ausgabe` (Helligkeit+Farbtemperatur / DPT 249.600 / Beides)
  - `Aktualisierungsintervall` (60..3600 s, Standard 60 s)
  - `Überblendzeit` (1..60 s, Standard 6 s)
- ETS-Hilfe-Texte zu allen neuen Parametern.

### Geändert

- **Breaking:** `Aktualisierungsintervall` und `Überblendzeit` sind nicht mehr global, sondern pro Kanal konfigurierbar. Die bisherigen globalen Parameter bleiben im Projekt als `(legacy)` erhalten (Memory-Layout), werden aber nicht mehr ausgewertet → nach dem Update müssen die Kanal-Einstellungen je Lichtmanager neu projektiert werden.
- ETS-UI je Kanal restrukturiert: neue Abschnitte `Ausgabe` und `Zeitverhalten`.
- KO-Block je Lichtmanager-Kanal wächst von 8 auf 12 Einträge (`LMG_KoBlockSize`). Eigene OAM-Projekte, die `KoOffset`/`KoSingleOffset` nachgelagerter Module manuell setzen, müssen ihre Werte um `+64` (16 Kanäle × 4 neue KOs) anpassen.

### Intern

- `HCL::MasterManager` enthält keine globalen `updateInterval`/`fadeDuration`-Felder mehr; Timing liegt vollständig in `LightManagerChannel`.
- Neue Throttle-Logik `LightManagerChannel::pushIfChanged()` ersetzt die globale Loop-Push-Logik.

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
