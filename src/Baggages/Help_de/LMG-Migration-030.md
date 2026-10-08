### Migration auf 0.3.0

> **Breaking — keine Auto-Migration aus 0.2.x.** ETS-Projekte aus 0.2.x sind nicht migrationsfähig; alle HCL-Konfigurationen müssen je Lichtmanager neu projektiert werden.

**Wesentliche Änderungen**:
- HCL-Datenmodell auf **ProfileV2** umgestellt: 4 Profile × 10 Stützpunkte je Master, mit Wochentag-/Saison-/Default-Maske.
- **62 Legacy-Stützpunkt-Parameter** entfernt; `CurveType`-Parameter komplett gestrichen.
- **`KoBlockSize` wächst von 12 auf 22**: nachgelagerte Module (insb. OFM-HueGatewayModule) müssen ihre `KoOffset`/`KoSingleOffset` neu berechnen. Alle Gruppenadressen nachgelagerter Module müssen neu verknüpft werden.
- `SeasonMode` → `SeasonSource` umbenannt; Werte abgebildet wie folgt:
  - 0.2 `Standard` → 0.3 `Aus`
  - 0.2 `Automatisch (DST)` → 0.3 `Automatisch (DST)`
  - 0.2 `Festes Datum` → 0.3 `Festes Datum`
  - 0.2 `Per KO` → 0.3 `Per KO`
- **NVS-Magic 0x02 → 0x03**: alte Per-Master-Summer-Slots werden beim Boot ignoriert; `SummerActiveInit` greift, neuer Block mit Magic 0x03 wird geschrieben.
- Variante-E-Dispatch ersetzt 0.2.0 `StatusKoEnable`/`StatusKoDpt`: neue Parameter `IntegrationMode`, `BusStatusEnable`, `StatusKoOutput` mit feiner KO-Granularität.

**Out-of-Box**: nach ETS-Download liefert HCL sofort Werte (Default-SPs in Profil 1: morgens 2700 K / 30 %, mittags 5000 K / 80 %, abends 2700 K / 20 %).
