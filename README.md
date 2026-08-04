# Seiko L465 Thermal Printer Driver (CUPS 2 / ESC/POS)

A custom CUPS 2 raster filter and driver for the **Seiko Instruments Inc. (SII) MPU-L465** thermal printer.

Despite limited native Linux support for this model, this driver enables reliable printing across various paper roll widths at 300 DPI resolution. The driver is written in C and includes dynamic page cropping, stucki error-diffusion dithering for 8-bit grayscale input, and multi-chunk page processing.

---

## Features

* **300 DPI Resolution:** Maps up to 1280 printable pixels across a 115mm max paper width.
* **Flexible Paper Widths:** Native support for media sizes ranging from 50mm to 115mm (416px up to 1280px wide).
* **Built-in Dithering:** Converts 8-bit grayscale raster inputs into 1-bit monochrome using Stucki error diffusion[.
* **Auto-Cropping:** Detects trailing white space on pages (`find_real_height`) to prevent unnecessary blank paper feeds.
* **Custom Driver Options:** Includes sample standalone conversion tools (such as `png2escpos` using `lodepng`) alongside the CUPS raster filter (`rastertoescpos`).

---

## Technical Specifications

| Parameter | Specification |
| :--- | :--- |
| **Manufacturer** | Seiko Instruments Inc. (SII) |
| **Model** | MPU-L465 |
| **Resolution** | 300 DPI (12 dots/mm) |
| **Max Print Width** | 1280 pixels (approx. 115 mm) |
| **Supported Width Range**| 50 mm – 115 mm |
| **Input Format** | `application/vnd.cups-raster` |
| **Filter Binary** | `rastertoescpos` |
| **License** | GPL-2.0+ |

---

## Media / Paper Sizes

The printer definition file (`seikol465.drv` / PPD) pre-defines the following paper roll layouts with physical zero margins:

* `Roll57mm`: 57mm Roll (416px max width)
* `Roll80mmSafe`: 80mm Roll (Safe 904px max width)
* `Roll80mmFull`: 80mm Roll (Full 944px max width)
* `Roll96mm`: 96mm Roll (1120px max width)
* `Roll102mm`: 102mm Roll (1152px max width)
* `Roll105mm`: 105mm Roll (1184px max width)
* `Roll110mm`: 110mm Roll (1224px max width)
* `Roll112mm`: 112mm Roll (1248px max width)
* `Roll115mm`: 115mm Roll (1280px max width) *(Default)*

---

## Known Issues & Help Wanted

> ⚠️ **Known Bug — Multi-Chunk Gap (2–3mm)** - FIXED!!
> 
> **Contributions and pull requests to help solve bugs are highly welcome!**

---

## Roadmap & Contributing

Contributions are welcome! Specifically looking for help in the following areas:

* [X] **Fixing the multi-chunk paper gap bug** on long documents. - DONE
* [ ] **Printer-App / IPP Everywhere Migration:** Converting this driver from a legacy CUPS filter to a modern **Printer Application** (using `libprinterdriver` or `PAPPL`) for IPP Everywhere compatibility.
* [ ] Refining hardware-level ESC/POS / DC2 initialization flags for specific hardware revisions.

Feel free to open an issue or submit a Pull Request!

---

## Building and Installation

### Prerequisites

Make sure you have CUPS development libraries installed:

```bash
# Debian / Ubuntu
sudo apt install build-essential libcups2-dev libcupsimage-dev cups
make
```

## Auxiliary Tool: `png2escpos`

In addition to the CUPS filter, this repository includes `png2escpos`—a lightweight standalone C tool that allows you to directly convert standard PNG images into Seiko-compatible ESC/POS raw command payloads without needing a running CUPS daemon[cite: 2].

### Features
* Decodes PNG files directly via `lodepng`.
* Converts 32-bit RGBA pixel data to 1-bit monochrome raster graphics (`GS v 0`).
* Automatically centers images under 1280 pixels wide.
* Prepends the full Seiko-specific NV memory handshake and hardware initialization sequences.

### Usage
```bash
# Build the tool
gcc -O2 -o png2escpos png2escpos.c lodepng.c

# Convert a PNG image to a raw ESC/POS binary stream
./png2escpos input.png output.bin

# Send directly to the printer device (e.g., USB)
cat output.bin > /dev/usb/lp0
```

## License

This project is licensed under the GNU General Public License v2.0 or later (GPL-2.0-or-later)

