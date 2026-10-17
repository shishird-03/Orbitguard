#include "ssa/SpatialGrid.h"
#include <algorithm>

namespace og {

static constexpr uint64_t NONE = ~uint64_t(0);
static constexpr int BIAS = 1 << 20;  // 21 bits per axis

uint64_t SpatialGrid::pack(int x, int y, int z) {
    return (uint64_t(x + BIAS) << 42) | (uint64_t(y + BIAS) << 21) | uint64_t(z + BIAS);
}

void SpatialGrid::unpack(uint64_t k, int& x, int& y, int& z) {
    const uint64_t m = (1u << 21) - 1;
    x = int((k >> 42) & m) - BIAS;
    y = int((k >> 21) & m) - BIAS;
    z = int(k & m) - BIAS;
}

static size_t slotOf(uint64_t key, size_t mask) { return size_t((key * 0x9E3779B97F4A7C15ull) >> 20) & mask; }

int SpatialGrid::find(uint64_t key) const {
    if (slotKey_.empty()) return -1;
    size_t mask = slotKey_.size() - 1;
    for (size_t s = slotOf(key, mask);; s = (s + 1) & mask) {
        if (slotKey_[s] == key) return slotList_[s];
        if (slotKey_[s] == NONE) return -1;
    }
}

void SpatialGrid::rehash(size_t cap) {
    slotKey_.assign(cap, NONE);
    slotList_.assign(cap, -1);
    for (size_t l = 0; l < listKey_.size(); ++l) {
        size_t s = slotOf(listKey_[l], cap - 1);
        while (slotKey_[s] != NONE) s = (s + 1) & (cap - 1);
        slotKey_[s] = listKey_[l];
        slotList_[s] = int(l);
    }
}

int SpatialGrid::findOrInsert(uint64_t key) {
    if ((lists_.size() + 1) * 2 > slotKey_.size()) rehash(std::max<size_t>(1024, slotKey_.size() * 2));  // load <= 0.5
    size_t mask = slotKey_.size() - 1;
    size_t s = slotOf(key, mask);
    for (; slotKey_[s] != NONE; s = (s + 1) & mask)
        if (slotKey_[s] == key) return slotList_[s];
    slotKey_[s] = key;
    slotList_[s] = int(lists_.size());
    lists_.emplace_back();
    listKey_.push_back(key);
    return slotList_[s];
}

void SpatialGrid::clear() {
    slotKey_.clear();
    slotList_.clear();
    lists_.clear();
    listKey_.clear();
    objCell_.clear();
    keys_.clear();
}

void SpatialGrid::setCellSize(double km) {
    if (km != cell_) { cell_ = km; clear(); }
}

int SpatialGrid::update(const std::vector<Vec3>& pos, const std::vector<uint8_t>& alive) {
    if (objCell_.size() < pos.size()) objCell_.resize(pos.size(), NONE);
    int moved = 0;
    for (size_t i = 0; i < pos.size(); ++i) {
        uint64_t k = NONE;
        if (alive[i]) {
            const Vec3& p = pos[i];
            k = pack(int(std::floor(p.x / cell_)), int(std::floor(p.y / cell_)), int(std::floor(p.z / cell_)));
        }
        uint64_t old = objCell_[i];
        if (k == old) continue;  // coherence: same cell as last time, nothing to do
        if (old != NONE) {
            auto& v = lists_[find(old)];
            *std::find(v.begin(), v.end(), int(i)) = v.back();
            v.pop_back();  // empty cells stay allocated (no churn); compacted below when stale
        }
        if (k != NONE) lists_[findOrInsert(k)].push_back(int(i));
        objCell_[i] = k;
        ++moved;
    }
    keys_.clear();
    for (size_t l = 0; l < lists_.size(); ++l)
        if (!lists_[l].empty()) keys_.push_back(listKey_[l]);
    if (lists_.size() > 4 * keys_.size() + 1024) {  // too many stale empty cells: compact
        std::vector<std::vector<int>> live;
        std::vector<uint64_t> liveKeys;
        for (size_t l = 0; l < lists_.size(); ++l)
            if (!lists_[l].empty()) {
                live.push_back(std::move(lists_[l]));
                liveKeys.push_back(listKey_[l]);
            }
        lists_.swap(live);
        listKey_.swap(liveKeys);
        rehash(slotKey_.size());
    }
    return moved;
}

}  // namespace og
