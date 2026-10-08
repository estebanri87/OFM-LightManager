### Saison-Quelle (F7-Hybrid)

Per-Master `SeasonSource`:

- **Aus**: Profile mit Sommer/Winter-Bit matchen nicht (saison-neutral). Sommer-Stützpunkte deaktiviert.
- **Automatisch (DST)**: nutzt System-DST-Flag; optional `SeasonOffsetDays` für regionale Verschiebung.
- **Festes Datum**: `SummerStart`/`SummerEnd` als (Monat, Tag); unterstützt Jahreswechsel-Übergang (Südhalbkugel).
- **Per KO**: K04 `SeasonInput` (DPT 1.001) schaltet zur Laufzeit.

**Initialisierung** (`SummerActiveInit`): Nichts / Vom Bus lesen / Winter (0) / Sommer (1).

**Persistenz** (`SummerActiveSavePower=Ja`): letzter K04-Wert in `OpenKNX::Flash` (Per-Master Slot `{magic:0x03, summerActive:bool}`); überschreibt Init beim Reboot. Bei `Nein` wird der Slot mit `0x00`-Bytes überschrieben. Magic 0x03 ⇒ Layout-Mismatch (z. B. alter 0x02-Block) wird erkannt und ignoriert.
