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

    // IMPORTANT
    // bucket_divisor means bucket size, not number of buckets.
    bucket_size_ = bucket_divisor_;

    // Width gate:
    // width <= 1 means singleton interval. 
    min_cache_width_ = min_cache_width;

    // Keep +1 slack bucket
    std::size_t count = (fm_size_ / bucket_size_) + 1;
    buckets_.assign(count, Bucket{});

    hits_ = 0;
    misses_ = 0;
    entries_ = 0;
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
    return old_left / bucket_size_;
}

bool FM_Wrapper_Cached::keys_equal(const CacheKey& a, const CacheKey& b)
{
    return a.left_remainder == b.left_remainder &&
           a.old_right == b.old_right &&
           a.symbol == b.symbol;
}

std::size_t FM_Wrapper_Cached::hash_key(const CacheKey& key)
{
    std::size_t h = key.left_remainder;

    h ^= key.old_right + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(key.symbol) +
         0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);

    return h;
}

void FM_Wrapper_Cached::init_bucket(Bucket& bucket)
{
    if (bucket.table.empty()) {
        bucket.table.resize(8);
        bucket.used = 0;
    }
}

void FM_Wrapper_Cached::grow_bucket(Bucket& bucket)
{
    std::vector<Slot> old_table;
    old_table.swap(bucket.table);

    bucket.table.clear();
    bucket.table.resize(old_table.empty() ? 8 : old_table.size() * 2);
    bucket.used = 0;

    for (const Slot& slot : old_table) {
        if (!slot.used) {
            continue;
        }

        std::size_t mask = bucket.table.size() - 1;
        std::size_t pos = hash_key(slot.key) & mask;

        while (bucket.table[pos].used) {
            pos = (pos + 1) & mask;
        }

        bucket.table[pos] = slot;
        ++bucket.used;
    }
}

bool FM_Wrapper_Cached::find_in_bucket(const Bucket& bucket,
                                       const CacheKey& key,
                                       CacheValue& value) const
{
    if (bucket.table.empty()) {
        return false;
    }

    std::size_t mask = bucket.table.size() - 1;
    std::size_t pos = hash_key(key) & mask;

    while (bucket.table[pos].used) {
        if (keys_equal(bucket.table[pos].key, key)) {
            value = bucket.table[pos].value;
            return true;
        }

        pos = (pos + 1) & mask;
    }

    return false;
}

void FM_Wrapper_Cached::insert_in_bucket(Bucket& bucket,
                                         const CacheKey& key,
                                         const CacheValue& value)
{
    init_bucket(bucket);

    if ((bucket.used + 1) * 10 >= bucket.table.size() * 7) {
        grow_bucket(bucket);
    }

    std::size_t mask = bucket.table.size() - 1;
    std::size_t pos = hash_key(key) & mask;

    while (bucket.table[pos].used) {
        if (keys_equal(bucket.table[pos].key, key)) {
            bucket.table[pos].value = value;
            return;
        }

        pos = (pos + 1) & mask;
    }

    bucket.table[pos].used = true;
    bucket.table[pos].key = key;
    bucket.table[pos].value = value;

    ++bucket.used;
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

    
    // For FM, singleton/narrow intervals are computed directly.
    if (width <= min_cache_width_) {
        return compute_fm_transition(
            fm_index, occs, old_left, old_right, next_char);
    }

    const std::size_t b = bucket_index(old_left);
    const std::size_t left_remainder = old_left % bucket_size_;

    CacheKey key{left_remainder, old_right, symbol};

    CacheValue cached_value;
    if (find_in_bucket(buckets_[b], key, cached_value)) {
        ++hits_;
        return std::make_tuple(cached_value.new_left, cached_value.new_right);
    }

    ++misses_;

    auto result = compute_fm_transition(
        fm_index, occs, old_left, old_right, next_char);

    const std::size_t new_left = std::get<0>(result);
    const std::size_t new_right = std::get<1>(result);

    // Skip cache failed/empty refinements.
    if (new_left == new_right) {
        return result;
    }

    insert_in_bucket(buckets_[b], key, CacheValue{new_left, new_right});

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
    for (Bucket& bucket : buckets_) {
        bucket.table.clear();
        bucket.used = 0;
    }

    hits_ = 0;
    misses_ = 0;
    entries_ = 0;
}

FM_Cache_Info FM_Wrapper_Cached::cache_info() const
{
    return FM_Cache_Info{
        hits_,
        misses_,
        entries_,
        bucket_size_,
        buckets_.size()
    };
}

std::size_t FM_Wrapper_Cached::bucket_size() const
{
    return bucket_size_;
}

std::size_t FM_Wrapper_Cached::bucket_count() const
{
    return buckets_.size();
}
