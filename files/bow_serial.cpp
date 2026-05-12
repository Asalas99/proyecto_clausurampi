// =============================================================================
//  bow_serial.cpp
//  -------------------------------------------------------------------------
//  Versión SERIAL del programa de Bolsa de Palabras.
//
//  Uso:
//      ./bow_serial <archivo_urls.txt> <salida.csv>
//
//  Comportamiento:
//      1. Lee la lista de URLs.
//      2. Por cada URL: descarga el libro, lo tokeniza y cuenta frecuencias.
//      3. Construye el vocabulario global como la UNIÓN ordenada de todas
//         las palabras observadas.
//      4. Escribe la matriz fila=libro, columna=palabra en CSV.
//      5. Imprime el tiempo total como TIEMPO_SERIAL=<segundos>.
//
//  Proyecto: Cómputo Paralelo y en la Nube
// =============================================================================
#include "bow_common.h"

#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Uso: " << argv[0] << " <salida.csv> <query> <count>\n";
        std::cerr << "Ejemplo: " << argv[0] << " out.csv shakespeare 6\n";
        return 1;
    }

    const std::string output_csv = argv[1];
    const std::string query      = argv[2];
    const int         count      = std::atoi(argv[3]);

    std::cerr << "[Serial] Consultando Gutendex API: query=\"" << query
              << "\", count=" << count << "\n";

    // ---------- 1) Descubrir URLs dinamicamente via Gutendex ----------
    std::vector<std::string> urls = fetch_urls_dynamic(query, count);
    const int K = static_cast<int>(urls.size());
    if (K == 0) {
        std::cerr << "[Serial] No se obtuvieron URLs de Gutendex. "
                     "Verifica conexion y query.\n";
        return 1;
    }
    std::cerr << "[Serial] Procesando " << K
              << " libros descubiertos dinamicamente...\n";

    // ---------- 2) Empezar a medir tiempo ----------
    auto t_start = std::chrono::high_resolution_clock::now();

    // ---------- 3) Descargar, tokenizar y contar por libro ----------
    std::vector<std::map<std::string, int>> book_counts(K);
    std::set<std::string> global_vocab;  // unión de palabras

    for (int i = 0; i < K; ++i) {
        std::cerr << "[Serial] (" << (i + 1) << "/" << K << ") "
                  << urls[i] << "\n";
        std::string text = download_text(urls[i], i);
        if (text.empty()) {
            std::cerr << "[Serial] AVISO: descarga vacía para " << urls[i] << "\n";
        }
        book_counts[i] = count_words(text);
        for (const auto& kv : book_counts[i]) global_vocab.insert(kv.first);
    }

    // ---------- 4) Construir vocabulario global ordenado ----------
    std::vector<std::string> vocab(global_vocab.begin(), global_vocab.end());
    const int V = static_cast<int>(vocab.size());

    // ---------- 5) Escribir CSV (libros x palabras) ----------
    std::ofstream out(output_csv);
    if (!out) {
        std::cerr << "[Serial] Error: no se pudo abrir " << argv[2] << " para escritura.\n";
        return 1;
    }
    // Encabezado
    out << "book";
    for (const auto& w : vocab) out << "," << w;
    out << "\n";
    // Filas
    for (int i = 0; i < K; ++i) {
        out << get_book_label(urls[i]);
        for (const auto& w : vocab) {
            auto it = book_counts[i].find(w);
            out << "," << (it == book_counts[i].end() ? 0 : it->second);
        }
        out << "\n";
    }
    out.close();

    // ---------- 6) Reportar tiempo ----------
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t_end - t_start).count();

    std::cout << "TIEMPO_SERIAL=" << elapsed << "\n";
    std::cout << "[Serial] Libros: " << K
              << " | Vocabulario: " << V
              << " | Archivo: " << output_csv << "\n";
    return 0;
}
