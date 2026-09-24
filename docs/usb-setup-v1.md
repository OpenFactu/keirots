# Configuracion USB v1

UART 115200, UTF-8, un objeto JSON por linea (maximo 8192 bytes incluyendo salto).
Solicitudes: `protocol: "keirost-usb-v1"`, `id: UUID`, `command`.
Respuestas: mismo protocolo/id y `ok: boolean`. No consumir texto de arranque
como respuesta. Nunca registrar cuerpos enviados ni respuestas de terceros.

- `hello` / `status`: firmware, configured, wifi, bootHeld, state. Sin secretos.
  Desde 0.2.1 anuncia `cardCapture: true`.
- `configure`: serverUrl (origen HTTPS), tenantId, deviceToken, ssid, password,
  caCert opcional (PEM de CA privada, maximo 4096 bytes). Exige BOOT pulsado.
  `ok` significa guardado local, no conexion validada. Soltar BOOT reinicia.
  Errores fijos: BOOT_REQUIRED, INVALID_CONFIG, STORAGE_ERROR.
- `capture_start`: `{captureId: UUID}` abre una ventana de alta de 30 segundos.
  Solo se consume la siguiente presentacion nueva. Mientras esta activa no se
  envian lecturas al kiosco. Una sesion distinta recibe CAPTURE_BUSY; repetir
  la misma no amplía la caducidad. No cambia configuracion ni requiere BOOT.
- `capture_poll`: mismo captureId, devuelve captureState waiting/captured/expired.
  Solo captured incluye uid hexadecimal en mayusculas (4, 7 o 10 bytes).
  Una sesion ajena recibe CAPTURE_INVALID. La primera tarjeta no se sustituye.
- `capture_cancel`: mismo captureId; borra UID y sesion RAM y termina la captura.
  La caducidad tambien borra los datos, incluso si el navegador desaparece.
  El navegador cancela al completar/cancelar/cerrar/cambiar de empleado, y cierra
  el puerto. Leer no asigna automaticamente: se usa el guardado existente de la ficha.

El cliente web tiene un solo comando pendiente, correlaciona id y limita
respuestas a 4096 caracteres. Ignora lineas ajenas. Cierra el puerto al salir.
El firmware drena lineas por fragmentos y dispone de un RX de 8192 bytes.

Comprobaciones ejecutables:
- `node --test scripts/timeclock-usb.test.mjs` en apps/web de la plataforma.
- Compilacion Arduino de este sketch incluye `src/PolicyChecks.cpp`: estados
  reintentables, limites de espera/caducidad y wraparound de millis.
- Placa: comprobar hello/status sin configurar y configure sin BOOT (rechazo).
- Prueba final: usuario configura su Wi-Fi en Keirost, se observa ready,
  tarjeta en kiosco correcto, retirada y error de credencial desconocida.

No hay protocolo de lectura de token ni volcado de configuracion. No hay
comando USB para fichar ni inyectar UID. El UID de alta solo sale como respuesta
solicitada de capture_poll; nunca en logs. Sin PIN ERP ni conexion a base de datos.
