# Lichtmanager

Der Lichtmanager ist eine eigenständige Sollwertquelle für Farbtemperatur und
Helligkeit nach dem Prinzip des **Human Centric Lighting (HCL)**. Es stehen bis
zu 8 unabhängige HCL-Master zur Verfügung, deren Sollwerte von beliebigen
Consumer-Modulen (z. B. Hue Gateway, DALI-Gateway, LED-Controller) übernommen
werden können.

Die tageszeitabhängige Steuerung erfolgt wahlweise über frei konfigurierbare
Stützpunkttabellen oder ein astronomisches Sonnenfenster auf Basis von
Sonnenauf- und -untergang. Ergänzend stehen Saison-Profile (Sommer/Winter),
eine adaptive Helligkeitsregelung (Open-Loop oder Konstantlichtregelung) sowie
eine per KNX schaltbare Sperre je Master zur Verfügung.

## Applikationen

Diese Funktion wird nicht als eigenständige [OpenKNX-Applikation](https://openknx.atlassian.net/wiki/spaces/OpenKNX/pages/3571727) angeboten,
sondern ist Bestandteil folgender Applikationen:

- [OAM-HueGateway](https://github.com/OpenKNX/OAM-HueGateway)

## Kompatible Geräte

Bezieht sich auf die einbindenden Applikationen. Der Lichtmanager kann Bestandteil
verschiedener Applikationen sein und ist somit mit allen Geräten kompatibel,
auf denen diese Applikationen lauffähig sind.

## Weitere Informationen

- [Releases](https://github.com/OpenKNX/OFM-LightManager/releases)
- [Dokumentation](https://github.com/OpenKNX/OFM-LightManager/blob/main/doc/Applikationsbeschreibung-LightManager.md)
- [Github-Repository](https://github.com/OpenKNX/OFM-LightManager)
- Thread im KNX-User-Forum *(folgt)*
