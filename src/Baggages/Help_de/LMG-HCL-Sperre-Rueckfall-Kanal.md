### Rückfallstrategie nach Per-Kanal-Sperre

Per-Kanal `FallbackPolicy` (6 Werte):

| Wert | Verhalten |
|---|---|
| **Definierte Rückfallzeit** | `LockFallback` (Minuten) — Auto-Release nach Ablauf |
| **Freie Dauer** | `FallbackDurationSec` (Sekunden) frei konfigurierbar |
| **Freie Uhrzeit** | `FallbackReleaseTime` (HH:MM) — Release am nächsten Erreichen |
| **Dauer oder Uhrzeit** | Was zuerst eintritt |
| **Nur externes Entsperren** | Kein Auto-Release, bleibt bis K02=0 |
| **Deaktiviert** | Bleibt bis Reboot |

Gilt einheitlich für alle aktiven Per-Kanal-Locks (Vollsperre und Getrennt). Bei Getrennt-Modus + K09=1 + K02=1 gleichzeitig werden beim Auto-Release beide gemeinsam freigegeben.

Globaler und Per-Kanal-Fallback laufen **unabhängig** auf ihren jeweiligen Lock-Quellen.
