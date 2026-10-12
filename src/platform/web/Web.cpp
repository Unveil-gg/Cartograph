#include "Web.h"
#include "../Paths.h"
#include "../Time.h"
#include <emscripten.h>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

// Implemented in library.js
extern "C" {
void js_pick_file(const char* accept, const char* destDir);
int js_download_file(const char* path);
void js_sync_storage();
}

namespace Platform {
namespace Web {

namespace {

constexpr const char* kUploadDir = "/tmp/uploads/";
constexpr const char* kExportDir = "/tmp/export/";
constexpr double kStorageSyncSeconds = 2.0;

// Only one picker can be open at a time.
PathCallback g_pickCallback;

}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void CartographWebFilePicked(
    const char* path
) {
    PathCallback callback = std::move(g_pickCallback);
    g_pickCallback = nullptr;
    if (callback) {
        callback(path ? path : "");
    }
}

void PickFile(const std::string& accept, PathCallback callback) {
    g_pickCallback = std::move(callback);
    js_pick_file(accept.c_str(), kUploadDir);
}

std::string ImportProject(const std::string& uploadPath) {
    std::string projectsDir = GetDefaultProjectsDir();
    if (!EnsureDirectoryExists(projectsDir)) {
        return "";
    }

    fs::path dest = fs::path(projectsDir) / fs::path(uploadPath).filename();
    std::error_code ec;
    fs::copy_file(uploadPath, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return "";
    }
    fs::remove(uploadPath, ec);
    return dest.string();
}

std::string GetExportDir() {
    EnsureDirectoryExists(kExportDir);
    return kExportDir;
}

bool DownloadFile(const std::string& path) {
    bool ok = js_download_file(path.c_str()) != 0;
    std::error_code ec;
    fs::remove(path, ec);
    return ok;
}

void UpdateStorage() {
    static double lastSync = 0.0;
    double now = GetTime();
    if (now - lastSync < kStorageSyncSeconds) {
        return;
    }
    lastSync = now;
    js_sync_storage();
}

}  // namespace Web
}  // namespace Platform
