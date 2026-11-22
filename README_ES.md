> **Nueva arquitectura nativa 0.7:** el prototipo para Switch 2 **sin Chromium ni EXT4** está en [standalone/](standalone/README.md). Incluye reproducción de WAV PCM local y búsqueda de metadatos de Apple Music con una credencial de desarrollador. La reproducción completa protegida de Apple Music no está implementada y falta validar el NRO en la consola.

# Turnwave NX2 v0.6.0 — Native + MusicKit catalog IDs + playback probe

**Estado: prototipo en pruebas reales sobre Switch 2.** La interfaz nativa
y el salto a Chromium ya se han verificado; la sincronización de biblioteca
y la reproducción siguen sin validar. No incluye descifrado de DRM.

## Correcciones de 0.6.0

La version nativa nueva debe aparecer como **0.6.0-dev** en hbmenu y
mostrar **BUILD 0.6.0** arriba a la derecha. Si no ves esas dos señales,
el NRO que ejecutas todavia es el anterior. El script `build-native.sh`
borra los objetos de compilacion temporales de Docker y no reutiliza
un NRO antiguo cuando falla la compilacion.

**Turnwave usa ahora un perfil independiente en Chromium:**
`/switch/chromium-nx-diagnostico/tw-profile-v06.ext4`.
Chromium lo creara automaticamente desde la plantilla incluida en el
ejecutable al iniciar Turnwave con X. No hay que borrar, renombrar,
reparar ni sustituir `profile.ext4`. El perfil nuevo estara vacio y
tendras que iniciar sesion en Apple Music otra vez.

Esto solo modifica el perfil elegido por Turnwave: si abres Chromium
directamente todavia intentara usar su perfil original, que puede
seguir fallando. La creacion del nuevo perfil y el login deben
verificarse con una prueba real en la consola.

**Actualizacion completa:** ejecuta `git pull --ff-only origin main`,
`./tests/syntax-check.sh`, `./build-native.sh`, vuelve a instalar
`chromium-bridge/install.py` en el arbol de Chromium y ejecuta
`docker exec -e SWITCH2=1 chromium-nx-diagnostico-build bash /project/gx/build-nx.sh`.
Copia **los dos NRO** a sus rutas correctas: el nativo en
`/switch/TurnwaveNX2/TurnwaveNX2.nro` y el ejecutable de GX en
`/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro`.

En caso de problemas, revisa `native.log`, `bridge.log` y
`chromium-nx.log`. No afirmes que la reproduccion de Apple Music
funciona solo porque el navegador o el inicio de sesion funcionan.
## Arquitectura

```text
TurnwaveNX2.nro (aplicación NATIVA: interfaz Liquid / mando)
  ├─ X: login/sincronización --> Chromium NX2 --browser --turnwave-login
  │     ├─ music.apple.com / Apple ID
  │     ├─ MusicKit JS lee la biblioteca ya autorizada
  │     ├─ biblioteca V2: ID de biblioteca + catalogId + nombre + artista
  │     └─ guarda library.tsv y solicita volver al NRO nativo
  └─ L/R hasta Songs, arriba/abajo, A: solicitud de canción concreta
        ├─ guarda play-request.txt (solo catalogId, SIN credenciales)
        └─ Chromium NX2 --browser --turnwave-play
             ├─ abre music.apple.com y reutiliza su sesión web
             ├─ MusicKit: await setQueue({song:catalogId}); await play()
             ├─ guarda playback.log: QUEUE / QUEUED / REQUESTED / ERROR / STATE
             └─ permanece en primer plano hasta que el usuario sale
```

**REPRODUCCIÓN:** La prueba de `setQueue`/`play` se ejecuta exclusivamente en
Chromium, no en la interfaz nativa. Una promesa cumplida por `play()` **no prueba
que haya sonido** ni que Widevine/CDM esté funcionando. Al salir de Chromium,
el audio no se mantiene en segundo plano. Integrar un motor de audio bajo la
interfaz nativa es un hito posterior y requiere reproducción real verificada.

Este diseño se inspira en la integración mediante la sesión oficial MusicKit
que hace [AMTV](https://github.com/megarohas/apple-music-android-tv), pero no
utiliza ni empaqueta Android, APKs, tokens o el código de descifrado.

## Instalación en WSL

1. Clona el repositorio o actualiza el código existente:

```bash
cd ~/switch2-dev
git clone https://github.com/amariscalc/turnwave-nx2.git
cd turnwave-nx2
# Para futuras actualizaciones: git pull origin main
```
2. Mantén intactas las imágenes EXT4 y el perfil de tu copia de Chromium.
3. Parchea el código fuente de Chromium (el instalador es idempotente y puede
   actualizar un puente v0.4 ya instalado):

```bash
cd ~/switch2-dev/turnwave-nx2
./tests/syntax-check.sh
python3 chromium-bridge/install.py ~/switch2-dev/chromium-nx-diagnostico --check
python3 chromium-bridge/install.py ~/switch2-dev/chromium-nx-diagnostico
```

4. Compila Chromium NX2 con el contenedor que ya tienes configurado:

```bash
cd ~/switch2-dev/chromium-nx-diagnostico
git diff --check
docker exec -e SWITCH2=1 chromium-nx-diagnostico-build bash /project/gx/build-nx.sh
find gx/dist-nx2 -name '*.nro' -ls
```

Coloca el NRO **de Chromium** en
`sdmc:/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro`.
Si tu build produce el nombre `TurnwaveNX2.nro` en la carpeta GX, renombra
**únicamente ese ejecutable GX** al nombre indicado; NO confundas ese archivo
con el NRO nativo de Turnwave.

5. Compila el **NRO nativo** desde WSL:

```bash
cd ~/switch2-dev/turnwave-nx2
./build-native.sh
```

Copia la salida `native/TurnwaveNX2.nro` a
`sdmc:/switch/TurnwaveNX2/TurnwaveNX2.nro` y ejecútala desde hbmenu.

**Configuración opcional:** si el `.nro` de Chromium está en otra ruta, crea
`sdmc:/switch/TurnwaveNX2/chromium-nro.txt` con la ruta absoluta `sdmc:/switch/...nro`.

## Recuperación del perfil Chromium tras exit code 21 / errores EXT4

El código original de Chromium NX ya intenta eliminar los bloqueos
`SingletonLock`, `SingletonCookie` y `SingletonSocket` al arrancar.
Si aparece `EXT4-fs error` junto con `ProcessSingleton` y salida 21,
**no borres** la sesión. Puede haber metadatos EXT4 dañados; hay que
inspeccionar/reparar la imagen sobre una copia fuera de la consola.

1. Cierra Chromium y kaiHEN; copia por USB o FTP a tu PC el archivo
   `/switch/chromium-nx-diagnostico/profile.ext4`. No modifiques el archivo
   de la consola mientras el navegador está abierto.
2. Desde WSL instala `e2fsprogs` si no está instalado y ejecuta:

```bash
sudo apt install -y e2fsprogs
cd ~/switch2-dev/turnwave-nx2
python3 tools/recover_profile.py ~/switch2-dev/profile.ext4
```

3. El script conserva `profile.ext4.original` y genera
   `profile.ext4.repaired` en `~/turnwave-profile-recovery/run-...`.
   Ejecuta `e2fsck` **solo en una copia**, limpia bloqueos temporales y
   comprueba la consistencia del sistema de archivos. No se borran
   cookies ni contraseñas intencionadamente, aunque `e2fsck` podría
   descartar estructuras dañadas. Guarda el respaldo.
4. Si informa `RECOVERY OK`, cierra cualquier app que use el perfil y
   copia el archivo reparado de nuevo a
   `/switch/chromium-nx-diagnostico/profile.ext4`.
   Arranca Chromium y verifica si conserva la sesión.

Si no puede recuperarlo, **no sobrescribas el original**. Guárdalo para
investigación; solo después, y por decisión expresa, se podrá arrancar
con una nueva imagen `profile.ext4` (con necesidad de iniciar sesión).

## Controles

- **X:** conectar Apple Music o sincronizar biblioteca mediante Chromium.
- **L/R:** cambiar Playlists, Albums, Songs, Artists y Recent.
- **Arriba/Abajo:** seleccionar un registro.
- **A (pestaña Songs):** intentar reproducir la canción seleccionada mediante
  el MusicKit de Chromium; requiere `catalogId` decimal válido.
- **A (otras pestañas o biblioteca vacía):** conectar y sincronizar.
- **Y:** volver a cargar `library.tsv` al regresar de Chromium.
- **B/+ (Turnwave nativo):** cerrar Turnwave. En Chromium el control de salida
  depende del navegador y del modo de pantalla usado.

## Diagnóstico cuando Chromium no vuelve automáticamente a Turnwave

Después del login, la sesión web puede estar iniciada sin que
`MusicKit.getInstance()` esté disponible o autorizado. Para separar
estos casos, el puente diagnóstico genera:

- `sdmc:/switch/TurnwaveNX2/bridge.log`: estados de MusicKit, consultas
  de biblioteca, guardado en SD y disponibilidad EME/Widevine.
- `sdmc:/switch/chromium-nx-diagnostico/chromium-nx.log`: debería registrar
  `gx: turnwave: CDP page injection queued` al inyectar el script.
  Que se encole no garantiza que JavaScript lo haya ejecutado.

`DIAG|WIDEVINE_ACCESS|NotSupportedError` indica que la configuración
EME de prueba fue rechazada; **no confirma por sí solo** la causa del
fallo de reproducción de Apple Music.

No se registran contraseñas, cookies ni tokens. Para instalar los
diagnósticos ejecuta otra vez `chromium-bridge/install.py` y recompila
**solo Chromium NX2**. No modifiques las imágenes EXT4 ni el perfil.

## Comparar Widevine 4.10.3112.0 con la instalacion nueva (solo lectura)

En las pruebas del 20 de noviembre, el CDM ARM64 se encontraba en una
imagen de **perfil**, no en `chromium-full.ext4`:

`/profile/WidevineCdm/4.10.3112.0/_platform_specific/linux_arm64/libwidevinecdm.so`

Una copia identificada como `profiledanado.ext4` contenia la biblioteca
(20.041.928 bytes). El perfil nuevo de v0.6 se crea vacio, por lo que
**no debe suponerse que incluye Widevine**. El estado `Enabled` que
mostro `chrome://media-internals` solo acredita el registro del CDM:
en pruebas posteriores hubo `failed to map segment`, `mmap ... refused:
-12` y `no veneer left for the svc`. La carga real no se confirmo.

Para comparar copias **offline** de imagenes EXT4, sin montar, reparar,
extraer credenciales ni escribir en la tarjeta SD, ejecutar en WSL:

```bash
sudo apt install -y sleuthkit
python3 tools/audit_widevine.py \
  /mnt/c/NintendoSwitch/profiledanado.ext4 \
  /mnt/c/NintendoSwitch/tw-profile-v06.ext4
```

Ajustar los nombres a las copias reales existentes; no se necesitan ambas
para inspeccionar una de ellas. Nunca sustituir una imagen con la otra.
El script enumera solo los nombres que contienen `Widevine` o `CDM`.

El puente v0.6 consulta EME **antes de la autorizacion MusicKit**, y
registra en `bridge.log`:

- `DIAG|EME_API|present`: la API EME esta expuesta.
- `DIAG|WIDEVINE_ACCESS|supported`: Chromium acepta los parametros EME.
- `DIAG|CDM_CREATE|starting`: se solicita la instancia MediaKeys.
- `DIAG|CDM_CREATE|success`: se creo MediaKeys; aun no prueba descifrado.
- `DIAG|CDM_CREATE|NotSupportedError` u otro error: fallo al inicializar CDM.

Si la prueba de MediaKeys falla, comprobar tambien el log de GX para
errores `mmap`, `veneer`, `dlopen`, `failed to map segment` o biblioteca
ausente. La diferencia entre registro, carga y descifrado es esencial.
Para desplegar esta instrumentacion hay que volver a ejecutar
`chromium-bridge/install.py` y recompilar **Chromium NX2**; el NRO
nativo de Turnwave no necesita cambios.

## Diagnostico reversible del mapeador GX para Widevine (NX2_CDM_DIAG_V1)

Si el log de Chromium muestra `no veneer left for the svc`,
`mmap ... refused: -12` o `failed to map segment`, no aumentes
`BRANCH_RANGE` ni cambies `--jitless` a ciegas. En el GX personalizado,
`adopt_image()` ya identifica si un pool no cabe dentro del alcance
de las instrucciones ARM64; `patch_syscalls()` no distingue una
imagen sin pool de otra que agoto su capacidad. El parche **solo de
diagnostico** etiqueta el motivo y registra reservas de entre
4 y 32 MiB con su procedencia real (`loader`, `other`, `unknown`).

```bash
cd ~/switch2-dev/turnwave-nx2
git pull --ff-only origin main
./tests/syntax-check.sh
python3 tools/instrument_gmem.py ~/switch2-dev/chromium-nx-diagnostico --check
python3 tools/instrument_gmem.py ~/switch2-dev/chromium-nx-diagnostico
python3 chromium-bridge/install.py ~/switch2-dev/chromium-nx-diagnostico
cd ~/switch2-dev/chromium-nx-diagnostico
docker exec -e SWITCH2=1 chromium-nx-diagnostico-build bash /project/gx/build-nx.sh
```

Guarda el NRO de GX recien compilado en
`/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro`
(con el mismo nombre y ruta que utiliza el Turnwave nativo).
**No modificamos las imagenes EXT4 ni la biblioteca Widevine.**
Se conserva `gx/gmem.c.before-turnwave-cdm-diag` para volver al
archivo original de antes de esta instrumentacion.

Despues de reproducir el error, busca en
`/switch/chromium-nx-diagnostico/chromium-nx.log`:

```text
NX2_CDM_DIAG origin=loader ...
NX2_CDM_DIAG origin=other ...
NX2_CDM_DIAG origin=unknown ...
NX2_CDM_DIAG veneer=no-image ...
NX2_CDM_DIAG veneer=exhausted ...
gx: diagnostic: ...
gx: mmap of ... refused: -12
```

`no-image`: el codigo no obtuvo una imagen con un pool valido.
`exhausted`: existia pool, pero no quedaba capacidad.
`origin=other`: la reserva fue clasificada como procedente de otro
codigo y se dirigio a la ventana de datos; **no implica por si mismo
una clasificacion incorrecta**. La combinacion de lineas del mismo
intento determinara si corresponde corregir la clasificacion o
las reservas, sin cambiar las reglas arbitrariamente.

## Cómo interpretar `playback.log`

Archivo: `sdmc:/switch/TurnwaveNX2/playback.log`.

- `REQUEST|123...`: la interfaz nativa ha guardado la solicitud correctamente.
- `PLAY|QUEUE|123...`: el puente JavaScript ha leído el ID y va a llamar `setQueue`.
- `PLAY|QUEUED|123...`: MusicKit ha resuelto la promesa de `setQueue`.
- `PLAY|REQUESTED|123...`: MusicKit ha resuelto la promesa de `play`; **no confirma sonido**.
- `PLAY|STATE|playing|7`: estado MusicKit y posición que informa la web, sin confirmar audio físico.
- `PLAY|ERROR|...`: un fallo que MusicKit ha comunicado (cola, sesión, API, etc.).

Si solo aparece `REQUEST`, investiga primero el arranque de GX, el CDP,
el login o la inyección de JavaScript. Si aparece `PLAY|ERROR`, analiza su
mensaje. Si aparece `REQUESTED` pero no se escucha nada, revisa el diagnóstico
multimedia/Widevine de Chromium antes de modificar Turnwave.

Los otros archivos son `sdmc:/switch/TurnwaveNX2/native.log` y
`sdmc:/switch/chromium-nx-diagnostico/chromium-nx.log`.

## Formato de biblioteca V2

`sdmc:/switch/TurnwaveNX2/library.tsv` es texto plano UTF-8 con cabecera:

```text
TURNWAVE_LIBRARY_V2
S\ti.LIBRARY_ID\t123456789\tExample Track\tExample Artist
```

Los tipos son `P` playlist, `A` álbum, `S` canción, `R` artista y `N` recientes.
El archivo guarda **metadatos**, no credenciales. `catalogId` puede estar vacío:
no se inventará a partir de `libraryId`. La aplicación sigue pudiendo leer
bibliotecas V1 antiguas, pero esas canciones no son reproducibles por ID hasta
una nueva sincronización.

El puente limita la biblioteca inicial a 180 registros. No implementa aún
paginación completa ni álbumes/carátulas ni la vista Now Playing nativa.

## Límites y riesgos conocidos

- El chainload entre dos `.nro` de hbmenu, especialmente el regreso automático,
  depende del loader que estés usando y no se ha verificado en hardware.
- Chromium NX2 usa un perfil web propio; no se garantiza que la autorización
  del sitio sobreviva a cambios de perfil o errores de EXT4.
- La salida de audio, EME y Widevine de Switch 2 siguen pendientes de una
  prueba real. No se incluye CDM ni se intenta eludir DRM.
- Todo funciona en primer plano: no hay Chromium oculto reproduciendo mientras
  Turnwave nativo pinta sus ventanas.
- Si hay errores de compilación debido a cambios en GX, captura el error
  exacto del compilador, sin modificar el perfil.

## Fuentes y referencias

- Turnwave NX2: https://github.com/amariscalc/turnwave-nx2
- Chromium NX: https://github.com/shodowlo/chromium-nx
- AMTV: https://github.com/megarohas/apple-music-android-tv
- MusicKit: https://developer.apple.com/musickit/
