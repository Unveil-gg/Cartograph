#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to add a new color to the palette.
 */
class AddPaletteColorCommand : public ICommand {
public:
    AddPaletteColorCommand(const std::string& name, const Color& color);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::string m_name;
    Color m_color;
    int m_tileId;  // Set after first Execute
};

} // namespace Cartograph

