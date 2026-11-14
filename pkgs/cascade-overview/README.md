# Cascade Overview

El cierre animado devuelve el cursor al modo de hardware antes del siguiente frame, evitando un render anidado que dejaba el monitor nulo y hacía caer Hyprland. Se conserva el cursor de la aplicación al salir y se liberan los bloqueos antes de desactivar un monitor.

Plugin nativo de Hyprland para organizar escritorios en grupos verticales y recorrer horizontalmente los escritorios de cada grupo. El overview dibuja miniaturas reales de las ventanas y permite seleccionarlas y moverlas con el ratón. Los grupos y los escritorios son dinámicos; diez es el número de atajos numéricos disponibles, no el límite del modelo.

Las tarjetas de escritorio usan un relleno gris tenue y translúcido, contornos plateados visibles y texto grafito contrastado. El fondo general es transparente, sin velo de color, y aplica un único desenfoque nativo; el relleno y el reflejo superior quedan dentro de las tarjetas, sin desenfocar cada miniatura por separado. Las ventanas conservan los colores de su contenido.

Al arrastrar una ventana o el título de un escritorio aparece una miniatura real junto al cursor. Incluye las ventanas del escritorio, sigue el puntero directamente y permanece dentro del monitor, también con escala fraccionaria. El origen se atenúa y se señala el destino. Al soltar, la miniatura aterriza suavemente en su posición; un destino inválido la devuelve al origen. Escape y el cierre de la ventana cancelan el arrastre.

Al entrar, las ventanas del escritorio activo pasan de su posición y tamaño reales a sus miniaturas, mientras el escritorio se reduce hasta encajar en su tarjeta. Al elegir otro escritorio, su tarjeta y sus ventanas se expanden hacia la pantalla; esa transición sustituye al deslizamiento nativo para evitar dos animaciones simultáneas. Escape vuelve al escritorio activo. Los espacios especiales, que no tienen tarjeta, conservan el fundido.

El desenfoque comienza antes que el zoom y se retira al final del cierre. Los títulos y controles aparecen cuando el encaje está avanzado. Los demás escritorios aparecen y desaparecen juntos, con un fundido más pausado que el zoom. La apertura y el cierre tienen algo más de duración; conservan el movimiento amortiguado, su posición y su velocidad al cambiar de destino, también en la navegación y el reordenamiento. El cierre sigue siendo más rápido que la entrada. Las animaciones no dependen de la frecuencia del monitor y `animate=false` las resuelve inmediatamente. Las superficies y las texturas de texto se reutilizan, también durante el zoom y el aterrizaje. Al quedar quieto, Cascade deja de solicitar frames: los cambios de las superficies visibles, el ratón y las transiciones vuelven a activar el renderizado.

Fuera del overview, el gesto horizontal recorre tantos escritorios como se desplace la mano, sin levantar los dedos entre ellos. El recorrido es circular dentro del grupo: del último se pasa al primero y del primero al último, conservando el orden visual aunque los IDs nativos estén reordenados. No se crean escritorios adicionales al completar una vuelta. Cada escritorio completo queda confirmado; cancelar revierte solo el tramo pendiente, como en el gesto continuo nativo de Hyprland.

El primer movimiento, los cambios de vecino y el paso a otro escritorio se aplican inmediatamente; las muestras restantes se agrupan por frame, sin un filtro que retrase el seguimiento. Se conserva el movimiento sobrante al cruzar un escritorio, también al invertir el gesto. Un gesto nuevo retoma la posición visible si la transición anterior aún continúa. Soltar confirma desde la posición arrastrada y respeta la curva de animación configurada. Dentro del overview se conserva la navegación de tarjetas existente.

Cada grupo termina en un único escritorio vacío disponible para entrar con el gesto, sin abrir el overview ni crearlo manualmente. Al abrir o mover una ventana a ese último escritorio aparece otro vacío a su derecha. Si quedan vacíos consecutivos al final, se conserva el del extremo derecho y se retiran los anteriores hasta encontrar un escritorio con ventanas. Esto se aplica también al crear escritorios manualmente, cerrar o mover ventanas y restaurar la sesión. Los vacíos interiores separados del extremo por un escritorio ocupado conservan su orden y sus IDs.

Si se retira el escritorio activo, se entra en la reserva superviviente del mismo grupo. La selección que se esté previsualizando en otro grupo del overview se conserva. La limpieza se agrupa al finalizar los eventos de ventanas; no se cierran aplicaciones ni se eliminan grupos.

La fuente se incluye localmente en estos dotfiles. Deriva de [Hyprspace](https://github.com/KZDKM/Hyprspace) y de la adaptación para la API actual de Hyprland de [ImanolBarba/Hyprspace](https://github.com/ImanolBarba/Hyprspace), revisión `0799be7464fac7ea959b7c6c7809dadd6c21c5aa`, usada por el paquete anterior. Se conserva la licencia [GNU GPL versión 2](LICENSE). Cascade añade su modelo de grupos, persistencia, disposición e interacción.

## Controles de la sesión

| Control | Acción |
| --- | --- |
| Super+Tab | Abrir o cerrar Cascade |
| Tres dedos horizontal | Escritorio anterior o siguiente |
| Tres dedos vertical | Grupo anterior o siguiente |
| Cuatro dedos vertical | Abrir o cerrar el overview |
| Super+Ctrl+←/→ | Recorrer escritorios del grupo |
| Super+Ctrl+↑/↓ | Recorrer grupos |
| Super+Ctrl+Shift+flecha | Mover la ventana activa al destino |
| Super+1…9, Super+0 | Elegir escritorio 1…10 del grupo actual |
| Super+Shift+1…9, Super+Shift+0 | Mover ventana al escritorio 1…10 del grupo |
| Super+rueda | Escritorio anterior o siguiente |
| Super+Shift+rueda | Mover ventana al escritorio anterior o siguiente |
| Super+D | Lanzador de aplicaciones |

Los atajos numéricos crean los escritorios que falten hasta la posición elegida. Alt+Tab, los controles multimedia, las capturas y el selector de ventanas de Super+Grave conservan su configuración existente. Los gestos de tres y cuatro dedos anteriores se retiran para que un mismo gesto tenga un solo receptor.

## Controles dentro del overview

| Control | Acción |
| --- | --- |
| Flechas | Seleccionar escritorio o grupo |
| Shift+flechas | Mover la ventana activa y navegar al destino |
| Enter | Entrar al escritorio o enfocar la ventana seleccionada |
| Escape | Cerrar |
| Tab / Shift+Tab | Recorrer ventanas del escritorio seleccionado |
| N / Ctrl+N | Nuevo escritorio / nuevo grupo |
| Delete / Ctrl+Delete | Borrar escritorio vacío / grupo vacío |
| Alt+←/→ | Reordenar el escritorio |
| Alt+↑/↓ | Reordenar el grupo |

Se puede pulsar una tarjeta para entrar al escritorio, pulsar una ventana para enfocarla y arrastrar una miniatura a otro escritorio, incluida la reserva vacía, o a una tarjeta de otro grupo. El overview muestra los escritorios del grupo y su reserva automática, sin una tarjeta «+ Escritorio» adicional ni botones de borrado en cada escritorio. Los títulos interiores de escritorios se arrastran para reordenarlos o transferirlos entre grupos. Encima de cada fila quedan los controles ↑ y ↓ para reordenar el grupo y × para eliminarlo si está vacío; sus símbolos se centran según el tamaño real de la textura. El control de nuevo grupo muestra solo «+».

No hay nombres de grupo, editor F2, cabecera, pie ni ayudas de texto alrededor de los paneles. Los textos de escritorio y ventana permanecen dentro de sus tarjetas; los errores temporales aparecen dentro del escritorio seleccionado. Los grupos nuevos reciben únicamente una etiqueta interna automática para conservar el formato de estado anterior. Se rechaza eliminar escritorios con ventanas o grupos con ventanas; siempre queda un grupo con un escritorio.

## API e inspección

Lua registra `hl.plugin.cascade.toggle`, `open`, `close`, `navigate` y `command`. Los argumentos son strings. `toggle`, `open` y `close` aceptan `""` para el monitor actual o `"all"` para todos. Las funciones devuelven éxito o un error; los callbacks de los dotfiles comprueban que el plugin esté cargado antes de invocarlo.

```lua
hl.plugin.cascade.toggle("")
hl.plugin.cascade.navigate("right")
hl.plugin.cascade.navigate("move-down")
hl.plugin.cascade.command("group-new")
```

En esta sesión Lua, `hyprctl eval` permite automatizar las mismas acciones:

```sh
hyprctl eval 'assert(hl.plugin.cascade.toggle(""))'
hyprctl eval 'assert(hl.plugin.cascade.open(""))'
hyprctl eval 'assert(hl.plugin.cascade.navigate("right"))'
hyprctl eval 'assert(hl.plugin.cascade.navigate("move-down"))'
hyprctl eval 'assert(hl.plugin.cascade.command("group-new"))'
hyprctl eval 'assert(hl.plugin.cascade.command("desktop-new"))'
hyprctl eval 'assert(hl.plugin.cascade.command("group-up"))'
hyprctl eval 'assert(hl.plugin.cascade.command("group-down"))'
hyprctl eval 'assert(hl.plugin.cascade.command("desktop-left"))'
hyprctl eval 'assert(hl.plugin.cascade.command("desktop-right"))'
hyprctl eval 'assert(hl.plugin.cascade.command("desktop-to 2"))'
hyprctl eval 'assert(hl.plugin.cascade.command("desktop-select 3"))'
hyprctl eval 'assert(hl.plugin.cascade.command("window-to 3"))'
hyprctl eval 'assert(hl.plugin.cascade.command("desktop-delete"))'
hyprctl eval 'assert(hl.plugin.cascade.command("group-delete"))'
hyprctl eval 'assert(hl.plugin.cascade.close(""))'
```

`desktop-to` recibe el ID del grupo de destino. `desktop-select` y `window-to` reciben una posición desde 1 dentro del grupo actual. Las posiciones 1…10 pueden crear escritorios; una posición mayor selecciona uno que ya exista. `navigate` admite `left`, `right`, `up`, `down` y las mismas direcciones con el prefijo `move-`.

Para capturar y contrastar el estado del monitor actual:

```sh
hyprctl cascade > /tmp/cascade-estado.json
hyprctl cascade | jq .
hyprctl clients -j > /tmp/cascade-ventanas.json
hyprctl monitors -j > /tmp/cascade-monitores.json
```

El estado de grupos es independiente por monitor y se comparte entre sus aplicaciones. Se guarda en `$XDG_STATE_HOME/cascade-overview/<nombre-monitor-saneado>.state`, o en `~/.local/state/cascade-overview/` cuando no se define `XDG_STATE_HOME`. Guarda nombres, orden y selección, sin guardar contenido de ventanas. No se reserva un bloque de diez IDs por grupo. Si Hyprland reutiliza en otro monitor el ID de un escritorio vacío, Cascade adapta esa asociación conservando el orden y sin trasladar ventanas. Al reconciliar la sesión se incorporan los workspaces normales existentes sin cerrar sus ventanas.

## Configuración y construcción

Las opciones Lua están bajo `plugin.cascade`: `animate`, `gestures`, `reverseSwipe`, `cardWidth` y `rowHeight`. Los dotfiles usan animación y gestos activos, desplazamiento sin invertir y tarjetas de 480 con filas de 340 unidades lógicas; la geometría se adapta a monitores pequeños. El plugin se carga después del primer parseo y se recarga la configuración una vez disponible su API.

Desde la raíz de los dotfiles:

```sh
nix build .#cascade-overview
```

El paquete se llama `cascade-overview-1.0.0`; conserva el archivo `lib/libHyprspace.so` para la ruta de carga de la sesión. Usa el mismo `pkgs.hyprland` y sus headers, el compilador GCC 14 y el lock existente. `checkPhase` ejecuta `make test`; las pruebas de modelo y layout también se pueden ejecutar con `make -C pkgs/cascade-overview test` cuando hay un compilador C++23 disponible.

## Validación y límites

Las 22 pruebas unitarias cubren el modelo, la geometría, las fases de zoom/desenfoque y el movimiento a 60/120 Hz con los tiempos de entrada, salida y fundido. Las 67 pruebas gráficas crean una sesión Wayland aislada; comprueban también el fundido simultáneo de los demás escritorios al entrar y salir. El modo anidado conserva una salida con cursor de hardware para cubrir la ruta del crash. El modo headless sigue disponible. No utilizan el socket ni las ventanas de la sesión principal:

```sh
nix build .#cascade-overview
# Con foot, grim y wtype disponibles en PATH:
python3 pkgs/cascade-overview/tests/run-runtime.py ./result
# Cursor de hardware y cierre animado:
python3 pkgs/cascade-overview/tests/run-runtime.py ./result --nested
```

El ejecutor conserva los resultados en un directorio `/tmp/cascade-*` y cierra su sesión al terminar. `--hyprglass /ruta/hyprglass.so` añade la configuración real de HyprGlass y sus reglas para las ventanas de prueba; se mantiene tras recargar. Comprueba creación, orden, borrado seguro, restauración al recargar, teclado, arrastre de ventanas y escritorios, reordenamiento de grupos con sus botones, fullscreen, escala fraccionaria, miniaturas actualizadas, varios monitores, cierre animado, desactivación de salidas y restauración del cursor al cerrar o descargar. Las pruebas de cristal y arrastre contrastan los píxeles reales de las miniaturas, su seguimiento del cursor, la transparencia, las posiciones intermedias y la cancelación. Los gestos se pueden emitir por fases: las pruebas verifican posiciones y capturas de pantalla mientras el gesto sigue activo, dirección con IDs invertidos, reversión, cancelación y descarga durante un gesto. El controlador solo puede cargarse en estas sesiones aisladas; el desplazamiento de dos dedos queda libre para las aplicaciones. Una prueba automática de input no confirma la sensación del touchpad físico.

Los plugins dependen de la ABI de Hyprland: al actualizar el compositor hay que reconstruir Cascade y comprobar la carga. La reconstrucción de Nix no debe reiniciar greetd: `restartIfChanged = false` conserva la sesión abierta. Estas instrucciones no activan ni reinician la sesión principal.
