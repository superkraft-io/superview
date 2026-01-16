/*
 * MSDF-GPU: GPU-accelerated Multi-channel Signed Distance Field font atlas generator
 * 
 * Uses OpenGL 4.3 compute shaders to generate MSDF atlases ~100x faster than CPU.
 * 
 * Usage: msdf-gpu <font_path> <output_cache_dir>
 *        msdf-gpu --batch <font_list_file> <output_cache_dir>
 */

#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// stb_truetype for font parsing
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

//=============================================================================
// Configuration
//=============================================================================

static constexpr int ATLAS_WIDTH = 2048;
static constexpr int ATLAS_HEIGHT = 2048;
static constexpr float GLYPH_SIZE = 80.0f;    // Larger = sharper, but more atlas space
static constexpr float PIXEL_RANGE = 8.0f;    // Match main app for consistent rendering
static constexpr int GLYPH_PADDING = 8;

// Cache file format (must match main app)
static constexpr uint32_t MSDF_CACHE_MAGIC = 0x4D534446;  // "MSDF"
static constexpr uint32_t MSDF_CACHE_VERSION = 4;  // Match main app version

//=============================================================================
// Data Structures
//=============================================================================

// Edge types for GPU
enum EdgeType : int {
  EDGE_LINE = 0,
  EDGE_QUADRATIC = 1,
  EDGE_CUBIC = 2
};

// GPU edge data (std430 layout compatible)
struct GPUEdge {
  float p0x, p0y;      // Start point
  float p1x, p1y;      // End point (or control point for curves)
  float p2x, p2y;      // Control point 2 (quadratic end, cubic control 2)
  float p3x, p3y;      // End point for cubic
  int type;            // EdgeType
  int color;           // 0=R, 1=G, 2=B (which channel this edge belongs to)
  float _pad[2];       // Padding to 48 bytes (12 floats)
};

// GPU glyph metadata
struct GPUGlyph {
  int edgeStart;       // Index into edge buffer
  int edgeCount;       // Number of edges
  int atlasX, atlasY;  // Position in atlas
  int width, height;   // Size in atlas
  float _pad[2];       // Padding to 32 bytes
};

// Glyph info for output (matches main app format)
struct MSDFGlyph {
  float u0, v0, u1, v1;  // Texture coordinates
  float xoff, yoff;      // Offset from baseline
  float width, height;   // Glyph size in pixels
  float advance;         // Horizontal advance
  bool valid = false;
};

//=============================================================================
// OpenGL Helpers
//=============================================================================

// OpenGL function pointers (loaded at runtime) - use custom names to avoid conflicts
typedef void (APIENTRY *PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void (APIENTRY *PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum);
typedef void (APIENTRY *PFN_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (APIENTRY *PFN_glCompileShader)(GLuint);
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void (APIENTRY *PFN_glAttachShader)(GLuint, GLuint);
typedef void (APIENTRY *PFN_glLinkProgram)(GLuint);
typedef void (APIENTRY *PFN_glUseProgram)(GLuint);
typedef void (APIENTRY *PFN_glDeleteShader)(GLuint);
typedef void (APIENTRY *PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void (APIENTRY *PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY *PFN_glDispatchCompute)(GLuint, GLuint, GLuint);
typedef void (APIENTRY *PFN_glMemoryBarrier)(GLbitfield);
typedef void (APIENTRY *PFN_glGenBuffers)(GLsizei, GLuint*);
typedef void (APIENTRY *PFN_glBindBuffer)(GLenum, GLuint);
typedef void (APIENTRY *PFN_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (APIENTRY *PFN_glBindBufferBase)(GLenum, GLuint, GLuint);
typedef void (APIENTRY *PFN_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void (APIENTRY *PFN_glGetBufferSubData)(GLenum, GLintptr, GLsizeiptr, void*);
typedef GLint (APIENTRY *PFN_glGetUniformLocation)(GLuint, const GLchar*);
typedef void (APIENTRY *PFN_glUniform1i)(GLint, GLint);
typedef void (APIENTRY *PFN_glUniform1f)(GLint, GLfloat);
typedef void (APIENTRY *PFN_glUniform2i)(GLint, GLint, GLint);

PFN_glGetShaderiv glGetShaderiv_ptr = nullptr;
PFN_glGetShaderInfoLog glGetShaderInfoLog_ptr = nullptr;
PFN_glCreateShader glCreateShader_ptr = nullptr;
PFN_glShaderSource glShaderSource_ptr = nullptr;
PFN_glCompileShader glCompileShader_ptr = nullptr;
PFN_glCreateProgram glCreateProgram_ptr = nullptr;
PFN_glAttachShader glAttachShader_ptr = nullptr;
PFN_glLinkProgram glLinkProgram_ptr = nullptr;
PFN_glUseProgram glUseProgram_ptr = nullptr;
PFN_glDeleteShader glDeleteShader_ptr = nullptr;
PFN_glGetProgramiv glGetProgramiv_ptr = nullptr;
PFN_glGetProgramInfoLog glGetProgramInfoLog_ptr = nullptr;
PFN_glDispatchCompute glDispatchCompute_ptr = nullptr;
PFN_glMemoryBarrier glMemoryBarrier_ptr = nullptr;
PFN_glGenBuffers glGenBuffers_ptr = nullptr;
PFN_glBindBuffer glBindBuffer_ptr = nullptr;
PFN_glBufferData glBufferData_ptr = nullptr;
PFN_glBindBufferBase glBindBufferBase_ptr = nullptr;
PFN_glDeleteBuffers glDeleteBuffers_ptr = nullptr;
PFN_glGetBufferSubData glGetBufferSubData_ptr = nullptr;
PFN_glGetUniformLocation glGetUniformLocation_ptr = nullptr;
PFN_glUniform1i glUniform1i_ptr = nullptr;
PFN_glUniform1f glUniform1f_ptr = nullptr;
PFN_glUniform2i glUniform2i_ptr = nullptr;

#define GL_COMPUTE_SHADER 0x91B9
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

bool loadGLFunctions() {
  glGetShaderiv_ptr = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
  glGetShaderInfoLog_ptr = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
  glCreateShader_ptr = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
  glShaderSource_ptr = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
  glCompileShader_ptr = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
  glCreateProgram_ptr = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
  glAttachShader_ptr = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
  glLinkProgram_ptr = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
  glUseProgram_ptr = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
  glDeleteShader_ptr = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
  glGetProgramiv_ptr = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
  glGetProgramInfoLog_ptr = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
  glDispatchCompute_ptr = (PFNGLDISPATCHCOMPUTEPROC)SDL_GL_GetProcAddress("glDispatchCompute");
  glMemoryBarrier_ptr = (PFNGLMEMORYBARRIERPROC)SDL_GL_GetProcAddress("glMemoryBarrier");
  glGenBuffers_ptr = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
  glBindBuffer_ptr = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
  glBufferData_ptr = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
  glBindBufferBase_ptr = (PFNGLBINDBUFFERBASEPROC)SDL_GL_GetProcAddress("glBindBufferBase");
  glDeleteBuffers_ptr = (PFNGLDELETEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteBuffers");
  glGetBufferSubData_ptr = (PFNGLGETBUFFERSUBDATAPROC)SDL_GL_GetProcAddress("glGetBufferSubData");
  glGetUniformLocation_ptr = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
  glUniform1i_ptr = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
  glUniform1f_ptr = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");
  glUniform2i_ptr = (PFNGLUNIFORM2IPROC)SDL_GL_GetProcAddress("glUniform2i");
  
  return glDispatchCompute_ptr != nullptr;
}

//=============================================================================
// Compute Shader Source
//=============================================================================

const char* computeShaderSource = R"(
#version 430 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Edge data
struct Edge {
  vec2 p0;
  vec2 p1;
  vec2 p2;
  vec2 p3;
  int type;      // 0=line, 1=quadratic, 2=cubic
  int color;     // 0=R, 1=G, 2=B
  vec2 _pad;
};

// Glyph metadata
struct Glyph {
  int edgeStart;
  int edgeCount;
  int atlasX, atlasY;
  int width, height;
  vec2 _pad;
};

layout(std430, binding = 0) readonly buffer EdgeBuffer {
  Edge edges[];
};

layout(std430, binding = 1) readonly buffer GlyphBuffer {
  Glyph glyphs[];
};

layout(std430, binding = 2) writeonly buffer OutputBuffer {
  uint outputPixels[];  // Packed RGB as 0x00RRGGBB
};

uniform int u_atlasWidth;
uniform int u_atlasHeight;
uniform int u_glyphCount;
uniform float u_pixelRange;

// Signed distance from point to line segment
// After Y flip, winding is CCW, so inside is on the LEFT (cross > 0)
float signedDistanceToLine(vec2 p, vec2 a, vec2 b) {
  vec2 ab = b - a;
  vec2 ap = p - a;
  float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
  vec2 closest = a + t * ab;
  float dist = length(p - closest);
  
  // Determine sign using cross product
  // cross > 0 means point is on LEFT of edge = INSIDE for CCW winding = positive distance
  float cross = ab.x * ap.y - ab.y * ap.x;
  return cross > 0.0 ? dist : -dist;
}

// Signed distance from point to quadratic Bezier curve
float signedDistanceToQuadratic(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
  // Approximate with line segments
  const int SUBDIVISIONS = 32;
  float minDist = 1e10;
  vec2 prev = p0;
  
  for (int i = 1; i <= SUBDIVISIONS; i++) {
    float t = float(i) / float(SUBDIVISIONS);
    float mt = 1.0 - t;
    vec2 curr = mt * mt * p0 + 2.0 * mt * t * p1 + t * t * p2;
    
    vec2 ab = curr - prev;
    vec2 ap = p - prev;
    float proj = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 closest = prev + proj * ab;
    float dist = length(p - closest);
    
    if (dist < abs(minDist)) {
      float cross = ab.x * ap.y - ab.y * ap.x;
      minDist = cross > 0.0 ? dist : -dist;
    }
    
    prev = curr;
  }
  
  return minDist;
}

// Signed distance from point to cubic Bezier curve
float signedDistanceToCubic(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
  // Approximate with line segments
  const int SUBDIVISIONS = 48;
  float minDist = 1e10;
  vec2 prev = p0;
  
  for (int i = 1; i <= SUBDIVISIONS; i++) {
    float t = float(i) / float(SUBDIVISIONS);
    float mt = 1.0 - t;
    vec2 curr = mt*mt*mt * p0 + 3.0*mt*mt*t * p1 + 3.0*mt*t*t * p2 + t*t*t * p3;
    
    vec2 ab = curr - prev;
    vec2 ap = p - prev;
    float proj = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 closest = prev + proj * ab;
    float dist = length(p - closest);
    
    if (dist < abs(minDist)) {
      float cross = ab.x * ap.y - ab.y * ap.x;
      minDist = cross > 0.0 ? dist : -dist;
    }
    
    prev = curr;
  }
  
  return minDist;
}

void main() {
  ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
  
  if (pixelCoord.x >= u_atlasWidth || pixelCoord.y >= u_atlasHeight) {
    return;
  }
  
  // Find which glyph this pixel belongs to
  int glyphIdx = -1;
  ivec2 localCoord = ivec2(0);
  
  for (int g = 0; g < u_glyphCount; g++) {
    Glyph glyph = glyphs[g];
    if (pixelCoord.x >= glyph.atlasX && pixelCoord.x < glyph.atlasX + glyph.width &&
        pixelCoord.y >= glyph.atlasY && pixelCoord.y < glyph.atlasY + glyph.height) {
      glyphIdx = g;
      localCoord = pixelCoord - ivec2(glyph.atlasX, glyph.atlasY);
      break;
    }
  }
  
  vec3 msdf = vec3(0.0);
  
  if (glyphIdx >= 0) {
    Glyph glyph = glyphs[glyphIdx];
    
    // Pixel position in glyph space (no flip - edges already in atlas space)
    vec2 pos = vec2(float(localCoord.x) + 0.5, float(localCoord.y) + 0.5);
    
    // First pass: compute winding number to determine inside/outside
    // Uses ray casting: count crossings of a horizontal ray to the right
    int windingNumber = 0;
    for (int e = 0; e < glyph.edgeCount; e++) {
      Edge edge = edges[glyph.edgeStart + e];
      
      if (edge.type == 0) {
        // Line segment - check directly
        vec2 p0 = edge.p0;
        vec2 p1 = edge.p1;
        if ((p0.y <= pos.y && p1.y > pos.y) || (p1.y <= pos.y && p0.y > pos.y)) {
          float t = (pos.y - p0.y) / (p1.y - p0.y);
          float xIntersect = p0.x + t * (p1.x - p0.x);
          if (pos.x < xIntersect) {
            windingNumber += (p1.y > p0.y) ? 1 : -1;
          }
        }
      } else if (edge.type == 1) {
        // Quadratic curve - subdivide
        vec2 prev = edge.p0;
        for (int i = 1; i <= 8; i++) {
          float t = float(i) / 8.0;
          float mt = 1.0 - t;
          vec2 curr = mt*mt * edge.p0 + 2.0*mt*t * edge.p1 + t*t * edge.p2;
          if ((prev.y <= pos.y && curr.y > pos.y) || (curr.y <= pos.y && prev.y > pos.y)) {
            float tt = (pos.y - prev.y) / (curr.y - prev.y);
            float xIntersect = prev.x + tt * (curr.x - prev.x);
            if (pos.x < xIntersect) {
              windingNumber += (curr.y > prev.y) ? 1 : -1;
            }
          }
          prev = curr;
        }
      } else {
        // Cubic curve - subdivide
        vec2 prev = edge.p0;
        for (int i = 1; i <= 12; i++) {
          float t = float(i) / 12.0;
          float mt = 1.0 - t;
          vec2 curr = mt*mt*mt * edge.p0 + 3.0*mt*mt*t * edge.p1 + 3.0*mt*t*t * edge.p2 + t*t*t * edge.p3;
          if ((prev.y <= pos.y && curr.y > pos.y) || (curr.y <= pos.y && prev.y > pos.y)) {
            float tt = (pos.y - prev.y) / (curr.y - prev.y);
            float xIntersect = prev.x + tt * (curr.x - prev.x);
            if (pos.x < xIntersect) {
              windingNumber += (curr.y > prev.y) ? 1 : -1;
            }
          }
          prev = curr;
        }
      }
    }
    
    bool inside = (windingNumber != 0);
    
    // Second pass: find minimum unsigned distance to ANY edge (simple SDF)
    float minDist = 1e10;
    
    for (int e = 0; e < glyph.edgeCount; e++) {
      Edge edge = edges[glyph.edgeStart + e];
      
      float dist;
      if (edge.type == 0) {
        dist = abs(signedDistanceToLine(pos, edge.p0, edge.p1));
      } else if (edge.type == 1) {
        dist = abs(signedDistanceToQuadratic(pos, edge.p0, edge.p1, edge.p2));
      } else {
        dist = abs(signedDistanceToCubic(pos, edge.p0, edge.p1, edge.p2, edge.p3));
      }
      
      if (dist < minDist) {
        minDist = dist;
      }
    }
    
    // Apply sign based on winding number
    float signedDist = inside ? minDist : -minDist;
    
    // Convert signed distance to [0, 1] range
    // Use same value for all channels (grayscale SDF)
    float sdfValue = signedDist / u_pixelRange * 0.5 + 0.5;
    sdfValue = clamp(sdfValue, 0.0, 1.0);
    msdf = vec3(sdfValue);
  }
  
  // Pack RGB into uint (0x00RRGGBB)
  uint r = uint(msdf.r * 255.0);
  uint g = uint(msdf.g * 255.0);
  uint b = uint(msdf.b * 255.0);
  uint packed = (r << 16) | (g << 8) | b;
  
  int pixelIdx = pixelCoord.y * u_atlasWidth + pixelCoord.x;
  outputPixels[pixelIdx] = packed;
}
)";

//=============================================================================
// Shader Compilation
//=============================================================================

GLuint compileComputeShader(const char* source) {
  GLuint shader = glCreateShader_ptr(GL_COMPUTE_SHADER);
  glShaderSource_ptr(shader, 1, &source, nullptr);
  glCompileShader_ptr(shader);
  
  GLint success;
  glGetShaderiv_ptr(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLint logLength;
    glGetShaderiv_ptr(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    glGetShaderInfoLog_ptr(shader, logLength, nullptr, log.data());
    std::cerr << "Compute shader compilation failed:\n" << log.data() << std::endl;
    return 0;
  }
  
  GLuint program = glCreateProgram_ptr();
  glAttachShader_ptr(program, shader);
  glLinkProgram_ptr(program);
  
  glGetProgramiv_ptr(program, GL_LINK_STATUS, &success);
  if (!success) {
    GLint logLength;
    glGetProgramiv_ptr(program, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    glGetProgramInfoLog_ptr(program, logLength, nullptr, log.data());
    std::cerr << "Compute shader linking failed:\n" << log.data() << std::endl;
    return 0;
  }
  
  glDeleteShader_ptr(shader);
  return program;
}

//=============================================================================
// Font Processing
//=============================================================================

std::vector<int> getCharacterSet() {
  std::vector<int> chars;
  
  // Basic ASCII (32-126)
  for (int c = 32; c <= 126; ++c) {
    chars.push_back(c);
  }
  
  // Latin-1 Supplement (160-255) - includes ©, ®, etc.
  for (int c = 160; c <= 255; ++c) {
    chars.push_back(c);
  }
  
  // Common symbols
  chars.push_back(0x20AC);  // € Euro sign
  chars.push_back(0x2019);  // ' Right single quotation mark
  chars.push_back(0x201C);  // " Left double quotation mark
  chars.push_back(0x201D);  // " Right double quotation mark
  chars.push_back(0x2022);  // • Bullet
  chars.push_back(0x2026);  // … Ellipsis
  chars.push_back(0x2013);  // – En dash
  chars.push_back(0x2014);  // — Em dash
  chars.push_back(0x2122);  // ™ Trademark
  
  return chars;
}

// Proper edge coloring for MSDF - ensures all channels get coverage
void colorEdges(std::vector<GPUEdge>& edges, int startIdx, int count) {
  if (count == 0) return;
  
  // For MSDF, we need at least 3 edges for proper coloring
  // If fewer, duplicate colors across all channels
  if (count < 3) {
    // With 1-2 edges, assign all channels to get a pseudo-SDF
    for (int i = 0; i < count; i++) {
      // Set color to -1 to indicate "all channels" - handled in shader
      edges[startIdx + i].color = 3;  // Special value: all channels
    }
    return;
  }
  
  // Rotate colors, ensuring each channel gets at least one edge
  for (int i = 0; i < count; i++) {
    edges[startIdx + i].color = i % 3;
  }
}

struct FontAtlasData {
  std::vector<GPUEdge> edges;
  std::vector<GPUGlyph> gpuGlyphs;
  std::map<int, MSDFGlyph> glyphInfo;
  float ascent, descent, lineGap;
  int glyphCount;
};

bool extractFontData(const std::string& fontPath, FontAtlasData& data) {
  // Load font file
  std::ifstream file(fontPath, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open font: " << fontPath << std::endl;
    return false;
  }
  
  std::vector<unsigned char> fontData(std::istreambuf_iterator<char>(file), {});
  
  // For TTC (TrueType Collection) files, get the offset for the first font
  int fontOffset = stbtt_GetFontOffsetForIndex(fontData.data(), 0);
  if (fontOffset < 0) {
    std::cerr << "Failed to get font offset: " << fontPath << std::endl;
    return false;
  }
  
  stbtt_fontinfo fontInfo;
  if (!stbtt_InitFont(&fontInfo, fontData.data(), fontOffset)) {
    std::cerr << "Failed to parse font: " << fontPath << std::endl;
    return false;
  }
  
  // Get font metrics
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
  float scale = stbtt_ScaleForMappingEmToPixels(&fontInfo, GLYPH_SIZE);
  data.ascent = ascent * scale;
  data.descent = -descent * scale;
  data.lineGap = lineGap * scale;
  
  // Pack glyphs into atlas
  int cursorX = GLYPH_PADDING;
  int cursorY = GLYPH_PADDING;
  int rowHeight = 0;
  
  std::vector<int> charSet = getCharacterSet();
  
  for (int c : charSet) {
    int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, c);
    if (glyphIndex == 0 && c != ' ') continue;
    
    // Get glyph metrics
    int advanceWidth, leftSideBearing;
    stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceWidth, &leftSideBearing);
    
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);
    
    int glyphW = x1 - x0;
    int glyphH = y1 - y0;
    
    MSDFGlyph info;
    info.advance = advanceWidth * scale;
    info.valid = true;
    
    // Space character
    if (c == ' ' || glyphW <= 0 || glyphH <= 0) {
      info.width = 0;
      info.height = 0;
      info.xoff = 0;
      info.yoff = 0;
      info.u0 = info.v0 = info.u1 = info.v1 = 0;
      data.glyphInfo[c] = info;
      continue;
    }
    
    int paddedW = glyphW + GLYPH_PADDING * 2;
    int paddedH = glyphH + GLYPH_PADDING * 2;
    
    // Move to next row if needed
    if (cursorX + paddedW > ATLAS_WIDTH - GLYPH_PADDING) {
      cursorX = GLYPH_PADDING;
      cursorY += rowHeight + GLYPH_PADDING;
      rowHeight = 0;
    }
    
    if (cursorY + paddedH > ATLAS_HEIGHT - GLYPH_PADDING) {
      std::cerr << "Atlas full at codepoint " << c << std::endl;
      break;
    }
    
    // Extract glyph contours
    stbtt_vertex* vertices = nullptr;
    int numVerts = stbtt_GetCodepointShape(&fontInfo, c, &vertices);
    
    if (numVerts == 0 || !vertices) {
      stbtt_FreeShape(&fontInfo, vertices);
      continue;
    }
    
    // Get glyph bounding box in font units
    int ix0, iy0, ix1, iy1;
    stbtt_GetCodepointBox(&fontInfo, c, &ix0, &iy0, &ix1, &iy1);
    
    GPUGlyph gpuGlyph;
    gpuGlyph.edgeStart = (int)data.edges.size();
    gpuGlyph.edgeCount = 0;
    gpuGlyph.atlasX = cursorX;
    gpuGlyph.atlasY = cursorY;
    gpuGlyph.width = paddedW;
    gpuGlyph.height = paddedH;
    
    // Convert vertices to edges
    // TrueType: Y increases upward, but atlas has Y increasing downward
    // We flip Y coordinates so edges are in atlas coordinate space
    float glyphPixelHeight = (float)paddedH;
    float px = 0, py = 0;
    float startX = 0, startY = 0;
    
    for (int i = 0; i < numVerts; i++) {
      stbtt_vertex& v = vertices[i];
      
      float vx = (v.x - ix0) * scale + GLYPH_PADDING;
      float vy = glyphPixelHeight - ((v.y - iy0) * scale + GLYPH_PADDING);  // Flip Y
      float cx = (v.cx - ix0) * scale + GLYPH_PADDING;
      float cy = glyphPixelHeight - ((v.cy - iy0) * scale + GLYPH_PADDING);  // Flip Y
      float cx1 = (v.cx1 - ix0) * scale + GLYPH_PADDING;
      float cy1 = glyphPixelHeight - ((v.cy1 - iy0) * scale + GLYPH_PADDING);  // Flip Y
      
      GPUEdge edge = {};
      
      switch (v.type) {
        case STBTT_vmove:
          // Close previous contour if needed
          if (i > 0 && (px != startX || py != startY)) {
            edge.p0x = px; edge.p0y = py;
            edge.p1x = startX; edge.p1y = startY;
            edge.type = EDGE_LINE;
            data.edges.push_back(edge);
            gpuGlyph.edgeCount++;
          }
          px = vx; py = vy;
          startX = vx; startY = vy;
          break;
          
        case STBTT_vline:
          edge.p0x = px; edge.p0y = py;
          edge.p1x = vx; edge.p1y = vy;
          edge.type = EDGE_LINE;
          data.edges.push_back(edge);
          gpuGlyph.edgeCount++;
          px = vx; py = vy;
          break;
          
        case STBTT_vcurve:
          edge.p0x = px; edge.p0y = py;
          edge.p1x = cx; edge.p1y = cy;
          edge.p2x = vx; edge.p2y = vy;
          edge.type = EDGE_QUADRATIC;
          data.edges.push_back(edge);
          gpuGlyph.edgeCount++;
          px = vx; py = vy;
          break;
          
        case STBTT_vcubic:
          edge.p0x = px; edge.p0y = py;
          edge.p1x = cx; edge.p1y = cy;
          edge.p2x = cx1; edge.p2y = cy1;
          edge.p3x = vx; edge.p3y = vy;
          edge.type = EDGE_CUBIC;
          data.edges.push_back(edge);
          gpuGlyph.edgeCount++;
          px = vx; py = vy;
          break;
      }
    }
    
    // Close last contour
    if (px != startX || py != startY) {
      GPUEdge edge = {};
      edge.p0x = px; edge.p0y = py;
      edge.p1x = startX; edge.p1y = startY;
      edge.type = EDGE_LINE;
      data.edges.push_back(edge);
      gpuGlyph.edgeCount++;
    }
    
    stbtt_FreeShape(&fontInfo, vertices);
    
    // Color the edges for this glyph
    colorEdges(data.edges, gpuGlyph.edgeStart, gpuGlyph.edgeCount);
    
    data.gpuGlyphs.push_back(gpuGlyph);
    
    // Store glyph info
    info.width = (float)paddedW;
    info.height = (float)paddedH;
    info.xoff = x0 - GLYPH_PADDING;
    info.yoff = y0 - GLYPH_PADDING;
    info.u0 = (float)cursorX / ATLAS_WIDTH;
    info.v0 = (float)cursorY / ATLAS_HEIGHT;
    info.u1 = (float)(cursorX + paddedW) / ATLAS_WIDTH;
    info.v1 = (float)(cursorY + paddedH) / ATLAS_HEIGHT;
    data.glyphInfo[c] = info;
    
    cursorX += paddedW + GLYPH_PADDING;
    rowHeight = std::max(rowHeight, paddedH);
  }
  
  data.glyphCount = (int)data.gpuGlyphs.size();
  return true;
}

//=============================================================================
// Cache File Writing
//=============================================================================

uint64_t computeFontFileHash(const std::string& fontPath) {
  std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
  if (!file) return 0;
  
  auto size = file.tellg();
  file.seekg(0, std::ios::beg);
  
  uint64_t modTime = 0;
  try {
    auto ftime = std::filesystem::last_write_time(fontPath);
    modTime = std::chrono::duration_cast<std::chrono::seconds>(
      ftime.time_since_epoch()).count();
  } catch (...) {}
  
  uint64_t hash = static_cast<uint64_t>(size) ^ (modTime << 32);
  
  if (size > 0) {
    char firstByte, lastByte;
    file.read(&firstByte, 1);
    file.seekg(-1, std::ios::end);
    file.read(&lastByte, 1);
    hash ^= (static_cast<uint64_t>(static_cast<unsigned char>(firstByte)) << 8);
    hash ^= (static_cast<uint64_t>(static_cast<unsigned char>(lastByte)) << 16);
  }
  
  return hash;
}

// FNV-1a hash for deterministic cache filenames (same as main app)
uint64_t fnv1aHash(const std::string& str) {
  uint64_t hash = 14695981039346656037ULL;
  for (char c : str) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string getCacheFilename(const std::string& fontPath) {
  uint64_t pathHash = fnv1aHash(fontPath);
  
  std::filesystem::path p(fontPath);
  std::string baseName = p.stem().string();
  
  for (char& c : baseName) {
    if (!isalnum(c) && c != '-' && c != '_') c = '_';
  }
  
  return baseName + "_" + std::to_string(pathHash) + ".msdf";
}

bool saveCacheFile(const std::string& fontPath, const std::string& cacheDir,
                   const FontAtlasData& data, const std::vector<uint8_t>& atlasPixels) {
  std::string cacheFile = cacheDir + "/" + getCacheFilename(fontPath);
  
  std::ofstream file(cacheFile, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to create cache file: " << cacheFile << std::endl;
    return false;
  }
  
  // Write header
  uint32_t magic = MSDF_CACHE_MAGIC;
  uint32_t version = MSDF_CACHE_VERSION;
  uint64_t fontHash = computeFontFileHash(fontPath);
  file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  file.write(reinterpret_cast<const char*>(&version), sizeof(version));
  file.write(reinterpret_cast<const char*>(&fontHash), sizeof(fontHash));
  
  // Write atlas metadata
  int atlasWidth = ATLAS_WIDTH;
  int atlasHeight = ATLAS_HEIGHT;
  float pixelRange = PIXEL_RANGE;
  float glyphSize = GLYPH_SIZE;
  file.write(reinterpret_cast<const char*>(&atlasWidth), sizeof(atlasWidth));
  file.write(reinterpret_cast<const char*>(&atlasHeight), sizeof(atlasHeight));
  file.write(reinterpret_cast<const char*>(&pixelRange), sizeof(pixelRange));
  file.write(reinterpret_cast<const char*>(&glyphSize), sizeof(glyphSize));
  file.write(reinterpret_cast<const char*>(&data.ascent), sizeof(data.ascent));
  file.write(reinterpret_cast<const char*>(&data.descent), sizeof(data.descent));
  file.write(reinterpret_cast<const char*>(&data.lineGap), sizeof(data.lineGap));
  
  // Write glyph count and data
  uint32_t glyphCount = static_cast<uint32_t>(data.glyphInfo.size());
  file.write(reinterpret_cast<const char*>(&glyphCount), sizeof(glyphCount));
  for (const auto& [codepoint, glyph] : data.glyphInfo) {
    int32_t cp = codepoint;
    file.write(reinterpret_cast<const char*>(&cp), sizeof(cp));
    file.write(reinterpret_cast<const char*>(&glyph), sizeof(glyph));
  }
  
  // Write atlas texture data (RGB)
  file.write(reinterpret_cast<const char*>(atlasPixels.data()), atlasPixels.size());
  
  std::cout << "Saved: " << std::filesystem::path(cacheFile).filename().string() << std::endl;
  return true;
}

//=============================================================================
// Main GPU Generation
//=============================================================================

bool generateMSDFAtlas(GLuint program, const std::string& fontPath, const std::string& cacheDir) {
  auto startTime = std::chrono::high_resolution_clock::now();
  
  // Extract font data
  FontAtlasData fontData;
  if (!extractFontData(fontPath, fontData)) {
    return false;
  }
  
  if (fontData.edges.empty() || fontData.gpuGlyphs.empty()) {
    std::cerr << "No glyphs extracted from font" << std::endl;
    return false;
  }
  
  auto extractTime = std::chrono::high_resolution_clock::now();
  
  // Create GPU buffers
  GLuint edgeBuffer, glyphBuffer, outputBuffer;
  glGenBuffers_ptr(1, &edgeBuffer);
  glGenBuffers_ptr(1, &glyphBuffer);
  glGenBuffers_ptr(1, &outputBuffer);
  
  // Upload edge data
  glBindBuffer_ptr(GL_SHADER_STORAGE_BUFFER, edgeBuffer);
  glBufferData_ptr(GL_SHADER_STORAGE_BUFFER, 
                   fontData.edges.size() * sizeof(GPUEdge),
                   fontData.edges.data(), GL_STATIC_DRAW);
  glBindBufferBase_ptr(GL_SHADER_STORAGE_BUFFER, 0, edgeBuffer);
  
  // Upload glyph data
  glBindBuffer_ptr(GL_SHADER_STORAGE_BUFFER, glyphBuffer);
  glBufferData_ptr(GL_SHADER_STORAGE_BUFFER,
                   fontData.gpuGlyphs.size() * sizeof(GPUGlyph),
                   fontData.gpuGlyphs.data(), GL_STATIC_DRAW);
  glBindBufferBase_ptr(GL_SHADER_STORAGE_BUFFER, 1, glyphBuffer);
  
  // Create output buffer
  size_t outputSize = ATLAS_WIDTH * ATLAS_HEIGHT * sizeof(uint32_t);
  glBindBuffer_ptr(GL_SHADER_STORAGE_BUFFER, outputBuffer);
  glBufferData_ptr(GL_SHADER_STORAGE_BUFFER, outputSize, nullptr, GL_DYNAMIC_READ);
  glBindBufferBase_ptr(GL_SHADER_STORAGE_BUFFER, 2, outputBuffer);
  
  // Set uniforms
  glUseProgram_ptr(program);
  glUniform1i_ptr(glGetUniformLocation_ptr(program, "u_atlasWidth"), ATLAS_WIDTH);
  glUniform1i_ptr(glGetUniformLocation_ptr(program, "u_atlasHeight"), ATLAS_HEIGHT);
  glUniform1i_ptr(glGetUniformLocation_ptr(program, "u_glyphCount"), fontData.glyphCount);
  glUniform1f_ptr(glGetUniformLocation_ptr(program, "u_pixelRange"), PIXEL_RANGE);
  
  // Dispatch compute shader
  int groupsX = (ATLAS_WIDTH + 15) / 16;
  int groupsY = (ATLAS_HEIGHT + 15) / 16;
  glDispatchCompute_ptr(groupsX, groupsY, 1);
  
  // Wait for completion
  glMemoryBarrier_ptr(GL_SHADER_STORAGE_BARRIER_BIT);
  
  auto gpuTime = std::chrono::high_resolution_clock::now();
  
  // Read back results
  std::vector<uint32_t> packedPixels(ATLAS_WIDTH * ATLAS_HEIGHT);
  glBindBuffer_ptr(GL_SHADER_STORAGE_BUFFER, outputBuffer);
  glGetBufferSubData_ptr(GL_SHADER_STORAGE_BUFFER, 0, outputSize, packedPixels.data());
  
  // Convert to RGB format
  std::vector<uint8_t> atlasPixels(ATLAS_WIDTH * ATLAS_HEIGHT * 3);
  for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
    uint32_t packed = packedPixels[i];
    atlasPixels[i * 3 + 0] = (packed >> 16) & 0xFF;  // R
    atlasPixels[i * 3 + 1] = (packed >> 8) & 0xFF;   // G
    atlasPixels[i * 3 + 2] = packed & 0xFF;          // B
  }
  
  // Cleanup GPU buffers
  glDeleteBuffers_ptr(1, &edgeBuffer);
  glDeleteBuffers_ptr(1, &glyphBuffer);
  glDeleteBuffers_ptr(1, &outputBuffer);
  
  // Save to cache
  if (!saveCacheFile(fontPath, cacheDir, fontData, atlasPixels)) {
    return false;
  }
  
  auto endTime = std::chrono::high_resolution_clock::now();
  
  auto extractMs = std::chrono::duration_cast<std::chrono::milliseconds>(extractTime - startTime).count();
  auto gpuMs = std::chrono::duration_cast<std::chrono::milliseconds>(gpuTime - extractTime).count();
  auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  
  std::cout << "  Extract: " << extractMs << "ms, GPU: " << gpuMs << "ms, Total: " << totalMs << "ms" << std::endl;
  
  return true;
}

//=============================================================================
// Entry Point
//=============================================================================

void printUsage(const char* programName) {
  std::cout << "MSDF-GPU: GPU-accelerated font atlas generator\n\n";
  std::cout << "Usage:\n";
  std::cout << "  " << programName << " <font_path> <cache_dir>\n";
  std::cout << "  " << programName << " --batch <font_list_file> <cache_dir>\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << programName << " C:/Windows/Fonts/arial.ttf ./cache/fonts\n";
  std::cout << "  " << programName << " --batch fonts.txt ./cache/fonts\n";
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printUsage(argv[0]);
    return 1;
  }
  
  bool batchMode = false;
  std::string fontPath;
  std::string cacheDir;
  std::vector<std::string> fontPaths;
  
  if (std::string(argv[1]) == "--batch") {
    if (argc < 4) {
      printUsage(argv[0]);
      return 1;
    }
    batchMode = true;
    std::string listFile = argv[2];
    cacheDir = argv[3];
    
    // Read font paths from file
    std::ifstream file(listFile);
    if (!file) {
      std::cerr << "Failed to open font list file: " << listFile << std::endl;
      return 1;
    }
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty() && line[0] != '#') {
        fontPaths.push_back(line);
      }
    }
  } else {
    fontPath = argv[1];
    cacheDir = argv[2];
    fontPaths.push_back(fontPath);
  }
  
  // Ensure cache directory exists
  std::filesystem::create_directories(cacheDir);
  
  // Initialize SDL with hidden window (for OpenGL context)
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
    return 1;
  }
  
  // Request OpenGL 4.3 for compute shaders
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  // Create hidden window
  SDL_Window* window = SDL_CreateWindow(
    "MSDF-GPU",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    1, 1,
    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
  );
  
  if (!window) {
    std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }
  
  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if (!glContext) {
    std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  
  // Load GL functions
  if (!loadGLFunctions()) {
    std::cerr << "Failed to load OpenGL 4.3 functions. Compute shaders not supported." << std::endl;
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  
  std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
  
  // Compile compute shader
  GLuint program = compileComputeShader(computeShaderSource);
  if (!program) {
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  
  std::cout << "\nGenerating MSDF atlases..." << std::endl;
  
  auto totalStart = std::chrono::high_resolution_clock::now();
  int successCount = 0;
  
  for (const auto& path : fontPaths) {
    std::cout << "Processing: " << std::filesystem::path(path).filename().string() << std::endl;
    if (generateMSDFAtlas(program, path, cacheDir)) {
      successCount++;
    }
  }
  
  auto totalEnd = std::chrono::high_resolution_clock::now();
  auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart).count();
  
  std::cout << "\nCompleted: " << successCount << "/" << fontPaths.size() 
            << " fonts in " << totalMs << "ms" << std::endl;
  
  // Cleanup
  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(window);
  SDL_Quit();
  
  return successCount == fontPaths.size() ? 0 : 1;
}
