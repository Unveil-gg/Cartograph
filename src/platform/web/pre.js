// Runs before main(). Mounts IndexedDB at /persist and loads it, so
// projects, preferences and autosaves survive a page reload.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
    FS.mkdir('/persist');
    FS.mount(IDBFS, {}, '/persist');

    // Hold main() until the saved files are in memory.
    addRunDependency('cartograph-idbfs');
    FS.syncfs(true, function (err) {
        if (err) {
            console.warn('Cartograph: browser storage unavailable', err);
        }
        removeRunDependency('cartograph-idbfs');
    });

    // Ask the browser not to evict our storage under disk pressure.
    if (navigator.storage && navigator.storage.persist) {
        navigator.storage.persist();
    }
});

// Write /persist to IndexedDB. Overlapping calls are merged into one
// follow-up flush.
Module.cartographSyncing = false;
Module.cartographSyncAgain = false;
Module.cartographSyncStorage = function () {
    if (Module.cartographSyncing) {
        Module.cartographSyncAgain = true;
        return;
    }
    Module.cartographSyncing = true;
    FS.syncfs(false, function (err) {
        if (err) {
            console.warn('Cartograph: could not save to browser storage', err);
        }
        Module.cartographSyncing = false;
        if (Module.cartographSyncAgain) {
            Module.cartographSyncAgain = false;
            Module.cartographSyncStorage();
        }
    });
};

// Flush when the tab is hidden or closed.
document.addEventListener('visibilitychange', function () {
    if (document.visibilityState === 'hidden') {
        Module.cartographSyncStorage();
    }
});
window.addEventListener('pagehide', function () {
    Module.cartographSyncStorage();
});
