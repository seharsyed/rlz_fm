/*
* RLZ - Compute the RLZ parse of a sequence file using a reference file
* Copyright (C) 2025-current Rahul Varki
* Licensed under the GNU General Public License v3 or later.
* See the LICENSE file or <https://www.gnu.org/licenses/> for details.
*/

#ifndef UTILITY_H
#define UTILITY_H

#include <sdsl/csa_wt.hpp>
#include <sdsl/wavelet_trees.hpp>

// SA_SAMPLE_RATE, ISA_SAMPLE_RATE
using rlz_fm_index_t = sdsl::csa_wt<sdsl::wt_huff<sdsl::bit_vector>, 4, 4096>;

#endif // UTILITY_H