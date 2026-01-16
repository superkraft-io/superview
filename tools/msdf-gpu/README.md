# MSDF-GPU

GPU-accelerated Signed Distance Field font atlas generator for the Skene browser engine.

## Overview

Uses OpenGL 4.3 compute shaders to generate SDF font atlases significantly faster than CPU-based generation.

## Configuration

Current tuned settings in `main.cpp`:

| Setting | Value | Description |
|---------|-------|-------------|
| `ATLAS_WIDTH` | 2048 | Atlas texture width in pixels |
| `ATLAS_HEIGHT` | 2048 | Atlas texture height in pixels |
| `GLYPH_SIZE` | 80.0 | Size of each glyph in the atlas (larger = sharper) |
| `PIXEL_RANGE` | 8.0 | SDF distance range in pixels |
| `GLYPH_PADDING` | 8 | Padding around each glyph |

## Shader Edge Rendering

The main app's fragment shader uses `smoothstep(-0.4, 0.4, screenPxDistance)` for balanced crisp-but-smooth text edges.

Adjusting the smoothstep range:
- **Tighter range** (e.g., `-0.2, 0.2`): Sharper edges, may look jagged on small text
- **Wider range** (e.g., `-0.5, 0.5`): Softer edges, smoother but less crisp
- **Current** (`-0.4, 0.4`): Balanced for both large headers and small body text

## Building

```bash
cd tools/msdf-gpu
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Usage

```bash
# Single font
msdf-gpu <font_path> <output_cache_dir>

# Batch mode
msdf-gpu --batch <font_list_file> <output_cache_dir>
```

## Cache Format

Cache files use the `.msdf` extension with:
- Magic: `0x4D534446` ("MSDF")
- Version: 4
- Filename hash: FNV-1a

## Dependencies

- SDL2 (fetched via CMake)
- OpenGL 4.3+ with compute shader support
- stb_truetype (included)
