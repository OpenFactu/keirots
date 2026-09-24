# Keirost: lector ESP32 / RC522

Firmware **0.3.0** para un lector **vinculado a un kiosco** de Keirost.
El ESP32 lee la tarjeta y envia una identificacion temporal. El empleado elige
la accion en la pantalla del kiosco. El destello indica lectura, nunca un fichaje
confirmado. No muestra UID ni credenciales en los mensajes de diagnostico.

Repositorio oficial: [OpenFactu/keirots](https://github.com/OpenFactu/keirots).
Licencia del codigo propio: [MIT](LICENSE).

## Actualizar desde la plataforma

En **Dispositivos fisicos**, pulsa **Actualizar firmware** en el lector.
Puedes instalar por **USB** o solicitar la actualizacion por **Wi-Fi**.
Los lectores con 0.2.x necesitan la primera actualizacion por USB. Se conserva
la configuracion y la plataforma solo confirma la instalacion al recibir
la version nueva desde el lector. [Detalles y publicacion de versiones](docs/firmware-updates.md).

## Configurar desde Keirost

1. Con el firmware cargado, abre **Configuracion > Dispositivos fisicos** en
   Chrome o Edge de escritorio. Tambien esta disponible en **Kioskos de fichaje**.
2. Crea un lector vinculado a un kiosco activo o pulsa **Configurar por USB** en
   uno existente. No necesitas copiar tokens: Keirost envia el de ese lector.
3. Cierra el monitor serie de Arduino, conecta por USB y pulsa **Conectar lector**.
4. Introduce nombre y clave de una red Wi-Fi de **2,4 GHz**. La direccion HTTPS
   de Keirost se rellena con el origen web, salvo si abres la web en localhost.
   En ese caso indica una direccion accesible desde el Wi-Fi del ESP32. No sirve
   localhost, una URL HTTP ni una URL con contrasenas o tokens.
5. Si la instalacion usa una CA privada, introduce su certificado publico PEM
   en las opciones de certificado. No se desactiva la validacion TLS.
6. Manten **BOOT** pulsado mientras guardas. Tras **Configuracion guardada**,
   sueltalo: el ESP32 reinicia y conecta por Wi-Fi. No pulses EN con BOOT pulsado.
7. Espera a **El lector ha conectado con Keirost y esta listo**. Acerca una
   tarjeta asignada a un empleado y confirma la accion en la pantalla vinculada.

Guardar un lector existente renueva su token y sustituye su configuracion
anterior. Si falla el envio USB, manten el asistente abierto para reintentarlo
con el mismo token; si lo cierras, vuelve a configurar el lector.
La clave Wi-Fi viaja del navegador al USB; no se envia al servidor ni se guarda
en localStorage. Empresa y token proceden de la sesion de Keirost.

## Asignar una tarjeta desde la ficha del empleado

1. Cierra el asistente de configuracion USB y el monitor serie de Arduino.
2. En **Empleados**, abre la persona y **Identificacion para fichar > Asignar tarjeta**.
3. Con el lector conectado por USB al ordenador, pulsa **Leer tarjeta** en Chrome o Edge.
4. Espera el mensaje para acercar la tarjeta. Si ya estaba apoyada, retirala y acercala otra vez.
5. El UID se rellena automaticamente. Revisa la persona y pulsa **Guardar tarjeta**.

La lectura para asignar dura hasta 30 segundos; puede cancelarse. En ese intervalo
las tarjetas no se envian al kiosco. Se retiene solo la primera tarjeta, en RAM,
y se devuelve unicamente por el protocolo USB solicitado, sin imprimirla en logs.
El UID no se envia al servidor hasta guardar la tarjeta con los permisos existentes.
No requiere mantener BOOT. El firmware anterior 0.2.0 necesita una carga normal
por USB; conserva Wi-Fi y token en NVS, sin borrar toda la memoria.

El firmware no contiene secretos. La configuracion persiste como un documento
en NVS y no puede consultarse por USB. **Esta placa de desarrollo no tiene la
flash cifrada**: esto no protege frente a extraccion fisica. No se han cambiado
eFuses ni opciones irreversibles. Protege fisicamente el dispositivo y rota el
token desde la plataforma si se pierde.

El modo independiente se rechaza: este montaje no tiene selector de acciones.
No deduce entrada/salida alternando lecturas. Una caida de red no crea una cola
offline. Solo se conserva una intencion en RAM durante 60 segundos para sus
reintentos inmediatos, con el mismo UUID/cuerpo y respetando Retry-After.
Tras un reinicio no se reenvian lecturas anteriores. Un 202 requiere confirmacion
en pantalla; un 401 bloquea los envios hasta reconfigurar o reiniciar.

La configuracion del servidor se consulta al iniciar y cada 60 segundos mientras
haya conexion, salvo bloqueos de autorizacion o espera indicada por el servidor.
Se necesita acceso a NTP para comprobar la vigencia del certificado; la fecha de
los fichajes siempre la asigna Keirost.

## Abrir en Visual Studio Code

1. Descarga el repositorio en una carpeta llamada `keirost-rfid-fichaje` y abre esa carpeta.
2. Abre `keirost-rfid-fichaje.ino`.
3. Usa la extension **Arduino Community Edition** (`vscode-arduino.vscode-arduino-community`).
4. La placa configurada es **DOIT ESP32 DEVKIT V1**, con Arduino CLI.
5. Selecciona el puerto de tu placa usando
   `Ctrl+Shift+P` -> **Arduino: Select Serial Port**.

Dependencias: **esp32 by Espressif Systems 3.3.12** y **MFRC522 1.4.12**.
Se instalan mediante **Arduino: Board Manager** y **Arduino: Library Manager**.

## Compilar, cargar y comprobar

- **Arduino: Verify** compila el programa.
- **Arduino: Upload** lo carga en la placa. Cierra otros monitores serie antes.
- **Arduino: Open Serial Monitor** abre los mensajes; selecciona **115200 baudios**.
- Pulsa **EN** en el ESP32 si necesitas volver a ver los mensajes de arranque.
- Debe aparecer `RC522 responde` y, al acercar una tarjeta, `TARJETA DETECTADA`.
- La misma tarjeta mantenida sobre el lector no debe aumentar el contador.
- Cada nueva deteccion produce un destello de unos 250 ms en el LED del ESP32
  conectado a GPIO2, si esta variante de placa lo incorpora. No indica fichaje confirmado.
- Retirala al menos medio segundo y acercala de nuevo para probar otra lectura.
- Si aparece `RC522 sin respuesta`, desconecta el USB antes de revisar cables.
- El LED rojo del RC522 indica alimentacion y no se controla con este programa.

La prueba fisica anterior se conserva en `examples/diagnostic/diagnostic.ino`.
La carga inicial de una placa de fabrica se realiza con Arduino. El actualizador
web admite lectores ya instalados con el mismo esquema de particiones.

Si la carga se queda en `Connecting...`, manten pulsado **BOOT**, inicia la carga
y sueltalo cuando comience a escribir. No borres toda la memoria ni cambies eFuses.

## Cableado usado por este programa

Guia local para el propietario; no forma parte de la web de Keirost.
Conectar siempre con el USB desenchufado. Alimentar el RC522 a **3,3 V**, nunca a VIN/5 V.

| RC522 | ESP32 |
| --- | --- |
| 3.3V | 3V3 |
| GND | GND |
| SCK | D18 / GPIO18 |
| MISO | D19 / GPIO19 |
| MOSI | D23 / GPIO23 |
| SDA / SS | D21 / GPIO21 |
| RST | D22 / GPIO22 |
| IRQ | Sin conectar |

Usa las etiquetas de los pines, no la posicion fisica de otra placa.
Los cables y las soldaduras deben estar firmes. La tarjeta debe ser compatible
con el RC522 (por ejemplo, la tarjeta o llavero de 13,56 MHz del kit).

## Controlador USB

Esta placa usa **CP2102**. El controlador oficial se obtiene de
[Silicon Labs](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers).
Windows debe mostrar **Silicon Labs CP210x USB to UART Bridge (COM...)** sin errores.

La carpeta `work/` contiene descargas y compilaciones locales; no se publica.
