### HCL-Manager-18

Jeder Lichtmanager 1..N besitzt identischen Aufbau (HCL-Konfiguration):

- **Beschreibung**: Freie ETS-Bezeichnung des Lichtmanagers (auch in der Kanalauswahl editierbar).
- **Suspendiert**: Der Lichtmanager bleibt projektiert, wird aber nicht ausgeführt.
- **Lichtmanager Sperre (spezifisch)**: Sperrt nur den jeweiligen Manager.
- **Erweiterte Kurve**: Kurventyp `FixedTime`, `SunPosition`, `Manual` oder `Astronomischer Sonnenstand`.
- **Stützpunkte**: Bis zu 10 Stützpunkte je Manager (bei `FixedTime` oder `SunPosition`).
- **Saison-Profil**: Optionale Sommer-/Winter-Stützpunkte (Modus `Standard`, `Auto-DST`, `Festes Datum` oder `Per Objekt`).
- **Adaptive Helligkeit**: Optionale Tageslicht-Kompensation (Open-Loop) oder Konstantlichtregelung (Closed-Loop) per Helligkeitssensor.
