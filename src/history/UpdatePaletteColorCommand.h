#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to update palette color name/color.
 */
class UpdatePaletteColorCommand : public ICommand {
public:
    UpdatePaletteColorCommand(
        int tileId, 
        const std::string& newName, 
        const Color& newColor
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    int m_tileId;
    std::string m_newName;
    Color m_newColor;
    std::string m_oldName;
    Color m_oldColor;
};

} // namespace Cartograph

