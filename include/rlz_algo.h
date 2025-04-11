/*
* RLZ - Compute the RLZ parse of a sequence file using a reference file
* Copyright (C) 2025-current Rahul Varki
* Licensed under the GNU General Public License v3 or later.
* See the LICENSE file or <https://www.gnu.org/licenses/> for details.
*/

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
        sdsl::bit_vector ref_bit_array;

        RLZ(const std::string ref_file);
        ~RLZ();

        void compress(int threads, const std::string& seq_file);

        void parse(const sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32>& fm_index,
            FM_Wrapper& fm_support,
            const std::map<char, uint64_t>& occs,
            const std::string& seq_file,
            std::vector<std::vector<std::tuple<uint64_t, uint64_t>>>& seq_parse_vec_vec,
            size_t num_bits_to_process,
            size_t loop_iter,
            size_t num_threads);

        void decompress(const std::string& parse_file);
        void load_file_to_bit_vector(const std::string& input_file, sdsl::bit_vector& bit_array);
        void load_reverse_file_to_bit_vector(const std::string& input_file, sdsl::bit_vector& bit_array);
        void calculate_occs(std::string content, std::map<char, uint64_t>& occs);
        void serialize(const std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse, const std::string& seq_file);
        std::vector<std::tuple<uint64_t, uint64_t>> deserialize(const std::string& parse_file);

        void bits_to_str(sdsl::bit_vector bit_array, std::string prefix);
        void print_serialize(const std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse, const std::string& seq_file);
};

#endif  // RLZ_ALGO_H
