### KO Status-Soll-Farbtemperatur

`DPT 7.600`, 2 Byte, Kelvin (typisch 2000–6500 K).

Aktiv wenn **Ausgabe verwenden** = `Extern` oder `Beides`, und **Datentyp** = `Helligkeit + Farbtemperatur` oder `Beides`.

Der Kanal sendet den berechneten Farbtemperatur-Sollwert zyklisch gemäß **Aktualisierungsintervall** sowie sofort bei geändertem Wert.

Hinweis: Beide KOs (Helligkeit + Farbtemperatur) werden stets gemeinsam gesendet, auch wenn sich nur ein Wert geändert hat.
