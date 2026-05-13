// =============================================================================
//  count_word.cpp
//  Lee un CSV de bolsa de palabras y reporta cuantas veces aparece una
//  palabra dada en cada libro y su total.
//  Uso: ./count_word <archivo.csv> <palabra>
// =============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <archivo.csv> <palabra>\n";
        return 1;
    }

    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "No se pudo abrir " << argv[1] << "\n";
        return 1;
    }

    // Pasar la palabra a minusculas (el tokenizer lo hace tambien)
    std::string target = argv[2];
    std::transform(target.begin(), target.end(), target.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // Parsear encabezado
    std::string header_line;
    if (!std::getline(f, header_line)) {
        std::cerr << "CSV vacio\n"; return 1;
    }
    std::vector<std::string> header;
    {
        std::stringstream ss(header_line);
        std::string col;
        while (std::getline(ss, col, ',')) header.push_back(col);
    }

    int col_idx = -1;
    for (size_t i = 0; i < header.size(); ++i)
        if (header[i] == target) { col_idx = static_cast<int>(i); break; }

    if (col_idx < 0) {
        std::cout << "La palabra \"" << target << "\" no aparece en "
                  << argv[1] << "\n";
        return 0;
    }

    // Recorrer filas
    std::cout << "\n=== Conteo de \"" << target << "\" en " << argv[1] << " ===\n";
    long total = 0;
    std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string cell, book_name;
        int count = 0, col = 0;
        while (std::getline(ss, cell, ',')) {
            if (col == 0) book_name = cell;
            else if (col == col_idx) {
                try { count = std::stoi(cell); } catch (...) { count = 0; }
            }
            col++;
        }
        std::cout << "  " << book_name << ": " << count << "\n";
        total += count;
    }
    std::cout << "  TOTAL: " << total << "\n";
    return 0;
}