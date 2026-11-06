# push_swap-annealing

[English](README.md) · **Español**

Una solución al *push_swap* de 42 que trata el ejercicio como un problema de optimización: en vez de seguir una receta de ordenación fija, busca una secuencia corta de operaciones mediante recocido simulado (*simulated annealing*). El nombre viene de la metalurgia: [más abajo](#2-recocido-simulado-sobre-las-cuatro-a-la-vez) se explica.

| | n = 100 | n = 500 |
|---|---|---|
| **Este programa** | **400,4** (372–425) | **3274,9** (3145–3452) |
| Límite de 42 para la nota máxima | 700 | 5500 |

Medias sobre las mismas 20 entradas aleatorias con semilla en cada tamaño; cada salida la comprueba un verificador independiente. Tarda unos 0,9 s por ejecución con n = 100 y 1,6 s con n = 500. Buscando durante más tiempo da soluciones más cortas, hasta 382,3 / 3119,4 en unos 20 s: ver [Rapidez frente a longitud](#rapidez-frente-a-longitud).

---

## Contenido

1. [El ejercicio](#el-ejercicio)
2. [El reto que nos pusimos](#el-reto-que-nos-pusimos)
3. [Cómo funciona](#cómo-funciona)
4. [Rapidez frente a longitud](#rapidez-frente-a-longitud)
5. [Cómo llegamos aquí](#cómo-llegamos-aquí)
6. [Lo que no funcionó](#lo-que-no-funcionó)
7. [¿A qué distancia del óptimo?](#a-qué-distancia-del-óptimo)
8. [Reglas y cumplimiento](#reglas-y-cumplimiento)
9. [Compilar, ejecutar y probar](#compilar-ejecutar-y-probar)
10. [Estructura del repositorio](#estructura-del-repositorio)
11. [Licencia](#licencia)

---

## El ejercicio

*push_swap* es un proyecto del currículo de la escuela 42. Aquí se resume con nuestras palabras.

Se recibe una lista de enteros distintos en una pila **a**, y una pila **b** vacía. Hay que imprimir una lista de operaciones que deje **a** ordenada de menor a mayor, con el menor en la cima, y **b** vacía. Hay once operaciones:

| Operación | Efecto |
|---|---|
| `sa`, `sb`, `ss` | Intercambia los dos primeros elementos de a, de b o de ambas |
| `pa`, `pb` | Pasa el primer elemento de b a a, o de a a b |
| `ra`, `rb`, `rr` | Rota hacia arriba: el primero pasa al final (a, b o ambas) |
| `rra`, `rrb`, `rrr` | Rota hacia abajo: el último pasa a la cima (a, b o ambas) |

El programa debe imprimir **la lista más corta que pueda**. En la corrección se prueba con entradas aleatorias de 100 y 500 números. La nota máxima pide menos de 700 operaciones con 100 números y no más de 5500 con 500. El código debe estar en C, seguir la guía de estilo de 42 (la *Norma*) y usar solo `read`, `write`, `malloc`, `free` y `exit`. Tampoco puede tener variables globales, cerrarse inesperadamente ni perder memoria, y debe imprimir `Error` ante una entrada inválida.

Lo habitual es un algoritmo directo, como la ordenación por bloques o cubos, la inserción del movimiento más barato o radix. Estos algoritmos tardan milisegundos.

## El reto que nos pusimos

Este repositorio no es una entrega de la escuela. Es un intento personal de averiguar **cuántas operaciones hacen falta de verdad**, sin salirse de las reglas del enunciado:

- **Objetivos:** menos de 350 operaciones con 100 números y menos de 2500 con 500. No se ha alcanzado ninguno; las mejores cifras hasta ahora son 382 y 3119, con un presupuesto de 20 s.
- **Reglas:** todo lo que exige el enunciado. Eso supone un solo hilo (sin `pthread`, `fork` ni reloj), sin biblioteca matemática y respetando la Norma.
- **Tiempo:** el enunciado no fija ningún límite. Elegimos la rapidez: alrededor de un segundo por ejecución por defecto, con búsquedas más largas a una línea de distancia ([Rapidez frente a longitud](#rapidez-frente-a-longitud)).
- **Método:** cada experimento, incluido cada fallo, está anotado con sus cifras en [`research/BITACORA.md`](research/BITACORA.md). Son 39 experimentos numerados y varios diagnósticos.

## Cómo funciona

### 1. Una solución son cuatro decisiones

Toda solución de este programa tiene dos fases. En la **fase de push**, algunos elementos pasan de a a b. En la **fase de pull**, vuelven todos, cada uno a su sitio ordenado. Una solución así queda descrita por cuatro decisiones:

| Parte | Significado |
|---|---|
| **keep** | Elementos que nunca salen de a. Deben estar ya en orden creciente circular. |
| **sigma** | El orden en que salen de a los demás. |
| **pi** | El orden circular que forman en b. |
| **tau** | El orden en que vuelven a a. |

Con estas cuatro, las operaciones salen solas. Cada movimiento lleva un elemento a su sitio en la otra pila, rotando ambas con la más barata de cuatro combinaciones: `rr` a la vez, `rrr` a la vez, o en sentidos opuestos. La búsqueda exacta en entradas pequeñas muestra que esta familia de dos fases casi no pierde nada: con n = 10, obligar a hacer todos los push antes que los pull cuesta 0,04 operaciones de media.

### 2. Recocido simulado sobre las cuatro a la vez

**De dónde viene el nombre.** El *recocido* (en inglés, *annealing*) es un tratamiento térmico de metales y vidrio: se calienta el material y se enfría muy despacio. Así los átomos tienen tiempo de colocarse en una estructura ordenada y de baja energía. Si se enfría de golpe, quedan atrapados en una estructura desordenada y frágil. El *recocido simulado* es un método de optimización de 1983 que imita ese proceso, y es el nombre habitual en español de *simulated annealing*.

**Cómo se aplica aquí.** El programa hace una y otra vez un pequeño cambio al azar, y lo conserva si acorta la solución. A veces conserva también un cambio que la alarga. La probabilidad de aceptar un empeoramiento es la "temperatura": al principio es alta y la búsqueda explora con libertad; luego baja poco a poco, y al final solo se aceptan mejoras. Eso permite salir de mínimos locales en vez de quedarse en el primero que se encuentra. En el código, `SA_T0` y `SA_T1` son las temperaturas inicial y final, y "recalentar" significa volver a subirla.

Los movimientos incluyen:

- mover un elemento en sigma o tau;
- mover un bloque de elementos;
- meter o sacar un elemento de keep;
- reordenar pi;
- movimientos compuestos, en los que un elemento se coloca junto al mismo vecino en pi y en uno de los dos órdenes.

### 3. Lo que más pesa es la forma inicial de b

El recocido puede pulir una solución, pero no reorganizar la pila b a gran escala. El punto de partida decide buena parte del resultado: en una entrada de 500 números, distintos arranques dieron desde 3391 hasta 5332. Por eso el programa no se fía de un solo arranque:

- **Compiten 160 formas en "anillo".** Cada una se construye alrededor de un rango. Barrido tras barrido, a entrega los rangos cada vez más alejados de él: los superiores a un extremo de b y los inferiores al otro. Las formas cambian en centro, sentido, número de barridos (de 1 a 10) y en cómo crece el ancho de cada barrido.
- **Eliminación por mitades.** Las 160 corren un poco, se descarta la peor mitad, las supervivientes reciben más trabajo, y así sucesivamente. Esto se lleva el 30% del presupuesto.
- **Pulido.** La ganadora se recalienta y recibe el 70% restante.

### 4. Evaluación rápida

La búsqueda pasa casi todo su tiempo preguntándose "¿cuánto mide esta solución?". Dos trucos lo abaratan:

- **Bitsets.** Cada paso necesita contar elementos entre dos posiciones, y eso son unas pocas operaciones de palabra.
- **Puntos de control.** Un movimiento solo cambia una parte de la secuencia, así que el evaluador retoma desde un estado guardado justo antes del cambio, y abandona en cuanto el movimiento está claramente perdido.

Juntos llevaron la búsqueda de unas 600 a más de 800.000 evaluaciones por segundo con n = 500.

### 5. Retoques finales

- **Zigzags convertidos en swaps.** Una última pasada convierte patrones como `ra pb rra pb` en `sa pb pb`.
- **Entradas pequeñas,** de hasta 5 elementos, tienen código propio: como mucho 2 operaciones con 3 números y 10 con 5.
- **Entradas muy grandes,** de más de 512 números, recurren a radix.

### 6. Determinista a propósito

La búsqueda se detiene tras una **cantidad fija de trabajo** (`SA_BUDGET` en `src/sa.h`), no tras un tiempo fijo. La misma entrada da siempre la misma salida, en cualquier máquina. Una máquina más rápida termina antes y una más lenta tarda más, pero imprime exactamente la misma respuesta.

## Rapidez frente a longitud

El programa está ajustado para responder en alrededor de un segundo. Buscando durante más tiempo da soluciones más cortas. Esta sección muestra cuánto más cortas, hasta dónde llega eso y por qué elegimos la rapidez.

**Qué significa "más tiempo".** La búsqueda hace pequeños cambios al azar y se queda con los buenos (ver [Cómo funciona](#cómo-funciona)). `SA_BUDGET`, en `src/sa.h`, fija cuánto de ese trabajo hace, así que doblar el presupuesto dobla más o menos el tiempo. El motor de investigación prueba unos 1,8 millones de cambios por segundo con n = 100 y 0,6 millones con n = 500.

El mismo programa con distintos presupuestos, sobre las mismas 20 entradas:

| `SA_BUDGET` | Tiempo ejecutando solo, n = 100 / 500 | n = 100 | n = 500 |
|---|---|---|---|
| **4·10⁷ (por defecto)** | **0,9 s / 1,6 s** | **400,4** | **3274,9** |
| 8·10⁷ | 1,7 s / 2,6 s | 396,9 | 3206,1 |
| 2·10⁸ | 4,4 s / 5,7 s | 392,0 | 3164,4 |
| 9·10⁸ | 20 s / 23 s | 382,3 | 3119,4 |

**Hasta dónde sigue mejorando:**

- **n = 500 es el que más gana al principio.** Pasar de 1,6 s a 5,7 s ahorra 110 operaciones (un 3,4%). Seguir hasta 23 s ahorra otras 45 (un 1,4%).
- **n = 100 mejora despacio pero sin parar,** unas 13 operaciones cada vez que la ejecución es diez veces más larga. Una versión anterior del motor de investigación, sin las formas en anillo, seguía mejorando tras 30 minutos por entrada, con una media de 371,4.
- **Así que no hay un punto final claro,** solo ganancias cada vez menores. A partir de unos 20-30 s, multiplicar el tiempo por diez compra como mucho unos pocos puntos porcentuales, y las ejecuciones de media hora no son prácticas.

**Por qué las ganancias se reducen.** El recocido deja resuelto el orden local en el primer segundo: una búsqueda exacta sobre ventanas de 8-12 elementos ya no encuentra nada que mejorar. Lo que queda es la estructura a gran escala, es decir, la forma de b y qué elementos se quedan en a. Eso solo cambia con movimientos raros y coordinados, así que cada mejora tarda más en aparecer. Por eso también un resultado mucho mejor necesitaría una idea nueva, no más tiempo (ver [Lo que no funcionó](#lo-que-no-funcionó)).

**Por qué elegimos la rapidez.** Con alrededor de un segundo, el programa responde tan rápido como se espera de él, y ya saca 400 / 3275, muy por debajo de los límites de 42 (700 / 5500). Las búsquedas largas compran el último 5%. Quien quiera la salida más corta puede subir `SA_BUDGET`. Cada valor da siempre la misma salida para la misma entrada.

El programa usa un solo núcleo, porque el enunciado no permite hilos ni procesos.

## Cómo llegamos aquí

Cada paso se midió sobre entradas emparejadas con semilla y comprobadas por un verificador. Las cifras son la media de operaciones para n = 100 / n = 500.

| Paso | n = 100 | n = 500 | Qué cambió |
|---|---|---|---|
| Punto de partida (heurísticas + búsqueda en haz sobre el orden del pull) | 456,8 | 3924,8 | Versión anterior |
| Recocido de la solución entera, primer prototipo | 406,8 (600 s) | — | Demostró que el supuesto "suelo" de 420-440 no era real |
| Evaluación con bitsets y puntos de control | 400 (60 s) | 3427 (60 s) | Unas 600 veces más evaluaciones por segundo |
| Competición entre formas iniciales de b | 392 (60 s) | 3302 (60 s) | La forma de b pesa más que cualquier ajuste fino |
| Ajuste sobre 20 entradas emparejadas | 383,8 (60 s) | 3235,5 (60 s) | La mayor parte del trabajo va a pulir a la ganadora |
| Paso a C con la Norma y presupuesto de trabajo fijo | 397 (~4 s) | 3309 (~5 s) | Primera versión dentro de push_swap |
| Formas en anillo con primeros barridos más anchos | 382,3 (~20 s) | 3119,4 (~23 s) | La mayor mejora individual con n = 500 |
| Presupuesto por defecto bajado a alrededor de 1 s | **400,4** (~0,9 s) | **3274,9** (~1,6 s) | Unas 20 veces más rápido a cambio de un 4,7% / 5,0% más de operaciones |

## Lo que no funcionó

Es la parte más útil de [`research/BITACORA.md`](research/BITACORA.md). Cada idea se probó frente al programa del momento, con las mismas entradas.

| Idea | Resultado | Por qué falló |
|---|---|---|
| Pasadas fijas tipo quicksort antes del recocido | 499 / 3892 | Cada pasada cuesta el doble de lo que ahorra |
| Recocer dos pasadas con un orden intermedio | 550 / 4207 | El mismo problema, aun optimizándolo |
| Superoptimizar ventanas de la salida | −3 con n = 100 | El recocido apenas deja margen local |
| Pares invertidos en a, arreglados con `sa` | +4 / +13 | El ahorro prometido desaparecía al contar las rotaciones |
| Resolver la permutación inversa | Dentro del ruido | Cambia el arranque, no el resultado |
| Destruir y reconstruir (quitar 2-30 elementos y reinsertarlos) | Llega al óptimo de la familia con n = 10; nada con n = 100 / 500 | El recocido ya encuentra esos mismos movimientos |
| Programación dinámica exacta en ventanas de 8-12 | Ganancia 0 | La solución ya es óptima localmente; lo que falta es estructura |
| Multiescala (resolver una muestra de 100 y ampliarla a 500) | Peores arranques que los anillos | — |
| Cuatro transferencias (dejar algunos elementos sin ordenar en a para una segunda vuelta) | 3889-4715 frente a 3080 | Los elementos que esperan estorban a todos los demás movimientos |
| Reinicios: la mejor de muchas ejecuciones cortas | Pierde frente a una sola larga | La competición entre formas ya aprovecha esa variación |
| **Segundas salidas** (aparcar un elemento en la otra pila o volver a sacarlo) | Ganancia de 0-4 sobre ~2000 | Cada viaje extra cuesta 2 y casi nunca ahorra 2 rotaciones |
| **Heurística aprendida** (transformer entrenado en GPU, búsqueda en haz guiada) | Óptima con n = 10; con n = 100 resuelve 0 de 5 entradas tras 6 rondas de DAgger | La búsqueda cae en estados que la red cree casi resueltos y no lo están; además es demasiado lenta para C en 30 s |

## ¿A qué distancia del óptimo?

Una búsqueda exacta en anchura sobre todos los estados da el óptimo real con n pequeño:

| n | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|
| Óptimo medio | 1,17 | 2,88 | 4,86 | 7,06 | 9,31 | 11,59 | 13,91 | 16,28 |

Con n = 10 este programa queda unas 2,5 operaciones por encima del óptimo. De ellas, 1,7 vienen de los swaps, que las soluciones óptimas usan para arreglar parejas por el camino. Con n grande no se conoce la respuesta exacta. Contar lo rápido que crece el número de estados alcanzables da un mínimo aproximado de unas 237 operaciones para n = 100 y 1700 para n = 500. No es una demostración, así que el óptimo real puede estar bastante por encima.

Otra forma de verlo es la información por operación. Ordenar n números requiere log2(n!) bits: 525 para 100 y 3768 para 500.

| | n = 100 | n = 500 |
|---|---|---|
| Este programa, presupuesto por defecto (~1 s) | 1,31 bits/op | 1,15 |
| Este programa, presupuesto de 20 s | 1,37 | 1,21 |
| Necesario para 350 / 2500 | 1,50 | 1,51 |
| Óptimo con n = 10 | 1,34 | — |

## Reglas y cumplimiento

| Regla | Estado |
|---|---|
| Solo `read`, `write`, `malloc`, `free`, `exit` | Sí. Popcount y el recuento de ceros finales están escritos a mano; no hay biblioteca matemática |
| Un solo hilo, sin reloj | Sí. Se detiene tras una cantidad fija de trabajo |
| Sin variables globales | Sí, en `src/` |
| Norma | `norminette` solo protesta por la cabecera de 42, que falta en los archivos. Es personal de cada estudiante y no se puso porque esto no es una entrega |
| Sin cierres inesperados ni fugas | Valgrind limpio con n = 30 y n = 700, y en los caminos de error |
| `Error` ante entradas inválidas; nada sin argumentos | Sí |
| Makefile con `cc`, `-Wall -Wextra -Werror`, sin relink | Sí |

`research/` y `tools/` son programas de prueba. No siguen la Norma y no forman parte de la compilación.

## Compilar, ejecutar y probar

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

## Estructura del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | El programa. `sa_*.c` es el recocido, `sa_arms.c` y `sa_seed.c` construyen los anillos, y `sa_race.c` hace la eliminación por mitades |
| `tools/bench.py` | Banco de pruebas con semilla y verificador propio |
| `research/engine.c` | Motor de investigación: la misma búsqueda, con todos los interruptores experimentales de la bitácora |
| `research/bfs_exact.c` | Óptimo exacto para n ≤ 10 |
| `research/park.c` | Prueba de las segundas salidas |
| `research/s9_*.py` | Experimentos con heurística aprendida (PyTorch) |
| `research/BITACORA.md` | La bitácora completa: reglas, mediciones, cada experimento y cada error |

## Licencia

El código y la documentación se publican con la [licencia MIT](LICENSE). El enunciado del ejercicio pertenece a 42 y no se incluye aquí.
