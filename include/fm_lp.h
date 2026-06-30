#ifndef FM_LP_H
#define FM_LP_H

#include "utility.h"

#include <cstddef>
#include <tuple>
#include <vector>

struct FM_LP_Info
{
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t entries = 0;
    std::size_t div_p = 0;
    std::size_t table_slots = 0;
    std::size_t min_cache_width = 0;
    std::size_t max_probe_cluster = 0;
    double hit_rate = 0.0;
    double load_factor = 0.0;
    std::size_t approx_bytes = 0;
};

class FM_LP
{
public:
    FM_LP();

    FM_LP(std::size_t fm_size,
          std::size_t div_p,
          std::size_t min_cache_width = 1);

    void configure(std::size_t fm_size,
                   std::size_t div_p,
                   std::size_t min_cache_width = 1);

    std::tuple<std::size_t, std::size_t> backward_match(
        const rlz_fm_index_t& fm_index,
        const std::vector<std::size_t>& occs,
        const std::tuple<std::size_t, std::size_t>& prev_backward_range,
        char next_char);

    std::size_t get_suffix_array_value(const rlz_fm_index_t& fm_index,
                                       std::size_t location);

    void clear_cache();

    FM_LP_Info cache_info() const;

    static void check_character_exists(const std::vector<std::size_t>& occs,
                                       unsigned char symbol);

private:
    struct LookupKey
    {
        std::size_t old_left = 0;
        std::size_t old_right = 0;
        unsigned char symbol = 0;

        bool operator==(const LookupKey& other) const noexcept
        {
            return old_left == other.old_left &&
                   old_right == other.old_right &&
                   symbol == other.symbol;
        }
    };

    struct Interval
    {
        std::size_t new_left = 0;
        std::size_t new_right = 0;
    };

    struct Slot
    {
        bool occupied = false;
        LookupKey key{};
        Interval interval{};
    };

    std::size_t fm_size_ = 0;
    std::size_t div_p_ = 64;
    std::size_t min_cache_width_ = 1;

    std::vector<Slot> table_;

    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    std::size_t entries_ = 0;

    static constexpr double max_load_factor_ = 0.70;

    void rebuild_table();

    void ensure_configured(std::size_t fm_size);

    static std::size_t mix_hash(std::size_t x) noexcept;

    std::size_t key_hash(const LookupKey& key) const noexcept;

    std::size_t start_slot(const LookupKey& key) const;

    LookupKey make_key(std::size_t old_left,
                       std::size_t old_right,
                       unsigned char symbol) const;

    bool lookup(std::size_t old_left,
                std::size_t old_right,
                unsigned char symbol,
                Interval& cached_interval);

    void insert(std::size_t old_left,
                std::size_t old_right,
                unsigned char symbol,
                std::size_t new_left,
                std::size_t new_right);

    void maybe_resize();

    void reinsert(const LookupKey& key, const Interval& interval);

    std::size_t max_probe_cluster() const;

    static std::tuple<std::size_t, std::size_t> compute_fm_transition(
        const rlz_fm_index_t& fm_index,
        const std::vector<std::size_t>& occs,
        std::size_t old_left,
        std::size_t old_right,
        char next_char);
};

#endif
