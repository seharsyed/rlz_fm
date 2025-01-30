#ifndef RLZ_ALGO_H
#define RLZ_ALGO_H

#include "fm_wrapper.h"
#include <sdsl/bit_vectors.hpp>
#include <sdsl/csa_wt.hpp>
#include <sdsl/wavelet_trees.hpp>
#include <fstream>
#include <vector>
#include <tuple>
#include <map>

class RLZ {
    public:
        std::string ref_file;
        std::string seq_file;
        sdsl::bit_vector ref_bit_array;
        sdsl::bit_vector seq_bit_array;

        RLZ(const std::string ref_file, const std::string seq_file);
        ~RLZ();
        void compress(int threads);

        void parse(const sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<127>>, 512, 1024>& fm_index,
            FM_Wrapper& fm_support,
            const std::map<char, uint64_t>& occs,
            const sdsl::bit_vector& seq_bit_array,
            std::vector<std::vector<std::tuple<uint64_t, uint64_t>>>& seq_parse_stack_vec,
            size_t num_bits_to_process,
            size_t loop_iter,
            size_t num_threads);

        void decompress();
        void load_file_to_bit_vector(const std::string& input_file, sdsl::bit_vector& bit_array);
        void load_reverse_file_to_bit_vector(const std::string& input_file, sdsl::bit_vector& bit_array);
        void calculate_occs(std::string content, std::map<char, uint64_t>& occs);
        void serialize(const std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse);
        std::vector<std::tuple<uint64_t, uint64_t>> deserialize();

        void bits_to_str(sdsl::bit_vector bit_array, std::string prefix);
        void print_serialize(const std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse);
        void clean();
};

#endif  // RLZ_ALGO_H
