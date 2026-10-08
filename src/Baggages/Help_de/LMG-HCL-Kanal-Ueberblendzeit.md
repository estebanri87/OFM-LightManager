### Überblendzeit

Zeit, in der ein Aktor von seinem aktuellen Wert auf den neuen HCL-Sollwert überblenden soll.

- Bereich: **1 .. 60 s**
- Standard: **6 s**

Verwendung je nach Datentyp:

- **Helligkeit + Farbtemperatur** – Die Überblendzeit wird *nicht* mitgesendet; verwendet wird die im Aktor projektierte Überblendzeit. Der Wert dient hier nur internen Verbrauchern (z. B. Hue-Gateway).
- **Kombiniert (DPT 249.600)** – Die Überblendzeit ist Teil des 6-Byte-Telegramms und wird vom Aktor unmittelbar verwendet.
- **Beides** – wirkt wie oben kombiniert.

Empfehlung: Werte zwischen 2 und 10 s vermeiden sichtbares „Springen“ und sind für Wohnräume angenehm.
