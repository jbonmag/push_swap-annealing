# push_swap-annealing

[English](README.md) · **Español**

> *La mayoría de push_swap siguen una receta. Este se forja.*

Una solución al *push_swap* de 42 que trata la ordenación como un trabajo de herrería. En vez de seguir un procedimiento fijo, calienta una solución en bruto, la martillea miles de veces por segundo y la deja enfriar despacio hasta que la hoja toma su forma. La técnica se llama **recocido simulado** (*simulated annealing*), y su nombre viene justo de ahí: de lo que hace un herrero con el acero.

| | n = 100 | n = 500 |
|---|---|---|
| **Esta hoja** | **400,4** (372–425) | **3274,9** (3145–3452) |
| Límite de 42 para la nota máxima | 700 | 5500 |

Son medias sobre las mismas 20 entradas aleatorias con semilla en cada tamaño, y un verificador independiente comprueba cada salida. Forjar una hoja lleva unos 0,9 s con n = 100 y 1,6 s con n = 500. Si se deja más tiempo en el yunque, el filo se afina, hasta 382,3 / 3119,4 en unos 20 s: ver [Cuánto tiempo en el yunque](#cuánto-tiempo-en-el-yunque).

---

## Contenido

1. [El mineral: el ejercicio](#el-mineral-el-ejercicio)
2. [El encargo: nuestro reto](#el-encargo-nuestro-reto)
3. [La forja: cómo funciona](#la-forja-cómo-funciona)
4. [Cuánto tiempo en el yunque: rapidez frente a longitud](#cuánto-tiempo-en-el-yunque)
5. [Del hierro viejo al acero: cómo llegamos aquí](#del-hierro-viejo-al-acero-cómo-llegamos-aquí)
6. [La chatarra: lo que no funcionó](#la-chatarra-lo-que-no-funcionó)
7. [La hoja perfecta: ¿a qué distancia del óptimo?](#la-hoja-perfecta-a-qué-distancia-del-óptimo)
8. [Las normas del gremio: cumplimiento](#las-normas-del-gremio-cumplimiento)
9. [Probar el filo: compilar, ejecutar y probar](#probar-el-filo-compilar-ejecutar-y-probar)
10. [El taller: estructura del repositorio](#el-taller-estructura-del-repositorio)
11. [Licencia](#licencia)

---

## El mineral: el ejercicio

*push_swap* es un proyecto del currículo de la escuela 42. Aquí se resume con nuestras palabras.

Se recibe una lista de enteros distintos en una pila **a**, y una pila **b** vacía. Hay que imprimir una lista de operaciones que deje **a** ordenada de menor a mayor, con el menor en la cima, y **b** vacía. Hay once operaciones, las únicas herramientas del banco:

| Operación | Efecto |
|---|---|
| `sa`, `sb`, `ss` | Intercambia los dos primeros elementos de a, de b o de ambas |
| `pa`, `pb` | Pasa el primer elemento de b a a, o de a a b |
| `ra`, `rb`, `rr` | Rota hacia arriba: el primero pasa al final (a, b o ambas) |
| `rra`, `rrb`, `rrr` | Rota hacia abajo: el último pasa a la cima (a, b o ambas) |

El programa debe imprimir **la lista más corta que pueda**. En la corrección se prueba con entradas aleatorias de 100 y 500 números. La nota máxima pide menos de 700 operaciones con 100 números y no más de 5500 con 500. El código debe estar en C, seguir la guía de estilo de 42 (la *Norma*) y usar solo `read`, `write`, `malloc`, `free` y `exit`. Tampoco puede tener variables globales, cerrarse inesperadamente ni perder memoria, y debe imprimir `Error` ante una entrada inválida.

Lo habitual es un algoritmo directo, como la ordenación por bloques o cubos, la inserción del movimiento más barato o radix. Eso es fundir: se vierte el metal en el molde una vez, y la forma es la que da el molde. Es rápido, y es fijo.

## El encargo: nuestro reto

Este repositorio no es una entrega de la escuela. Es un intento personal de averiguar **cuántas operaciones hacen falta de verdad**, sin salirse de las reglas del enunciado:

- **Objetivos:** menos de 350 operaciones con 100 números y menos de 2500 con 500. Todavía no se ha alcanzado ninguno; las mejores cifras hasta ahora son 382 y 3119, con un presupuesto de 20 s.
- **Reglas:** todo lo que exige el enunciado. Eso supone un solo hilo (sin `pthread`, `fork` ni reloj), sin biblioteca matemática y respetando la Norma.
- **Tiempo:** el enunciado no fija ningún límite. Elegimos la rapidez: alrededor de un segundo por hoja por defecto, con forjas más largas a una línea de distancia ([Cuánto tiempo en el yunque](#cuánto-tiempo-en-el-yunque)).
- **Método:** cada experimento, incluida cada hoja que se partió, está anotado con sus cifras en [`research/BITACORA.md`](research/BITACORA.md), el cuaderno del herrero. Son 43 experimentos numerados y varios diagnósticos.

## La forja: cómo funciona

### 1. La anatomía de una hoja: cuatro decisiones

Toda solución que forja este programa tiene dos fases. En la **fase de push**, algunos elementos pasan de a a b. En la **fase de pull**, vuelven todos, cada uno a su sitio ordenado. Una solución queda descrita por cuatro decisiones:

| Parte | Significado |
|---|---|
| **keep** | Elementos que nunca salen de a. Deben estar ya en orden creciente circular. |
| **sigma** | El orden en que salen de a los demás. |
| **pi** | El orden circular que forman en b. |
| **tau** | El orden en que vuelven a a. |

Con estas cuatro, las operaciones salen solas. Cada movimiento lleva un elemento a su sitio en la otra pila, rotando ambas con la más barata de cuatro combinaciones: `rr` a la vez, `rrr` a la vez, o en sentidos opuestos. La búsqueda exacta en entradas pequeñas muestra que este diseño de dos fases casi no pierde nada: con n = 10, obligar a hacer todos los push antes que los pull cuesta 0,04 operaciones de media.

### 2. Calor y enfriamiento lento: el recocido simulado

**Qué es recocer.** Para *recocer* el acero, el herrero lo calienta y lo deja enfriar muy despacio. El enfriamiento lento da tiempo a los átomos para colocarse en una estructura ordenada y sin tensiones; si se enfría demasiado rápido, quedan congelados en una estructura desordenada y frágil. (Es el primo tranquilo del *temple*, el chapuzón brusco en agua que endurece la hoja. Nuestro algoritmo solo recuece.) El *recocido simulado*, de 1983, hace lo mismo con soluciones en vez de átomos.

**En nuestro yunque.** El programa golpea la solución una y otra vez: cada golpe es un pequeño cambio al azar, que se conserva si acorta la solución. Mientras el metal está caliente, también acepta algunos golpes que la alargan. Eso es lo que le permite salir de una mala forma, en vez de endurecerse en la primera que encuentra. La probabilidad de aceptar uno de esos golpes es la **temperatura**. Empieza alta, baja poco a poco, y al final solo pasan las mejoras. En el código, `SA_T0` y `SA_T1` son las temperaturas inicial y final, y "recalentar" es volver a meter el metal en el fuego.

Los martillazos incluyen:

- mover un elemento en sigma o tau;
- mover un bloque de elementos;
- meter o sacar un elemento de keep;
- reordenar pi;
- golpes compuestos, en los que un elemento se coloca junto al mismo vecino en pi y en uno de los dos órdenes.

### 3. 160 lingotes en el fuego

El martillo puede afinar una hoja, pero no convertir un puñal en un sable: el recocido no puede reorganizar la pila b a gran escala. La barra de partida decide buena parte del resultado. En una entrada de 500 números, distintos arranques dieron desde 3391 hasta 5332. Por eso el herrero nunca se fía de una sola barra:

- **Van al fuego 160 lingotes.** Cada uno es una forma en "anillo" construida alrededor de un rango. Barrido tras barrido, a entrega los rangos cada vez más alejados de él: los superiores a un extremo de b y los inferiores al otro. Los lingotes cambian en centro, sentido, número de barridos (de 1 a 10) y en cómo crece el ancho de cada barrido.
- **La peor mitad va a la chatarra, ronda tras ronda.** Se martillean los 160 un poco, se descarta la peor mitad, y los supervivientes reciben más martillazos. Es la eliminación por mitades, y se lleva el 30% del trabajo.
- **El lingote elegido vuelve al fuego.** El ganador se recalienta y recibe el 70% restante del trabajo.

### 4. El ojo del herrero: evaluación rápida

Entre golpe y golpe, el herrero tiene que juzgar la hoja: "¿cuánto mide esta solución?". Esa pregunta se hace cientos de miles de veces por segundo, así que tiene que ser barata:

- **Bitsets.** Cada paso necesita contar elementos entre dos posiciones, y eso son unas pocas operaciones de palabra.
- **Puntos de control.** Un golpe solo cambia una parte de la hoja, así que el evaluador retoma desde un estado guardado justo antes del cambio. Y abandona en cuanto el golpe es claramente malo.

Juntos llevaron la forja de unos 600 a más de 800.000 juicios por segundo con n = 500.

### 5. Afilado y pulido

- **Zigzags convertidos en swaps.** Una última pasada convierte patrones como `ra pb rra pb` en `sa pb pb`.
- **Entradas diminutas:** 2 y 3 elementos tienen código propio, y 5 elementos nunca necesitan más de 10 operaciones.
- **Entradas enormes,** de más de 512 números, recurren a radix. Hay barras demasiado grandes para esta forja.

### 6. El mismo acero, la misma espada

La forja se detiene tras una **cantidad fija de trabajo** (`SA_BUDGET` en `src/sa.h`), no tras un tiempo fijo. La misma entrada da siempre la misma salida, en cualquier máquina. Una máquina más rápida termina antes y una más lenta tarda más, pero forja exactamente la misma hoja.

## Cuánto tiempo en el yunque

*Rapidez frente a longitud.* La forja está ajustada para entregar una hoja en alrededor de un segundo. Más horas en el yunque dan un filo más fino. Esta sección muestra cuánto más fino, hasta dónde llega eso y por qué elegimos la rapidez.

**Qué significa "más tiempo".** Más tiempo son más martillazos. `SA_BUDGET` fija cuánto trabaja la forja, así que doblarlo dobla más o menos el tiempo. El motor de investigación da unos 1,8 millones de golpes por segundo con n = 100 y 0,6 millones con n = 500.

La misma forja con distintos presupuestos, sobre las mismas 20 entradas:

| `SA_BUDGET` | Tiempo ejecutando solo, n = 100 / 500 | n = 100 | n = 500 |
|---|---|---|---|
| **4·10⁷ (por defecto)** | **0,9 s / 1,6 s** | **400,4** | **3274,9** |
| 8·10⁷ | 1,7 s / 2,6 s | 396,9 | 3206,1 |
| 2·10⁸ | 4,4 s / 5,7 s | 392,0 | 3164,4 |
| 9·10⁸ | 20 s / 23 s | 382,3 | 3119,4 |

**Hasta dónde sigue mejorando:**

- **n = 500 es el que más gana al principio.** Pasar de 1,6 s a 5,7 s ahorra 110 operaciones (un 3,4%). Seguir hasta 23 s ahorra otras 45 (un 1,4%).
- **n = 100 mejora despacio pero sin parar,** unas 13 operaciones cada vez que la ejecución es diez veces más larga. Una versión anterior del motor de investigación, sin los lingotes en anillo, seguía mejorando tras 30 minutos por entrada, con una media de 371,4.
- **Así que no hay un punto final claro,** solo ganancias cada vez menores. A partir de unos 20-30 s, multiplicar el tiempo por diez compra como mucho unos pocos puntos porcentuales, y media hora en el yunque no es práctica.

**Por qué las ganancias se reducen.** El detalle fino del filo queda resuelto en el primer segundo: una búsqueda exacta sobre ventanas de 8-12 elementos ya no encuentra nada que mejorar. Lo que queda es la forma de la hoja entera, es decir, la forma de b y qué elementos se quedan en a. Eso solo cambia con golpes raros y coordinados, así que cada mejora tarda más en aparecer. Por eso también una hoja mucho mejor necesitaría una técnica nueva, no más martillazos (ver [La chatarra](#la-chatarra-lo-que-no-funcionó)).

**Por qué elegimos la rapidez.** En alrededor de un segundo, el programa responde tan rápido como se espera de él, y ya saca 400 / 3275, muy por debajo de los límites de 42 (700 / 5500). Las forjas largas compran el último 5%. Quien quiera el filo más fino puede subir `SA_BUDGET`. Cada valor forja siempre la misma hoja para la misma entrada.

La forja tiene un solo herrero: el enunciado no permite hilos ni procesos, así que el programa usa un solo núcleo.

## Del hierro viejo al acero: cómo llegamos aquí

Cada paso se midió sobre entradas emparejadas con semilla y comprobadas por un verificador. Las cifras son la media de operaciones para n = 100 / n = 500.

| Paso | n = 100 | n = 500 | Qué cambió |
|---|---|---|---|
| Punto de partida (heurísticas + búsqueda en haz sobre el orden del pull) | 456,8 | 3924,8 | Versión anterior |
| Recocido de la solución entera, primer prototipo | 406,8 (600 s) | — | Demostró que el supuesto "suelo" de 420-440 no era real |
| Evaluación con bitsets y puntos de control | 400 (60 s) | 3427 (60 s) | Unas 600 veces más juicios por segundo |
| Lingotes iniciales en competición | 392 (60 s) | 3302 (60 s) | La forma de b pesa más que cualquier ajuste fino |
| Ajuste sobre 20 entradas emparejadas | 383,8 (60 s) | 3235,5 (60 s) | La mayor parte del trabajo va al lingote elegido |
| Paso a C con la Norma y presupuesto de trabajo fijo | 397 (~4 s) | 3309 (~5 s) | Primera hoja salida de la forja de verdad |
| Lingotes en anillo con primeros barridos más anchos | 382,3 (~20 s) | 3119,4 (~23 s) | La mayor mejora individual con n = 500 |
| Presupuesto por defecto bajado a alrededor de 1 s | **400,4** (~0,9 s) | **3274,9** (~1,6 s) | Unas 20 veces más rápido a cambio de un 4,7% / 5,0% más de operaciones |

## La chatarra: lo que no funcionó

Todo herrero tiene un montón de hojas que se partieron. El nuestro es la parte más útil de [`research/BITACORA.md`](research/BITACORA.md). Cada idea se probó frente a la forja del momento, con las mismas entradas.

| Idea | Resultado | Por qué se partió |
|---|---|---|
| Pasadas fijas tipo quicksort antes del recocido | 499 / 3892 | Cada pasada cuesta el doble de lo que ahorra |
| Recocer dos pasadas con un orden intermedio | 550 / 4207 | El mismo problema, aun optimizándolo |
| Superoptimizar ventanas de la salida | −3 con n = 100 | El recocido apenas deja margen local |
| Pares invertidos en a, arreglados con `sa` | +4 / +13 | El ahorro prometido desaparecía al contar las rotaciones |
| Resolver la permutación inversa | Dentro del ruido | Cambia la barra de partida, no la hoja |
| Destruir y reconstruir (quitar 2-30 elementos y reinsertarlos) | Llega al óptimo de la familia con n = 10; nada con n = 100 / 500 | El recocido ya encuentra esos mismos movimientos |
| Programación dinámica exacta en ventanas de 8-12 | Ganancia 0 | El filo ya es perfecto localmente; lo que falta es estructura |
| Multiescala (resolver una muestra de 100 y ampliarla a 500) | Peores arranques que los anillos | — |
| Cuatro transferencias (dejar algunos elementos sin ordenar en a para una segunda vuelta) | 3889-4715 frente a 3080 | Los elementos que esperan estorban a todos los demás golpes |
| Reinicios: la mejor de muchas ejecuciones cortas | Pierde frente a una sola larga | Los 160 lingotes ya aprovechan esa variación |
| **Segundas salidas** (aparcar un elemento en la otra pila o volver a sacarlo) | Ganancia de 0-4 sobre ~2000 | Cada viaje extra cuesta 2 y casi nunca ahorra 2 rotaciones |
| **Heurística aprendida** (transformer entrenado en GPU, búsqueda en haz guiada) | Óptima con n = 10; con n = 100 resuelve 0 de 5 entradas tras 6 rondas de DAgger | La búsqueda cae en estados que la red cree casi terminados y no lo están; además es demasiado lenta para C |

## La hoja perfecta: ¿a qué distancia del óptimo?

La hoja perfecta existe, pero nadie conoce su forma con n grande. Con n pequeño, una búsqueda exacta en anchura sobre todos los estados la encuentra:

| n | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|
| Óptimo medio | 1,17 | 2,88 | 4,86 | 7,06 | 9,31 | 11,59 | 13,91 | 16,28 |

Con n = 10 esta forja queda unas 2,5 operaciones por encima del óptimo. De ellas, 1,7 vienen de los swaps, que las soluciones óptimas usan para arreglar parejas por el camino. Con n grande, contar lo rápido que crece el número de estados alcanzables da un mínimo aproximado de unas 237 operaciones para n = 100 y 1700 para n = 500. No es una demostración, así que el óptimo real puede estar bastante por encima.

Otra forma de verlo es la información por operación. Ordenar n números requiere log2(n!) bits: 525 para 100 y 3768 para 500.

| | n = 100 | n = 500 |
|---|---|---|
| Esta forja, presupuesto por defecto (~1 s) | 1,31 bits/op | 1,15 |
| Esta forja, presupuesto de 20 s | 1,37 | 1,21 |
| Necesario para 350 / 2500 | 1,50 | 1,51 |
| Óptimo con n = 10 | 1,34 | — |

## Las normas del gremio: cumplimiento

| Regla | Estado |
|---|---|
| Solo `read`, `write`, `malloc`, `free`, `exit` | Sí. Popcount y el recuento de ceros finales están forjados a mano; no hay biblioteca matemática |
| Un solo hilo, sin reloj | Sí. Se detiene tras una cantidad fija de trabajo |
| Sin variables globales | Sí, en `src/` |
| Norma | `norminette` solo protesta por la cabecera de 42, que falta en los archivos. Es personal de cada estudiante y no se puso porque esto no es una entrega |
| Sin cierres inesperados ni fugas | Valgrind limpio con n = 30 y n = 700, y en los caminos de error |
| `Error` ante entradas inválidas; nada sin argumentos | Sí |
| Makefile con `cc`, `-Wall -Wextra -Werror`, sin relink | Sí |

`research/` y `tools/` son los bancos de pruebas del taller. No siguen la Norma y no forman parte de la compilación.

## Probar el filo: compilar, ejecutar y probar

```bash
make
./push_swap 3 1 2 5 4
ARG=$(python3 -c "import random; print(*random.sample(range(100000), 100))")
./push_swap $ARG | wc -l
./push_swap $ARG | ./checker_linux $ARG
```

`tools/bench.py` ejecuta cualquier programa sobre entradas con semilla y verifica cada salida por su cuenta:

```bash
python3 tools/bench.py run ./push_swap 100 20 --seed 8
python3 tools/bench.py exhaustive ./push_swap 5
python3 tools/bench.py compare ./otro_push_swap ./push_swap 500 20
```

Las cifras de este README usan `--seed 8`.

## El taller: estructura del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | La forja en sí. `sa_*.c` es el recocido, `sa_arms.c` y `sa_seed.c` funden los lingotes en anillo, y `sa_race.c` manda la peor mitad a la chatarra |
| `tools/bench.py` | Banco de pruebas con semilla y verificador propio |
| `research/engine.c` | La forja experimental: la misma búsqueda, con todos los interruptores del cuaderno |
| `research/bfs_exact.c` | Óptimo exacto para n ≤ 10: la hoja perfecta, donde se puede encontrar |
| `research/park.c` | Prueba de las segundas salidas |
| `research/s9_*.py` | Experimentos con heurística aprendida (PyTorch) |
| `research/BITACORA.md` | El cuaderno del herrero: reglas, mediciones, cada experimento y cada error |

## Licencia

El código y la documentación se publican con la [licencia MIT](LICENSE). El enunciado del ejercicio pertenece a 42 y no se incluye aquí.
