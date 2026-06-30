#include "fm_lp.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

FM_LP::FM_LP() = default;

FM_LP::FM_LP(std::size_t fm_size,
             std::size_t div_p,
             std::size_t min_cache_width)
{
    configure(fm_size, div_p, min_cache_width);
}

void FM_LP::configure(std::size_t fm_size,
                      std::size_t div_p,
                      std::size_t min_cache_width)
{
    if (fm_size == 0) {
        throw std::invalid_argument("FM_LP: fm_size must be non-zero");
    }

    if (div_p == 0) {
        throw std::invalid_argument("FM_LP: div_p must be non-zero");
    }

    fm_size_ = fm_size;
    div_p_ = div_p;
    min_cache_width_ = min_cache_width;

    rebuild_table();

    hits_ = 0;
    misses_ = 0;
    entries_ = 0;
}

void FM_LP::rebuild_table()
{
    const std::size_t slots = std::max<std::size_t>(1, fm_size_ / div_p_);
    table_.assign(slots, Slot{});
}

void FM_LP::ensure_configured(std::size_t fm_size)
{
    if (fm_size_ == 0) {
        configure(fm_size, div_p_, min_cache_width_);
        return;
    }

    if (fm_size_ != fm_size) {
        throw std::runtime_error("FM_LP used with a different FM-index/reference");
    }
}

std::size_t FM_LP::key_hash(const LookupKey& key) const noexcept
{
    return key.old_left ^
           (key.old_right << 1) ^
           (static_cast<std::size_t>(key.symbol) << 2);
}

std::size_t FM_LP::start_slot(const LookupKey& key) const
{
    return key_hash(key) % table_.size();
}

FM_LP::LookupKey FM_LP::make_key(std::size_t old_left,
                                 std::size_t old_right,
                                 unsigned char symbol) const
{
    return LookupKey{old_left, old_right, symbol};
}

bool FM_LP::lookup(std::size_t old_left,
                   std::size_t old_right,
                   unsigned char symbol,
                   Interval& cached_interval)
{
    const LookupKey key = make_key(old_left, old_right, symbol);
    std::size_t pos = start_slot(key);

    for (std::size_t probe = 0; probe < table_.size(); ++probe) {
        const Slot& slot = table_[pos];

        if (!slot.occupied) {
            ++misses_;
            return false;
        }

        if (slot.key == key) {
            cached_interval = slot.interval;
            ++hits_;
            return true;
        }

        pos = (pos + 1 == table_.size()) ? 0 : pos + 1;
    }

    ++misses_;
    return false;
}

void FM_LP::insert(std::size_t old_left,
                   std::size_t old_right,
                   unsigned char symbol,
                   std::size_t new_left,
                   std::size_t new_right)
{
    const LookupKey key = make_key(old_left, old_right, symbol);
    maybe_resize();

    std::size_t pos = start_slot(key);

    for (std::size_t probe = 0; probe < table_.size(); ++probe) {
        Slot& slot = table_[pos];

        if (!slot.occupied) {
            slot.occupied = true;
            slot.key = key;
            slot.interval = Interval{new_left, new_right};
            ++entries_;
            return;
        }

        if (slot.key == key) {
            slot.interval = Interval{new_left, new_right};
            return;
        }

        pos = (pos + 1 == table_.size()) ? 0 : pos + 1;
    }

    throw std::runtime_error("FM_LP: linear probing table is full after resize");
}

void FM_LP::maybe_resize()
{
    if (table_.empty()) {
        table_.assign(1, Slot{});
        return;
    }

    const double next_load =
        static_cast<double>(entries_ + 1) / static_cast<double>(table_.size());

    if (next_load <= max_load_factor_) {
        return;
    }

    std::vector<Slot> old_table = std::move(table_);
    table_.assign(old_table.size() * 2, Slot{});
    entries_ = 0;

    for (const Slot& slot : old_table) {
        if (slot.occupied) {
            reinsert(slot.key, slot.interval);
        }
    }
}

void FM_LP::reinsert(const LookupKey& key, const Interval& interval)
{
    std::size_t pos = start_slot(key);

    for (std::size_t probe = 0; probe < table_.size(); ++probe) {
        Slot& slot = table_[pos];

        if (!slot.occupied) {
            slot.occupied = true;
            slot.key = key;
            slot.interval = interval;
            ++entries_;
            return;
        }

        pos = (pos + 1 == table_.size()) ? 0 : pos + 1;
    }

    throw std::runtime_error("FM_LP: reinsert failed during resize");
}

void FM_LP::check_character_exists(const std::vector<std::size_t>& occs,
                                   unsigned char symbol)
{
    if (occs.size() < 256) {
        throw std::runtime_error("FM_LP: occs must have 256 entries");
    }

    if (symbol < 255 && occs[symbol] == occs[symbol + 1]) {
        std::cerr << "Character code: " << static_cast<int>(symbol)
                  << " not found in reference text!" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

std::tuple<std::size_t, std::size_t> FM_LP::compute_fm_transition(
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

std::tuple<std::size_t, std::size_t> FM_LP::backward_match(
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

std::size_t FM_LP::get_suffix_array_value(
    const rlz_fm_index_t& fm_index,
    std::size_t location)
{
    return fm_index[location];
}

void FM_LP::clear_cache()
{
    rebuild_table();
    hits_ = 0;
    misses_ = 0;
    entries_ = 0;
}

FM_LP_Info FM_LP::cache_info() const
{
    FM_LP_Info info;

    info.hits = hits_;
    info.misses = misses_;
    info.entries = entries_;
    info.div_p = div_p_;
    info.table_slots = table_.size();
    info.min_cache_width = min_cache_width_;

    const std::size_t total = hits_ + misses_;
    info.hit_rate = total == 0
        ? 0.0
        : static_cast<double>(hits_) / static_cast<double>(total);

    info.load_factor = table_.empty()
        ? 0.0
        : static_cast<double>(entries_) / static_cast<double>(table_.size());

    info.approx_bytes = table_.size() * sizeof(Slot);

    return info;
}
