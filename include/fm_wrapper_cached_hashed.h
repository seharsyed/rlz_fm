#ifndef FM_WRAPPER_CACHED_H
#define FM_WRAPPER_CACHED_H

#include "utility.h"

#include <cstddef>
#include <tuple>
#include <vector>

struct FM_Cache_Info
{
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t entries = 0;
    std::size_t bucket_size = 0;
    std::size_t bucket_count = 0;
    std::size_t min_cache_width = 0;
    std::size_t max_bucket_index = 0;
    std::size_t max_bucket_entries = 0;
};

class FM_Wrapper_Cached
{
public:
    FM_Wrapper_Cached();

    FM_Wrapper_Cached(std::size_t fm_size,
                      std::size_t bucket_divisor,
                      std::size_t min_cache_width = 1);

    void configure(std::size_t fm_size,
                   std::size_t bucket_divisor,
                   std::size_t min_cache_width = 1);

    std::tuple<std::size_t, std::size_t> backward_match(
        const rlz_fm_index_t& fm_index,
        const std::vector<std::size_t>& occs,
        const std::tuple<std::size_t, std::size_t>& prev_backward_range,
        char next_char);

    std::size_t get_suffix_array_value(const rlz_fm_index_t& fm_index,
                                       std::size_t location);

    void clear_cache();

    FM_Cache_Info cache_info() const;

    std::size_t bucket_size() const;
    std::size_t bucket_count() const;

private:
    struct LookupKey
    {
        std::size_t left_remainder = 0;
        std::size_t old_right = 0;
        unsigned char symbol = 0;

        bool operator==(const LookupKey& other) const noexcept
        {
            return left_remainder == other.left_remainder &&
                   old_right == other.old_right &&
                   symbol == other.symbol;
        }
    };

    struct Interval
    {
        std::size_t new_left = 0;
        std::size_t new_right = 0;
    };

    struct Entry
    {
        bool used = false;
        LookupKey key;
        Interval interval;
    };

    struct Bucket
    {
        std::vector<Entry> entries;
        std::size_t used = 0;
    };

    std::size_t fm_size_ = 0;
    std::size_t bucket_divisor_ = 128;
    std::size_t min_cache_width_ = 1;

    std::vector<Bucket> table_;

    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    std::size_t entries_ = 0;

    void rebuild_table();

    void ensure_configured(std::size_t fm_size);

    std::size_t bucket_index(std::size_t old_left) const;

    LookupKey make_key(std::size_t old_left,
                       std::size_t old_right,
                       unsigned char symbol) const;

    static std::size_t key_position(const LookupKey& key);

    static void initialise_bucket(Bucket& bucket);

    static void rebuild_bucket(Bucket& bucket);

    bool lookup(std::size_t old_left,
                std::size_t old_right,
                unsigned char symbol,
                Interval& cached_interval);

    void insert(std::size_t old_left,
                std::size_t old_right,
                unsigned char symbol,
                std::size_t new_left,
                std::size_t new_right);

    static void check_character_exists(const std::vector<std::size_t>& occs,
                                       unsigned char symbol);

    static std::tuple<std::size_t, std::size_t> compute_fm_transition(
        const rlz_fm_index_t& fm_index,
        const std::vector<std::size_t>& occs,
        std::size_t old_left,
        std::size_t old_right,
        char next_char);
};

#endif