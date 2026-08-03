#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // REQUIRED FOR sleep()
#include <cups/cups.h>
#include <cups/raster.h>


//#include <stdlib.h>
//#include <string.h>

void dither_stucki_image(const unsigned char *src_image, unsigned char *dst_image, 
                         int width, int height, size_t src_bytes_per_line) 
{
    size_t dst_bytes_per_line = (width + 7) / 8;

    // Allocate error rows ONCE for the entire image session
    float *err_row0 = (float *)calloc(width + 4, sizeof(float));
    float *err_row1 = (float *)calloc(width + 4, sizeof(float));
    float *err_row2 = (float *)calloc(width + 4, sizeof(float));

    if (!err_row0 || !err_row1 || !err_row2) {
        free(err_row0); free(err_row1); free(err_row2);
        return;
    }

    for (int y = 0; y < height; y++) {
        const unsigned char *src_row = src_image + (y * src_bytes_per_line);
        unsigned char *dst_row = dst_image + (y * dst_bytes_per_line);

        for (int x = 0; x < width; x++) {
            int err_idx = x + 2; // Offset by 2 for edge conditions

            float old_val = (float)src_row[x] + err_row0[err_idx];

            // Clamp pixel value
            if (old_val < 0.0f) old_val = 0.0f;
            if (old_val > 255.0f) old_val = 255.0f;

            // Thresholding: 0 = Black, 255 = White
            unsigned char new_val = (old_val < 128.0f) ? 0 : 255;

            // Pack 1-bit output
            if (new_val != 0) { //change polarity of the image
                dst_row[x / 8] |= (1 << (7 - (x % 8)));
            }

            // Calculate error and distribute using Stucki weights
            float error = old_val - (float)new_val;
            float e = error / 42.0f;

            // Current row (+0)
            err_row0[err_idx + 1] += e * 8.0f;
            err_row0[err_idx + 2] += e * 4.0f;

            // Next row (+1)
            err_row1[err_idx - 2] += e * 2.0f;
            err_row1[err_idx - 1] += e * 4.0f;
            err_row1[err_idx]     += e * 8.0f;
            err_row1[err_idx + 1] += e * 4.0f;
            err_row1[err_idx + 2] += e * 2.0f;

            // Row (+2)
            err_row2[err_idx - 2] += e * 1.0f;
            err_row2[err_idx - 1] += e * 2.0f;
            err_row2[err_idx]     += e * 4.0f;
            err_row2[err_idx + 1] += e * 2.0f;
            err_row2[err_idx + 2] += e * 1.0f;
        }

        // Shift error rows down for the next line
        float *tmp = err_row0;
        err_row0 = err_row1;
        err_row1 = err_row2;
        err_row2 = tmp;
        memset(err_row2, 0, (width + 4) * sizeof(float));
    }

    // Free buffers at the end of the full image
    free(err_row0);
    free(err_row1);
    free(err_row2);
}

// Original initialization sequence from png2escpos (completely untouched)
unsigned char init_seq[] = {
    0x1b, 0x3d, 0x01,       // ESC = 0x01  -> Select Peripheral Device (Enable printer)
    0x1d, 0x61, 0x1e,       // GS a 0x1e   -> Enable Automatic Status Back (ASB) for paper/error states
    0x1b, 0x40,             // ESC @       -> Initialize printer (Resets settings to defaults)
    0x12, 0x3d, 0x01,       // DC2 = 0x01  -> Select Primary Printing Device
    0x1d, 0x50, 0x00, 0x00, // GS P 0 0 -> Reset horizontal and vertical motion units to default
    0x1d, 0x45, 0x13, 0x13, 0x28, 0x28,
    0x53, 0x65, 0x69, 0x6b, 0x6f, 0x20, 0x49, 0x6e, 0x73, 0x74, 0x72, 0x75, 
    0x6d, 0x65, 0x6e, 0x74, 0x73, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x29,
    0x1c, 0x43, 0x01,       // FS C 0x01   -> Select Kanji Character Mode / Japanese Code System
    0x1b, 0x74, 0x00,       // ESC t 0x00  -> Select Character Code Table 0 (PC437)
    0x1b, 0x52, 0x00,       // ESC R 0x00  -> Select International Character Set (USA)
    0x12, 0x4b, 0x00, 0x05, // DC2 K       -> Print Area Width (1280 dots)
    0x12, 0x48, 0x5d, 0x0c  // DC2 H       -> Page Length / Max Form Length
};

// Original footer sequence from png2escpos (completely untouched)
unsigned char footer[] = { 0x0c, 0x1d, 0x0c, 0x1d, 0x56, 0x00, 0x12, 0x3f };
// Modified footer: Removes the Form Feed and Tear-Feed instructions
//unsigned char footer[] = { 0x1D, 0x0C, 0x12, 0x3F };

// Your original find_real_height function
unsigned int find_real_height(unsigned char *image_data, unsigned int width, unsigned int height, cups_page_header2_t *header) {
    unsigned int cups_bytes_per_line = header->cupsBytesPerLine;

    for (int y = (int)height - 1; y >= 0; y--) {
        unsigned char *cups_row = image_data + (y * cups_bytes_per_line);

        for (unsigned int px = 0; px < width; px++) {
            if (header->cupsBitsPerColor == 1) {
                unsigned char cups_byte = cups_row[px / 8];
                int shift = 7 - (px % 8);
                int bit_val = (cups_byte >> shift) & 1;

                if (header->cupsColorSpace == CUPS_CSPACE_W) {
                    if (bit_val == 0) return (y + 1); // Found black pixel
                } else {
                    if (bit_val == 1) return (y + 1); // Found black pixel
                }
            } 
            else if (header->cupsBitsPerPixel == 24) {
                unsigned char r = cups_row[px * 3 + 0];
                unsigned char g = cups_row[px * 3 + 1];
                unsigned char b = cups_row[px * 3 + 2];
                if (((r + g + b) / 3) < 128) return (y + 1);
            } 
            else if (header->cupsBitsPerColor == 8) {
                if (cups_row[px] < 128) return (y + 1);
            }
        }
    }
    return 10; // Fallback
}

int main(int argc, char *argv[]) {
    // Disable stdout buffering at the OS level
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 6 || argc > 7) {
        fprintf(stderr, "ERROR: %s job-id user title copies options [file]\n", argv[0]);
        return 1;
    }

    FILE *fp = stdin;
    if (argc == 7) {
        fp = fopen(argv[6], "rb");
        if (!fp) {
            fprintf(stderr, "ERROR: Unable to open print file: %s\n", argv[6]);
            return 1;
        }
    }

    cups_raster_t *ras = cupsRasterOpen(fileno(fp), CUPS_RASTER_READ);
    if (!ras) {
        fprintf(stderr, "ERROR: Unable to open CUPS raster stream\n");
        if (argc == 7) fclose(fp);
        return 1;
    }

    cups_page_header2_t header;
    unsigned int chunk_max_height = 3000;

    while (cupsRasterReadHeader2(ras, &header)) {
        if (header.cupsWidth == 0 || header.cupsHeight == 0) continue;

        unsigned int width = header.cupsWidth;
        unsigned int initial_height = header.cupsHeight;
        unsigned int cups_bytes_per_line = header.cupsBytesPerLine;

// 1. Allocate and ZERO OUT the image buffer using calloc
        unsigned char *image_data = calloc(1, cups_bytes_per_line * initial_height);
        if (!image_data) {
            fprintf(stderr, "ERROR: Out of memory\n");
            break;
        }

        // 2. Safely read raw raster pages from CUPS
        unsigned int actual_rows_read = 0;
        for (unsigned int y = 0; y < initial_height; y++) {
            if (cupsRasterReadPixels(ras, image_data + (y * cups_bytes_per_line), cups_bytes_per_line) == 0) {
                break; // Stream ended early
            }
            actual_rows_read++;
        }

// 2.5  - Added step - Convert 8-bit grayscale to 1-bit in-place using Stucki dithering upfront
        if (header.cupsBitsPerColor == 8) {
            unsigned int packed_bytes_per_line = (width + 7) / 8;
            unsigned char *dithered_data = calloc(1, packed_bytes_per_line * actual_rows_read);

            if (dithered_data) {
                // Process the FULL image in a single pass
                dither_stucki_image(image_data, dithered_data, width, actual_rows_read, cups_bytes_per_line);

                // Replace image buffer with dithered 1-bit data
                free(image_data);
                image_data = dithered_data;

                // Adjust header properties to reflect 1-bit monochrome stream
                cups_bytes_per_line = packed_bytes_per_line;
                header.cupsBytesPerLine = packed_bytes_per_line;
                header.cupsBitsPerColor = 1;
                header.cupsBitsPerPixel = 1;
                header.cupsColorSpace = CUPS_CSPACE_K;
            }
        }

        // 3. Find the real content height up to what we actually read
        unsigned int height = find_real_height(image_data, width, actual_rows_read, &header);

        // Debug output to the CUPS error log
        fprintf(stderr, "DEBUG: rastertoescpos - Rows read: %u, Cropped height: %u\n", 
                actual_rows_read, height);

        unsigned int current_y_offset = 0;
        while (current_y_offset < height) {
            unsigned int current_chunk_height = height - current_y_offset;
            if (current_chunk_height > chunk_max_height) {
                current_chunk_height = chunk_max_height;
            }

            // Write Init Header
            fwrite(init_seq, 1, sizeof(init_seq), stdout);

            // Start Page Mode
            unsigned char page_mode[] = { 0x1b, 0x4c, 0x1b, 0x54, 0x00 };
            fwrite(page_mode, 1, 5, stdout);

            // Define Print Area (ESC W)
            int start_x_pixels = 0;
            if (width < 1280) {  
                start_x_pixels = (1280 - width) / 2;
            }
            int start_y_pixels = 0;
            
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
            fwrite(esc_w, 1, 10, stdout);

            // Send Raster Graphics command header (GS v 0)
            int bytes_per_line = (width + 7) / 8;
            unsigned char gs_v_0[] = {
                0x1d, 0x76, 0x30, 0x00, 
                (unsigned char)(bytes_per_line & 0xFF), (unsigned char)((bytes_per_line >> 8) & 0xFF),
                (unsigned char)(current_chunk_height & 0xFF), (unsigned char)((current_chunk_height >> 8) & 0xFF)
            };
            fwrite(gs_v_0, 1, 8, stdout);

            // Send exactly the number of bytes specified by gs_v_0
            for (unsigned int y = 0; y < current_chunk_height; y++) {
                unsigned int absolute_y = current_y_offset + y;
                unsigned char *cups_row = image_data + (absolute_y * cups_bytes_per_line);

                for (int x = 0; x < bytes_per_line; x++) {
                    unsigned char byte = 0;
                    for (int bit = 0; bit < 8; bit++) {
                        int px = x * 8 + bit;
                        
                        if (px < (int)width) {
                            if (header.cupsBitsPerColor == 1) {
                                unsigned char cups_byte = cups_row[px / 8];
                                int shift = 7 - (px % 8);
                                int bit_val = (cups_byte >> shift) & 1;

                                if (header.cupsColorSpace == CUPS_CSPACE_W) {
                                    if (bit_val == 0) byte |= (1 << (7 - bit));
                                } else {
                                    if (bit_val == 1) byte |= (1 << (7 - bit));
                                }
                            } 
                            else if (header.cupsBitsPerPixel == 24 || header.cupsBitsPerPixel == 32) {
                                int bytes_per_pixel = header.cupsBitsPerPixel / 8;
                                unsigned char r = cups_row[px * bytes_per_pixel + 0];
                                unsigned char g = cups_row[px * bytes_per_pixel + 1];
                                unsigned char b = cups_row[px * bytes_per_pixel + 2];
                                if (((r + g + b) / 3) < 128) {
                                    byte |= (1 << (7 - bit));
                                }
                            } 
                            else if (header.cupsBitsPerColor == 8) {
                                if (cups_row[px] < 128) {
                                    byte |= (1 << (7 - bit));
                                }
                            }
                        }
                    }
                    fputc(byte, stdout); 
                }
            }

            // Write Original Footer
            fwrite(footer, 1, sizeof(footer), stdout);
            fflush(stdout); 

            current_y_offset += current_chunk_height;
        }

        free(image_data);
    }

    cupsRasterClose(ras);
    sleep(5);
    if (argc == 7) fclose(fp);
    return 0;
}
