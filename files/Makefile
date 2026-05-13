CXX      = g++
MPICXX   = mpic++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra

all: bow_serial bow_mpi bow_serial_caso bow_mpi_caso count_word

bow_serial: bow_serial.cpp bow_common.h
	$(CXX) $(CXXFLAGS) -o bow_serial bow_serial.cpp

bow_mpi: bow_mpi.cpp bow_common.h
	$(MPICXX) $(CXXFLAGS) -o bow_mpi bow_mpi.cpp

bow_serial_caso: bow_serial_caso.cpp bow_common.h
	$(CXX) $(CXXFLAGS) -o bow_serial_caso bow_serial_caso.cpp

bow_mpi_caso: bow_mpi_caso.cpp bow_common.h
	$(MPICXX) $(CXXFLAGS) -o bow_mpi_caso bow_mpi_caso.cpp

count_word: count_word.cpp
	$(CXX) $(CXXFLAGS) -o count_word count_word.cpp

clean:
	rm -f bow_serial bow_mpi bow_serial_caso bow_mpi_caso count_word \
	      salida_serial.csv salida_mpi.csv \
	      salida_serial_caso.csv salida_mpi_caso.csv

.PHONY: all clean