#pragma once

// Web build stand-in for GLAD. Browsers expose WebGL2 (OpenGL ES 3.0)
// and Emscripten links the GL entry points directly, so no loader runs.
#include <GLES3/gl3.h>
