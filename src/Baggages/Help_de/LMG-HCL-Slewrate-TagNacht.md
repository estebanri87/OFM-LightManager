### HCL-Slewrate Tag/Nacht (F5, Per-Kanal)

`UseDayNightSlew=Nein` → `_slewRate` 24/7 konstant.

`UseDayNightSlew=Ja`:

| DayNightSource | Verhalten |
|---|---|
| **KO** | `SlewRateNight` wenn K06=0, sonst `SlewRateDay` |
| **AstroIntern** | `SlewRateNight` wenn Sonne unter Horizont, sonst `SlewRateDay` |
| **Aus** | Fallback auf `SlewRateDay` (kein Wechsel) |

Per-Kanal — kein Master-Aggregat. Jeder Kanal entscheidet eigenständig.
