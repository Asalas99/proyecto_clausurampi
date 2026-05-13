// =============================================================================
//  bow_mpi_caso.cpp
//  Version MPI: lee case.txt, reparte libros round-robin, genera la matriz
//  de bolsa de palabras en paralelo. Sin conteo embebido (eso lo hace
//  count_word despues a partir del CSV).
// =============================================================================
#include "bow_common.h"
#include <mpi.h>
#include <iostream>
#include <cstring>

static std::vector<char> pack_strings(const std::vector<std::string>& strs) {
    std::vector<char> buf;
    for (const auto& s : strs) {
        buf.insert(buf.end(), s.begin(), s.end());
        buf.push_back('\0');
    }
    return buf;
}
static std::vector<std::string> unpack_strings(const char* buf, int len) {
    std::vector<std::string> result;
    int start = 0;
    for (int i = 0; i < len; ++i)
        if (buf[i] == '\0') { result.emplace_back(buf+start, i-start); start = i+1; }
    return result;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0) std::cerr << "Uso: " << argv[0] << " <salida.csv>\n";
        MPI_Finalize(); return 1;
    }
    const std::string output_csv = argv[1];

    // PASO 1: Rank 0 lee case.txt y difunde
    std::vector<std::string> urls;
    int K = 0;
    if (rank == 0) {
        std::ifstream f("case.txt");
        if (!f) std::cerr << "[MPI] No se encontro case.txt\n";
        std::string line;
        while (std::getline(f, line)) {
            auto a = line.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) continue;
            auto b = line.find_last_not_of(" \t\r\n");
            line = line.substr(a, b - a + 1);
            if (line.empty() || line[0] == '#') continue;
            urls.push_back(line);
        }
        K = urls.size();
        std::cerr << "[MPI] " << size << " procs | " << K << " libros\n";
    }
    MPI_Bcast(&K, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (K == 0) { MPI_Finalize(); return 0; }

    std::vector<char> url_buf;
    int url_buf_size = 0;
    if (rank == 0) { url_buf = pack_strings(urls); url_buf_size = url_buf.size(); }
    MPI_Bcast(&url_buf_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) url_buf.resize(url_buf_size);
    MPI_Bcast(url_buf.data(), url_buf_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (rank != 0) urls = unpack_strings(url_buf.data(), url_buf_size);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    // PASO 2: Reparto round-robin y conteo local
    std::vector<int> my_indices;
    for (int i = rank; i < K; i += size) my_indices.push_back(i);

    std::vector<std::map<std::string, int>> my_book_counts(my_indices.size());
    std::set<std::string> my_vocab;
    for (size_t j = 0; j < my_indices.size(); ++j) {
        int idx = my_indices[j];
        std::cerr << "[Rank " << rank << "] libro " << idx << ": " << urls[idx] << "\n";
        std::string text = download_text(urls[idx], rank*10000 + idx);
        my_book_counts[j] = count_words(text);
        for (const auto& kv : my_book_counts[j]) my_vocab.insert(kv.first);
    }

    // PASO 3: Union global del vocabulario
    std::vector<std::string> my_vocab_vec(my_vocab.begin(), my_vocab.end());
    std::vector<char> my_vocab_buf = pack_strings(my_vocab_vec);
    int my_vocab_size = my_vocab_buf.size();

    std::vector<int> all_vocab_sizes(size);
    MPI_Gather(&my_vocab_size, 1, MPI_INT, all_vocab_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<int> displs(size, 0);
    int total_vocab_buf_size = 0;
    if (rank == 0)
        for (int i = 0; i < size; ++i) {
            displs[i] = total_vocab_buf_size;
            total_vocab_buf_size += all_vocab_sizes[i];
        }
    std::vector<char> all_vocab_buf;
    if (rank == 0) all_vocab_buf.resize(total_vocab_buf_size);
    MPI_Gatherv(my_vocab_buf.data(), my_vocab_size, MPI_CHAR,
                all_vocab_buf.data(), all_vocab_sizes.data(), displs.data(), MPI_CHAR,
                0, MPI_COMM_WORLD);

    std::vector<std::string> global_vocab;
    std::vector<char> global_vocab_buf;
    int global_vocab_buf_size = 0;
    if (rank == 0) {
        std::set<std::string> vocab_set;
        int start = 0;
        for (int i = 0; i < total_vocab_buf_size; ++i)
            if (all_vocab_buf[i] == '\0') {
                vocab_set.emplace(all_vocab_buf.data() + start, i - start);
                start = i + 1;
            }
        global_vocab.assign(vocab_set.begin(), vocab_set.end());
        global_vocab_buf = pack_strings(global_vocab);
        global_vocab_buf_size = global_vocab_buf.size();
    }
    MPI_Bcast(&global_vocab_buf_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) global_vocab_buf.resize(global_vocab_buf_size);
    MPI_Bcast(global_vocab_buf.data(), global_vocab_buf_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (rank != 0) global_vocab = unpack_strings(global_vocab_buf.data(), global_vocab_buf_size);

    const int V = static_cast<int>(global_vocab.size());
    std::map<std::string,int> word_to_idx;
    for (int i = 0; i < V; ++i) word_to_idx[global_vocab[i]] = i;

    // PASO 4: Filas locales
    int my_n_books = my_indices.size();
    std::vector<int> my_matrix(static_cast<size_t>(my_n_books) * V, 0);
    for (int j = 0; j < my_n_books; ++j)
        for (const auto& kv : my_book_counts[j]) {
            auto it = word_to_idx.find(kv.first);
            if (it != word_to_idx.end())
                my_matrix[static_cast<size_t>(j)*V + it->second] = kv.second;
        }

    // PASO 5: Reunir filas e indices
    std::vector<int> all_n_books(size);
    MPI_Gather(&my_n_books, 1, MPI_INT, all_n_books.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<int> indices_displs(size, 0);
    if (rank == 0)
        for (int i = 1; i < size; ++i)
            indices_displs[i] = indices_displs[i-1] + all_n_books[i-1];
    std::vector<int> all_indices(rank == 0 ? K : 0);
    MPI_Gatherv(my_indices.data(), my_n_books, MPI_INT,
                rank == 0 ? all_indices.data() : nullptr,
                all_n_books.data(), indices_displs.data(), MPI_INT, 0, MPI_COMM_WORLD);

    int my_data_size = my_n_books * V;
    std::vector<int> all_data_sizes(size,0), all_data_displs(size,0);
    if (rank == 0)
        for (int i = 0; i < size; ++i) {
            all_data_sizes[i] = all_n_books[i] * V;
            if (i > 0) all_data_displs[i] = all_data_displs[i-1] + all_data_sizes[i-1];
        }
    std::vector<int> all_matrix(rank == 0 ? static_cast<size_t>(K)*V : 0);
    MPI_Gatherv(my_matrix.data(), my_data_size, MPI_INT,
                rank == 0 ? all_matrix.data() : nullptr,
                all_data_sizes.data(), all_data_displs.data(), MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double elapsed = t_end - t_start;

    // PASO 6: Rank 0 escribe CSV
    if (rank == 0) {
        std::vector<std::vector<int>> final_matrix(K, std::vector<int>(V, 0));
        for (int i = 0; i < K; ++i) {
            int book_idx = all_indices[i];
            for (int v = 0; v < V; ++v)
                final_matrix[book_idx][v] = all_matrix[static_cast<size_t>(i)*V + v];
        }
        std::ofstream out(output_csv);
        out << "book";
        for (const auto& w : global_vocab) out << "," << w;
        out << "\n";
        for (int i = 0; i < K; ++i) {
            out << get_book_label(urls[i]);
            for (int v = 0; v < V; ++v) out << "," << final_matrix[i][v];
            out << "\n";
        }
        out.close();
        std::cout << "TIEMPO_MPI=" << elapsed << "\n";
        std::cout << "[MPI] " << size << " procs | " << K << " libros | vocab=" << V << "\n";
    }
    MPI_Finalize();
    return 0;
}