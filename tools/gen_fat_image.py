#!/usr/bin/env python3
"""Generate C source and header from a binary FAT image file."""

import sys

if len(sys.argv) != 4:
    print(f"Usage: {sys.argv[0]} <image.bin> <output.c> <output.h>", file=sys.stderr)
    sys.exit(1)

img_path = sys.argv[1]
c_path = sys.argv[2]
h_path = sys.argv[3]

data = open(img_path, 'rb').read()

with open(h_path, 'w') as h:
    h.write('/* Auto-generated FAT image data */\n')
    h.write('#ifndef FAT_IMAGE_DATA_H\n')
    h.write('#define FAT_IMAGE_DATA_H\n')
    h.write('#include <stdint.h>\n')
    h.write('extern const uint8_t fat_flash_image[];\n')
    h.write('extern const uint32_t fat_flash_image_size;\n')
    h.write('#endif\n')

with open(c_path, 'w') as c:
    c.write('#include <stdint.h>\n')
    c.write('const uint8_t fat_flash_image[] __attribute__((section(".rodata"))) = {\n')
    # Write 16 bytes per line for readability
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        c.write('    ' + ','.join(f'0x{b:02x}' for b in chunk) + ',\n')
    c.write('};\n')
    c.write(f'const uint32_t fat_flash_image_size = {len(data)};\n')
