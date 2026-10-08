### Externe Eingänge (F12 + L6)

Per-Kanal überschreib- oder fallback-bare Werte aus externen Quellen:

**Farbtemperatur** (`ExtColorTempSource` = Aus / Override / Fallback):
- `ExtColorTempDpt = 2B Kelvin` → **K18 ExtColorTempKelvin** (DPT 7.600), Bereich 1500…10000 K.
- `ExtColorTempDpt = 1B Skalar` → **K19 ExtColorTempScalar** (DPT 5.001, 0…255), mit Skalierung `k = ExtKelvinMin + scalar * (ExtKelvinMax − ExtKelvinMin) / 255` (Defaults 2700 / 6500 K).

**Helligkeit** (`ExtBrightnessSource` = Aus / Override / Fallback):
- `ExtBrightnessDpt = 1B Prozent` → **K16 ExtBrightnessPercent** (DPT 5.001, 0…100 %).
- `ExtBrightnessDpt = 2B Lux` → **K17 ExtBrightnessLux** (DPT 9.004), mit Skalierung `pct = lux * 100 / ExtLuxMax` (Default 500 lx).

**Fallback-Timeout** (`ExtFallbackTimeoutSec`): bei Fallback-Modus springt der Kanal nach Ablauf ohne neues Telegramm zurück auf den internen HCL-Wert.

Per-SP **ExtColorTempMode** + **ExtMixPercent** erlauben gezieltes Mischen (z. B. „abends nur einbeziehen wenn externer Wert kleiner ist"). Mix-Formel: `(interp * (100 − mix) + extK * mix) / 100`.
