#pragma once

#ifdef __EMSCRIPTEN__

#include <functional>
#include <string>

/**
 * Browser integration for the Emscripten build.
 *
 * Files live in Emscripten's virtual file system. /persist is backed by
 * IndexedDB so projects, preferences and autosaves survive a page reload.
 * The browser cannot hand us real paths, so uploads land in a temp folder
 * and exports are handed to the browser as downloads.
 */
namespace Platform {
namespace Web {

/** Receives a virtual path, or an empty string if the user cancelled. */
using PathCallback = std::function<void(const std::string& path)>;

/**
 * Open the browser file picker. Must be called from a user action.
 * The chosen file is copied into the upload folder.
 * @param accept Comma-separated extensions or MIME types (".cart")
 * @param callback Runs on a later frame with the uploaded file's path
 */
void PickFile(const std::string& accept, PathCallback callback);

/**
 * Move an uploaded project into the persistent projects folder, replacing
 * a project with the same name, so later saves are kept across reloads.
 * @param uploadPath File from PickFile or a drag and drop
 * @return New path, or empty on failure
 */
std::string ImportProject(const std::string& uploadPath);

/**
 * Folder where exports are written before they are downloaded.
 * @return Folder path with a trailing slash
 */
std::string GetExportDir();

/**
 * Hand a file to the browser as a download, then delete it.
 * @param path File in the virtual file system
 * @return true if the download was started
 */
bool DownloadFile(const std::string& path);

/**
 * Write pending changes under /persist to IndexedDB.
 * Call every frame; the actual flush is throttled.
 */
void UpdateStorage();

}  // namespace Web
}  // namespace Platform

#endif  // __EMSCRIPTEN__
