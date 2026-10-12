// JavaScript half of Web.cpp. Linked with --js-library.
mergeInto(LibraryManager.library, {
    // Open a hidden <input type=file>. Copies the chosen file into destDir
    // and calls CartographWebFilePicked with its virtual path, or '' on
    // cancel or failure.
    js_pick_file__deps: ['$UTF8ToString'],
    js_pick_file: function (accept, destDir) {
        const dir = UTF8ToString(destDir);
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = UTF8ToString(accept);
        input.style.display = 'none';
        const finish = function (path) {
            input.remove();
            ccall('CartographWebFilePicked', null, ['string'], [path]);
        };
        input.addEventListener('cancel', function () { finish(''); });
        input.addEventListener('change', async function () {
            const file = input.files && input.files[0];
            if (!file) {
                finish('');
                return;
            }
            try {
                const data = new Uint8Array(await file.arrayBuffer());
                const name = file.name.replace(/[\\/:*?"<>|]/g, '_');
                try { FS.mkdir(dir); } catch (e) { /* already exists */ }
                FS.writeFile(dir + name, data);
                finish(dir + name);
            } catch (e) {
                console.error('Cartograph: upload failed', e);
                finish('');
            }
        });
        document.body.appendChild(input);
        input.click();
    },

    // Offer a virtual file as a browser download. Returns 0 on failure.
    js_download_file__deps: ['$UTF8ToString'],
    js_download_file: function (path) {
        try {
            const p = UTF8ToString(path);
            const blob = new Blob([FS.readFile(p)],
                                  {type: 'application/octet-stream'});
            const url = URL.createObjectURL(blob);
            const link = document.createElement('a');
            link.href = url;
            link.download = p.substring(p.lastIndexOf('/') + 1);
            document.body.appendChild(link);
            link.click();
            link.remove();
            setTimeout(function () { URL.revokeObjectURL(url); }, 10000);
            return 1;
        } catch (e) {
            console.error('Cartograph: download failed', e);
            return 0;
        }
    },

    // Flush /persist to IndexedDB (see pre.js).
    js_sync_storage: function () {
        if (Module.cartographSyncStorage) {
            Module.cartographSyncStorage();
        }
    },
});
