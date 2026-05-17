### Kommunikationsobjekte

### Globale Kommunikationsobjekte

#### Sperre (global) / Status Sperre
Globale Sperre inkl. Statusrückmeldung.

#### Entsperren Trigger
1-Bit Triggerobjekt zum gleichzeitigen Aufheben aller globalen, manager-spezifischen und kanal-spezifischen Sperren.

#### Sperre Lichtmanager 1..16 / Status
Lichtmanager-spezifische Sperrobjekte inkl. Statusrückmeldung.
Sichtbarkeit abhängig von der konfigurierten Anzahl Lichtmanager.

#### Lichtmanager Status Helligkeit Soll / Farbtemperatur Soll
Je Lichtmanager zwei Sollwert-KOs; Sichtbarkeit abhängig von Option **Status-KOs je Lichtmanager**.

### Pro-Lichtmanager Kommunikationsobjekte

#### LM x: Sommer aktiv
1-Bit Eingang (DPT 1.001). Nur sichtbar bei Saison-Modus **Per Objekt**. Zustand wird im Flash persistiert.

#### Helligkeitssensor (Lux)
2-Byte Eingang (DPT 9.004). Eingang für die adaptive Helligkeitsregelung.

#### Tag/Nacht
1-Bit Eingang (DPT 1.001). Nur sichtbar bei Aktivierung der adaptiven Regelung = „Nur tagsüber (per KO)".

#### Adaptive Helligkeit aktiv
1-Bit Ausgang (DPT 1.011). Status der adaptiven Regelung.

