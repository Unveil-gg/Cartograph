// Read-only miniature of the interior layer on the main canvas.

#include "Canvas.h"
#include "InteriorColors.h"
#include "render/Renderer.h"

namespace Cartograph {

/**
 * Draw every interior tile inside its cell. Skipped when the layer is
 * off or the tiles would be smaller than a pixel.
 * @param renderer Renderer for rects
 * @param model Model holding the interior chunks
 */
void Canvas::RenderInterior(IRenderer& renderer, const Model& model) {
    if (!showInterior || model.interior.empty()) return;
    const int w = model.interiorConfig.width;
    const int h = model.interiorConfig.height;
    const int tw = model.grid.tileWidth;
    const int th = model.grid.tileHeight;
    const float pw = tw * zoom / w;
    const float ph = th * zoom / h;
    if (pw < 1.0f || ph < 1.0f) return;

    for (const auto& [cell, chunk] : model.interior) {
        float wx, wy, sx, sy;
        TileToWorld(cell.first, cell.second, tw, th, &wx, &wy);
        WorldToScreen(wx, wy, &sx, &sy);
        for (int ly = 0; ly < h; ++ly) {
            for (int lx = 0; lx < w; ++lx) {
                uint8_t t = chunk.tiles[static_cast<size_t>(ly) * w + lx];
                if (t == 0) continue;
                // Row 0 is the south row, which is the bottom on screen.
                renderer.DrawRect(sx + lx * pw, sy + (h - 1 - ly) * ph, pw,
                                  ph, InteriorTileColor(
                                         static_cast<InteriorTile>(t)));
            }
        }
    }
}

}  // namespace Cartograph
