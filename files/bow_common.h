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
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <chrono>

// -----------------------------------------------------------------------------
// download_text
//   Descarga el contenido de `url` a un archivo temporal usando `curl`,
//   lee el archivo y lo regresa como std::string.
//   `unique_id` evita colisiones entre procesos MPI corriendo en paralelo.
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// fetch_urls_dynamic
//   Consulta la API publica de Gutendex (https://gutendex.com) con un termino
//   de busqueda y devuelve hasta `count` URLs de descarga en formato texto plano.
//   El programa nunca conoce las URLs por adelantado: las descubre en tiempo
//   de ejecucion a partir del catalogo de Project Gutenberg.
// -----------------------------------------------------------------------------
inline std::vector<std::string> fetch_urls_dynamic(const std::string& query_input, int count) {
    std::vector<std::string> urls;
    std::string query = query_input;

    // Si el usuario pasa "random", elegir un tema aleatorio del catalogo interno.
    // El RNG se siembra con el reloj del sistema, asi cada corrida da algo distinto.
    if (query == "random") {
        static const std::vector<std::string> topics = {
            "shakespeare", "dickens", "philosophy", "science",
            "adventure", "mystery", "poetry", "history",
            "romance", "war", "love", "nature", "music",
            "tolstoy", "twain", "austen", "wilde", "hugo",
            "fairy", "magic", "journey", "ocean", "detective"
        };
        auto seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mt19937 rng(static_cast<unsigned>(seed));
        std::uniform_int_distribution<size_t> dist(0, topics.size() - 1);
        query = topics[dist(rng)];
        std::cerr << "[random] Tema elegido aleatoriamente: " << query << "\n";
    }

    // 1) Codificar el query para URL (sustituye espacios por +)
    std::string url_query;
    for (char c : query) {
        if (std::isalnum(static_cast<unsigned char>(c))) url_query += c;
        else if (c == ' ') url_query += '+';
    }

    // 2) Escribir un script auxiliar en Python que parsea el JSON de Gutendex
    std::string py_file = "/tmp/bow_extract_urls.py";
    {
        std::ofstream py(py_file);
        py << "import json, sys\n"
              "data = json.load(sys.stdin)\n"
              "count = int(sys.argv[1])\n"
              "for book in data.get('results', [])[:count]:\n"
              "    fmts = book.get('formats', {})\n"
              "    url = (fmts.get('text/plain; charset=us-ascii') or\n"
              "           fmts.get('text/plain; charset=utf-8') or\n"
              "           fmts.get('text/plain'))\n"
              "    if url and not url.endswith('.zip'):\n"
              "        print(url)\n";
    }

    // 3) Pipeline: curl a Gutendex -> python extrae URLs -> capturamos stdout
    std::string cmd = "curl -sL -A \"Mozilla/5.0\" --max-time 30 "
                      "'https://gutendex.com/books?search=" + url_query + "' | "
                      "python3 " + py_file + " " + std::to_string(count);

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[ERROR] No se pudo invocar curl/python3\n";
        std::remove(py_file.c_str());
        return urls;
    }

    char buffer[2048];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line = buffer;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (!line.empty()) urls.push_back(line);
    }
    pclose(pipe);
    std::remove(py_file.c_str());

    return urls;
}

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
