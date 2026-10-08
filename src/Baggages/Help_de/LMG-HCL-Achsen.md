### HCL-Achsen (F1)

Per-Kanal-Parameter **HCL-Achsen** (`CHHclAxes`) bestimmt, welche Ausgangsgrößen der Kanal produziert:

- **Aus**: HCL ist deaktiviert. K00/K01/K08/K11/K12/K13/K14/K15 senden nicht, `onLightManagerPartial()` wird nicht aufgerufen, `IntegrationMode`/`BusStatusEnable`/`StatusKoOutput` sind in ETS ausgeblendet.
- **Helligkeit + Farbtemperatur** (Default): beide Achsen aktiv.
- **Nur Farbtemperatur**: K00 (Brightness) und K13 (NextBrightness) verstummen; `validMask` an Senken auf Bit 0 reduziert.
- **Nur Helligkeit**: K01 (ColorTemp) und K12 (NextColorTemp) verstummen; `validMask` an Senken auf Bit 1 reduziert.

Die Filterung greift in `_shouldSendAxis()` **vor** dem KO-Send und vor dem partial-Sink-Aufruf. K11/K14/K15 sind achsen-neutral und gelten als „aktiv" sobald mindestens eine Achse aktiv ist.
