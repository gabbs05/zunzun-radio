# ESTADO — capa de radio ZUNZUN

Última actualización: 25/08/2026

## Qué es esto

Banco de pruebas del enlace de radio para el auricular receptor de ZUNZUN.
Dos placas: **nRF52840-DK** transmite, **ESP32-S3** recibe.

No es el diseño final. El Camino B aprobado por Gonzalo usa sub-1 GHz
(915 MHz) propietario. Esto es el ensayo del **modelo de difusión**:
unidireccional puro, sin conexión, sin acuses de recibo, sin
retransmisión. Lo que se aprenda aquí sobre trama, troceado, buffering y
medida es transferible; los parámetros de radio no.

## Por qué periodic advertising y no Auracast

La nRF52840 es Bluetooth 5.0 **sin canales isócronos**. No hace LE Audio,
así que Auracast con BIS y subgrupos queda descartado con esta placa
(haría falta una nRF5340 Audio DK). Los samples `nrf_auraconfig` e
`iso_combined_bis_and_cis` están en el SDK pero no compilan para este
target.

El ESP32-S3 tampoco hace LE Audio. Sí recibe periodic advertising, que es
lo que se usa aquí.

Alternativa descartada: **ESB propietario** (`esb_ptx` / `esb_prx`). Más
cercano al sub-1 GHz final, pero el ESP32 no habla ESB y sólo hay una
placa Nordic.

## Estado actual — MEDIDO

| Métrica | Valor |
|---|---|
| Paquetes | 7 /s |
| Carga útil | 100 bytes (102 con cabecera) |
| Caudal | **714 B/s** |
| Periodo real | ~143 ms |
| Truncados (`data_status != 0`) | 0 |
| RSSI | −40 a −43 dBm |

Punto de partida: 3 bytes cada 1200 ms = **2,5 B/s**. Factor de mejora
~285.

**Objetivo:** ~12.500 B/s para cuatro canales Opus a 24 kbps. Falta un
orden de magnitud. Palancas restantes: subir la carga de 100 a ~250
bytes, y bajar el intervalo.

## Entorno

- **NCS v3.4.0** en `~/ncs`, toolchain `fbf7391cab`
- **ESP-IDF v5.5.5** en `~/esp/esp-idf`
- VS Code con extensión de Nordic (la de Espressif NO está instalada;
  el ESP32 va por línea de comandos)
- VS Code corre como **flatpak** — no dio problemas, pero tenerlo presente

Los dos entornos **no se pueden activar en la misma terminal**. Usar
terminales separadas: la de VS Code trae el de Nordic; para el ESP32,
`source ~/esp/esp-idf/export.sh` en una terminal limpia.

## Placas y puertos

**Los puertos cambian entre sesiones según el orden de enchufe.** Usar
siempre `/dev/serial/by-id/`:

```bash
ls -l /dev/serial/by-id/
```

- ESP32-S3: `usb-Espressif_USB_JTAG_serial_debug_unit_B8:F8:62:E0:C2:58-if00`
- nRF52840-DK (consola): `usb-SEGGER_J-Link_001050223508-if00`

Serie de la DK para `west flash --dev-id`: `1050223508`. Board version
`PCA10056`.

## Comandos

Transmisor (nRF):
```bash
cd ~/zunzun_radio/periodic_adv
west build -b nrf52840dk/nrf52840 && west flash
```

Tras cambiar `prj.conf` hace falta `--pristine`, y con `--pristine`
**hay que volver a indicar la placa** (borra el CMakeCache):
```bash
west build --pristine -b nrf52840dk/nrf52840
```

Receptor (ESP32), en terminal con IDF activado:
```bash
cd ~/esp/pruebas/ble_periodic_sync
idf.py -p /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_B8:F8:62:E0:C2:58-if00 flash
```

Consola (cualquiera de las dos):
```bash
python3 -m serial.tools.miniterm --exit-char 3 <ruta by-id> 115200
```

## Premisas falsas y trampas encontradas

| Premisa | Realidad |
|---|---|
| La DK se desconectaba sola por cable o USB defectuoso | **El interruptor de alimentación estaba mal puesto.** Con la palanca fuera de VDD enumera en modo degradado (`1366:0101`), sin puerto serie, y se cae a los ~7 s. En VDD enumera como `1366:1061`, modo compuesto, con dos `ttyACM`. Síntoma: LED5 parpadea rápido |
| El flatpak de VS Code impedía grabar | No. Faltaba el **J-Link Software Pack de SEGGER** (`libjlinkarm.so`). `nrfutil` detecta la placa por USB pero sin esa librería no puede grabar. Aviso: `JLinkARM DLL not found` |
| `Unable to find a board` significa que no está conectada | Puede significar AP-Protect activo. Se resuelve con `west flash --recover` (borra toda la flash) |
| Los datos del anuncio periódico se configuran con un símbolo de longitud | **No con el SoftDevice Controller.** `BT_CTLR_ADV_PER_DATA_LEN_MAX` no existe. El parámetro real es `BT_CTLR_SDC_PERIODIC_ADV_EVENT_LEN_DEFAULT` (tiempo de aire por evento, en µs), y **requiere activar además su `_OVERRIDE`** |
| El sample del ESP32 no sincronizaba por el nombre del dispositivo | Filtraba por **SID**: `if (disc->sid == 2 ...)` en la línea ~106. Zephyr no fija SID, así que emite con 0 |
| `params.skip = 10` en el receptor no afecta a la medida | Multiplica el intervalo aparente. Puesto a 0 para audio |
| `cp -r` de un sample de Zephyr es inocuo | Arrastra el `build/` si ya se compiló, y el `CMakeCache` heredado hace que CMake compile **dentro del árbol del SDK** |

## Configuración que importa (`periodic_adv/prj.conf`)

```
CONFIG_BT_CTLR_SDC_PERIODIC_ADV_EVENT_LEN_DEFAULT_OVERRIDE=y
CONFIG_BT_CTLR_SDC_PERIODIC_ADV_EVENT_LEN_DEFAULT=50000
```

50.000 µs = 50 ms de aire reservado por evento. Es el parámetro primario
de cuántos datos caben (lo dice la ayuda del propio Kconfig). Sin él:
`opcode 0x203f status 0x07` (memoria insuficiente en el controlador),
`err -5` en el host, y `data_length: 0` en el receptor.

**Cuidado:** ese tiempo se reserva dentro de cada intervalo. Al bajar el
intervalo hay que vigilar que no se solapen.

Símbolos que NO existen en este contexto y abortan la compilación:
`BT_CTLR_ADV_PER_DATA_LEN_MAX`, `BT_PER_ADV_SYNC_BUF_SIZE` (es del
receptor), `BT_CTLR_ADV_EXT`.

## Instrumentación del receptor

`print_periodic_adv_data` se reescribió para acumular y emitir **una
línea por segundo**:

```
7 paq/s  714 B/s  rssi -40  trunc 0  seq 1e
```

El volcado hexadecimal original imprimía 9 líneas por paquete. A
115200 baudios eso se convierte en el cuello de botella y falsea la
medida en cuanto se suben los parámetros. También se eliminó la línea
`Periodic adv report event:` del `case`.

`trunc` cuenta los paquetes con `data_status != 0`.

## Pendientes

1. Subir la carga útil de 100 a ~250 bytes y medir
2. Bajar el intervalo por debajo de 143 ms y medir — **una variable a la
   vez**, y vigilando `trunc`
3. Diseñar la trama: cabecera con nº de secuencia e identificador de
   idioma + carga
4. Decidir el códec. Propuesta abierta: Opus 16 kHz mono ~24 kbps, tramas
   de 20 ms, codificado en el Orange Pi (la nRF sólo reenvía bytes)
5. Enlace Orange Pi → nRF. `config.py` conserva `UART_PORT="/dev/ttyS3"`
   a 115200 (~11 kB/s útiles) de la arquitectura anterior — habría que
   confirmar si basta o hay que pasar a SPI/USB
6. Botón de cambio de canal en el ESP32
7. El J-Link no expone `ttyACM` en modo degradado — comprobado que en VDD
   sí. Si vuelve a faltar, mirar firmware del J-Link

## Nota sobre latencia

El pipeline del Orange Pi está en **4,3–4,8 s** de extremo a extremo. La
latencia del enlace de radio (decenas de ms) es irrelevante frente a eso
y **no es el cuello de botella**. Lo que sí importa del enlace es que **no
se corte**: un hueco en el audio molesta mucho más que 100 ms extra de
retardo. Medir pérdida de paquetes, no latencia.
