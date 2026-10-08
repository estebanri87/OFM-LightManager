### Per-Kanal-Sperre (F2)

Parameter **CHUseLock** (3-Wege):

- **Nein**: K02/K03/K09/K10 in ETS unsichtbar, Handler ignorieren Telegramme, Rückfall-Block weg.
- **Vollsperre**: nur K02 (Eingang) + K03 (Status) sichtbar; sperrt beide Achsen.
- **Getrennt**: zusätzlich **K09 LockColor** und **K10 LockBrightness** (DPT 1.003 disable/enable) sichtbar. Einseitige Sperre friert nur die betroffene Achse ein. K08-Validity-Bit reflektiert achsenspezifisch.

**Hierarchie**: K02 = 1 dominiert über K09/K10. Globale Sperre (Modul-K02/K03/K04) dominiert über **alle** Per-Kanal-Locks, auch bei `UseLock=Nein`. Lock-Auswertung liegt **vor** `_shouldSendAxis()`.
