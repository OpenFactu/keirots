# Keirost 0.3.1

Versión de verificación del proceso de actualización de 0.3.0 a 0.3.1 por Wi-Fi o USB.
El único cambio en el firmware es el número de versión que comunica el lector.
Mantiene el funcionamiento de 0.3.0 y conserva Wi-Fi, certificado y token.

Después de instalarla, el lector reinicia y comunica la versión 0.3.1 a Keirost.

Firmware para ESP32 DevKit V1 y lector RC522, en modo vinculado a kiosco.

- Actualización por USB desde Keirost.
- Actualización por Wi-Fi solicitada expresamente desde la plataforma.
- Validación HTTPS y SHA-256 del firmware antes de activar la nueva versión.
- Conserva Wi-Fi, certificado y token en NVS.
- Compatible con configuración USB y asignación de tarjetas.

La versión 0.2.x necesita una primera instalación por USB para habilitar Wi-Fi.
No se instala en ESP32-C3/S3 ni en placas con un esquema de particiones diferente.
Una interrupción durante la descarga OTA deja activa la aplicación anterior.
No incorpora rollback automático si una versión instalada arranca pero falla: recuperar por USB.
