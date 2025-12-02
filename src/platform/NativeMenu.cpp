#include "NativeMenu.h"

#ifdef __APPLE__
#include "NativeMenu_macos.h"
#else
#include "NativeMenu_imgui.h"
#endif

namespace Cartograph {

std::unique_ptr<INativeMenu> CreateNativeMenu() {
#ifdef __APPLE__
    return std::make_unique<NativeMenuMacOS>();
#else
    return std::make_unique<NativeMenuImGui>();
#endif
}

} // namespace Cartograph

