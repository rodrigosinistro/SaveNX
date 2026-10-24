# Public libraries used by SaveNX

The SaveNX application code and artwork are original. The build links these general-purpose libraries supplied by devkitPro:

| Library | Role | Upstream license/source |
| --- | --- | --- |
| libnx | Switch homebrew OS services | ISC — https://github.com/switchbrew/libnx |
| SDL2 | Window, drawing and presentation | zlib — https://github.com/libsdl-org/SDL/tree/SDL2 |
| FreeType | System font rasterization | FTL or GPLv2 — https://freetype.org/license.html (FTL selected) |
| HarfBuzz | FreeType port dependency | MIT-style — https://github.com/harfbuzz/harfbuzz |
| curl and its TLS dependencies | HTTPS to Google APIs | curl license — https://curl.se/docs/copyright.html; bundled port dependencies retain their own licenses |
| json-c | JSON encoding/decoding | MIT — https://github.com/json-c/json-c |
| minizip/zlib | ZIP backup files and CRC checking | zlib — https://github.com/madler/zlib |
| bzip2 | FreeType port dependency | BSD-style — https://sourceware.org/bzip2/ |
| libpng | FreeType port dependency | libpng license — https://www.libpng.org/pub/png/src/libpng-LICENSE.txt |

The runtime font is provided by the console OS; no Nintendo font is redistributed. TLS certificate authorities are taken from the build environment's CA bundle. The devkitA64 toolchain includes GCC runtime components under their upstream licenses and runtime exceptions.

SaveNX does not bundle JKSV, FsLib, SDLLib, NPShop source code, imagery or layouts. Names of third-party products identify compatibility only.
