// Interior sub-grid storage for Model.

#include "Model.h"
#include "Config.h"

#include <algorithm>

namespace Cartograph {

/**
 * Read one interior tile.
 * @param ix Global interior X (cell * width + local x)
 * @param iy Global interior Y, Y-up (cell * height + local y)
 * @return Tile type, Empty when the cell has no chunk
 */
InteriorTile Model::GetInteriorTile(int ix, int iy) const {
    const int w = interiorConfig.width;
    const int h = interiorConfig.height;
    int cx = InteriorFloorDiv(ix, w);
    int cy = InteriorFloorDiv(iy, h);
    auto it = interior.find({cx, cy});
    if (it == interior.end()) return InteriorTile::Empty;
    int lx = ix - cx * w;
    int ly = iy - cy * h;
    size_t idx = static_cast<size_t>(ly) * w + lx;
    if (idx >= it->second.tiles.size()) return InteriorTile::Empty;
    return static_cast<InteriorTile>(it->second.tiles[idx]);
}

/**
 * Write one interior tile, creating or dropping chunks as needed.
 * @param ix Global interior X
 * @param iy Global interior Y, Y-up
 * @param tile New tile type
 */
void Model::SetInteriorTile(int ix, int iy, InteriorTile tile) {
    const int w = interiorConfig.width;
    const int h = interiorConfig.height;
    int cx = InteriorFloorDiv(ix, w);
    int cy = InteriorFloorDiv(iy, h);
    auto it = interior.find({cx, cy});
    if (it == interior.end()) {
        if (tile == InteriorTile::Empty) return;
        if (interior.size() >= Limits::MAX_INTERIOR_CHUNKS) return;
        it = interior.emplace(std::make_pair(cx, cy), InteriorChunk{}).first;
        it->second.tiles.assign(static_cast<size_t>(w) * h, 0);
    }
    int lx = ix - cx * w;
    int ly = iy - cy * h;
    uint8_t& slot = it->second.tiles[static_cast<size_t>(ly) * w + lx];
    if (slot == static_cast<uint8_t>(tile)) return;
    slot = static_cast<uint8_t>(tile);
    if (tile == InteriorTile::Empty && it->second.IsEmpty()) {
        interior.erase(it);
    }
    MarkDirty();
}

/**
 * Change the interior sub-grid size. Refused once any chunk exists.
 * @param width Tiles per cell horizontally (1..MAX_INTERIOR_DIMENSION)
 * @param height Tiles per cell vertically
 * @return true when applied
 */
bool Model::SetInteriorSize(int width, int height) {
    if (!interior.empty()) return false;
    if (width < 1 || height < 1 ||
        width > Limits::MAX_INTERIOR_DIMENSION ||
        height > Limits::MAX_INTERIOR_DIMENSION) {
        return false;
    }
    interiorConfig.width = width;
    interiorConfig.height = height;
    MarkDirty();
    return true;
}

}  // namespace Cartograph
