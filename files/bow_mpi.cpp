// =============================================================================
//  bow_mpi.cpp
//  -------------------------------------------------------------------------
//  Versión PARALELA con MPI del programa de Bolsa de Palabras.
//
//  Uso:
//      mpirun -np <q> ./bow_mpi <archivo_urls.txt> <salida.csv>
//
//  Estrategia de paralelización
//  ----------------------------
//   1. Rank 0 lee las URLs y las difunde con MPI_Bcast a todos.
//   2. Los libros se reparten ROUND-ROBIN: el proceso p atiende los libros
//      con índice i tales que (i % size == rank). Esto balancea bien aunque
//      los libros tengan tamaños muy distintos.
//   3. Cada proceso descarga y cuenta palabras de SUS libros en paralelo.
//   4. Cada proceso envía su vocabulario local a rank 0 con MPI_Gatherv.
//      Rank 0 hace la unión ordenada y difunde el vocabulario global.
//   5. Cada proceso construye sus filas alineadas al vocabulario global y
//      las envía con MPI_Gatherv a rank 0, junto con los índices de libro
//      que le tocaron, para poder reordenar.
//   6. Rank 0 escribe el CSV final y reporta TIEMPO_MPI.
//
//  Detalle técnico:
//   - Para enviar vectores de strings por MPI se empacan en un buffer
//     contiguo separando con '\0' (null byte) y se usa MPI_CHAR.
//
//  Proyecto: Cómputo Paralelo y en la Nube
// =============================================================================
#include "bow_common.h"

#include <mpi.h>
#include <iostream>
#include <cstring>

// -----------------------------------------------------------------------------
// pack_strings / unpack_strings
//   Serializan / deserializan un vector<string> a un buffer contiguo con
//   separadores '\0', útil para MPI_Bcast / MPI_Gatherv con MPI_CHAR.
// -----------------------------------------------------------------------------
static std::vector<char> pack_strings(const std::vector<std::string>& strs) {
    std::vector<char> buf;
    size_t total = 0;
    for (const auto& s : strs) total += s.size() + 1;
    buf.reserve(total);
    for (const auto& s : strs) {
        buf.insert(buf.end(), s.begin(), s.end());
        buf.push_back('\0');
    }
    return buf;
}

static std::vector<std::string> unpack_strings(const char* buf, int len) {
    std::vector<std::string> result;
    int start = 0;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\0') {
            result.emplace_back(buf + start, i - start);
            start = i + 1;
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0)
            std::cerr << "Uso: " << argv[0] << " <urls.txt> <salida.csv>\n";
        MPI_Finalize();
        return 1;
    }

    // =========================================================================
    // PASO 1: Rank 0 lee URLs y las difunde
    // =========================================================================
    std::vector<std::string> urls;
    int K = 0;
    if (rank == 0) {
        urls = read_urls(argv[1]);
        K = static_cast<int>(urls.size());
        std::cerr << "[MPI] Procesos: " << size << " | Libros: " << K << "\n";
    }
    MPI_Bcast(&K, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (K == 0) {
        if (rank == 0) std::cerr << "[MPI] No hay URLs.\n";
        MPI_Finalize();
        return 0;
    }

    // Empaquetar URLs y difundir
    std::vector<char> url_buf;
    int url_buf_size = 0;
    if (rank == 0) {
        url_buf = pack_strings(urls);
        url_buf_size = static_cast<int>(url_buf.size());
    }
    MPI_Bcast(&url_buf_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) url_buf.resize(url_buf_size);
    MPI_Bcast(url_buf.data(), url_buf_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (rank != 0) urls = unpack_strings(url_buf.data(), url_buf_size);

    // Sincronizar y arrancar cronómetro
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    // =========================================================================
    // PASO 2: Reparto ROUND-ROBIN de libros y procesamiento local
    // =========================================================================
    std::vector<int> my_indices;
    for (int i = rank; i < K; i += size) my_indices.push_back(i);

    std::vector<std::map<std::string, int>> my_book_counts(my_indices.size());
    std::set<std::string> my_vocab;

    for (size_t j = 0; j < my_indices.size(); ++j) {
        int idx = my_indices[j];
        std::cerr << "[Rank " << rank << "] descargando libro " << idx
                  << ": " << urls[idx] << "\n";
        std::string text = download_text(urls[idx], rank * 10000 + idx);
        my_book_counts[j] = count_words(text);
        for (const auto& kv : my_book_counts[j]) my_vocab.insert(kv.first);
    }

    // =========================================================================
    // PASO 3: Unión global del vocabulario (gather a rank 0, broadcast de vuelta)
    // =========================================================================
    std::vector<std::string> my_vocab_vec(my_vocab.begin(), my_vocab.end());
    std::vector<char> my_vocab_buf = pack_strings(my_vocab_vec);
    int my_vocab_size = static_cast<int>(my_vocab_buf.size());

    // Reunir tamaños de cada buffer en rank 0
    std::vector<int> all_vocab_sizes(size);
    MPI_Gather(&my_vocab_size, 1, MPI_INT,
               all_vocab_sizes.data(), 1, MPI_INT,
               0, MPI_COMM_WORLD);

    std::vector<int> displs(size, 0);
    int total_vocab_buf_size = 0;
    if (rank == 0) {
        for (int i = 0; i < size; ++i) {
            displs[i] = total_vocab_buf_size;
            total_vocab_buf_size += all_vocab_sizes[i];
        }
    }

    std::vector<char> all_vocab_buf;
    if (rank == 0) all_vocab_buf.resize(total_vocab_buf_size);

    MPI_Gatherv(my_vocab_buf.data(), my_vocab_size, MPI_CHAR,
                all_vocab_buf.data(),
                all_vocab_sizes.data(), displs.data(), MPI_CHAR,
                0, MPI_COMM_WORLD);

    // Rank 0 construye el vocabulario global ordenado
    std::vector<std::string> global_vocab;
    std::vector<char> global_vocab_buf;
    int global_vocab_buf_size = 0;
    if (rank == 0) {
        std::set<std::string> vocab_set;
        int start = 0;
        for (int i = 0; i < total_vocab_buf_size; ++i) {
            if (all_vocab_buf[i] == '\0') {
                vocab_set.emplace(all_vocab_buf.data() + start, i - start);
                start = i + 1;
            }
        }
        global_vocab.assign(vocab_set.begin(), vocab_set.end());
        global_vocab_buf = pack_strings(global_vocab);
        global_vocab_buf_size = static_cast<int>(global_vocab_buf.size());
    }

    // Difundir vocabulario global a todos
    MPI_Bcast(&global_vocab_buf_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) global_vocab_buf.resize(global_vocab_buf_size);
    MPI_Bcast(global_vocab_buf.data(), global_vocab_buf_size, MPI_CHAR,
              0, MPI_COMM_WORLD);
    if (rank != 0)
        global_vocab = unpack_strings(global_vocab_buf.data(), global_vocab_buf_size);

    const int V = static_cast<int>(global_vocab.size());

    // Índice rápido palabra -> columna
    std::map<std::string, int> word_to_idx;
    for (int i = 0; i < V; ++i) word_to_idx[global_vocab[i]] = i;

    // =========================================================================
    // PASO 4: Construir filas locales alineadas al vocabulario global
    // =========================================================================
    int my_n_books = static_cast<int>(my_indices.size());
    std::vector<int> my_matrix(static_cast<size_t>(my_n_books) * V, 0);
    for (int j = 0; j < my_n_books; ++j) {
        for (const auto& kv : my_book_counts[j]) {
            auto it = word_to_idx.find(kv.first);
            if (it != word_to_idx.end()) {
                my_matrix[static_cast<size_t>(j) * V + it->second] = kv.second;
            }
        }
    }

    // =========================================================================
    // PASO 5: Recolectar filas e índices en rank 0
    // =========================================================================
    std::vector<int> all_n_books(size);
    MPI_Gather(&my_n_books, 1, MPI_INT,
               all_n_books.data(), 1, MPI_INT,
               0, MPI_COMM_WORLD);

    std::vector<int> indices_displs(size, 0);
    int total_books = 0;
    if (rank == 0) {
        for (int i = 0; i < size; ++i) {
            indices_displs[i] = total_books;
            total_books += all_n_books[i];
        }
    }

    std::vector<int> all_indices;
    if (rank == 0) all_indices.resize(K);
    MPI_Gatherv(my_indices.data(), my_n_books, MPI_INT,
                rank == 0 ? all_indices.data() : nullptr,
                all_n_books.data(), indices_displs.data(), MPI_INT,
                0, MPI_COMM_WORLD);

    int my_data_size = my_n_books * V;
    std::vector<int> all_data_sizes(size, 0), all_data_displs(size, 0);
    if (rank == 0) {
        for (int i = 0; i < size; ++i) {
            all_data_sizes[i] = all_n_books[i] * V;
            if (i > 0)
                all_data_displs[i] = all_data_displs[i - 1] + all_data_sizes[i - 1];
        }
    }

    std::vector<int> all_matrix;
    if (rank == 0) all_matrix.resize(static_cast<size_t>(K) * V);
    MPI_Gatherv(my_matrix.data(), my_data_size, MPI_INT,
                rank == 0 ? all_matrix.data() : nullptr,
                all_data_sizes.data(), all_data_displs.data(), MPI_INT,
                0, MPI_COMM_WORLD);

    // Sincronizar y detener cronómetro (sin contar la escritura del CSV,
    // como suele hacerse para medir el cómputo paralelo).
    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double elapsed = t_end - t_start;

    // =========================================================================
    // PASO 6: Rank 0 reordena filas por índice de libro y escribe el CSV
    // =========================================================================
    if (rank == 0) {
        std::vector<std::vector<int>> final_matrix(
            K, std::vector<int>(V, 0));
        for (int i = 0; i < K; ++i) {
            int book_idx = all_indices[i];
            for (int v = 0; v < V; ++v) {
                final_matrix[book_idx][v] =
                    all_matrix[static_cast<size_t>(i) * V + v];
            }
        }

        std::ofstream out(argv[2]);
        if (!out) {
            std::cerr << "[MPI] Error abriendo " << argv[2] << "\n";
        } else {
            out << "book";
            for (const auto& w : global_vocab) out << "," << w;
            out << "\n";
            for (int i = 0; i < K; ++i) {
                out << get_book_label(urls[i]);
                for (int v = 0; v < V; ++v) out << "," << final_matrix[i][v];
                out << "\n";
            }
            out.close();
        }

        std::cout << "TIEMPO_MPI=" << elapsed << "\n";
        std::cout << "[MPI] Procesos: " << size
                  << " | Libros: " << K
                  << " | Vocabulario: " << V
                  << " | Archivo: " << argv[2] << "\n";
    }

    MPI_Finalize();
    return 0;
}
