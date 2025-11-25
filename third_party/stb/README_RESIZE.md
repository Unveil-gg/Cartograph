# stb_image_resize2.h

To enable icon resizing during import, download `stb_image_resize2.h` from:
https://github.com/nothings/stb/blob/master/stb_image_resize2.h

Place it in this directory (`third_party/stb/`).

## Alternative
If not available, the system will:
- Accept icons at their native size (up to 512×512)
- Reject non-square icons
- Center smaller icons in 64×64 canvas

With stb_image_resize2.h, the system will automatically resize any square icon to 64×64.

