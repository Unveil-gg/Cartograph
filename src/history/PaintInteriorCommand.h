#pragma once

#include "ICommand.h"
#include "../InteriorTypes.h"

#include <vector>

namespace Cartograph {

/**
 * Command for one interior brush stroke or fill. The editor changes the
 * model while the mouse drags and pushes this with execute=false on
 * release, so Execute and Undo must both write tiles.
 */
class PaintInteriorCommand : public ICommand {
public:
    struct Change {
        int ix;  // Global interior X
        int iy;  // Global interior Y, Y-up
        InteriorTile oldTile;
        InteriorTile newTile;
    };

    explicit PaintInteriorCommand(std::vector<Change> changes);

    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;

private:
    std::vector<Change> m_changes;
};

}  // namespace Cartograph
