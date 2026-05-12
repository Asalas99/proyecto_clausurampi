# Bolsa de Palabras con MPI

Proyecto de clausura — *Cómputo Paralelo y en la Nube*.

Construye una matriz de **Bolsa de Palabras (Bag of Words)** a partir de libros
descargados dinámicamente desde Project Gutenberg, usando una versión serial y
una versión paralela con **MPI**, y reporta el **speed-up**.

## Archivos

| Archivo              | Descripción                                              |
|----------------------|----------------------------------------------------------|
| `bow_common.h`       | Funciones compartidas: descarga, tokenización, lectura.  |
| `bow_serial.cpp`     | Versión serial.                                          |
| `bow_mpi.cpp`        | Versión paralela con MPI.                                |
| `Makefile`           | Compilación de ambas versiones.                          |
| `urls.txt`           | Lista de URLs (una por línea, `#` para comentarios).     |
| `run_experiment.sh`  | Corre ambas versiones y reporta el speed-up.             |

## Requisitos

- `g++` con soporte C++17.
- Una implementación de MPI: **OpenMPI** o **MPICH** (`mpic++`, `mpirun`).
- `curl` instalado en el sistema (se usa para descargar los libros).

En Ubuntu/Debian:

```bash
sudo apt-get install -y g++ openmpi-bin libopenmpi-dev curl
```

## Compilación

```bash
make
```

Esto produce dos binarios: `bow_serial` y `bow_mpi`.

## Ejecución

### Manual

```bash
# Serial
./bow_serial urls.txt salida_serial.csv

# Paralela con q procesos (por ejemplo q = 4)
mpirun --oversubscribe -np 4 ./bow_mpi urls.txt salida_mpi.csv
```

### Automática (recomendada): corre ambas y calcula speed-up

```bash
chmod +x run_experiment.sh
./run_experiment.sh urls.txt 4
```

Salida esperada (resumen):

```
Tiempo SERIAL: 14.32 s
Tiempo MPI   : 4.18 s
Speed-up     : 3.4258x
¿Cumple >1.2x?: SI
```

## Entrada

- **Archivo de URLs**: una URL de Project Gutenberg por línea. Funciona para
  cualquier número `k` de libros.
- **Número de procesos `q`**: se especifica con `-np q` en `mpirun`.

## Salida

- `salida_serial.csv` / `salida_mpi.csv`: matriz con:
  - Primera columna `book` = etiqueta del libro (derivada de la URL).
  - Demás columnas = una por cada palabra única del vocabulario global, en
    orden alfabético.
  - Cada celda = frecuencia de la palabra en ese libro.
- Tiempos impresos en stdout como `TIEMPO_SERIAL=...` y `TIEMPO_MPI=...`.

## Estrategia de paralelización

1. **Rank 0** lee las URLs y las difunde con `MPI_Bcast`.
2. Los libros se reparten **round-robin**: el proceso `p` se queda con los
   libros cuyos índices cumplen `i mod size == rank`.
3. Cada proceso **descarga y cuenta palabras** de sus libros en paralelo.
4. Los **vocabularios locales** se reúnen en rank 0 con `MPI_Gatherv`, se
   construye la unión ordenada y se difunde con `MPI_Bcast`.
5. Cada proceso construye sus **filas** alineadas al vocabulario global.
6. Las filas y sus índices de libro se reúnen en rank 0 con `MPI_Gatherv`.
7. Rank 0 reordena por índice y escribe el CSV.

El cuello de botella principal es la **descarga por red**, que es trivialmente
paralelizable: por eso el speed-up con varios procesos es bastante alto.

## Probar sin internet (opcional)

Si quieres validar el speed-up sin depender de la conexión a Project Gutenberg
(por ejemplo en una computadora sin internet o con un firewall restrictivo),
se incluye `slow_server.py`, un mini servidor HTTP local que sirve archivos
locales agregando latencia simulada (1.5 s por petición por defecto).

```bash
# 1) Pon algunos archivos .txt en una carpeta libros/
# 2) Levanta el servidor en una terminal:
python3 slow_server.py libros/
# 3) En urls.txt apunta a http://127.0.0.1:8765/<archivo>.txt
# 4) Corre el experimento normalmente
./run_experiment.sh urls.txt 4
```

Con esto se reproduce el escenario de descarga real (en el que la latencia de
red, no la CPU, domina el tiempo total) y el speed-up se observa claramente.

## Resultado obtenido en pruebas

Con el servidor local simulando 1 s de latencia por libro, 6 libros:

| Configuración    | Tiempo  | Speed-up |
|------------------|---------|----------|
| Serial           | 6.35 s  | —        |
| MPI, 2 procesos  | 3.13 s  | **2.03x**|
| MPI, 3 procesos  | 2.11 s  | **3.00x**|
| MPI, 4 procesos  | 2.12 s  | **3.00x**|
| MPI, 6 procesos  | 1.22 s  | **5.19x**|

(Todos superan el requisito mínimo de **1.2x**.)
