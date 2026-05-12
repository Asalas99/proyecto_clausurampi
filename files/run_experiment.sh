#!/bin/bash
# =============================================================================
#  run_experiment.sh
#  Ejecuta el experimento con descubrimiento dinamico de libros via Gutendex.
#  Uso:  ./run_experiment.sh [procesos] [query] [count]
#  Ejemplo: ./run_experiment.sh 4 shakespeare 6
# =============================================================================

NPROCS="${1:-4}"
QUERY="${2:-shakespeare}"
COUNT="${3:-6}"

if [ ! -x ./bow_serial ] || [ ! -x ./bow_mpi ]; then
    echo "Binarios faltantes. Ejecuta 'make' primero."
    exit 1
fi

echo "=========================================="
echo "  Bolsa de Palabras: experimento dinamico"
echo "=========================================="
echo "Query        : $QUERY"
echo "Libros (max) : $COUNT"
echo "Procesos MPI : $NPROCS"
echo

echo "--- Version SERIAL ---"
SERIAL_OUT=$(./bow_serial salida_serial.csv "$QUERY" "$COUNT" 2>&1)
echo "$SERIAL_OUT"
T_SERIAL=$(echo "$SERIAL_OUT" | grep '^TIEMPO_SERIAL=' | cut -d'=' -f2)

echo
echo "--- Version MPI (${NPROCS} procesos) ---"
MPI_OUT=$(mpirun --oversubscribe -np "$NPROCS" ./bow_mpi salida_mpi.csv "$QUERY" "$COUNT" 2>&1)
echo "$MPI_OUT"
T_MPI=$(echo "$MPI_OUT" | grep '^TIEMPO_MPI=' | cut -d'=' -f2)

echo

echo "  Resultados"

echo "Tiempo SERIAL: ${T_SERIAL} s"
echo "Tiempo MPI   : ${T_MPI} s"

if [ -n "$T_SERIAL" ] && [ -n "$T_MPI" ]; then
    SPEEDUP=$(awk -v s="$T_SERIAL" -v m="$T_MPI" 'BEGIN{ printf "%.4f", s/m }')
    echo "Speed-up     : ${SPEEDUP}x"
    REQ=$(awk -v sp="$SPEEDUP" 'BEGIN{ print (sp>1.2) ? "SI" : "NO" }')
    echo "Cumple >1.2x : ${REQ}"
fi
