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
    struct CacheKey
    {
        std::size_t left_remainder = 0;
        std::size_t old_right = 0;
        unsigned char symbol = 0;
    };

    struct CacheValue
    {
        std::size_t new_left = 0;
        std::size_t new_right = 0;
    };

    struct Slot
    {
        bool used = false;
        CacheKey key;
        CacheValue value;
    };

    struct Bucket
    {
        std::vector<Slot> table;
        std::size_t used = 0;
    };

    std::size_t fm_size_ = 0;
    std::size_t bucket_divisor_ = 128;
    std::size_t bucket_size_ = 0;
    std::size_t min_cache_width_ = 1;

    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    std::size_t entries_ = 0;

    std::vector<Bucket> buckets_;

    void ensure_configured(std::size_t fm_size);
    std::size_t bucket_index(std::size_t old_left) const;

    static bool keys_equal(const CacheKey& a, const CacheKey& b);
    static std::size_t hash_key(const CacheKey& key);
    static void init_bucket(Bucket& bucket);
    static void grow_bucket(Bucket& bucket);

    bool find_in_bucket(const Bucket& bucket,
                        const CacheKey& key,
                        CacheValue& value) const;

    void insert_in_bucket(Bucket& bucket,
                          const CacheKey& key,
                          const CacheValue& value);

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
