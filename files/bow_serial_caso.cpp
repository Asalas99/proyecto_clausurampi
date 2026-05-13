// =============================================================================
//  bow_serial_caso.cpp
//  Genera la matriz de bolsa de palabras a partir de las URLs en case.txt.
//  El conteo de palabras especificas se hace despues con count_word.
// =============================================================================
#include "bow_common.h"
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <salida.csv>\n";
        return 1;
    }
    const std::string output_csv = argv[1];

    // 1) Leer URLs de case.txt
    std::vector<std::string> urls;
    {
        std::ifstream f("case.txt");
        if (!f) { std::cerr << "[Serial] No se encontro case.txt\n"; return 1; }
        std::string line;
        while (std::getline(f, line)) {
            auto a = line.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) continue;
            auto b = line.find_last_not_of(" \t\r\n");
            line = line.substr(a, b - a + 1);
            if (line.empty() || line[0] == '#') continue;
            urls.push_back(line);
        }
    }
    const int K = static_cast<int>(urls.size());
    if (K == 0) { std::cerr << "[Serial] case.txt vacio\n"; return 1; }

    auto t_start = std::chrono::high_resolution_clock::now();

    // 2) Descargar + tokenizar + contar
    std::vector<std::map<std::string, int>> book_counts(K);
    std::set<std::string> global_vocab;
    for (int i = 0; i < K; ++i) {
        std::cerr << "[Serial] (" << i+1 << "/" << K << ") " << urls[i] << "\n";
        std::string text = download_text(urls[i], i);
        book_counts[i] = count_words(text);
        for (const auto& kv : book_counts[i]) global_vocab.insert(kv.first);
    }

    // 3) Escribir CSV
    std::vector<std::string> vocab(global_vocab.begin(), global_vocab.end());
    std::ofstream out(output_csv);
    out << "book";
    for (const auto& w : vocab) out << "," << w;
    out << "\n";
    for (int i = 0; i < K; ++i) {
        out << get_book_label(urls[i]);
        for (const auto& w : vocab) {
            auto it = book_counts[i].find(w);
            out << "," << (it == book_counts[i].end() ? 0 : it->second);
        }
        out << "\n";
    }
    out.close();

    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t_end - t_start).count();
    std::cout << "TIEMPO_SERIAL=" << elapsed << "\n";
    std::cout << "[Serial] Libros: " << K << " | Vocab: " << vocab.size() << "\n";
    return 0;
}