// Interior tile storage and the undo command that paints it.

#include "TestUtil.h"

#include "Model.h"
#include "history/History.h"
#include "history/PaintInteriorCommand.h"

#include <memory>

using testutil::Expect;
using namespace Cartograph;

namespace {

/** Tiles read back across a cell border, including negative cells. */
void TestStorage() {
    Model m;
    const int w = m.interiorConfig.width;
    const int h = m.interiorConfig.height;
    m.SetInteriorTile(-1, -1, InteriorTile::Solid);
    Expect(m.GetInteriorTile(-1, -1) == InteriorTile::Solid,
           "negative coordinates round trip");
    Expect(m.interior.count({-1, -1}) == 1, "tile lands in cell (-1, -1)");
    m.SetInteriorTile(w, h, InteriorTile::Water);
    Expect(m.interior.count({1, 1}) == 1, "tile lands in cell (1, 1)");
    m.SetInteriorTile(w, h, InteriorTile::Empty);
    Expect(m.interior.count({1, 1}) == 0, "empty chunk is dropped");
    Expect(!m.SetInteriorSize(8, 8), "size locks once painted");
}

/** A stroke applied during the drag undoes and redoes cleanly. */
void TestStrokeUndo() {
    Model m;
    History history;
    std::vector<PaintInteriorCommand::Change> changes;
    for (int x = 0; x < 4; ++x) {
        m.SetInteriorTile(x, 2, InteriorTile::Solid);
        changes.push_back({x, 2, InteriorTile::Empty, InteriorTile::Solid});
    }
    history.AddCommand(
        std::make_unique<PaintInteriorCommand>(changes), m, false);
    Expect(m.GetInteriorTile(3, 2) == InteriorTile::Solid, "stroke kept");

    history.Undo(m);
    Expect(m.interior.empty(), "undo clears the stroke and its chunk");
    history.Redo(m);
    Expect(m.GetInteriorTile(0, 2) == InteriorTile::Solid &&
               m.GetInteriorTile(3, 2) == InteriorTile::Solid,
           "redo restores the stroke");
}

/** A command pushed with execute=true applies itself. */
void TestExecutedCommand() {
    Model m;
    History history;
    std::vector<PaintInteriorCommand::Change> changes{
        {5, 5, InteriorTile::Empty, InteriorTile::Ladder}};
    history.AddCommand(
        std::make_unique<PaintInteriorCommand>(changes), m, true);
    Expect(m.GetInteriorTile(5, 5) == InteriorTile::Ladder, "applied");
    history.Undo(m);
    Expect(m.GetInteriorTile(5, 5) == InteriorTile::Empty, "undone");
}

}  // namespace

int main() {
    TestStorage();
    TestStrokeUndo();
    TestExecutedCommand();
    return testutil::Finish("interior_tests");
}
