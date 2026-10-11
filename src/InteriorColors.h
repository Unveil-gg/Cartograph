#pragma once

#include "Color.h"
#include "InteriorTypes.h"

namespace Cartograph {

/**
 * Display color for an interior tile type (editor and canvas preview).
 * @param t Tile type
 * @return Color; transparent for Empty
 */
inline Color InteriorTileColor(InteriorTile t) {
    switch (t) {
        case InteriorTile::Solid: return Color(0.55f, 0.56f, 0.62f, 1.0f);
        case InteriorTile::Platform: return Color(0.85f, 0.65f, 0.3f, 1.0f);
        case InteriorTile::Hazard: return Color(0.9f, 0.2f, 0.2f, 1.0f);
        case InteriorTile::Water: return Color(0.2f, 0.5f, 0.95f, 0.7f);
        case InteriorTile::Breakable: return Color(0.65f, 0.45f, 0.3f, 1.0f);
        case InteriorTile::Ladder: return Color(0.4f, 0.8f, 0.4f, 1.0f);
        default: return Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

}  // namespace Cartograph
