#ifndef RLZ_LIST_RUNNER_H
#define RLZ_LIST_RUNNER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct RLZListConfig
{
    std::string reference_file;
    std::string input_list_file;
    std::string csv_file;
    std::string mode = "baseline";
    std::size_t bucket_divisor = 32;
    std::size_t min_cache_width = 1;
    std::size_t max_len = 0;
};

struct RLZListResult
{
    std::string filename;
    std::uintmax_t input_size = 0;
    std::uintmax_t parse_size = 0;
    std::size_t factor_count = 0;
    double time_sec = 0.0;
    std::string mode;
    std::size_t bucket_divisor = 0;
    std::size_t cache_hits = 0;
    std::size_t cache_misses = 0;
    std::size_t cache_entries = 0;
    std::size_t bucket_size = 0;
    std::size_t bucket_count = 0;
    std::size_t min_cache_width = 0;
    std::size_t max_bucket_index = 0;
    std::size_t max_bucket_entries = 0;
};

std::vector<std::string> read_rlz_input_list(const std::string& input_list_file);

template <typename int_t>
std::vector<RLZListResult> run_rlz_list(const RLZListConfig& config);

#endif