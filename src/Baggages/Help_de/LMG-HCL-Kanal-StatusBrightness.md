### KO Status-Soll-Helligkeit

`DPT 5.001`, 1 Byte, 0…00 %.

Aktiv wenn **Ausgabe verwenden** = `Extern` oder `Beides`, und **Datentyp** = `Helligkeit + Farbtemperatur` oder `Beides`.

Der Kanal sendet den berechneten Helligkeits-Sollwert zyklisch gemäß **Aktualisierungsintervall** sowie sofort bei geändertem Wert.

Hinweis: Die Überblendzeit ist in diesem KO nicht enthalten — für weiche Übergänge im Aktor die **Slew-Rate** des Lichtmanagers verwenden oder das kombinierte DPT 249.600-KO nutzen.
