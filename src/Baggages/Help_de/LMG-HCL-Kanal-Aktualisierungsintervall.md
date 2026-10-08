### Aktualisierungsintervall

Zeitlicher Abstand, in dem der Kanal seine berechneten Sollwerte erneut auswertet und – bei Wertänderung oder Ablauf des Intervalls – sendet.

- Bereich: **60 .. 3600 s** (1 Minute bis 1 Stunde)
- Standard: **60 s**

Empfehlung: 60–300 s ist für Wohnbereiche meist ausreichend. Sehr kurze Intervalle (< 60 s) sind nicht zulässig, um Busbelastung und Aktor-Logging zu schonen.

Hinweis: Wertänderungen, die durch HCL-Kurvenpunkte oder externe Eingriffe entstehen, werden auch zwischen den Zyklen sofort gesendet, wenn sie sich vom zuletzt gesendeten Wert unterscheiden.
