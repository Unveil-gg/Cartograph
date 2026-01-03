#include "SetZoomCommand.h"
#include "../Canvas.h"

namespace Cartograph {

SetZoomCommand::SetZoomCommand(Canvas& canvas, float oldZoom, float newZoom,
                               int displayPercent)
    : m_canvas(canvas)
    , m_oldZoom(oldZoom)
    , m_newZoom(newZoom)
    , m_displayPercent(displayPercent) {}

void SetZoomCommand::Execute(Model& /*model*/) {
    m_canvas.SetZoom(m_newZoom);
}

void SetZoomCommand::Undo(Model& /*model*/) {
    m_canvas.SetZoom(m_oldZoom);
}

std::string SetZoomCommand::GetDescription() const {
    return "Zoom to " + std::to_string(m_displayPercent) + "%";
}

} // namespace Cartograph

