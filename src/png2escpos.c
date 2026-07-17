#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"

// Initialization sequence from your working examples
/*unsigned char init_seq[] = {
    0x1b, 0x3d, 0x01, 0x1d, 0x61, 0x1e, 0x1b, 0x40, 0x12, 0x3d, 0x01, 0x1d, 
    0x50, 0x00, 0x00, 0x1d, 0x45, 0x13, 0x13, 0x28, 0x28, 0x53, 0x65, 0x69, 
    0x6b, 0x6f, 0x20, 0x49, 0x6e, 0x73, 0x74, 0x72, 0x75, 0x6d, 0x65, 0x6e, 
    0x74, 0x73, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x29, 0x1c, 0x43, 0x01, 0x1b, 
    0x74, 0x00, 0x1b, 0x52, 0x00, 0x12, 0x4b, 0x79, 0x04, 0x12, 0x48, 0x5d, 0x0c
}; */

// Initialize sequence for Seiko Instruments Inc. (SII) thermal printers
unsigned char init_seq[] = {
    // 1. DEVICE SELECTION & ASB STATUS
    0x1b, 0x3d, 0x01,  // ESC = 0x01  -> Select Peripheral Device (Enable printer)
    0x1d, 0x61, 0x1e,  // GS a 0x1e   -> Enable Automatic Status Back (ASB) for paper/error states

    // 2. PRINTER INITIALIZATION
    0x1b, 0x40,        // ESC @       -> Initialize printer (Resets settings to defaults)

    // 3. DEVICE SELECTION (SEIKO SPECIFIC EXTENSION)
    0x12, 0x3d, 0x01,  // DC2 = 0x01  -> Select Primary Printing Device

    // 4. MOTION UNIT INITIALIZATION
    0x1d, 0x50, 0x00, 0x00, // GS P 0 0 -> Reset horizontal and vertical motion units to default

    // 5. SEIKO NV-MEMORY SECURITY HANDSHAKE (Unlocks write access for printer settings)
    0x1d, 0x45,        // GS E        -> NV Memory / Software Lock command prefix
    0x13, 0x13,        // Length parameters (0x13, 0x13)
    0x28, 0x28,        // "((" character markers
    // ASCII "Seiko Instruments Inc."
    0x53, 0x65, 0x69, 0x6b, 0x6f, 0x20, 0x49, 0x6e, 0x73, 0x74, 0x72, 0x75, 
    0x6d, 0x65, 0x6e, 0x74, 0x73, 0x20, 0x49, 0x6e, 0x63, 0x2e, 
    0x29,              // ")" character marker

    // 6. CHARACTER & CODEPAGE SELECTION
    0x1c, 0x43, 0x01,  // FS C 0x01   -> Select Kanji Character Mode / Japanese Code System
    0x1b, 0x74, 0x00,  // ESC t 0x00  -> Select Character Code Table 0 (PC437 / USA, Standard Europe)
    0x1b, 0x52, 0x00,  // ESC R 0x00  -> Select International Character Set (USA)

    // 7. PRINT HEAD OPTIMIZATION (SEIKO SPECIFIC)
    0x12, 0x4b, 0x00, 0x05, /* DC2 K ... [12 4b] (DC2 K) — Print Area Width  00 05 = 1280 as datasheet says
     * This command sets the physical page/print width parameters in dots using a 16-bit little-endian value
     * 115x80 Example: 12 4b ef 04 → 0x04EF = 1,263 dots (Matches the ~1280 pixel range for a wide 115mm head).
     * 105x273 Example: 12 4b 79 04 → 0x0479 = 1,145 dots (Narrower print zone matching the 105mm width specification).
     * 1224 for 110mm paper
     * 904 for 80mm paper safe (or 944 max)*/
     
    0x12, 0x48, 0x5d, 0x0c      /* (DC2 H) — Page Length / Max Form Length
    * 115x80 Example: 12 48 52 03 → 0x0352 = 850 dots wide/long page boundary.
    * 105x273 Example: 12 48 5d 0c → 0x0C5D = 3,165 dots long form boundary (significantly taller to accommodate a longer receipt format).
    */
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <input.png> <output.bin>\n", argv[0]);
        return 1;
    }

    unsigned char *image;
    unsigned width, height;
    unsigned error = lodepng_decode32_file(&image, &width, &height, argv[1]);
    if (error) return 1;

    FILE *f = fopen(argv[2], "wb");
    if (!f) {
        free(image);
        return 1;
    }

    // Process the image in chunks of at most 3000 vertical pixels
    unsigned int chunk_max_height = 3000;
    unsigned int current_y_offset = 0;

    while (current_y_offset < height) {
        // Calculate the height of the current slice/chunk
        unsigned int current_chunk_height = height - current_y_offset;
        if (current_chunk_height > chunk_max_height) {
            current_chunk_height = chunk_max_height;
        }

        // 1. Write Header (Repeated initialization for every page/chunk)
        fwrite(init_seq, 1, sizeof(init_seq), f);

        // 2. Start Page Mode
        unsigned char page_mode[] = { 0x1b, 0x4c, 0x1b, 0x54, 0x00 };
        fwrite(page_mode, 1, 5, f);

        // 3. Define Print Area (ESC W) - CORRECTED
        // Parameters are in 0.125mm units (1 pixel = 1 unit for 8 pixels/mm)
        int start_x_pixels = 0;
        if (width < 1280) {  
            start_x_pixels = (1280 - width) / 2;
        }
        
        int start_y_pixels = 0;
        
        // Convert to units (1 pixel = 1 unit for 0.125mm resolution)
        unsigned short xL = start_x_pixels;
        unsigned short yL = start_y_pixels;
        unsigned short dxL = width;
        unsigned short dyL = current_chunk_height;
        
        unsigned char esc_w[] = {
            0x1b, 0x57,
            (unsigned char)(xL & 0xFF), (unsigned char)((xL >> 8) & 0xFF),
            (unsigned char)(yL & 0xFF), (unsigned char)((yL >> 8) & 0xFF),
            (unsigned char)(dxL & 0xFF), (unsigned char)((dxL >> 8) & 0xFF),
            (unsigned char)(dyL & 0xFF), (unsigned char)((dyL >> 8) & 0xFF)
        };
        fwrite(esc_w, 1, 10, f);

        // 4. Send Raster Graphics (GS v 0)
        int bytes_per_line = (width + 7) / 8;
        unsigned char gs_v_0[] = {
            0x1d, 0x76, 0x30, 0x00, 
            (unsigned char)(bytes_per_line & 0xFF), (unsigned char)((bytes_per_line >> 8) & 0xFF),
            (unsigned char)(current_chunk_height & 0xFF), (unsigned char)((current_chunk_height >> 8) & 0xFF)
        };
        fwrite(gs_v_0, 1, 8, f);

        // 5. Pixel Data Loop (offset dynamically by current_y_offset)
        for (unsigned y = 0; y < current_chunk_height; y++) {
            unsigned absolute_y = current_y_offset + y;
            for (unsigned x = 0; x < bytes_per_line; x++) {
                unsigned char byte = 0;
                for (unsigned bit = 0; bit < 8; bit++) {
                    unsigned px = x * 8 + bit;
                    if (px < width) {
                        unsigned char r = image[4 * width * absolute_y + 4 * px + 0];
                        unsigned char g = image[4 * width * absolute_y + 4 * px + 1];
                        unsigned char b = image[4 * width * absolute_y + 4 * px + 2];
                        if (((r + g + b) / 3) < 128) {
                            byte |= (1 << (7 - bit));
                        }
                    }
                }
                fputc(byte, f);
            }
        }

        // 6. Print Page (FF) and finish
        unsigned char footer[] = { 0x0c, 0x1d, 0x0c, 0x1d, 0x56, 0x00, 0x12, 0x3f };
        fwrite(footer, 1, sizeof(footer), f);

        // Move to the next chunk
        current_y_offset += current_chunk_height;
    }

    fclose(f);
    free(image);
    return 0;
}
