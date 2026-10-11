#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to change the kind of one or more markers. Several markers
 * change together when a new map-start marker demotes the old one.
 */
class SetMarkerKindCommand : public ICommand {
public:
    struct Change {
        std::string markerId;
        std::string oldKind;
        std::string newKind;
    };

    explicit SetMarkerKindCommand(const std::vector<Change>& changes);

    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;

private:
    std::vector<Change> m_changes;
};

}  // namespace Cartograph
