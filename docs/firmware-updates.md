# Actualizaciones desde Keirost

El repositorio público es https://github.com/OpenFactu/keirots y el código propio usa MIT.
Las dependencias conservan sus licencias: Arduino-ESP32 (LGPL-2.1), ESP-IDF
(Apache-2.0 y las licencias de sus componentes) y MFRC522 (Unlicense).

Cada etiqueta vX.Y.Z compila el sketch con Arduino-ESP32 3.3.12 y MFRC522 1.4.12.
GitHub Releases publica firmware.bin, boot_app0.bin, partitions.bin y manifest.json.
El manifiesto contiene placa, versión, tamaños y hashes SHA-256/MD5. Para publicar:
cambiar src/FirmwareVersion.h, revisar y probar, hacer commit, crear etiqueta y subirla.
El trabajo de GitHub comprueba que etiqueta y versión coinciden y conserva el diseño
de particiones. No sustituir archivos de una versión ya publicada; publicar otra versión.

En la plataforma: Dispositivos físicos → lector → Actualizar firmware.

- USB usa el cargador oficial de Espressif, comprueba ESP32 y la tabla de particiones.
  Escribe la aplicación y reinicia la selección de arranque, sin borrar NVS.
  Requiere Chrome/Edge de escritorio, cable de datos y cerrar el monitor Arduino.
  Si falla la conexión automática, mantener BOOT durante la conexión y soltar al escribir.
- Wi-Fi requiere 0.3.0 o posterior y una configuración válida. La plataforma autoriza
  una versión concreta; el lector la recibe en su próxima consulta, cuando no haya
  identificación activa ni lectura pendiente. Descarga desde la plataforma, nunca envía
  el token del dispositivo a GitHub. Tras reiniciar comunica la versión instalada.
  La orden puede cancelarse antes de comenzar la descarga; no interrumpe una escritura iniciada.
- Antes de escribir OTA, se comprueban tamaño y partición alternativa; Update valida
  la imagen y SHA-256 antes de cambiar el arranque. Si falla, comunica el error y conserva
  la aplicación anterior. No hay rollback automático tras arrancar una imagen defectuosa.
- Se confía en los mantenedores del repositorio y en HTTPS. El hash verifica integridad,
  no sustituye una firma independiente ni activa Secure Boot/eFuses en esta placa.

El servidor necesita salida HTTPS a GitHub para obtener versiones. La placa solo necesita
acceso HTTPS a su plataforma y NTP, como antes. No requiere cuenta de GitHub ni usuario ERP.
La configuración y las copias de flash nunca se incluyen en el repositorio ni en Releases.
