### Häufige Fehler und Lösungen

### Lichtmanager-Parameter oder HCL-KOs fehlen
- Lichtmanager global aktiviert?
- Anzahl Lichtmanager in **Lichtmanager Auswahl** ausreichend?
- Erst nach Aktivierung des globalen Lichtmanagers werden Zuordnung und Sperrparameter sichtbar.

### Lichtmanager wirkt nicht
- Lichtmanager global aktiviert?
- Manager im Konsumenten-Modul zugewiesen?
- Bei `FixedTime`/`SunPosition`: mind. 2 gültige Stützpunkte?
- Bei `Manual`: gewünschte manuelle Farbtemperatur gesetzt und optionaler Helligkeitsverlauf passend parametriert?
- Bei `Astronomischer Sonnenstand`: sinnvolle Astro-Min/Max-Werte gesetzt?
- Globale/spezifische Sperre aktiv?

### Saison-Profil schaltet nicht um
- Saison-Modus ist `Standard`? → dann sind Sommer-Stützpunkte absichtlich deaktiviert.
- Bei Modus `Festes Datum`: Start- und Ende-Datum korrekt eingetragen? Datum liegt im aktiven Bereich?
- Bei Modus `Auto-DST`: Systemzeit korrekt? DST-Erkennung setzt korrekte Uhrzeit voraus.
- Bei Modus `Per Objekt`: KO `LM x: Sommer aktiv` mit GA verbunden und Wert `1` gesendet?
- Im Sommer-Profil mindestens 2 Stützpunkte mit **Sommer Aktiv = Ja** vorhanden (bei `FixedTime`/`SunPosition`)?

