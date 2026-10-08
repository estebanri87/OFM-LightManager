### HCL-Profile (F7)

Pro Master stehen bis zu **4 Profil-Slots** mit je **10 Stützpunkten** (SetpointV2) zur Verfügung. `ProfileCount` (1–4) limitiert sichtbare Slots.

Jedes Profil besitzt eine **Wochentag-/Sonderbedingungs-Maske** (16 Bit, davon 12 genutzt):

| Bit | Bedeutung |
|---|---|
| 0–6 | Mo, Di, Mi, Do, Fr, Sa, So |
| 7 | Urlaub (`KoLOG_Vacation`) |
| 8 | Feiertag (OFM-LogicModule) |
| 9 | **Default-Fallback** (exklusiv: bei gesetztem Bit 9 werden alle anderen Bits ignoriert) |
| 10 | Sommer |
| 11 | Winter |

**Selektor-Spezifität**: Profile mit konkreter Wochentag-/Saison-Übereinstimmung gewinnen gegen Default-Fallback. Bei mehreren passenden Profilen gewinnt das Profil mit der höchsten Spezifität (Anzahl gesetzter, matchender Bits).
