// Uniform 3D spatial hash grid for the conjunction broad phase.
// Frame-to-frame coherence: each object remembers its cell, and only objects whose cell
// changed since the last update are moved (most objects stay put between nearby time slices).
// Cells live in a flat open-addressing hash table (linear probing): node-based std::unordered_map
// spent most of the update chasing pointers.
#pragma once
#include <cstdint>
#include <vector>
#include "math/Mat4.h"

namespace og {

class SpatialGrid {
public:
    void setCellSize(double km);  // clears the grid when the size changes
    double cellSize() const { return cell_; }
    // Returns how many objects were (re)hashed this update.
    int update(const std::vector<Vec3>& pos, const std::vector<uint8_t>& alive);
    void clear();
    size_t cellCount() const { return keys_.size(); }

    // Occupied cells after the last update (lets callers split the pair search across threads).
    const std::vector<uint64_t>& cellKeys() const { return keys_; }

    // Calls fn(i, j) for pairs within cell `k` and between `k` and its 13 "forward" neighbors, so that
    // over all cells every unordered pair in the 27-neighborhood is visited exactly once.
    template <class F>
    void pairsForCell(uint64_t k, F&& fn) const {
        int self = find(k);
        if (self < 0 || lists_[self].empty()) return;
        const auto& list = lists_[self];
        for (size_t a = 0; a < list.size(); ++a)
            for (size_t b = a + 1; b < list.size(); ++b) fn(list[a], list[b]);
        int cx, cy, cz;
        unpack(k, cx, cy, cz);
        for (int dz = 0; dz <= 1; ++dz)
            for (int dy = (dz ? -1 : 0); dy <= 1; ++dy)
                for (int dx = (dz || dy ? -1 : 1); dx <= 1; ++dx) {
                    int n = find(pack(cx + dx, cy + dy, cz + dz));
                    if (n < 0) continue;
                    for (int i : list)
                        for (int j : lists_[n]) fn(i, j);
                }
    }

    template <class F>
    void forEachCandidatePair(F&& fn) const {
        for (uint64_t k : keys_) pairsForCell(k, fn);
    }

    static uint64_t pack(int x, int y, int z);
    static void unpack(uint64_t k, int& x, int& y, int& z);

private:
    int find(uint64_t key) const;  // list index or -1
    int findOrInsert(uint64_t key);
    void rehash(size_t capacity);

    double cell_ = 50.0;
    std::vector<uint64_t> slotKey_;  // open-addressing table: key per slot (NONE if unused)
    std::vector<int> slotList_;      // slot -> index into lists_
    std::vector<std::vector<int>> lists_;
    std::vector<uint64_t> listKey_;
    std::vector<uint64_t> objCell_, keys_;
};

}  // namespace og
