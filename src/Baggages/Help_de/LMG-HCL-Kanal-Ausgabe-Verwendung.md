### Ausgabe verwenden

Legt fest, wohin der Lichtmanager-Kanal seine berechneten Sollwerte sendet:

- **Intern** – Der Kanal wird ausschließlich von internen Verbrauchern gelesen (z. B. Hue-Gateway-Modul). Es werden keine Status-KOs auf den KNX-Bus gesendet.
- **Extern** – Der Kanal sendet Sollwerte über die Status-KOs auf den KNX-Bus. Interne Verbraucher werden **nicht** beliefert.
- **Beides** – Der Kanal beliefert interne Verbraucher *und* sendet zusätzlich die Status-KOs auf den KNX-Bus.

Bei `Intern` bleiben die zugehörigen Status-KOs deaktiviert und belegen keine Gruppenadresse.
