# 🏗 TIFF to Pyramid Generator Script

This Bash script uses [libvips](https://libvips.github.io/libvips/) and related tools to generate a **multi-resolution pyramidal TIFF** from a source image. It supports color space conversion to sRGB, removes alpha channels, handles 16-bit grayscale images, and embeds metadata for public-resolution restriction.

Ideal for use with zoomable image viewers or IIIF-style tiling workflows.

---

## ✨ Features

- Converts 16-bit grayscale to 8-bit with accurate scaling
- Removes alpha transparency from RGBA inputs
- Converts and embeds sRGB ICC profiles
- Builds a pyramidal TIFF with JPEG-compressed tiles
- Optional resolution-restricted alternate tiles
- Injects custom metadata for viewer-side enforcement

---

## ✅ Requirements

### Software Dependencies

Make sure the following tools are installed on your system:

| Tool       | Purpose                                     | Install Instructions |
|------------|---------------------------------------------|-----------------------|
| **[VIPS](https://libvips.github.io/libvips/)** | Fast, memory-efficient image operations | See below |
| **ImageMagick** (`identify`) | Extract color depth and ICC profile info | See below |
| **tiffcp** (from libtiff) | Combines tiles into output TIFFs | See below |

### 📦 Installation Instructions

#### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y libvips-tools imagemagick libtiff-tools
```

#### macOS (with Homebrew)

```bash
brew install vips imagemagick libtiff
```

#### RHEL / CentOS / Rocky / AlmaLinux

You may need **EPEL** enabled for `libvips`.

```bash
sudo dnf install epel-release
sudo dnf install vips vips-tools ImageMagick libtiff-tools
```

> If `libtiff-tools` isn't available, `tiffcp` may be in the `libtiff` package.

---

## 🖼 ICC Profile

This script expects a standard sRGB ICC profile at:

```
/usr/local/nga/etc/sRGBProfile.icc
```

You can download one from:

- [ICC sRGB Profiles](https://www.color.org/srgbprofiles.xalter)
- Direct link: [sRGB_IEC61966-2-1_black_scaled.icc](https://www.color.org/srgbprofiles/sRGB_IEC61966-2-1_black_scaled.icc)

Place the file accordingly or adjust the script path if needed.

---

## 🚀 Usage

```bash
./tiff_to_pyramid.bash <tmpdir> <input.tif> <output_pyramid.tif> <max_pixels|'none'> [savefiles]
```

### Arguments

| Parameter             | Description                                                                        |
|-----------------------|------------------------------------------------------------------------------------|
| `<tmpdir>`            | Temporary directory (must exist and be writable)                                   |
| `<input.tif>`         | Full path to the source TIFF image                                                 |
| `<output_pyramid.tif>`| Full path to write the pyramidal TIFF output                                       |
| `<max_pixels>`        | Create a smaller pyramid after the normal one - requires IIIF Image Server support |
| `[savefiles]`         | Optional: any non-empty string preserves intermediate files for debugging          |

---

## ↻ Example

```bash
./tiff_to_pyramid.bash /tmp /data/image_input.tif /data/output_pyramid.tif 400
```

This will create a JPEG-compressed pyramidal TIFF with restricted resolution tiles for 400px max dimension, using `/tmp` for working files.

---

## 🪃 Cleanup

Temporary files are deleted automatically unless you pass a fifth argument (`savefiles`). This is useful for debugging intermediate steps.

---

## 🔭 Notes

- Alpha channels are removed to ensure viewer compatibility
- If the image lacks a color profile, sRGB is assumed and embedded
- Uses `vips icc_transform`, `vips copy`, and `vips tiffsave` to enforce standards
- Final pyramidal TIFF is optimized for zoom-based delivery systems

---

## 🧠 To Do

- Make ICC path configurable via ENV or flag
- Add optional IIIF manifest generation
- Dockerize for portable deployment

---

## 🧑‍💻 Author

Built and maintained by the [NGA](https://www.nga.gov) enterprise platforms team. Contributions welcome!

