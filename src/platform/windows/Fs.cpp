/**
 * Windows-specific file system operations.
 * 
 * Currently a placeholder - Windows uses SDL3's cross-platform file dialogs.
 * Add Windows-specific implementations here if needed in the future.
 */

#ifdef _WIN32

#include "../Fs.h"

namespace Platform {

// Windows-specific file dialog implementation
// Currently uses SDL3's cross-platform SDL_ShowOpenFileDialog/SDL_ShowSaveFileDialog
// which work well on Windows. Add native Win32 dialogs here if needed.

} // namespace Platform

#endif // _WIN32

