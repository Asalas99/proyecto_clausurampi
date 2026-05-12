// =============================================================================
//  bow_common.h
//  -------------------------------------------------------------------------
//  Funciones utilitarias compartidas por las versiones serial y paralela
//  del programa de Bolsa de Palabras (Bag of Words).
//
//  Incluye:
//    - download_text:  descarga un archivo de texto desde una URL usando curl
//    - count_words:    tokeniza un texto y devuelve un mapa palabra -> frecuencia
//    - get_book_label: obtiene una etiqueta legible a partir de la URL del libro
//    - read_urls:      lee un archivo con una URL por línea
//
//  Proyecto: Cómputo Paralelo y en la Nube - Bolsa de Palabras con MPI
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

// -----------------------------------------------------------------------------
// download_text
//   Descarga el contenido de `url` a un archivo temporal usando `curl`,
//   lee el archivo y lo regresa como std::string.
//   `unique_id` evita colisiones entre procesos MPI corriendo en paralelo.
// -----------------------------------------------------------------------------
inline std::string download_text(const std::string& url, int unique_id) {
    std::string tmp_file = "/tmp/bow_book_" + std::to_string(unique_id) + ".txt";
    // -s: silencioso, -L: sigue redirecciones, -o: archivo de salida
    std::string cmd = "curl -s -L --max-time 60 -o \"" + tmp_file + "\" \"" + url + "\"";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::remove(tmp_file.c_str());
        return "";
    }

    std::ifstream f(tmp_file);
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();
    std::remove(tmp_file.c_str());
    return ss.str();
}

// -----------------------------------------------------------------------------
// count_words
//   Tokeniza el texto:
//     - convierte a minúsculas
//     - considera "palabra" toda secuencia de caracteres alfabéticos
//     - todo lo demás (puntuación, dígitos, espacios) actúa como separador
//   Devuelve un std::map ordenado de palabra -> frecuencia.
// -----------------------------------------------------------------------------
inline std::map<std::string, int> count_words(const std::string& text) {
    std::map<std::string, int> counts;
    std::string word;
    word.reserve(32);

    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalpha(uc)) {
            word += static_cast<char>(std::tolower(uc));
        } else {
            if (!word.empty()) {
                counts[word]++;
                word.clear();
            }
        }
    }
    if (!word.empty()) counts[word]++;
    return counts;
}

// -----------------------------------------------------------------------------
// get_book_label
//   Extrae el nombre de archivo (sin extensión) de una URL, p.ej.:
//     "https://www.gutenberg.org/cache/epub/1513/pg1513.txt" -> "pg1513"
//   Sirve como identificador legible de cada libro en la primera columna del CSV.
// -----------------------------------------------------------------------------
inline std::string get_book_label(const std::string& url) {
    auto last_slash = url.find_last_of('/');
    std::string filename = (last_slash == std::string::npos)
                               ? url
                               : url.substr(last_slash + 1);
    auto dot = filename.find_last_of('.');
    if (dot != std::string::npos) filename = filename.substr(0, dot);
    return filename;
}

// -----------------------------------------------------------------------------
// read_urls
//   Lee `filename` línea por línea, ignorando líneas vacías o que empiezan con '#'.
//   Cada línea no vacía se asume como una URL.
// -----------------------------------------------------------------------------
inline std::vector<std::string> read_urls(const std::string& filename) {
    std::vector<std::string> urls;
    std::ifstream f(filename);
    std::string line;
    while (std::getline(f, line)) {
        // Quitar espacios/retornos al inicio y final
        auto a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        auto b = line.find_last_not_of(" \t\r\n");
        line = line.substr(a, b - a + 1);
        if (line.empty() || line[0] == '#') continue;
        urls.push_back(line);
    }
    return urls;
}
