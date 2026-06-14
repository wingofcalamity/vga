import struct

# ===== CONFIG =====
PSF_FILE = "Unifont-APL8x16-17.0.04.psf"
OUT_FILE = "font.h"
# ===================

PSF1_MAGIC = 0x0436

def load_psf(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, mode, charsize = struct.unpack_from("<HBB", data, 0)
    if magic != PSF1_MAGIC:
        raise ValueError("Not a PSF1 font")
    glyphs = []
    offset = 4
    for i in range(256):
        start = offset + i * charsize
        glyphs.append(data[start:start + charsize])
    return charsize, glyphs

def write_header(charsize, glyphs):
    with open(OUT_FILE, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define FONT_WIDTH 8\n")
        f.write(f"#define FONT_HEIGHT {charsize}\n\n")
        f.write("static const uint8_t font8x16[256][16] = {\n")
        for i, g in enumerate(glyphs):
            f.write(f"  /* {i} */ {{\n")
            for b in g:
                f.write(f"    0b{b:08b},\n")
            f.write("  },\n")
        f.write("};\n\n")
        f.write("static inline const uint8_t* font_get(char c)\n")
        f.write("{\n")
        f.write("  return font8x16[(unsigned char)c];\n")
        f.write("}\n")

if __name__ == "__main__":
    height, glyphs = load_psf(PSF_FILE)
    write_header(height, glyphs)