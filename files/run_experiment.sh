#!/bin/bash
# =============================================================================
#  run_experiment.sh
#  -------------------------------------------------------------------------
#  Ejecuta la version serial y la version MPI, mide los tiempos y reporta
#  el speed-up = T_serial / T_mpi.
#
#  Uso:
#      ./run_experiment.sh [archivo_urls] [num_procesos]
#
#  Por defecto usa urls.txt y 4 procesos.
# =============================================================================

URLS_FILE="${1:-urls.txt}"
NPROCS="${2:-4}"

if [ ! -f "$URLS_FILE" ]; then
    echo "Error: no existe $URLS_FILE"
    exit 1
fi

if [ ! -x ./bow_serial ] || [ ! -x ./bow_mpi ]; then
    echo "Binarios faltantes. Ejecuta 'make' primero."
    exit 1
fi

echo "=========================================="
echo "  Bolsa de Palabras: experimento"
echo "=========================================="
echo "Archivo URLs : $URLS_FILE"
echo "Libros       : $(grep -cvE '^\s*(#|$)' "$URLS_FILE")"
echo "Procesos MPI : $NPROCS"
echo

echo "--- Version SERIAL ---"
SERIAL_OUT=$(./bow_serial "$URLS_FILE" salida_serial.csv 2>&1)
echo "$SERIAL_OUT"
T_SERIAL=$(echo "$SERIAL_OUT" | grep '^TIEMPO_SERIAL=' | cut -d'=' -f2)

echo
echo "--- Version MPI (${NPROCS} procesos) ---"
MPI_OUT=$(mpirun --oversubscribe -np "$NPROCS" ./bow_mpi "$URLS_FILE" salida_mpi.csv 2>&1)
echo "$MPI_OUT"
T_MPI=$(echo "$MPI_OUT" | grep '^TIEMPO_MPI=' | cut -d'=' -f2)

echo
echo "=========================================="
echo "  Resultados"
echo "=========================================="
echo "Tiempo SERIAL: ${T_SERIAL} s"
echo "Tiempo MPI   : ${T_MPI} s"

if [ -n "$T_SERIAL" ] && [ -n "$T_MPI" ]; then
    SPEEDUP=$(awk -v s="$T_SERIAL" -v m="$T_MPI" 'BEGIN{ printf "%.4f", s/m }')
    echo "Speed-up     : ${SPEEDUP}x"
    REQ=$(awk -v sp="$SPEEDUP" 'BEGIN{ print (sp>1.2) ? "SI" : "NO" }')
    echo "¿Cumple >1.2x?: ${REQ}"
fi
echo "=========================================="
