#include "rlz_list_runner.h"

#include "fm_wrapper.h"
#include "fm_lp.h"
#include "utility.h"

#include <sdsl/construct.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace
{
bool char_exists_in_reference(const std::vector<std::size_t>& occs,
                              unsigned char symbol)
{
    if (symbol < 255) {
        return occs[symbol] != occs[symbol + 1];
    }

    return true;
}

std::string trim_line(std::string line)
{
    while (!line.empty() &&
           (line.back() == '\n' || line.back() == '\r' ||
            line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }

    std::size_t start = 0;
    while (start < line.size() &&
           (line[start] == ' ' || line[start] == '\t')) {
        ++start;
    }

    if (start > 0) {
        line.erase(0, start);
    }

    return line;
}

std::string csv_escape(const std::string& value)
{
    bool quote = false;

    for (char c : value) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            quote = true;
            break;
        }
    }

    if (!quote) {
        return value;
    }

    std::string out = "\"";

    for (char c : value) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out += c;
        }
    }

    out += "\"";
    return out;
}

void write_csv_header(std::ofstream& csv)
{
    csv << "filename,input_size,parse_size,factor_count,time_sec,mode,"
        << "div_p,min_cache_width,cache_hits,cache_misses,cache_entries,"
        << "table_slots,hit_rate,load_factor,approx_cache_MB\n";
}

void write_csv_row(std::ofstream& csv, const RLZListResult& r)
{
    const double total_queries = static_cast<double>(r.cache_hits + r.cache_misses);
    const double hit_rate = total_queries == 0.0
        ? 0.0
        : static_cast<double>(r.cache_hits) / total_queries;

    csv << csv_escape(r.filename) << ','
        << r.input_size << ','
        << r.parse_size << ','
        << r.factor_count << ','
        << r.time_sec << ','
        << r.mode << ','
        << r.bucket_divisor << ','
        << r.min_cache_width << ','
        << r.cache_hits << ','
        << r.cache_misses << ','
        << r.cache_entries << ','
        << r.bucket_count << ','
        << hit_rate << ','
        << (r.bucket_count == 0
                ? 0.0
                : static_cast<double>(r.cache_entries) / static_cast<double>(r.bucket_count)) << ','
        << (static_cast<double>(r.bucket_size) / (1024.0 * 1024.0))
        << '\n';
}

void load_reverse_reference(const std::string& ref_file, std::string& ref_content)
{
    std::error_code ec;
    std::uintmax_t ref_size = std::filesystem::file_size(ref_file, ec);

    if (ec) {
        throw std::runtime_error("Error getting reference size: " + ec.message());
    }

    std::ifstream ref(ref_file, std::ios::binary);

    if (!ref) {
        throw std::runtime_error("Error opening reference file: " + ref_file);
    }

    ref_content.resize(static_cast<std::size_t>(ref_size));

    if (ref_size > 0 &&
        !ref.read(&ref_content[0], static_cast<std::streamsize>(ref_size))) {
        throw std::runtime_error("Error reading reference file: " + ref_file);
    }

    while (!ref_content.empty() && ref_content.back() == '\0') {
        ref_content.pop_back();
    }

    if (ref_content.find('\0') != std::string::npos) {
        throw std::runtime_error(
            "Reference contains internal zero byte. FM-index cannot index byte 0.");
    }

    std::reverse(ref_content.begin(), ref_content.end());
}

void calculate_occs(const std::string& ref_content,
                    std::vector<std::size_t>& occs)
{
    occs.assign(256, 0);

    for (unsigned char c : ref_content) {
        occs[c]++;
    }

    std::size_t running_total = 0;

    for (std::size_t i = 0; i < 256; ++i) {
        std::size_t freq = occs[i];
        occs[i] = running_total;
        running_total += freq;
    }
}

template <typename int_t>
std::uintmax_t computed_parse_size(std::size_t factor_count)
{
    return sizeof(std::uint64_t) +
           static_cast<std::uintmax_t>(factor_count) * 2 * sizeof(int_t);
}

template <typename Wrapper>
std::size_t parse_file_count_factors(const rlz_fm_index_t& fm_index,
                                     Wrapper& fm_support,
                                     const std::vector<std::size_t>& occs,
                                     const std::string& seq_file,
                                     std::size_t max_len)
{
    std::ifstream sfile(seq_file, std::ios::binary);

    if (!sfile) {
        throw std::runtime_error("Error opening input file: " + seq_file);
    }

    std::size_t factor_count = 0;

    std::size_t pattern_len = 0;
    std::size_t prev_left = 0;
    std::size_t prev_right = fm_index.bwt.size();

    bool retry = false;
    char next_char = 0;

    while (true) {
        if (!retry) {
            if (!sfile.get(next_char)) {
                break;
            }
        }

        ++pattern_len;

        unsigned char symbol = static_cast<unsigned char>(next_char);

        if (!char_exists_in_reference(occs, symbol)) {
            if (pattern_len == 1) {
                ++factor_count;

                prev_left = 0;
                prev_right = fm_index.bwt.size();
                pattern_len = 0;
                retry = false;
                continue;
            }

            --pattern_len;
            ++factor_count;

            prev_left = 0;
            prev_right = fm_index.bwt.size();
            pattern_len = 0;
            retry = true;
            continue;
        }

        auto previous_range = std::make_tuple(prev_left, prev_right);
        auto next_range =
            fm_support.backward_match(fm_index, occs, previous_range, next_char);

        const std::size_t next_left = std::get<0>(next_range);
        const std::size_t next_right = std::get<1>(next_range);

        if (next_left == next_right) {
            --pattern_len;

            if (pattern_len == 0) {
                throw std::runtime_error(
                    "Zero-length RLZ factor detected. Check reference/input alphabet.");
            }

            ++factor_count;

            prev_left = 0;
            prev_right = fm_index.bwt.size();
            pattern_len = 0;
            retry = true;
        } else {
            const bool at_end =
                (sfile.peek() == std::char_traits<char>::eof());

            if (at_end) {
                ++factor_count;
                break;
            }

            if (max_len > 0 && pattern_len == max_len) {
                ++factor_count;

                prev_left = 0;
                prev_right = fm_index.bwt.size();
                pattern_len = 0;
                retry = false;
            } else {
                prev_left = next_left;
                prev_right = next_right;
                retry = false;
            }
        }
    }

    return factor_count;
}
}

std::vector<std::string> read_rlz_input_list(
    const std::string& input_list_file)
{
    std::ifstream in(input_list_file);

    if (!in) {
        throw std::runtime_error("Error opening input list: " + input_list_file);
    }

    std::vector<std::string> files;
    std::string line;

    while (std::getline(in, line)) {
        line = trim_line(line);

        if (!line.empty()) {
            files.push_back(line);
        }
    }

    if (files.empty()) {
        throw std::runtime_error("Input list is empty: " + input_list_file);
    }

    return files;
}

template <typename int_t>
std::vector<RLZListResult> run_rlz_list(const RLZListConfig& config)
{
    if (config.mode != "baseline" && config.mode != "cached") {
        throw std::runtime_error("mode must be baseline or cached");
    }

    if (config.reference_file.empty()) {
        throw std::runtime_error("reference_file is empty");
    }

    if (config.input_list_file.empty()) {
        throw std::runtime_error("input_list_file is empty");
    }

    if (!config.bucket_trace_file.empty()) {
        throw std::runtime_error("fm_lp does not write bucket-hit traces");
    }

    std::vector<std::string> input_files =
        read_rlz_input_list(config.input_list_file);

    std::string reversed_ref;
    load_reverse_reference(config.reference_file, reversed_ref);

    rlz_fm_index_t fm_index;
    sdsl::construct_im(fm_index, reversed_ref, 1);

    if (reversed_ref.size() + 1 != fm_index.bwt.size()) {
        throw std::runtime_error("FM-index size check failed");
    }

    std::vector<std::size_t> occs;
    calculate_occs(reversed_ref, occs);

    FM_Wrapper baseline_wrapper;
    FM_LP cached_wrapper;

    if (config.mode == "cached") {
        cached_wrapper.configure(
            fm_index.bwt.size(),
            config.bucket_divisor,
            config.min_cache_width);
    }

    std::ofstream csv;

    if (!config.csv_file.empty()) {
        csv.open(config.csv_file);

        if (!csv) {
            throw std::runtime_error("Error opening CSV file: " + config.csv_file);
        }

        write_csv_header(csv);
        csv.flush();
    }

    std::vector<RLZListResult> results;
    results.reserve(input_files.size());

    for (std::size_t i = 0; i < input_files.size(); ++i) {
        const std::string& file = input_files[i];

        std::cerr << "[" << config.mode << "] "
                  << (i + 1) << "/" << input_files.size()
                  << " " << file << std::endl;

        std::error_code ec;
        std::uintmax_t input_size = std::filesystem::file_size(file, ec);

        if (ec) {
            throw std::runtime_error("Error getting input size for " + file +
                                     ": " + ec.message());
        }

        std::size_t hits_before = 0;
        std::size_t misses_before = 0;

        if (config.mode == "cached") {
            FM_LP_Info before = cached_wrapper.cache_info();
            hits_before = before.hits;
            misses_before = before.misses;
        }

        auto start = std::chrono::high_resolution_clock::now();

        std::size_t factor_count = 0;

        if (config.mode == "baseline") {
            factor_count = parse_file_count_factors(
                fm_index, baseline_wrapper, occs, file, config.max_len);
        } else {
            factor_count = parse_file_count_factors(
                fm_index, cached_wrapper, occs, file, config.max_len);
        }

        auto end = std::chrono::high_resolution_clock::now();

        double time_sec =
            std::chrono::duration<double>(end - start).count();

        RLZListResult row;
        row.filename = file;
        row.input_size = input_size;
        row.parse_size = computed_parse_size<int_t>(factor_count);
        row.factor_count = factor_count;
        row.time_sec = time_sec;
        row.mode = config.mode;

        if (config.mode == "cached") {
            FM_LP_Info after = cached_wrapper.cache_info();

            row.bucket_divisor = config.bucket_divisor;
            row.min_cache_width = after.min_cache_width;
            row.cache_hits = after.hits - hits_before;
            row.cache_misses = after.misses - misses_before;
            row.cache_entries = after.entries;
            row.bucket_size = after.approx_bytes;
            row.bucket_count = after.table_slots;
            row.max_bucket_index = 0;
        }

        if (csv) {
            write_csv_row(csv, row);
            csv.flush();
        }

        results.push_back(row);
    }

    return results;
}

template std::vector<RLZListResult> run_rlz_list<int>(
    const RLZListConfig& config);

template std::vector<RLZListResult> run_rlz_list<std::uint8_t>(
    const RLZListConfig& config);

template std::vector<RLZListResult> run_rlz_list<std::uint16_t>(
    const RLZListConfig& config);

template std::vector<RLZListResult> run_rlz_list<std::uint32_t>(
    const RLZListConfig& config);

template std::vector<RLZListResult> run_rlz_list<std::uint64_t>(
    const RLZListConfig& config);
