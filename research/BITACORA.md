# Bitácora de push_swap

Registro de todo lo probado, sobre todo de lo que falló. Se actualiza con cada experimento, antes de pasar al siguiente.

Cifras como `397 / 3309` significan operaciones medias con n=100 / n=500. Salvo que se indique otra cosa, las medias son sobre instancias aleatorias con semilla, y cada salida está verificada.

## Estado actual

| | `main` | `push_swap` de la rama, ~4 s (commit `84db7bc`) | **`push_swap` con anillos, <30 s** | Motor con anillos, 30 s | Motor antiguo, 30 min | Objetivo |
|---|---|---|---|---|---|---|
| **n=100** | 456 | 397 | **382** | 377-380 | 371 (mejor caso 346) | <350 |
| **n=500** | 3866 | 3309 | **3119** | 3118-3137 | 3196 (mejor caso 3120) | <2500 |

Desde el 2026-11-03 `push_swap` lleva los anillos (punto 30). Desde el 2026-11-06 el presupuesto por defecto es de alrededor de 1 s: 400,4 / 3274,9 (puntos 40-42); con 5 s, 392,0 / 3164,4, y con 20-23 s, 382,3 / 3119,4. Cumple las reglas salvo la cabecera de 42. Es el primer resultado que cuenta.

- `push_swap` es un reto personal, no una entrega, pero el enunciado es la base de reglas: un resultado solo cuenta si lo produce un programa que las cumple.
- El motor de `research/` es un programa de prueba: usa `clock_gettime`, libm y variables globales. Sus cifras orientan, pero no cuentan como resultado.

## Reglas del ejercicio

Del enunciado, versión 8.1, leído entero el 2026-11-01.

| Regla | Qué implica | Estado de `annealing-engine` |
|---|---|---|
| Mostrar "la lista de instrucciones más corta posible" | El único criterio es el número de operaciones | Es lo que se mide |
| Sin tiempo máximo escrito, ni en el enunciado ni en la copia de la hoja de corrección consultada | El límite viene de la práctica: ver "Límite práctico de tiempo" | Calibrada: unos 20 s con n=100 y 23 s con n=500, ejecutando solo |
| 100 números en menos de 700; 500 sin superar 5500 | Es el mínimo para validar | Cumple |
| Autorizadas: `read`, `write`, `malloc`, `free`, `exit`, `ft_printf` | Un solo hilo: sin `pthread`, `fork`, reloj, `open` ni libm | Llama solo a `write`, `malloc`, `free` y `exit`; el compilador añade `memcpy` y `memmove`. `popcount` y `ctz` son propios (`sa_bits2.c`), sin pérdida de velocidad |
| Variables globales prohibidas | | Cumple en `src/` |
| Norma, incluidos los archivos adicionales | | `norminette` solo protesta por la cabecera de 42, que falta en `src/`. `research/*.c` no cumple |
| Makefile con `cc`, `-Wall -Werror -Wextra`, sin relink | | Cumple: `cc`, y los `.o` dependen también de las cabeceras |
| Bonus en archivos `_bonus.{c,h}`, con regla `bonus` | Opcional | Sin bonus: se quitó la regla `bonus` rota |
| Sin cierres inesperados ni fugas | | Valgrind limpio con n=30, n=700 y en los caminos de error: los argumentos se validan antes de reservar memoria |
| Cualquier cantidad de números | | Por encima de 512 usa radix. El búfer de la solución crece al llenarse |
| `Error\n` en stderr; sin parámetros no muestra nada | | Comprobado a mano |
| El primer argumento va en la cima; el menor acaba en la cima | | Cumple |
| Programas de prueba permitidos, no se evalúan | `tools/`, BFS y `research/` pueden usar cualquier cosa, pero no son el resultado | Cumple |
| `checker_OS` comprueba la salida | La medición final debe pasar por él | `bench.py` usa un verificador propio |

El enunciado no exige aceptar `"3 1 2"` como un solo argumento: lo describe como "una lista de números enteros", y sus ejemplos usan `$ARG` sin comillas.

### Límite práctico de tiempo

Decidido el 2026-11-01: **unos 30 s por ejecución en esta máquina**. Es una regla nuestra, no oficial. Un programa tiene que poder revisarse, y una revisión no debería durar más de una hora.

- **Qué pide la corrección:** una [copia no oficial de la hoja](https://wormav.github.io/42_eval/Cursus/Push_swap/push_swap.html) pide repetir 100 y 500 valores "couple of times with several permutations", sin dar un número.
- **Ejecuciones (estimación):** de 3 a 5 permutaciones × 2 tamaños × 2 comandos (`wc -l` y `checker`) = de 12 a 20 ejecuciones.
- **Tiempo disponible (estimación):** unos 20-30 minutos de la hora, porque el resto va a compilar, Norma, errores y explicar el código.
- **Resultado:** unos 60 s por ejecución como máximo en la máquina del revisor. Como el programa para tras un trabajo fijo, una máquina más lenta tarda más: con margen ×2, unos **30 s en la nuestra**.

La hoja también pide, y ya se cumple: como máximo 12 instrucciones con 5 valores (peor caso medido: 10), como máximo 3 con 3 valores (peor caso: 2), y ninguna si ya está ordenado.

## Cómo medir

- `tools/bench.py gate A B quick`: 5 s por instancia, 20 instancias emparejadas, unos 22 s en total.
- `tools/bench.py gate A B confirm`: 30 s, el límite del reto, con otras instancias. Solo para lo que pasa la prueba rápida. Los experimentos de esta bitácora anteriores a este cambio usaron 60 s.
- Ruido medido comparando el motor contra sí mismo a 5 s: −1,6 ±4,4 en n=100 y +5,6 ±16,0 en n=500.
- Las pruebas de 30 minutos son la excepción: resultan demasiado lentas como rutina.

## Hallazgos que siguen en pie

### Matemáticas

**Óptimo exacto**, por BFS sobre todos los estados:

| n | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|
| Media | 1,167 | 2,875 | 4,858 | 7,060 | 9,306 | 11,590 | 13,911 | 16,276 |
| Peor caso | 2 | 5 | 8 | 10 | 13 | 16 | 19 | 22 |

**Qué aporta cada operación:** óptimo exacto quitando operaciones.

| Operaciones | n=8 | n=9 | n=10 |
|---|---|---|---|
| Todas | 11,590 | 13,911 | 16,276 |
| Sin `ss` | 11,682 | 14,062 | 16,491 |
| Sin `sb` ni `ss` | 11,682 | 14,062 | 16,491 |
| Sin ningún swap | 12,998 | 15,361 | 17,777 |
| Sin swaps ni `rr`/`rrr` | 13,684 | 16,290 | 18,966 |

- `sb` no aporta nada por sí solo con n≤10.
- Quitar todos los swaps cuesta entre 1,41 y 1,50 operaciones, y crece despacio con n. Es casi una cantidad fija, no un porcentaje, así que no hay indicios de que importe mucho con n grande.

**Soluciones óptimas de n=10**, sobre 3000 casos y una ruta óptima por caso (la ruta prefiere `pb`/`pa`, luego rotaciones, y swaps al final):

- 4,35 `pb`, 5,68 rotaciones, 1,07 `sa`, 0,83 `ss`, 0,01 `sb`.
- Ningún elemento sale dos veces de `a`.
- Hay `pa` antes de algún `pb`, es decir, intercalan.

**Capacidad de las operaciones:** número de estados nuevos a cada profundidad, en pilas largas.

| Profundidad | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Razón | 11 | 7,455 | 6,256 | 5,696 | 5,362 | 5,093 | 4,928 | 4,795 | 4,699 | 4,621 |

Con 4,62 y bajando, salen como mucho unos 2,2 bits por operación, lo que da mínimos aproximados de 237 (n=100) y 1700 (n=500). No es una demostración, porque supone que la razón sigue bajando.

**Eficiencia en bits por operación**, con log2(n!) = 524,8 (n=100) y 3767,6 (n=500):

| | n=100 | n=500 |
|---|---|---|
| `main` | 1,15 | 0,97 |
| Rama actual | 1,32 | 1,14 |
| Motor, 30 min | 1,41 | 1,18 |
| Objetivo | 1,50 | 1,51 |

Con n=10 el óptimo da 1,34. Los dos objetivos piden la misma eficiencia.

**Presupuesto de rotaciones**, aproximado: cada elemento movido cuesta 2 operaciones.

| | Movidos × 2 | Rotaciones ahora | Rotaciones permitidas | Recorte |
|---|---|---|---|---|
| **n=100** | ~158 | ~239 | ~192 | −20% |
| **n=500** | ~904 | ~2405 | ~1596 | −34% |

### Estructura

**El coste del pull crece con √n sobre un `b` aleatorio.** Modelo voraz, sin contar el push:

| n | 50 | 100 | 200 | 400 | 800 |
|---|---|---|---|---|---|
| Rotaciones por pull | 3,03 | 4,17 | 5,68 | 7,89 | 10,84 |
| Dividido por √n | 0,428 | 0,417 | 0,402 | 0,394 | 0,383 |

**El orden de `b` sí importa**, en contra de lo que decían las notas antiguas. Mismo modelo, con bandas de rango:

| Bandas | 1 | 2 | 4 | 8 | 16 | 32 | 64 |
|---|---|---|---|---|---|---|---|
| n=100 | 4,14 | 3,52 | 2,97 | 2,60 | 2,29 | 1,99 | 1,74 |
| n=500 | 8,60 | 6,83 | 5,29 | 4,28 | 3,53 | 2,89 | 2,56 |

**La forma inicial de `b` decide el resultado y el recocido no la cambia.** Un mismo caso de n=500 a 60 s:

- Cambiando la semilla: 3635 a 3685.
- Cambiando las bandas: 3749 (1), 3391 (2), 3659 (3), 3785 (4), 3717 (6), 4376 (8), 4440 (12), 4793 (16), 4981 (24), 4636 (32), 5332 (48).

**La mejora sigue una ley logarítmica en el tiempo:** n=100 pierde unas 13 operaciones por cada ×10 de tiempo y aún no se detiene. n=500 se va frenando.

**El coste de un problema y de su inverso es el mismo.** Se deduce de que toda operación tiene inversa, no se ha medido: invertir y dar la vuelta a una solución de P⁻¹ da una solución de P con la misma longitud.

## Experimentos

Estado: ✅ se queda, ❌ descartado, ❓ sin concluir.

### Antes de esta bitácora

Vienen de las notas de sesiones anteriores, sin semillas ni verificación, así que son orientativos.

| Experimento | Resultado | Estado | Qué aprendimos |
|---|---|---|---|
| Beam search sobre el orden de push | 675 frente a 470 | ❌ | Optimizaba con un estimador que sobreestimaba 6 veces el ahorro |
| Turk + pull secuencial | 475 frente a 471 | ❌ | Mantener `b` ordenado pronto cuesta lo que ahorra |
| Radix como semilla de la carrera | 24 operaciones, dentro del ruido | ❌ | Además escondía el fallo de validez (ver abajo) |
| Más bandas (2..50) y más variantes de LIS | 24 y 1 operaciones | ❌ | Ajustar parámetros de una familia agotada no rinde |
| Beam search sobre el pull, con simulación voraz como estimación | −20 con n=100 | ❌ | Superado por el recocido |

### Esta campaña

| # | Hipótesis | Resultado | Estado | Qué aprendimos |
|---|---|---|---|---|
| 1 | Las cifras del README son reales | 456,8 / 3924,8 y 18-42 s, no 457 / 3828 y 3-4 s | ❌ | Medir siempre con semillas. "Nativo 10× más rápido" no se sostiene: `-O2` en Linux tardó 18-42 s |
| 2 | Las salidas siempre son válidas | Hasta 12% inválidas con n=6, 3,4% con n=7, 0,4% con n=10 | ❌ corregido | `pass_bit` de radix cortaba pronto, y la carrera solo comparaba longitudes |
| 3 | Recocer la solución entera supera a la carrera | n=100: 450 (60 s), 406,8 (600 s), frente a 459,4. n=500: 4915 frente a 3815 | ✅ | Refuta el "suelo de 420-440". En n=500 no llegó a converger: unas 370.000 iteraciones |
| 4 | Evaluar por tramos con bitsets da otro nivel | n=100: 421 / 400 / 387 (5, 60, 600 s). n=500: 3567 / 3427 / 3305. Gana 10 de 10 a `main` | ✅ | Pasa de unas 600 a unas 350.000 iteraciones/s en n=500 |
| 5 | Competir entre formas de `b` (56, por mitades sucesivas) | 405,6 / 3424 a 5 s; 392,1 / 3302 a 60 s; desviación de n=500 de ~120 a ~40-50 | ✅ | La forma importa más que el ajuste fino |
| 6 | 30 min dan el suelo de la familia | 371,4 (mejor 346) / 3195,6 (mejor 3120) | ❓ | n=100 no se estanca: no hemos encontrado el suelo |
| 7 | Una pasada fija de 3 vías antes del motor | 499 / 3892 (una pasada), 629 / 4337 (dos) | ❌ | Cuesta 189 / 945 operaciones y ahorra unas 98 / 480 |
| 8 | Optimizar también esa pasada, con un orden intermedio por bandas | Mejor caso 550 / 4207 | ❌ | El reparto de tiempo la perjudica, pero no explica cientos de operaciones. El orden intermedio estaba fijado de antemano |
| 9 | Ajustar temperaturas, `keep` y número de formas | Plano dentro del ruido | ❌ | Reducir a 1-2 bandas empeora n=500 (3894 y 3481) |
| 10 | Menos movimientos de `pi` | 3428 → 3401 en n=500; quitarlos del todo empeora n=100 (425) | ✅ | Cada uno recalculaba todo |
| 11 | Movimientos compuestos (vecino en `pi` y en un orden) | n=100: 408,5 → 400,7; n=500 dentro del ruido | ✅ | Mover en un solo orden se rechaza casi siempre. Pista para el escenario de reconstrucción |
| 12 | Dar más tiempo a la forma ganadora (70%) | n=500: 3388 → 3345; n=100 dentro del ruido | ✅ | Antes casi todo el tiempo iba a formas descartadas |
| 13 | Conjunto de 10-12 | 5 s: −19,4 ±6,2 / −99,8 ±24,2. 60 s: −8,8 ±4,4 / −74,6 ±16,6 | ✅ | Confirmado en otras instancias |
| 14 | Acelerar `pi` y abandonar intentos perdidos | 657.000 → 849.000 iter/s, unas −15 en n=500 | ✅ poco | La velocidad rinde poco: es la ley logarítmica |
| 15 | Elegir con preferencia los pasos caros; bloques largos | Plano | ❌ | |
| 16 | Superoptimizador por ventanas sobre la salida | 376 → 373 (tabla hasta 8 operaciones) | ❌ | La salida apenas tiene margen local: el recocido evita esas situaciones |
| 17 | Enseñar al recocido los zigzags con swap | Plano; ahorra unas 4 operaciones en n=500 | ✅ poco | Se queda como pasada final, porque es gratis |
| 18 | Parejas invertidas en `a`, corregidas con `sa` | `keep` sube a 24 / 55, pero +4,3 ±8,4 / +12,7 ±50,9 | ❌ | El cálculo prometía −15 / −55 sin contar rotaciones. Las `sa` se colocaban al final del push |
| 19 | Intercalar `pa` y `pb` | 387 → 380 en un caso (20 s, 6 cruces). 60 s en total: 391,4 frente a 383,8 | ❓ | Evaluación lenta y `keep` fijo. Con n≤10 no alcanzó el óptimo sin swaps, así que tampoco lo descarta |
| 20 | Traducir el motor a C conforme a la Norma | 397,0 / 3308,9 frente a 456,3 / 3865,7: gana 40 de 40 | ✅ | Ver el error de traducción abajo |
| 21 | S1: solo anillos completos (centro hacia fuera, ascendente o descendente), 2026-11-03 | 5 s: +1,4 ±3,4 / −33,3 ±49,9 frente al motor anterior | ❓ | Solos no se distinguen del ruido |
| 22 | S1: anillos y bandas compitiendo juntos | 5 s: +1,6 ±6,0 / −74,2 ±37,5 (15 de 20). Ganan anillos en 15 de 20 casos de n=500 | ✅ parcial | El fallo de forma era real, pero mayor en n=500 |
| 23 | S3: anillos con tres perfiles de ancho (1; 1,6; 0,6) frente al punto 22 | 5 s: −10,9 ±7,2 (16 de 20) / −57,9 ±39,3; n=500 en 3201 | ✅ | Gana casi siempre 0,6, es decir, primeras vueltas más anchas. El comentario del código decía lo contrario y estaba mal |
| 24 | Otros perfiles de ancho (0,4, 0,3, cinco perfiles, solo dos) | Todo dentro del ruido salvo 1,0,6 solo, que empeora n=500 (+30,6 ±29,8) | ❌ | Basta con tres perfiles |
| 25 | Confirmación a 30 s: anillos con anchos 1; 0,6; 0,4 frente al motor anterior | 382,3 → 380,0 (−2,3 ±4,3) / 3258,3 → 3137,2 (−121,2 ±37,4, 17 de 20) | ✅ | Primera mejora grande de n=500 desde la selección de formas. Construir las formas cuesta ~1,3 s que el presupuesto del motor no cuenta |
| 26 | S2: resolver la permutación inversa (`research/dual.py`) | Válido. 5 s: −4,8 ±4,6 / −10,1 ±47,1. 30 s, n=100: −2,5 ±5,4. Mejor de los dos con medio tiempo cada uno: −1,6 / +4,4 | ❌ | Cambia el punto de partida, no el resultado |
| 27 | ¿Cierra el tiempo la distancia de n=10 al óptimo de la familia? | 0,3 s → 3 s: 18,8 → 18,4 (óptimo 18,03); n=8: 13,5 → 13,3 (13,15) | ❓ | Con un problema tan pequeño, eso es un defecto de búsqueda, no de tiempo |
| 28 | ¿Se queda `keep` clavado en el LIS? | Las soluciones óptimas sin swaps mueven 5,16 elementos con n=10 y 3,69 con n=8; el motor, 4,84 y 3,63 | ✅ diagnóstico | El óptimo saca a veces elementos del LIS. Sacarlos con reinserción al azar casi nunca se acepta |
| 29 | S5: destruir y reconstruir (2-6 elementos cercanos en rango, incluidos de `keep`, reinsertados en el mejor hueco guiado de los tres órdenes) | n=10, 1 s: 18,6 → 18,2 / 18,0 (tasas 50 y 300 por 10.000), igual que el óptimo de la familia. 5 s: +1,1 / +20,4 (tasa 5), +8,7 / +62,1 (tasa 20). 30 s: +1,6 ±4,7 / +14,2 ±25,9 | ❓ | Arregla la búsqueda en pequeño, pero cada movimiento evalúa la solución entera unas 40 veces. Hace falta una evaluación aproximada de cada hueco antes de reintentarlo |
| 30 | R0: anillos en `push_swap` (160 formas: 1-10 vueltas con anchos 1, y 3-10 con 0,6 y 0,4), presupuesto ×6, reglas | Mismas instancias que la confirmación (semilla 8): **382,3 / 3119,4**, 0 inválidas de 40. El motor a 30 s: 380,0 / 3137,2. Solo, 19-20 s / 23-23,5 s; con 8 a la vez, 27 / 32 s | ✅ | Primer resultado que cumple las reglas. Un fallo por el camino: con n impar el rango más lejano da `1 − t = 0` y `sa_ln(0)` no termina; colgaba todo n=5 |
| 31 | S5 rápido (`RRF=1`): cada elemento vuelve a un hueco guiado y luego va al mejor hueco de cada orden según una estimación sobre los puntos de control (solo su paso y el siguiente). Una simulación completa por elemento en vez de ~40 | n=10, 1 s: 17,9 (lento 17,7; sin él 18,2). 5 s, tasa 20: −0,1 ±5,3 / +19,1 ±25,4. Tasa 100: +2,4 ±3,4 / +35,2 ±16,1. Grupos de hasta 12 en n=100: +1,3 ±3,1 | ❌ | Arregla n pequeño, pero a partir de n=100 no encuentra nada que el recocido no encuentre ya. Primer intento con un fallo: al rechazar no se restauraban los puntos de control (24,4 con n=10) |
| 32 | S6: ventanas exactas (`WIN=w`): orden óptimo de w posiciones seguidas de `tau` o `sigma` por programación dinámica sobre (conjunto movido, último, bandera de zigzag), exacto dentro de la ventana y en el paso siguiente; como pulido final | w = 8, 10, 12 tras 5 s: ganancia 0 en n=100 y n=500. Tras 0,02 s: 2 operaciones. Tras 0,3 s: 0 | ❌ diagnóstico | El recocido deja `tau` y `sigma` óptimos por tramos de hasta 12 en menos de un segundo. Lo que falta no es orden local: es estructura (forma de `b`, `keep`) |
| 33 | S4: multiescala (`MS=k`): resolver una muestra de 1 de cada 5 rangos (n=100) y elevar su forma a n=500 como salidas extra de la carrera | Elevar por grupos de rango: ~17.300 al empezar (los grupos están repartidos por `a`). Por barridos de igual duración y orden de `b` arbitrario: 7.900-17.400 (cada push gira `b`). Por barridos y cima/fondo cortando `b` en el primer empujado: 4.270-6.420, frente a 3.641 del mejor anillo. Prueba rápida n=500, k=4: +4,9 ±24,2, y ~2 s más por construir 160 formas en cada muestra | ❌ | La forma aprendida en pequeño es peor salida que un anillo, y el recocido no lo compensa |
| 34 | Carrera de `keep` (`KEEPV=1`): cada salida en dos versiones, `keep` normal y `keep` con pares invertidos que arregla un `sa` (D5) | n=10: 18,1 → 17,6, por debajo del óptimo de la familia sin swaps (18,03). 5 s: −1,9 ±4,9 / −20,4 ±21,2 (15 de 20). **30 s: +0,6 ±5,6 / −1,6 ±21,4.** La versión con pares gana la carrera en ~la mitad de los casos y deja más en `a` (55 frente a 44 en n=500) | ❌ | Ayuda con poco tiempo; con 30 s el recocido llega al mismo sitio sin pares. No se porta a `push_swap` |
| 35 | Cuatro transferencias (`FOUR=1`): un conjunto X se queda en `a` sin ordenar durante T1 y T2, como si estuviera en `keep`; luego sale (T3) y vuelve (T4) con sus propios órdenes. En T2, `a` usa coordenadas propias (cada X al principio del hueco de su `keep`, cada pull justo antes de su sucesor), así que el evaluador es el mismo. Movimientos: aplazar (torneo entre los más caros), devolver, y reordenar X evaluando solo T3/T4 | Salidas válidas y coste emitido = calculado. Con aplazamientos de uno en uno, X se queda vacío siempre. Aplazando de golpe los elementos con push+pull ≥ k (n=500): k=8, 145 elementos: T1+T2 pasan de ~3210 a 3431 y T3+T4 cuestan 3512; final 4715. k=12, 32: T1+T2 3213 y cola 1072; final 3917. k=16, 14: 3205 y 883; final 3889. Sin X: ~3080 | ❌ | **Los elementos aplazados son obstáculos:** mientras esperan en `a`, alargan cada barrido de T1 y cada rotación de `a` en T2, así que quitarlos no abarata las dos primeras transferencias. Y la cola cuesta como mínimo otro barrido más ordenar X. Coincide con D1: separar en dos transferencias solo cuesta 0,04 con n=10. Primer intento con mi error: T4 sacaba X en orden de posición, no de rango (cola de 18.000) |
| 36 | D6: ¿vale más una búsqueda larga o varias cortas quedándose con la mejor, a igual tiempo? n=100, 6 instancias, 20 semillas por presupuesto | Media de una ejecución de 30 s: 379,5. Mejor de 20 de 1,5 s: 384,2 (+4,8; pierde en 4 de 6). Mejor de 4 de 7,5 s: 378,9 (−0,6, empate). Mejor de 20 de 30 s (10 min en total): ~370 | ❌ diagnóstico | Reiniciar no ayuda: la carrera ya aprovecha la variación entre arranques. Con unas −13 por década de tiempo, bajar de 378 a 350 pediría unas 100 veces más búsqueda |
| 37 | S8: segundas salidas en el pull (`research/park.c`, 2026-11-06). Sobre soluciones del motor a 5 s: reconstruye el estado tras el último `pb` y el orden de pull, lo resimula (coincide: 2124 frente a 2125) y busca con mejora voraz tres movimientos. (a) Aparcar: un elemento de `b` va a `a` fuera de orden cuando pasa por la cima, y vuelve a `b` hasta 80 tareas antes de su pull. (b) Igual, pero se coloca justo al volver (reubicarlo en `a`). (c) Volver a sacar un elemento que ya está en `a`, `keep` incluido, y reinsertarlo hasta 80 tareas después | n=500, 4 casos: ganancia 0, 1, 1 y 4 sobre ~2000 de pull. n=100, 6 casos: 0. Los ~1400 candidatos de (c) en n=500 quedan todos 1 o más por encima; ninguno inválido | ❌ | Cada viaje extra cuesta 2 operaciones y casi nunca ahorra 2 rotaciones: el recocido ya elige el orden de pull siguiendo la forma de `b`. La cota optimista de D4 (−607) no se acerca: el coste de los pulls caros no se puede trasladar a otro sitio |
| 40 | `push_swap` a ~5 s (2026-11-06): `SA_BUDGET` 9·10⁸ → 2·10⁸, todo lo demás igual. Mismas 20 entradas (semilla 8) | **392,0 / 3164,4** frente a 382,3 / 3119,4: +9,7 / +45,0, 0 inválidas. Solo: 4,4 s / 5,7 s; con 4 a la vez, 5,0 / 6,3 s | ✅ medido | Cuatro veces menos tiempo cuesta un 2,5% / 1,4%. Sigue muy por delante de la versión anterior de ~4 s (397 / 3309), porque los anillos rinden incluso con poco presupuesto |
| 41 | Presupuestos de 1 y 2 s (2026-11-06), mismas 20 entradas. `SA_BUDGET` 4·10⁷ y 8·10⁷ | 4·10⁷: **400,4 / 3274,9**, solo 0,9 s / 1,6 s. 8·10⁷: **396,9 / 3206,1**, 1,7 s / 2,6 s. 0 inválidas | ✅ medido | Por debajo de unos 2 s, n=500 empeora claramente (+69 de 2,6 a 1,6 s); n=100 apenas cambia. **Decisión: 2·10⁸ (~5 s) por defecto** en `src/sa.h`, y los README lo explican con la tabla de presupuestos |
| 42 | Presupuesto por defecto de alrededor de 1 s (2026-11-06): `SA_BUDGET` 2·10⁸ → 4·10⁷ en `src/sa.h` | Medido en el punto 41: **400,4 / 3274,9**, 0,9 s / 1,6 s ejecutando solo | ✅ decisión | Se elige la rapidez: unas 20 veces más rápido que la versión de 20 s a cambio de un 4,7% / 5,0%. Los README presentan la tabla de presupuestos como "Rapidez frente a longitud": cuánto mejora con más tiempo, hasta dónde (sin final claro; a partir de 20-30 s, pocos puntos por cada ×10) y por qué se reduce la ganancia |
| 43 | Entradas diminutas lentas (2026-11-07): n=4-8 tardaban 0,5-1,1 s, más que n=10 (0,26 s). `x->work` solo cuenta pasos simulados, y con n muy pequeño muchos intentos no simulan nada, así que el bucle de `sa_anneal` daba vueltas sin gastar trabajo. Arreglo: limitar también el número de intentos al mismo presupuesto (`iters < work`) | n=4: 0,79 → 0,03 s; n=5: 1,24 → 0,05 s; n=6: 1,54 → 0,07 s; n=7: 1,40 → 0,13 s; n=8: 1,04 → 0,24 s. Mismas medias y máximos en todas las permutaciones de n=4-7 y en 100 entradas de n=8, 10 y 12. Con n=100 y n=500 las salidas son idénticas byte a byte | ✅ | Con n grande cada intento simula muchos pasos, así que el límite nunca se alcanza y el resultado no cambia |

### De dónde viene la distancia al óptimo con n pequeño (D1, 2026-11-03)

BFS exacto (`research/bfs_exact.c`); "dos transferencias" significa que todo `pb` va antes que todo `pa`.

| n | Óptimo | Solo dos transferencias | Sin swaps | Dos transferencias y sin swaps (familia del motor) | Motor |
|---|---|---|---|---|---|
| 6 | 7,060 | 7,060 | 8,433 | 8,494 | |
| 7 | 9,306 | 9,307 | 10,691 | 10,802 | |
| 8 | 11,590 | 11,596 | 12,998 | 13,154 | 13,6 |
| 9 | 13,911 | 13,930 | 15,361 | 15,567 | 16,3-16,4 |
| 10 | 16,276 | 16,316 | 17,777 | 18,034 | 18,8-18,9 |

- **Separar en dos transferencias casi no cuesta:** 0,04 con swaps y 0,26 sin ellos, con n=10. Intercalar `pa` y `pb` no es lo que falta.
- **Los swaps valen 1,72 operaciones con n=10** dentro de las dos transferencias, y crecen unas 0,07 por elemento (1,43 con n=6). Si ese ritmo siguiera, serían unas 8 con n=100: poco.
- **La búsqueda también se queda corta:** el motor saca 18,8 cuando el óptimo de su propia familia es 18,03, un 4% más, aun con tiempo de sobra para n=10. Puede deberse a la búsqueda o a los arranques, y no sabemos si pasa lo mismo con n=100.

### Dónde se van las rotaciones (D4, 2026-11-03)

Motor de investigación a 5 s, 10 instancias por tamaño (`research/step_hist.py`).

| | Push: rotaciones por elemento | Pull: rotaciones por elemento | Pulls con 5 o más rotaciones | Rotaciones de los pulls con 5 o más |
|---|---|---|---|---|
| **n=100** | 1,08 | 1,83 | 9,4% | 34,5% |
| **n=500** | 1,65 | 3,57 | 27,0% | 61,8% |

- **En n=500 el problema es el pull:** 1624 rotaciones por caso frente a 749 en el push.
- **La cola de pasos caros acota S8:** si cada elemento con más de 4 rotaciones pudiera resolverse con 4, se ahorrarían unas 607 operaciones por caso en n=500 y 24 en n=100. Es una cota optimista, pero dice que las segundas salidas solo tienen sentido en n=500.

### Para qué usan los swaps las soluciones óptimas (D5, 2026-11-03)

BFS exacto con n=10. Un camino óptimo por caso, 20.000 casos, eligiendo swaps solo cuando no hay empate sin ellos (`SWAPS=1 bfs_exact`).

| Dos transferencias, n=10 | Óptimo |
|---|---|
| Todos los swaps | 16,316 |
| Sin `sb` | 16,316 |
| Solo `ss` | 16,413 |
| Solo `sa` | 16,558 |
| Sin swaps (familia del motor) | 18,034 |

- **`sb` no sirve; `sa` solo ya da 1,48 de los 1,72.**
- **Casi todos los swaps van en el push:** 1,38 de 1,95 por solución. Los contextos más comunes son `pb sa ra`, `pb sa rra`, `rra sa rra` y `rrr sa pa` justo antes del primer pull.
- **Qué hacen:** reparar `a` al pasar. Quedan en `a` pares invertidos que un `sa` arregla cuando el barrido los deja juntos en la cima. Por eso las soluciones óptimas sacan menos elementos de `a` (4,35 frente a 4,84).
- **`PAIRS=1` ya modelaba esto, pero no ayudaba ni con n=10 (18,3 y 18,3):** elige los pares una vez al principio y no los revisa. En 6 casos ganó 21 → 18 en uno y perdió 19 → 23 en otro.
- **Dejar que la carrera elija entre `keep` con y sin pares** (punto 34) lo arregla con n=10, pero no gana nada a 30 s con n=100 ni con n=500. Lo que valen los swaps con n pequeño no se traslada.

## Errores de proceso

Fallos nuestros, no del algoritmo. Cada uno deja una regla.

| Error | Consecuencia | Regla |
|---|---|---|
| Un parche con Python falló en Windows y no lo comprobé | Un barrido entero midió el motor sin cambios | Confirmar que el cambio está aplicado antes de medir |
| Presenté como hechos 850 y 390 para la pasada fija, que eran estimaciones | Las cifras reales son 945 y 480 | Marcar siempre qué es estimación |
| En la traducción, los movimientos locales abandonaban a mitad de simulación | 115 operaciones perdidas en n=500, porque un buen cambio puede encarecer antes de compensar | Comparar la traducción con el original en mismo caso y misma forma |
| Comparar variantes con instancias distintas | Diferencias que eran solo muestreo | Usar siempre comparación emparejada |
| `time.time()` en WSL dio tiempos negativos | Una tanda con tiempos erróneos | Usar `time.monotonic()` |
| WSL se detiene al quedar inactivo y borra `/tmp` | Binarios desaparecidos entre pruebas | Recompilar antes de medir |
| Objetos `.o` antiguos copiados de `src/` | Error de enlace con funciones ya borradas | Usar `make re` en copias nuevas |
| Portar una fórmula con `pow` a `sa_exp(g · sa_ln(x))` sin mirar el caso x = 0 | `push_swap` colgado con todo n impar pequeño | Probar todas las permutaciones de n=4-6 tras tocar las semillas |
| Escribir `\n` dentro de un parche enviado por heredoc o `sed` desde este shell | Llega como salto de línea real y el C no compila (dos veces en la fase 2) | Cadenas con escapes, siempre con la herramienta de edición |
| Lanzar `python3` por stdin en Git Bash | Proceso colgado y 512 KB de salida basura | Ejecutar los scripts dentro de WSL, desde archivo |
| Planificar sin leer el enunciado: el PDF no se abrió a la primera y no busqué otra vía | Un límite de 5 s sin base, venido de una nota antigua; un plan con 20 núcleos que las reglas no permiten; y escribí que la Norma no obligaba | Leer entero el enunciado antes de plantear nada |

## Por dónde no ir

- **Más ajuste de parámetros:** temperaturas, frecuencias y número de formas dan resultados planos.
- **Más velocidad sin más:** ×2,5 da unas −10 operaciones. Sirve para la categoría de 5 s, pero no cambia de nivel.
- **Pasadas fijas de partición:** cuestan el doble de lo que ahorran.
- **Superoptimizar la salida:** 1% como mucho.
- **Más vecindario local dentro de la familia:** ventanas exactas de 12 (punto 32) y destruir y reconstruir rápido (punto 31) no encuentran nada a partir de n=100.
- **Formas aprendidas de una muestra:** peores que los anillos (punto 33).
- **Más transferencias dejando elementos esperando:** el que espera estorba a todos los demás (punto 35). Cualquier variante de cuatro transferencias tendría que evitar ese coste, y no veo cómo.
- **Segundas salidas en el pull (S8):** aparcar, reubicar o volver a sacar elementos gana 0-4 operaciones sobre ~2000 (punto 37).
- **Heurística aprendida (S9):** imita bien al motor, pero la búsqueda guiada cae en trampas de +160-190 operaciones y 6 rondas de DAgger no resuelven ni un caso con n=100 (etapa 3, DAgger).

No está descartado, aunque lo parezca:

- **Varias pasadas en general:** el recocido de cuatro transferencias con elementos aplazados ya está probado y falla (punto 35). Quedan otras formas de varias pasadas, sin una idea concreta que evite el coste de los obstáculos.
- **Intercalar:** el prototipo era lento y tenía `keep` fijo.

## Escenarios pendientes

Ninguno probado aún. Las previsiones son estimaciones mías, no medidas.

| # | Escenario | Idea | Previsión | Coste | Se descarta si |
|---|---|---|---|---|---|
| R0 | Cumplir todas las reglas | Hecho el 2026-11-03 (punto 30), salvo la cabecera de 42, que necesita el login | Medido: 382 / 3119 | — | Es obligatorio |
| — | Hecho el 2026-11-03 | D1, D4, S1, S2 y S3 (puntos 21-29). S1 y S3 se quedan; S2 se descarta. Fase 2 (puntos 31-33): S5 rápido, S6 y S4, los tres descartados | | | |
| D1 | BFS con bit de fase | Óptimo exacto de "dos transferencias" con n≤10 | Dice si la distancia viene de la familia o de la búsqueda | Minutos | Es solo un diagnóstico |
| D2 | Curva exacta hasta n≈14-16 | Búsqueda bidireccional en casos sueltos | Muestra si la eficiencia sube con n | Horas de CPU | Es solo un diagnóstico |
| D4 | Histograma de pasos caros | Cuánto coste hay en pasos con muchas rotaciones | Acota el valor de S8 | Minutos | No aplica |
| S1 | Anillos completos en `b` | Todas las formas actuales son V o Λ; probar anillo descendente y ascendente, de centro hacia fuera | n=500: de −100 a −400. n=100: poco | Horas | No gana a 5 s ni a 60 s |
| S2 | Resolver el inverso | Mismas formas sobre P⁻¹, invertir la solución | Formas por posición en vez de por rango | Horas | Ninguna gana en más de un 10% de casos |
| S3 | Bandas desiguales | Las primeras vueltas son las caras, así que deberían llevar más elementos | −20 a −100 en n=500 | Horas | Plano |
| S4 | Multiescala | Resolver una muestra de 100 elementos y ampliar | n=500 con la organización que n=100 alcanza sola | Medio día | No supera a S1 |
| S5 | Destruir y reconstruir | Quitar 5-30 elementos cercanos y reinsertarlos en los tres órdenes | −2% a −5% | 1 día | No gana a igual tiempo |
| S6 | Ventanas exactas | Reordenar óptimamente tramos de 8-12 elementos por programación dinámica | −1% a −3% | 1 día | Menos de −1% |
| S7 | Templado por réplicas | Varias temperaturas dentro de un solo hilo y dentro del límite de tiempo. Los 20 núcleos solo sirven para medir varias entradas a la vez | Ayuda a salir de mínimos locales, sin ganar velocidad | Medio día | No supera a un solo recocido con el mismo trabajo |
| S8 | Segundas salidas | Solo elementos cuyo acceso cuesta bastante más de 2 operaciones | Probada 2026-11-06 (punto 37): 0-4 de ganancia. Descartada | 2-3 días | D4 muestra poco margen |
| S9 | Heurística aprendida | Red que estima el coste restante, estilo DeepCubeA. Se entrena fuera con GPU y los pesos van dentro del fuente en C, porque `open` no está autorizado | Probada 2026-11-03/06: 0 resueltas con n=100 tras 6 rondas de DAgger. Descartada | Semanas | Último recurso |

## Plan de S9: heurística aprendida (2026-11-03)

Idea: una red que estime cuántas operaciones faltan desde cualquier estado (`a`, `b`), y una búsqueda en haz que la use para elegir cada operación. Es lo que hizo DeepCubeA con el cubo de Rubik. Cada etapa tiene un criterio para seguir o parar.

| Etapa | Qué | Sigue si | Para si |
|---|---|---|---|
| 1 | n=10: coste restante exacto del BFS como etiqueta, una red pequeña que lo aprenda, y una búsqueda en haz que la use | La búsqueda se acerca al óptimo (16,28) y supera al motor (17,6-18,3) | No baja de lo que da el motor: el método no sirve ni con etiquetas perfectas |
| 2 | Generalizar: entrenar con unas n y probar con otras más grandes (10 → 12-16), con rasgos que no dependan de n | El error crece despacio con n | La red no generaliza fuera de su tamaño |
| 3 | n=100 sin etiquetas exactas: arrancar con las soluciones del motor y mejorar las etiquetas con iteración de valores (h(s) = min sobre las operaciones de 1 + h(s')), en la GPU | La búsqueda guiada iguala al motor en n=100 | Se queda más de un 3% peor tras varias rondas |
| 4 | Llevarlo a `push_swap`: red en C, sin libm, pesos en el fuente, un solo hilo, <30 s | Mejora la media de n=100 | No cabe en el tiempo |

- **Riesgo principal:** la etapa 3. En n=100 no hay etiquetas exactas, y una red que imita al motor solo puede igualarlo. Superarlo exige que la iteración de valores mejore las etiquetas, y eso no está garantizado en un espacio tan grande.
- **Coste en tiempo de ejecución:** evaluar la red en cada nodo de la búsqueda es caro. Para n=100, un haz de 100 con una red de unos 100.000 multiplicaciones son unos 4·10¹⁰ por caso: cabe en 30 s, pero justo. n=500 no parece viable.
- **Entorno:** RTX 4060 Laptop (8 GB), visible desde WSL. PyTorch solo está en Windows y sin CUDA. Las etapas 1 y 2 caben en CPU; la 3 necesita instalar PyTorch con CUDA (~2,5 GB de descarga).

### Etapa 1 (2026-11-03): superada

`research/s9_stage1.py`, etiquetas exactas del BFS de n=10 (`DUMP=` en `bfs_exact`). Red de 110 → 512 → 512 → 256 → 1, con entrada one-hot (posición, rango) y si cada posición está en `a`.

| Entrenamiento | Error medio | Exacta al redondear | Haz 10 | Haz 100 | Haz 1000 |
|---|---|---|---|---|---|
| 3M estados, 6 épocas, CPU (7 min) | 0,50 | 58% | 6/200 resueltos | 7/200 | — |
| 4M estados, 25 épocas, GPU (53 s) | 0,38 | 71% | 156/200, 29,1 | 200/200, 16,33 | **200/200, 16,235 = óptimo** |

- **Con buenas etiquetas, la búsqueda guiada llega al óptimo** (16,235 en estos 200 casos), frente a 17,6-18,3 del motor.
- **La calidad de la red decide:** con 6 épocas, ni un haz de 100 resolvía los casos. La GPU hace el entrenamiento unas 35 veces más rápido.
- Entorno: PyTorch 2.11 con CUDA 12.8 en `~/psw/venv` (WSL). La ruta de Python, sin `-I`, porque NumPy y PyTorch están en el site-packages del usuario.

### Etapa 2 (2026-11-03): superada para un tamaño más

`research/s9_stage2.py`. Un token por elemento con 13 rasgos que no dependen de n: pila, distancia a la cima y al fondo, rango normalizado, si el de debajo es su vecino de rango, tamaño de cada pila. Un transformer pequeño (3 capas, d=96) y la suma de una contribución por token. Entrenado con n=7, 8 y 9, probado con n=10, que no ve nunca. 30 épocas, 14 min en GPU.

| | Error medio | Exacta al redondear |
|---|---|---|
| n=7 (entreno) | 0,18 | 97% |
| n=8 (entreno) | 0,21 | 94% |
| n=9 (entreno) | 0,25 | 88% |
| **n=10, sin verlo nunca** | **0,33** | **78%** |
| Red de la etapa 1, entrenada con n=10 | 0,38 | 71% |

- **Generaliza un tamaño más arriba, y mejor que una red entrenada en ese tamaño.** Con un haz de 100 da el óptimo exacto en los 200 casos de n=10 (16,235).
- **El error crece unos 0,04-0,07 por elemento.** Si siguiera así, serían unas 6 operaciones con n=100: un 2% de la longitud. Es una extrapolación, no una medida.
- **Problema para la etapa 4:** esta red cuesta unos 1,4·10⁷ operaciones por estado con n=100. Un haz de 100 en 400 pasos son ~4·10⁵ evaluaciones, ~6·10¹² operaciones: imposible en C con un hilo y 30 s. Si la red sirve, habrá que hacerla mucho más barata o usarla de otra forma.

### Etapa 2b (2026-11-03): no generaliza más allá de n≈12

El mismo modelo, entrenado con n=7-10 (n=10: error 0,28, 84% exacta), guardado y probado con búsqueda en haz frente al motor en n=15, 20 y 30 (`research/s9_eval.py`): **0 de 20 resueltos en cada tamaño.** Lo que predice:

| n | Estado ordenado (debería ser 0) | A un paso (debería ser 1) | Salida al azar |
|---|---|---|---|
| 8 (entreno) | 0,4 | 1,2 | 11,4 |
| 10 (entreno) | 1,6 | 2,2 | 16,2 |
| 12 | 4,4 | 4,8 | 20,8 |
| 15 | 12,4 | 12,2 | 27,0 |
| 20 | 26,4 | 26,3 | 34,6 |
| 30 | 43,7 | 43,6 | 49,1 |

- **Fuera de su rango no reconoce ni la solución:** aprendió sobre todo "cuanto más grande, más caro", no la estructura. El éxito con n=10 era interpolar entre tamaños cercanos.
- **Probable causa en parte:** varios rasgos crecen con n (distancias y rangos divididos por 10) y salen del rango de entrenamiento. Quitarlos no basta: el coste de rotar depende de distancias absolutas.
- **Lo que haría falta (etapa 3):** etiquetas para n grandes sin BFS, por iteración de valores (h(s) = min de 1 + h(s'), con la solución anclada a 0), subiendo n poco a poco. Es lo que hizo DeepCubeA, con días de varias GPU. Con n=100 la profundidad es ~380 pasos, así que harían falta cientos de rondas. Y sigue en pie el problema de velocidad en C.

### Etapa 3, ronda 0 (2026-11-03): la red imita bien, la búsqueda cae en trampas

`research/s9_stage3.py`. 8000 soluciones del motor a 0,5 s (media 405,9) → 3,26M estados etiquetados con las operaciones que faltan. El mismo transformer, entrenado ya en n=100 (6,5 min por época en la GPU; parado en la época 3, error medio 6,9). Búsqueda en haz de 100 sobre 10 entradas nuevas: **0 de 10 resueltas** (motor a 30 s: 373,4).

- **A lo largo de las soluciones del motor la red acierta:** a 374 operaciones del final predice 404; a 174, 178; a 94, 95; a 14, 13. Con el estado ordenado, −0,6.
- **La búsqueda va por delante del motor al principio** (profundidad 150: 211 restantes, frente a ~224 del motor), pero luego se atasca: unos 300 pasos en ~205, y desde la profundidad 600 en ~93, hasta rendirse en 800.
- **Causa:** la búsqueda entra en estados que el motor nunca visita, y ahí la red se inventa valores demasiado bajos ("valles" falsos). Es el fallo típico de entrenar solo por imitación.
- **Arreglo estándar (DAgger):** recoger los estados donde se atasca, etiquetarlos con un coste real y reentrenar. Haría falta un terminador que ordene desde cualquier estado (el motor solo arranca con `b` vacío) y varias rondas de ~1 hora.

### Etapa 3, DAgger (2026-11-06): 6 rondas, 0 de 5 resueltas en todas

`s9_stage3.py dagger s9/m0.pt s9/r0.npz ./rr2 100 6 s9`. Cada ronda: haz de 50 sobre 40 entradas nuevas, se guardan los estados donde se atasca (~2000), se etiquetan con el terminador (bajar todo `b` y luego el motor 0,3 s), se reentrena 3000 pasos mezclando con los datos base, y se prueba con haz de 100 en 5 entradas fijas (motor a 30 s: 370,6). ~12-20 min por ronda.

| Ronda | Estados recogidos | Etiqueta − red | Resueltas |
|---|---|---|---|
| 0 | 2184 | +192,8 | 0/5 |
| 1 | 1814 | +174,0 | 0/5 |
| 2 | 2280 | +182,4 | 0/5 |
| 3 | 2110 | +172,7 | 0/5 |
| 4 | 2136 | +164,4 | 0/5 |
| 5 | 2326 | +157,9 | 0/5 |

- **Las trampas no son pequeñas:** donde se atasca, la red cree que faltan unas pocas operaciones y en realidad faltan ~160-190 más. La búsqueda no se queda cerca del final: llega a estados muy malos que la red ve como casi resueltos.
- **El hueco baja ~7 por ronda,** pero la búsqueda encuentra trampas nuevas cada vez (siempre ~2000 estados) y no resuelve ninguna. A ese ritmo harían falta más de 20 rondas (~5 h) solo para cerrar el hueco medio, y nada indica que entonces resuelva.
- **Parte del hueco es del terminador** (bajar todo `b` cuesta), pero no puede explicar +160: el motor desde cero cuesta ~400.
- **Conclusión:** con este tamaño de red y estos datos, la heurística aprendida no sirve para n=100. Ni siquiera llega a resolver, y aún quedaría el coste en C (~10⁷ operaciones por evaluación) y meter los pesos cumpliendo la Norma. **S9 queda descartada** como camino al récord; lo aprendido (etapas 1-2) solo vale para n≤12, donde el BFS exacto ya basta.
