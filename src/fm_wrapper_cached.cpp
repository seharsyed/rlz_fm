#include "fm_wrapper_cached.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

FM_Wrapper_Cached::FM_Wrapper_Cached() = default;

FM_Wrapper_Cached::FM_Wrapper_Cached(std::size_t fm_size,
                                     std::size_t bucket_divisor,
                                     std::size_t min_cache_width)
{
    configure(fm_size, bucket_divisor, min_cache_width);
}

void FM_Wrapper_Cached::configure(std::size_t fm_size,
                                  std::size_t bucket_divisor,
                                  std::size_t min_cache_width)
{
    if (fm_size == 0) {
        throw std::invalid_argument("FM_Wrapper_Cached: fm_size must be non-zero");
    }

    if (bucket_divisor == 0) {
        throw std::invalid_argument("FM_Wrapper_Cached: bucket_divisor must be non-zero");
    }

    fm_size_ = fm_size;
    bucket_divisor_ = bucket_divisor;
    min_cache_width_ = min_cache_width;

    rebuild_table();

    hits_ = 0;
    misses_ = 0;
    entries_ = 0;
    bucket_hit_sequence_.clear();
}

void FM_Wrapper_Cached::rebuild_table()
{
    const std::size_t buckets = (fm_size_ / bucket_divisor_) + 1;

    table_.clear();
    table_.resize(buckets);
}

void FM_Wrapper_Cached::ensure_configured(std::size_t fm_size)
{
    if (fm_size_ == 0) {
        configure(fm_size, bucket_divisor_, min_cache_width_);
        return;
    }

    if (fm_size_ != fm_size) {
        throw std::runtime_error(
            "FM_Wrapper_Cached used with a different FM-index/reference");
    }
}

std::size_t FM_Wrapper_Cached::bucket_index(std::size_t old_left) const
{
    return old_left / bucket_divisor_;
}

FM_Wrapper_Cached::LookupKey FM_Wrapper_Cached::make_key(
    std::size_t old_left,
    std::size_t old_right,
    unsigned char symbol) const
{
    return LookupKey{old_left % bucket_divisor_, old_right, symbol};
}

bool FM_Wrapper_Cached::lookup(std::size_t old_left,
                               std::size_t old_right,
                               unsigned char symbol,
                               Interval& cached_interval)
{
    const std::size_t b = bucket_index(old_left);

    if (b >= table_.size()) {
        throw std::runtime_error("FM_Wrapper_Cached: bucket index out of range");
    }

    const LookupKey key = make_key(old_left, old_right, symbol);

    for (const Entry& entry : table_[b]) {
        if (entry.first == key) {
            cached_interval = entry.second;
            ++hits_;
            bucket_hit_sequence_.push_back(b);
            return true;
        }
    }

    ++misses_;
    return false;
}

void FM_Wrapper_Cached::insert(std::size_t old_left,
                               std::size_t old_right,
                               unsigned char symbol,
                               std::size_t new_left,
                               std::size_t new_right)
{
    const std::size_t b = bucket_index(old_left);

    if (b >= table_.size()) {
        throw std::runtime_error("FM_Wrapper_Cached: bucket index out of range");
    }

    table_[b].emplace_back(
        make_key(old_left, old_right, symbol),
        Interval{new_left, new_right}
    );

    ++entries_;
}

void FM_Wrapper_Cached::check_character_exists(
    const std::vector<std::size_t>& occs,
    unsigned char symbol)
{
    if (occs.size() < 256) {
        throw std::runtime_error("FM_Wrapper_Cached: occs must have 256 entries");
    }

    if (symbol < 255 && occs[symbol] == occs[symbol + 1]) {
        std::cerr << "Character code: " << static_cast<int>(symbol)
                  << " not found in reference text!" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

std::tuple<std::size_t, std::size_t> FM_Wrapper_Cached::compute_fm_transition(
    const rlz_fm_index_t& fm_index,
    const std::vector<std::size_t>& occs,
    std::size_t old_left,
    std::size_t old_right,
    char next_char)
{
    const unsigned char symbol = static_cast<unsigned char>(next_char);

    check_character_exists(occs, symbol);

    std::size_t new_left = fm_index.bwt.rank(old_left, next_char);
    std::size_t new_right = fm_index.bwt.rank(old_right, next_char);

    new_left += occs[symbol] + 1;
    new_right += occs[symbol] + 1;

    return std::make_tuple(new_left, new_right);
}

std::tuple<std::size_t, std::size_t> FM_Wrapper_Cached::backward_match(
    const rlz_fm_index_t& fm_index,
    const std::vector<std::size_t>& occs,
    const std::tuple<std::size_t, std::size_t>& prev_backward_range,
    char next_char)
{
    ensure_configured(fm_index.bwt.size());

    const std::size_t old_left = std::get<0>(prev_backward_range);
    const std::size_t old_right = std::get<1>(prev_backward_range);
    const unsigned char symbol = static_cast<unsigned char>(next_char);

    const std::size_t width = old_right - old_left;

    if (width <= min_cache_width_) {
        return compute_fm_transition(
            fm_index, occs, old_left, old_right, next_char
        );
    }

    Interval cached_interval{};

    if (lookup(old_left, old_right, symbol, cached_interval)) {
        return std::make_tuple(
            cached_interval.new_left,
            cached_interval.new_right
        );
    }

    auto result = compute_fm_transition(
        fm_index, occs, old_left, old_right, next_char
    );

    const std::size_t new_left = std::get<0>(result);
    const std::size_t new_right = std::get<1>(result);

    if (new_left == new_right) {
        return result;
    }

    insert(old_left, old_right, symbol, new_left, new_right);

    return result;
}

std::size_t FM_Wrapper_Cached::get_suffix_array_value(
    const rlz_fm_index_t& fm_index,
    std::size_t location)
{
    return fm_index[location];
}

void FM_Wrapper_Cached::clear_cache()
{
    rebuild_table();

    hits_ = 0;
    misses_ = 0;
    entries_ = 0;
    bucket_hit_sequence_.clear();
}

FM_Cache_Info FM_Wrapper_Cached::cache_info() const
{
    std::size_t max_bucket_index = 0;
    std::size_t max_bucket_entries = 0;

    for (std::size_t i = 0; i < table_.size(); ++i) {
        if (table_[i].size() > max_bucket_entries) {
            max_bucket_entries = table_[i].size();
            max_bucket_index = i;
        }
    }

    return FM_Cache_Info{
        hits_,
        misses_,
        entries_,
        bucket_divisor_,
        table_.size(),
        min_cache_width_,
        max_bucket_index,
        max_bucket_entries
    };
}

std::size_t FM_Wrapper_Cached::bucket_size() const
{
    return bucket_divisor_;
}

std::size_t FM_Wrapper_Cached::bucket_count() const
{
    return table_.size();
}

void FM_Wrapper_Cached::clear_bucket_hit_sequence()
{
    bucket_hit_sequence_.clear();
}

const std::vector<std::size_t>& FM_Wrapper_Cached::bucket_hit_sequence() const
{
    return bucket_hit_sequence_;
}