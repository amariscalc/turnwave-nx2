# Turnwave NX2 v0.5.0 — Native + MusicKit catalog IDs + playback probe

**Estado: prototipo fuente. La compilación con ounce-devkit y el funcionamiento real
sobre Nintendo Switch 2 están pendientes de validación.** No es un NRO ya probado
ni incluye descifrado de DRM. Esta revisión en GitHub sucede al prototipo v0.4 y admite
actualizar su puente Chromium sin restaurar respaldos anteriores.

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
