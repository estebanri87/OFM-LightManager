### KO Status-Soll Kombi (DPT 249.600)

`DPT 249.600` (`DPST-249-600`), 6 Byte, Tunable White kombiniert.

Aktiv wenn **Ausgabe verwenden** = `Extern` oder `Beides`, und **Datentyp** = `Kombiniert (DPT 249.600)` oder `Beides`.

**Telegrammaufbau (Big-Endian, 6 Byte)**:

| Byte | Inhalt | DPT | Wert |
|---|---|---|---|
| 0–1 | Übergangszeit | DPT 7.004 (100-ms-Einheiten) | `Überblendzeit × 10` |
| 2–3 | Farbtemperatur | DPT 7.600 (Kelvin) | berechneter HCL-Sollwert |
| 4 | Helligkeit | DPT 5.001 skaliert (0–255) | `Helligkeit% × 255 / 100` |
| 5 | Maskierung | B8 | `0x07` (alle drei Validity-Bits gesetzt) |

Die Maskierung `0x07` bedeutet: Validity-Bit für Übergangszeit (b2), Farbtemperatur (b1) und Helligkeit (b0) sind alle gesetzt — der Empfänger soll alle drei Felder auswerten.

**Empfehlung MDT AKD LED Controller**: Objekt 77 (`Tunable White combined`) mit diesem KO verbinden, **Datentyp** = `Kombiniert` oder `Beides` wählen, **Überblendzeit** ≈ **Aktualisierungsintervall** oder kleiner für fließende Übergänge ohne sichtbare Stufen.
