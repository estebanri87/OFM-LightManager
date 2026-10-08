### HCL-Vorausschau (F3)

Per-Kanal aktivierbar über `PreviewEnable`. Wenn aktiviert, sendet der Kanal:

- **K11 Minuten bis nächstem Stützpunkt** (DPT 7.006)
- **K12 Nächste Farbtemperatur** (DPT 7.600)
- **K13 Nächste Helligkeit** (DPT 5.001)

`LookAheadMinutes` legt fest, wie weit voraus gescannt wird. Δ-Schwellen verhindern Bus-Spam: ΔK ≥ 50 K, ΔBrightness ≥ 1 %, ΔMinutes ≥ 1.

Bei leerem Profil (kein aktiver SP) wird kein Send ausgeführt.
