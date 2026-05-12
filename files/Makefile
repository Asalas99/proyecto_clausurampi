CXX      = g++
MPICXX   = mpic++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra

all: bow_serial bow_mpi

bow_serial: bow_serial.cpp bow_common.h
	$(CXX) $(CXXFLAGS) -o bow_serial bow_serial.cpp

bow_mpi: bow_mpi.cpp bow_common.h
	$(MPICXX) $(CXXFLAGS) -o bow_mpi bow_mpi.cpp

clean:
	rm -f bow_serial bow_mpi salida_serial.csv salida_mpi.csv

.PHONY: all clean
