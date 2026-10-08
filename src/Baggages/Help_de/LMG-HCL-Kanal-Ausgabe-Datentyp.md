### Datentyp der Status-Ausgabe

Bestimmt, welche KNX-Status-KOs der Kanal verwendet:

- **Helligkeit + Farbtemperatur** – Zwei separate KOs:
  - `Status Helligkeit Soll` (`DPT 5.001`, 1 Byte, 0..100 %)
  - `Status Farbtemperatur Soll` (`DPT 7.600`, 2 Byte, K)
- **Kombiniert (DPT 249.600)** – Ein einzelnes 6-Byte-KO `Status Tunable White kombiniert` mit Helligkeit, Farbtemperatur und Überblendzeit in einem Telegramm.
- **Beides** – Sowohl die separaten KOs *als auch* das kombinierte DPT 249.600-KO werden gesendet.

Hinweis: Die separaten KOs nutzen die in den Aktoren konfigurierte Überblendzeit; das kombinierte KO 249.600 enthält die Überblendzeit als Telegrammbestandteil.
