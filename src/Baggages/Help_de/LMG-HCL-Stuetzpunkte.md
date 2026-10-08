### HCL-Stützpunkte (F8)

Jeder Stützpunkt (SetpointV2) trägt:

- **AnchorType**: `FixedTime`, `Sunrise±`, `Sunset±`, `CivilDawn±`, `CivilDusk±`, `SolarNoon±`
- **AnchorOffsetMin** (–720…+720 min)
- **ClampMode**: Frei / Nicht vor / Nicht nach / Festklemmen
- Kelvin (1500–10000) und Brightness (0–100 %)
- Per-SP **ExtColorTempMode** (Off / Always / OnlyGreater / OnlySmaller) und **ExtMixPercent**

Anker werden in `tickAstro()` aus dem aktuellen Sonnenstand aufgelöst; ein DST-Cache vermeidet Re-Resolves außerhalb von Datums-/DST-Wechseln.
