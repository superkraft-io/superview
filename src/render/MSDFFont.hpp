#pragma once

#include <SDL_opengl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

// msdfgen includes
#include <msdfgen.h>

// stb_truetype for parsing TTF files
#include "stb/stb_truetype.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <process.h>
#endif

namespace skene {

// Cache file magic and version
static constexpr uint32_t MSDF_CACHE_MAGIC = 0x4D534446;  // "MSDF"
static constexpr uint32_t MSDF_CACHE_VERSION = 4;  // Faster: smaller atlas, basic ASCII only

// Get the executable's directory
inline std::string getExecutableDirectory() {
  #ifdef _WIN32
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  std::string exePath(path);
  size_t lastSlash = exePath.find_last_of("\\/");
  return (lastSlash != std::string::npos) ? exePath.substr(0, lastSlash) : ".";
  #else
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of('/');
    return (lastSlash != std::string::npos) ? exePath.substr(0, lastSlash) : ".";
  }
  return ".";
  #endif
}

// Get the cache directory path (relative to executable)
inline std::string getMSDFCacheDirectory() {
  std::string cacheDir = getExecutableDirectory() + "/cache/fonts";
  
  // Ensure directory exists
  std::filesystem::create_directories(cacheDir);
  return cacheDir;
}

// Compute a simple hash of font file for cache invalidation
inline uint64_t computeFontFileHash(const std::string& fontPath) {
  std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
  if (!file) return 0;
  
  auto size = file.tellg();
  file.seekg(0, std::ios::beg);
  
  // Read file modification time
  uint64_t modTime = 0;
  try {
    auto ftime = std::filesystem::last_write_time(fontPath);
    modTime = std::chrono::duration_cast<std::chrono::seconds>(
      ftime.time_since_epoch()).count();
  } catch (...) {}
  
  // Simple hash: combine size, mod time, and first/last bytes
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

// FNV-1a hash for deterministic cache filenames (same across compilations)
inline uint64_t fnv1aHash(const std::string& str) {
  uint64_t hash = 14695981039346656037ULL;
  for (char c : str) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hash *= 1099511628211ULL;
  }
  return hash;
}

// Generate cache filename from font path
inline std::string getCacheFilename(const std::string& fontPath) {
  // Create a filename-safe hash of the font path (deterministic)
  uint64_t pathHash = fnv1aHash(fontPath);
  
  // Get just the font filename for readability
  std::filesystem::path p(fontPath);
  std::string baseName = p.stem().string();
  
  // Sanitize base name
  for (char& c : baseName) {
    if (!isalnum(c) && c != '-' && c != '_') c = '_';
  }
  
  return baseName + "_" + std::to_string(pathHash) + ".msdf";
}

// MSDF glyph data stored in atlas
struct MSDFGlyph {
  float u0, v0, u1, v1;  // Texture coordinates
  float xoff, yoff;       // Offset from baseline
  float width, height;    // Glyph size in pixels (at MSDF size)
  float advance;          // Horizontal advance
  bool valid = false;
};

// MSDF font atlas - stores all glyphs in one texture
struct MSDFAtlas {
  GLuint textureID = 0;
  int atlasWidth = 0;
  int atlasHeight = 0;
  float pixelRange = 4.0f;  // Distance field range in pixels
  float glyphSize = 32.0f;  // Size glyphs were rendered at
  std::map<int, MSDFGlyph> glyphs;  // Unicode codepoint -> glyph
  
  // Font metrics (in em units, multiply by fontSize/glyphSize)
  float ascent = 0;
  float descent = 0;
  float lineGap = 0;
  
  // Raw atlas data (for caching without GPU)
  std::vector<unsigned char> rawData;
  
  ~MSDFAtlas() {
    if (textureID) {
      glDeleteTextures(1, &textureID);
    }
  }
  
  // Upload raw data to GPU (must be called from main thread)
  void uploadToGPU() {
    if (textureID != 0 || rawData.empty()) return;
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlasWidth, atlasHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, rawData.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Clear raw data after upload to save memory
    rawData.clear();
    rawData.shrink_to_fit();
  }
};

class MSDFFont {
  std::vector<unsigned char> fontData;
  stbtt_fontinfo fontInfo;
  std::string fontPath;
  std::unique_ptr<MSDFAtlas> atlas;
  
  static constexpr float GLYPH_SIZE = 32.0f;      // Size to render glyphs at (balance quality/speed)
  static constexpr float PIXEL_RANGE = 4.0f;      // SDF range in pixels
  static constexpr int ATLAS_WIDTH = 512;         // Smaller atlas for faster generation
  static constexpr int ATLAS_HEIGHT = 512;
  static constexpr int GLYPH_PADDING = 2;         // Minimal padding for speed

public:
  // Decode UTF-8 codepoint from string, returns codepoint and advances index
  static int decodeUTF8(const std::string &text, size_t &i) {
    unsigned char c = text[i];
    if ((c & 0x80) == 0) {
      // ASCII (0xxxxxxx)
      return c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < text.length()) {
      // 2-byte sequence (110xxxxx 10xxxxxx)
      int cp = (c & 0x1F) << 6;
      cp |= (text[++i] & 0x3F);
      return cp;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < text.length()) {
      // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
      int cp = (c & 0x0F) << 12;
      cp |= (text[++i] & 0x3F) << 6;
      cp |= (text[++i] & 0x3F);
      return cp;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < text.length()) {
      // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
      int cp = (c & 0x07) << 18;
      cp |= (text[++i] & 0x3F) << 12;
      cp |= (text[++i] & 0x3F) << 6;
      cp |= (text[++i] & 0x3F);
      return cp;
    }
    return -1;  // Invalid
  }

  MSDFFont() = default;
  
  MSDFFont(const std::string &filename) {
    loadFont(filename);
  }

  ~MSDFFont() {
    atlas.reset();
  }
  
  // Font is loaded if we have an atlas (either from cache or generated)
  // Note: textureID may be 0 if generated in thread but not yet uploaded
  bool isLoaded() const { return atlas != nullptr && (atlas->textureID != 0 || !atlas->rawData.empty()); }
  
  // Check if atlas has been uploaded to GPU (ready for rendering)
  bool isReadyForRendering() const { return atlas != nullptr && atlas->textureID != 0; }
  
  // Ensure GPU resources are ready (upload if needed - must call from main thread)
  void ensureGPUReady() {
    if (atlas && atlas->textureID == 0 && !atlas->rawData.empty()) {
      atlas->uploadToGPU();
    }
  }
  
  const std::string& getPath() const { return fontPath; }
  float getPixelRange() const { return atlas ? atlas->pixelRange : PIXEL_RANGE; }
  float getGlyphSize() const { return atlas ? atlas->glyphSize : GLYPH_SIZE; }
  int getAtlasWidth() const { return atlas ? atlas->atlasWidth : ATLAS_WIDTH; }
  int getAtlasHeight() const { return atlas ? atlas->atlasHeight : ATLAS_HEIGHT; }

  // Try to load only from cache (fast path - no font file parsing)
  bool loadFromCacheOnly(const std::string &filename) {
    fontPath = filename;
    
    std::string cacheDir = getMSDFCacheDirectory();
    std::string cacheFile = cacheDir + "/" + getCacheFilename(fontPath);
    
    std::ifstream file(cacheFile, std::ios::binary);
    if (!file) return false;
    
    // Read and verify header
    uint32_t magic, version;
    uint64_t storedHash;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&storedHash), sizeof(storedHash));
    
    if (magic != MSDF_CACHE_MAGIC || version != MSDF_CACHE_VERSION) {
      return false;
    }
    
    // Skip hash validation for speed - trust the cache
    // (full loadFont will validate if cache fails)
    
    // Read atlas metadata
    atlas = std::make_unique<MSDFAtlas>();
    file.read(reinterpret_cast<char*>(&atlas->atlasWidth), sizeof(atlas->atlasWidth));
    file.read(reinterpret_cast<char*>(&atlas->atlasHeight), sizeof(atlas->atlasHeight));
    file.read(reinterpret_cast<char*>(&atlas->pixelRange), sizeof(atlas->pixelRange));
    file.read(reinterpret_cast<char*>(&atlas->glyphSize), sizeof(atlas->glyphSize));
    file.read(reinterpret_cast<char*>(&atlas->ascent), sizeof(atlas->ascent));
    file.read(reinterpret_cast<char*>(&atlas->descent), sizeof(atlas->descent));
    file.read(reinterpret_cast<char*>(&atlas->lineGap), sizeof(atlas->lineGap));
    
    // Read glyph count and data
    uint32_t glyphCount;
    file.read(reinterpret_cast<char*>(&glyphCount), sizeof(glyphCount));
    for (uint32_t i = 0; i < glyphCount; ++i) {
      int32_t codepoint;
      MSDFGlyph glyph;
      file.read(reinterpret_cast<char*>(&codepoint), sizeof(codepoint));
      file.read(reinterpret_cast<char*>(&glyph), sizeof(glyph));
      atlas->glyphs[codepoint] = glyph;
    }
    
    // Read atlas texture data
    size_t dataSize = atlas->atlasWidth * atlas->atlasHeight * 3;
    std::vector<unsigned char> atlasData(dataSize);
    file.read(reinterpret_cast<char*>(atlasData.data()), dataSize);
    
    if (!file) {
      atlas.reset();
      return false;
    }
    
    // Upload texture to GPU
    glGenTextures(1, &atlas->textureID);
    glBindTexture(GL_TEXTURE_2D, atlas->textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlas->atlasWidth, atlas->atlasHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, atlasData.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return true;
  }

  void loadFont(const std::string &filename) {
    // First try fast cache-only path
    if (loadFromCacheOnly(filename)) {
      return;
    }
    
    // Fall back to full load
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
      std::cerr << "MSDF: Failed to open font: " << filename << std::endl;
      return;
    }
    fontData = std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
    fontPath = filename;
    
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {
      std::cerr << "MSDF: Failed to parse font: " << filename << std::endl;
      fontData.clear();
      return;
    }
    
    // Generate MSDF atlas (with GPU upload)
    generateAtlas(true);
    
    // Save to cache for future use
    saveToCache();
  }
  
  // Generate font cache without OpenGL (thread-safe, for background caching)
  // Returns true if cache was successfully generated and saved
  bool generateCacheOnly(const std::string &filename) {
    // Check if cache already exists
    fontPath = filename;
    std::string cacheDir = getMSDFCacheDirectory();
    std::string cacheFile = cacheDir + "/" + getCacheFilename(fontPath);
    if (std::filesystem::exists(cacheFile)) {
      return true;  // Already cached
    }
    
    // Load font file
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
      std::cerr << "MSDF: [Thread] Failed to open font: " << filename << std::endl;
      return false;
    }
    fontData = std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
    
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {
      std::cerr << "MSDF: [Thread] Failed to parse font: " << filename << std::endl;
      fontData.clear();
      return false;
    }
    
    // Generate atlas WITHOUT OpenGL upload
    generateAtlas(false);
    
    if (!atlas || atlas->rawData.empty()) {
      std::cerr << "MSDF: [Thread] Failed to generate atlas: " << filename << std::endl;
      return false;
    }
    
    // Save to cache
    saveToCache();
    
    // Clear the atlas (we're only generating cache, not keeping the font)
    atlas.reset();
    fontData.clear();
    
    return true;
  }

  // Load atlas from disk cache (with full validation)
  bool loadFromCache() {
    std::string cacheDir = getMSDFCacheDirectory();
    std::string cacheFile = cacheDir + "/" + getCacheFilename(fontPath);
    
    std::ifstream file(cacheFile, std::ios::binary);
    if (!file) return false;
    
    // Read and verify header
    uint32_t magic, version;
    uint64_t storedHash;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&storedHash), sizeof(storedHash));
    
    if (magic != MSDF_CACHE_MAGIC || version != MSDF_CACHE_VERSION) {
      return false;  // Invalid or old cache format
    }
    
    // Verify font file hasn't changed
    uint64_t currentHash = computeFontFileHash(fontPath);
    if (storedHash != currentHash) {
      std::cout << "MSDF: Cache invalidated (font changed): " << fontPath << std::endl;
      return false;
    }
    
    // Read atlas metadata
    atlas = std::make_unique<MSDFAtlas>();
    file.read(reinterpret_cast<char*>(&atlas->atlasWidth), sizeof(atlas->atlasWidth));
    file.read(reinterpret_cast<char*>(&atlas->atlasHeight), sizeof(atlas->atlasHeight));
    file.read(reinterpret_cast<char*>(&atlas->pixelRange), sizeof(atlas->pixelRange));
    file.read(reinterpret_cast<char*>(&atlas->glyphSize), sizeof(atlas->glyphSize));
    file.read(reinterpret_cast<char*>(&atlas->ascent), sizeof(atlas->ascent));
    file.read(reinterpret_cast<char*>(&atlas->descent), sizeof(atlas->descent));
    file.read(reinterpret_cast<char*>(&atlas->lineGap), sizeof(atlas->lineGap));
    
    // Read glyph count and data
    uint32_t glyphCount;
    file.read(reinterpret_cast<char*>(&glyphCount), sizeof(glyphCount));
    for (uint32_t i = 0; i < glyphCount; ++i) {
      int32_t codepoint;
      MSDFGlyph glyph;
      file.read(reinterpret_cast<char*>(&codepoint), sizeof(codepoint));
      file.read(reinterpret_cast<char*>(&glyph), sizeof(glyph));
      atlas->glyphs[codepoint] = glyph;
    }
    
    // Read atlas texture data
    size_t dataSize = atlas->atlasWidth * atlas->atlasHeight * 3;
    std::vector<unsigned char> atlasData(dataSize);
    file.read(reinterpret_cast<char*>(atlasData.data()), dataSize);
    
    if (!file) {
      atlas.reset();
      return false;  // Read error
    }
    
    // Upload texture to GPU
    glGenTextures(1, &atlas->textureID);
    glBindTexture(GL_TEXTURE_2D, atlas->textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlas->atlasWidth, atlas->atlasHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, atlasData.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return true;
  }
  
  // Save atlas to disk cache (works with rawData OR GPU texture)
  void saveToCache() {
    if (!atlas) return;
    
    std::string cacheDir = getMSDFCacheDirectory();
    std::string cacheFile = cacheDir + "/" + getCacheFilename(fontPath);
    
    // Get atlas data from rawData or from GPU
    std::vector<unsigned char>* atlasDataPtr = nullptr;
    std::vector<unsigned char> gpuData;
    
    if (!atlas->rawData.empty()) {
      // Use rawData directly (thread-safe, no OpenGL)
      atlasDataPtr = &atlas->rawData;
    } else if (atlas->textureID != 0) {
      // Read back from GPU (only on main thread)
      gpuData.resize(atlas->atlasWidth * atlas->atlasHeight * 3);
      glBindTexture(GL_TEXTURE_2D, atlas->textureID);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, gpuData.data());
      atlasDataPtr = &gpuData;
    } else {
      std::cerr << "MSDF: No atlas data to save for: " << fontPath << std::endl;
      return;
    }
    
    std::ofstream file(cacheFile, std::ios::binary);
    if (!file) {
      std::cerr << "MSDF: Failed to save cache: " << cacheFile << std::endl;
      return;
    }
    
    // Write header
    uint32_t magic = MSDF_CACHE_MAGIC;
    uint32_t version = MSDF_CACHE_VERSION;
    uint64_t fontHash = computeFontFileHash(fontPath);
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&fontHash), sizeof(fontHash));
    
    // Write atlas metadata
    file.write(reinterpret_cast<const char*>(&atlas->atlasWidth), sizeof(atlas->atlasWidth));
    file.write(reinterpret_cast<const char*>(&atlas->atlasHeight), sizeof(atlas->atlasHeight));
    file.write(reinterpret_cast<const char*>(&atlas->pixelRange), sizeof(atlas->pixelRange));
    file.write(reinterpret_cast<const char*>(&atlas->glyphSize), sizeof(atlas->glyphSize));
    file.write(reinterpret_cast<const char*>(&atlas->ascent), sizeof(atlas->ascent));
    file.write(reinterpret_cast<const char*>(&atlas->descent), sizeof(atlas->descent));
    file.write(reinterpret_cast<const char*>(&atlas->lineGap), sizeof(atlas->lineGap));
    
    // Write glyph count and data
    uint32_t glyphCount = static_cast<uint32_t>(atlas->glyphs.size());
    file.write(reinterpret_cast<const char*>(&glyphCount), sizeof(glyphCount));
    for (const auto& [codepoint, glyph] : atlas->glyphs) {
      int32_t cp = codepoint;
      file.write(reinterpret_cast<const char*>(&cp), sizeof(cp));
      file.write(reinterpret_cast<const char*>(&glyph), sizeof(glyph));
    }
    
    // Write atlas texture data
    file.write(reinterpret_cast<const char*>(atlasDataPtr->data()), atlasDataPtr->size());
    
    std::cout << "MSDF: Saved to cache: " << std::filesystem::path(cacheFile).filename().string() << std::endl;
  }

  void bind() {
    if (atlas && atlas->textureID) {
      glBindTexture(GL_TEXTURE_2D, atlas->textureID);
    }
  }

  // Get glyph info for rendering
  const MSDFGlyph* getGlyph(int charCode) const {
    if (!atlas) return nullptr;
    auto it = atlas->glyphs.find(charCode);
    if (it != atlas->glyphs.end() && it->second.valid) {
      return &it->second;
    }
    return nullptr;
  }

  // Get text width at given font size (handles UTF-8)
  float getTextWidth(const std::string &text, float fontSize) {
    if (!atlas) return 0;
    float scale = fontSize / atlas->glyphSize;
    float width = 0;
    
    for (size_t i = 0; i < text.length(); ++i) {
      int cp = decodeUTF8(text, i);
      if (cp < 32) continue;
      auto it = atlas->glyphs.find(cp);
      if (it != atlas->glyphs.end() && it->second.valid) {
        width += it->second.advance * scale;
      }
    }
    return width;
  }

  // Get character positions for hit testing (handles UTF-8)
  std::vector<float> getCharacterPositions(const std::string &text, float fontSize) {
    std::vector<float> positions;
    if (!atlas) return positions;
    
    float scale = fontSize / atlas->glyphSize;
    float x = 0;
    positions.push_back(0);
    
    for (size_t i = 0; i < text.length(); ++i) {
      int cp = decodeUTF8(text, i);
      if (cp < 32) {
        positions.push_back(x);
        continue;
      }
      auto it = atlas->glyphs.find(cp);
      if (it != atlas->glyphs.end() && it->second.valid) {
        x += it->second.advance * scale;
      }
      positions.push_back(x);
    }
    return positions;
  }

  // Find character index at x position (handles UTF-8)
  size_t hitTestText(const std::string &text, float localX, float fontSize) {
    if (text.empty() || localX <= 0 || !atlas) return 0;
    
    float scale = fontSize / atlas->glyphSize;
    float x = 0;
    float prevX = 0;
    
    for (size_t i = 0; i < text.length(); ++i) {
      size_t charStart = i;
      int cp = decodeUTF8(text, i);
      if (cp < 32) continue;
      
      auto it = atlas->glyphs.find(cp);
      if (it != atlas->glyphs.end() && it->second.valid) {
        x += it->second.advance * scale;
      }
      
      float midpoint = prevX + (x - prevX) / 2.0f;
      if (localX < midpoint) {
        return charStart;
      }
      prevX = x;
    }
    
    return text.length();
  }

  // Get width of substring (handles UTF-8)
  float getSubstringWidth(const std::string &text, size_t start, size_t end, float fontSize) {
    if (!atlas || start >= end || start >= text.length()) return 0;
    
    float scale = fontSize / atlas->glyphSize;
    float startX = 0;
    float x = 0;
    size_t charIndex = 0;
    
    for (size_t i = 0; i < text.length() && charIndex < end; ++i) {
      int cp = decodeUTF8(text, i);
      if (cp < 32) {
        charIndex++;
        continue;
      }
      
      if (charIndex == start) {
        startX = x;
      }
      
      auto it = atlas->glyphs.find(cp);
      if (it != atlas->glyphs.end() && it->second.valid) {
        x += it->second.advance * scale;
      }
      charIndex++;
    }
    
    return x - startX;
  }

  // Get x position at character index (handles UTF-8)
  float getPositionAtIndex(const std::string &text, size_t index, float fontSize) {
    if (text.empty() || index == 0 || !atlas) return 0;
    
    float scale = fontSize / atlas->glyphSize;
    float x = 0;
    size_t charIndex = 0;
    
    for (size_t i = 0; i < text.length() && charIndex < index; ++i) {
      int cp = decodeUTF8(text, i);
      if (cp < 32) {
        charIndex++;
        continue;
      }
      
      auto it = atlas->glyphs.find(cp);
      if (it != atlas->glyphs.end() && it->second.valid) {
        x += it->second.advance * scale;
      }
      charIndex++;
    }
    
    return x;
  }

  // Get font ascent (scaled)
  float getAscent(float fontSize) const {
    if (!atlas) return fontSize * 0.8f;
    return atlas->ascent * (fontSize / atlas->glyphSize);
  }

  // Get font descent (scaled)  
  float getDescent(float fontSize) const {
    if (!atlas) return fontSize * 0.2f;
    return atlas->descent * (fontSize / atlas->glyphSize);
  }

private:
  // Get list of codepoints to include in atlas
  static std::vector<int> getCharacterSet() {
    std::vector<int> chars;
    
    // Basic ASCII (32-126)
    for (int c = 32; c <= 126; ++c) {
      chars.push_back(c);
    }
    
    // Latin-1 Supplement (160-255) - includes ©, ®, ±, etc.
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

  void generateAtlas(bool uploadToGPU = true) {
    atlas = std::make_unique<MSDFAtlas>();
    atlas->atlasWidth = ATLAS_WIDTH;
    atlas->atlasHeight = ATLAS_HEIGHT;
    atlas->pixelRange = PIXEL_RANGE;
    atlas->glyphSize = GLYPH_SIZE;
    
    // Get font metrics
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    // Use em-based scaling like browsers do (font-size = em-square, not pixel height)
    float scale = stbtt_ScaleForMappingEmToPixels(&fontInfo, GLYPH_SIZE);
    atlas->ascent = ascent * scale;
    atlas->descent = -descent * scale;  // descent is negative in stb
    atlas->lineGap = lineGap * scale;
    
    // Create atlas bitmap (RGB for MSDF) - store in rawData
    atlas->rawData.resize(ATLAS_WIDTH * ATLAS_HEIGHT * 3, 0);
    
    // Pack glyphs into atlas
    int cursorX = GLYPH_PADDING;
    int cursorY = GLYPH_PADDING;
    int rowHeight = 0;
    
    std::vector<int> charSet = getCharacterSet();
    
    for (int c : charSet) {
      MSDFGlyph glyph;
      
      // Get glyph index
      int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, c);
      if (glyphIndex == 0 && c != ' ') {
        continue;  // Missing glyph
      }
      
      // Get glyph metrics
      int advanceWidth, leftSideBearing;
      stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceWidth, &leftSideBearing);
      glyph.advance = advanceWidth * scale;
      
      // Get glyph bounding box
      int x0, y0, x1, y1;
      stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);
      
      int glyphW = x1 - x0;
      int glyphH = y1 - y0;
      
      // Space character - no visual, just advance
      if (c == ' ' || glyphW <= 0 || glyphH <= 0) {
        glyph.valid = true;
        glyph.width = 0;
        glyph.height = 0;
        glyph.xoff = 0;
        glyph.yoff = 0;
        glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0;
        atlas->glyphs[c] = glyph;
        continue;
      }
      
      // Add padding for SDF
      int paddedW = glyphW + GLYPH_PADDING * 2;
      int paddedH = glyphH + GLYPH_PADDING * 2;
      
      // Check if we need to move to next row
      if (cursorX + paddedW > ATLAS_WIDTH - GLYPH_PADDING) {
        cursorX = GLYPH_PADDING;
        cursorY += rowHeight + GLYPH_PADDING;
        rowHeight = 0;
      }
      
      // Check if atlas is full
      if (cursorY + paddedH > ATLAS_HEIGHT - GLYPH_PADDING) {
        std::cerr << "MSDF: Atlas full at codepoint " << c << std::endl;
        break;
      }
      
      // Generate MSDF for this glyph (uses atlas->rawData)
      if (!generateGlyphMSDF(c, cursorX, cursorY, paddedW, paddedH, atlas->rawData)) {
        continue;
      }
      
      // Store glyph info
      glyph.valid = true;
      glyph.width = (float)paddedW;
      glyph.height = (float)paddedH;
      glyph.xoff = x0 - GLYPH_PADDING;
      glyph.yoff = y0 - GLYPH_PADDING;
      glyph.u0 = (float)cursorX / ATLAS_WIDTH;
      glyph.v0 = (float)cursorY / ATLAS_HEIGHT;
      glyph.u1 = (float)(cursorX + paddedW) / ATLAS_WIDTH;
      glyph.v1 = (float)(cursorY + paddedH) / ATLAS_HEIGHT;
      atlas->glyphs[c] = glyph;
      
      // Advance cursor
      cursorX += paddedW + GLYPH_PADDING;
      rowHeight = std::max(rowHeight, paddedH);
    }
    
    std::cout << "MSDF: Generated atlas for " << std::filesystem::path(fontPath).filename().string() << std::endl;
    
    // Optionally upload to GPU (skip when generating in worker thread)
    if (uploadToGPU) {
      atlas->uploadToGPU();
    }
  }

  bool generateGlyphMSDF(int charCode, int atlasX, int atlasY, int width, int height,
                         std::vector<unsigned char>& atlasData) {
    // Get glyph shape from stb_truetype
    stbtt_vertex* vertices = nullptr;
    int numVerts = stbtt_GetCodepointShape(&fontInfo, charCode, &vertices);
    
    if (numVerts == 0 || !vertices) {
      return false;
    }
    
    // Convert stb_truetype vertices to msdfgen shape
    msdfgen::Shape shape;
    msdfgen::Contour* contour = nullptr;
    
    // Use em-based scaling like browsers do (font-size = em-square, not pixel height)
    float scale = stbtt_ScaleForMappingEmToPixels(&fontInfo, GLYPH_SIZE);
    
    // Get glyph bounding box in font units
    int ix0, iy0, ix1, iy1;
    stbtt_GetCodepointBox(&fontInfo, charCode, &ix0, &iy0, &ix1, &iy1);
    
    // Track the previous point for constructing edges
    msdfgen::Point2 prevPoint(0, 0);
    msdfgen::Point2 startPoint(0, 0);
    
    for (int i = 0; i < numVerts; ++i) {
      stbtt_vertex& v = vertices[i];
      
      // stb_truetype uses font units with Y up from baseline
      // msdfgen also uses Y up, so we just scale coordinates
      // Offset so glyph starts at GLYPH_PADDING from origin
      float px = (v.x - ix0) * scale + GLYPH_PADDING;
      float py = (v.y - iy0) * scale + GLYPH_PADDING;
      float cx = (v.cx - ix0) * scale + GLYPH_PADDING;
      float cy = (v.cy - iy0) * scale + GLYPH_PADDING;
      float cx1 = (v.cx1 - ix0) * scale + GLYPH_PADDING;
      float cy1 = (v.cy1 - iy0) * scale + GLYPH_PADDING;
      
      msdfgen::Point2 currentPoint(px, py);
      msdfgen::Point2 controlPoint(cx, cy);
      msdfgen::Point2 controlPoint2(cx1, cy1);
      
      switch (v.type) {
        case STBTT_vmove:
          // Close previous contour if needed (implicit close)
          if (contour && !contour->edges.empty()) {
            // Check if we need to close the contour
            if (prevPoint.x != startPoint.x || prevPoint.y != startPoint.y) {
              contour->addEdge(msdfgen::EdgeHolder(prevPoint, startPoint));
            }
          }
          // Start new contour
          contour = &shape.addContour();
          prevPoint = currentPoint;
          startPoint = currentPoint;
          break;
          
        case STBTT_vline:
          if (contour) {
            contour->addEdge(msdfgen::EdgeHolder(prevPoint, currentPoint));
          }
          prevPoint = currentPoint;
          break;
          
        case STBTT_vcurve:
          if (contour) {
            contour->addEdge(msdfgen::EdgeHolder(prevPoint, controlPoint, currentPoint));
          }
          prevPoint = currentPoint;
          break;
          
        case STBTT_vcubic:
          if (contour) {
            contour->addEdge(msdfgen::EdgeHolder(prevPoint, controlPoint, controlPoint2, currentPoint));
          }
          prevPoint = currentPoint;
          break;
      }
    }
    
    // Close the last contour if needed
    if (contour && !contour->edges.empty()) {
      if (prevPoint.x != startPoint.x || prevPoint.y != startPoint.y) {
        contour->addEdge(msdfgen::EdgeHolder(prevPoint, startPoint));
      }
    }
    
    stbtt_FreeShape(&fontInfo, vertices);
    
    if (shape.contours.empty()) {
      return false;
    }
    
    // Normalize the shape (fixes winding order for proper inside/outside detection)
    shape.normalize();
    
    // Orient contours correctly - this is critical for MSDF
    shape.orientContours();
    
    // Color the edges for MSDF
    msdfgen::edgeColoringSimple(shape, 3.0);
    
    // Generate MSDF
    msdfgen::Bitmap<float, 3> msdf(width, height);
    msdfgen::generateMSDF(msdf, shape, PIXEL_RANGE, 1.0, msdfgen::Vector2(0, 0));
    
    // Copy to atlas - MSDF bitmap is bottom-up, atlas texture is top-down
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int atlasIdx = ((atlasY + y) * ATLAS_WIDTH + (atlasX + x)) * 3;
        // Flip Y: msdfgen row 0 is bottom, we want row 0 to be top
        float* pixel = msdf(x, height - 1 - y);
        // MSDF values are typically in range [0, 1] from msdfgen
        atlasData[atlasIdx + 0] = (unsigned char)std::clamp(pixel[0] * 255.0f, 0.0f, 255.0f);
        atlasData[atlasIdx + 1] = (unsigned char)std::clamp(pixel[1] * 255.0f, 0.0f, 255.0f);
        atlasData[atlasIdx + 2] = (unsigned char)std::clamp(pixel[2] * 255.0f, 0.0f, 255.0f);
      }
    }
    
    return true;
  }
};

// Font weight and style enums (matching Font.hpp)
enum class MSDFFontWeight { Normal, Bold, Lighter, Bolder };
enum class MSDFFontStyle { Normal, Italic, Oblique };

// Structure to hold discovered system font info
struct SystemFontInfo {
  std::string path;
  std::string familyName;
  MSDFFontWeight weight;
  MSDFFontStyle style;
};

// Simple thread pool for parallel font cache generation
class FontCacheThreadPool {
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queueMutex;
  std::condition_variable condition;
  std::condition_variable idleCondition;
  std::atomic<bool> stop{false};
  std::atomic<int> activeTasks{0};
  size_t numThreads;

public:
  explicit FontCacheThreadPool(size_t threads = 0) 
    // Limit to max 4 threads to avoid starving the render loop
    : numThreads(threads == 0 ? std::min(4u, std::max(1u, std::thread::hardware_concurrency() / 2)) : threads) {
    
    workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { 
              return stop || !tasks.empty(); 
            });
            
            if (stop && tasks.empty()) return;
            
            task = std::move(tasks.front());
            tasks.pop();
            ++activeTasks;
          }
          
          task();
          
          {
            std::lock_guard<std::mutex> lock(queueMutex);
            --activeTasks;
          }
          idleCondition.notify_all();
        }
      });
    }
  }
  
  ~FontCacheThreadPool() {
    shutdown();
  }
  
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    workers.clear();
  }
  
  // Submit a task to the pool
  void submit(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      if (stop) return;
      tasks.push(std::move(task));
    }
    condition.notify_one();
  }
  
  // Wait for all tasks to complete
  void waitForAll() {
    std::unique_lock<std::mutex> lock(queueMutex);
    idleCondition.wait(lock, [this] {
      return tasks.empty() && activeTasks == 0;
    });
  }
  
  // Check if there are pending or active tasks
  bool isBusy() const {
    return activeTasks > 0 || !tasks.empty();
  }
  
  // Get number of threads in pool
  size_t threadCount() const { return numThreads; }
  
  // Get number of pending tasks
  size_t pendingTasks() const {
    return tasks.size();
  }
  
  // Get number of active tasks
  int activeTaskCount() const {
    return activeTasks.load();
  }
};

// MSDF Font manager - handles multiple font families with weight/style variants
// Includes background thread for font discovery and pre-caching with thread pool
class MSDFFontManager {
  struct FontEntry {
    std::string path;
    std::unique_ptr<MSDFFont> font;  // nullptr until loaded
    bool loadAttempted = false;
    bool isCached = false;  // true if cache file exists
  };
  
  std::map<std::string, FontEntry> fonts;
  std::set<std::string> knownFontPaths;  // All discovered font file paths
  std::set<std::string> pathsBeingCached;  // Paths currently being cached by thread pool
  std::string defaultSerifPath;
  std::string defaultSansSerifPath;
  std::string defaultMonospacePath;
  
  // Thread-safety
  mutable std::mutex fontsMutex;
  mutable std::mutex cachingMutex;  // For pathsBeingCached
  
  // Thread pool for parallel font cache generation
  std::unique_ptr<FontCacheThreadPool> cacheThreadPool;
  
  // Background thread for font discovery
  std::thread discoveryThread;
  std::atomic<bool> stopDiscovery{false};
  static constexpr int DISCOVERY_INTERVAL_SECONDS = 30;
  
  // Callback for when new fonts are discovered (called from main thread context)
  std::function<void()> onFontsDiscovered;
  
public:
  MSDFFontManager() {
    #ifdef _WIN32
    defaultSerifPath = "C:\\Windows\\Fonts\\times.ttf";
    defaultSansSerifPath = "C:\\Windows\\Fonts\\arial.ttf";
    defaultMonospacePath = "C:\\Windows\\Fonts\\cour.ttf";
    
    // Register core font paths (lazy loading - won't generate atlas until needed)
    registerFontPath("serif", MSDFFontWeight::Normal, MSDFFontStyle::Normal, defaultSerifPath);
    registerFontPath("sans-serif", MSDFFontWeight::Normal, MSDFFontStyle::Normal, defaultSansSerifPath);
    registerFontPath("monospace", MSDFFontWeight::Normal, MSDFFontStyle::Normal, defaultMonospacePath);
    
    // Bold variants
    registerFontPath("serif", MSDFFontWeight::Bold, MSDFFontStyle::Normal, "C:\\Windows\\Fonts\\timesbd.ttf");
    registerFontPath("sans-serif", MSDFFontWeight::Bold, MSDFFontStyle::Normal, "C:\\Windows\\Fonts\\arialbd.ttf");
    registerFontPath("monospace", MSDFFontWeight::Bold, MSDFFontStyle::Normal, "C:\\Windows\\Fonts\\courbd.ttf");
    
    // Italic variants
    registerFontPath("serif", MSDFFontWeight::Normal, MSDFFontStyle::Italic, "C:\\Windows\\Fonts\\timesi.ttf");
    registerFontPath("sans-serif", MSDFFontWeight::Normal, MSDFFontStyle::Italic, "C:\\Windows\\Fonts\\ariali.ttf");
    registerFontPath("monospace", MSDFFontWeight::Normal, MSDFFontStyle::Italic, "C:\\Windows\\Fonts\\couri.ttf");
    
    // Bold italic variants
    registerFontPath("serif", MSDFFontWeight::Bold, MSDFFontStyle::Italic, "C:\\Windows\\Fonts\\timesbi.ttf");
    registerFontPath("sans-serif", MSDFFontWeight::Bold, MSDFFontStyle::Italic, "C:\\Windows\\Fonts\\arialbi.ttf");
    registerFontPath("monospace", MSDFFontWeight::Bold, MSDFFontStyle::Italic, "C:\\Windows\\Fonts\\courbi.ttf");
    #else
    defaultSerifPath = "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf";
    defaultSansSerifPath = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
    defaultMonospacePath = "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf";
    
    registerFontPath("serif", MSDFFontWeight::Normal, MSDFFontStyle::Normal, defaultSerifPath);
    registerFontPath("sans-serif", MSDFFontWeight::Normal, MSDFFontStyle::Normal, defaultSansSerifPath);
    registerFontPath("monospace", MSDFFontWeight::Normal, MSDFFontStyle::Normal, defaultMonospacePath);
    #endif
    
    // Register common aliases (these point to existing paths)
    registerAlias("times", "serif");
    registerAlias("times new roman", "serif");
    registerAlias("arial", "sans-serif");
    registerAlias("helvetica", "sans-serif");
    registerAlias("courier", "monospace");
    registerAlias("courier new", "monospace");
    
    // Initialize thread pool for parallel font caching
    // Use hardware_concurrency - 1 threads (leave one for main thread)
    cacheThreadPool = std::make_unique<FontCacheThreadPool>();
    std::cout << "MSDF: Font cache thread pool initialized with " 
              << cacheThreadPool->threadCount() << " threads" << std::endl;
    
    // Preload essential fonts from cache (fast - no generation)
    preloadEssentialFonts();
  }
  
  ~MSDFFontManager() {
    stopBackgroundDiscovery();
    // Shut down thread pool (waits for pending tasks)
    if (cacheThreadPool) {
      cacheThreadPool->shutdown();
    }
  }
  
  // Preload essential fonts from cache only (no generation - instant if cached)
  void preloadEssentialFonts() {
    std::vector<std::string> essentialKeys = {
      "serif:normal:normal",
      "sans-serif:normal:normal",
      "monospace:normal:normal"
    };
    
    for (const auto& key : essentialKeys) {
      auto it = fonts.find(key);
      if (it != fonts.end() && it->second.isCached && !it->second.font) {
        // Try fast cache-only load
        auto font = std::make_unique<MSDFFont>();
        if (font->loadFromCacheOnly(it->second.path)) {
          it->second.font = std::move(font);
          it->second.loadAttempted = true;
        }
      }
    }
  }
  
  // Initialize core fonts synchronously (call before rendering starts)
  // This ensures essential fonts are ready before the first frame
  void initializeCoreFonts() {
    // Try GPU caching first for registered fonts
    int gpuResult = generateCachesWithGPU();
    
    if (gpuResult < 0) {
      // GPU not available, fall back to CPU for core fonts only
      std::cout << "MSDF: Pre-caching core fonts with CPU..." << std::endl;
      preCacheNewFonts(true);  // essentialOnly = true
      if (cacheThreadPool) {
        cacheThreadPool->waitForAll();
      }
    }
    
    // Preload essential fonts from cache
    preloadEssentialFonts();
  }
  
  // Start background thread that periodically scans for new fonts
  void startBackgroundDiscovery() {
    if (discoveryThread.joinable()) return;  // Already running
    
    stopDiscovery = false;
    discoveryThread = std::thread([this]() {
      // Core fonts already initialized synchronously, scan for system fonts
      std::cout << "MSDF: Scanning system fonts in background..." << std::endl;
      scanSystemFonts();
      
      // Try GPU caching for newly discovered fonts, fall back to CPU if needed
      int gpuResult = generateCachesWithGPU();
      if (gpuResult < 0) {
        preCacheNewFonts(false);
      }
      
      while (!stopDiscovery) {
        // Wait for thread pool to finish current batch before sleeping
        if (cacheThreadPool && cacheThreadPool->isBusy()) {
          std::cout << "MSDF: Waiting for " << cacheThreadPool->activeTaskCount() 
                    << " active + " << cacheThreadPool->pendingTasks() << " pending cache tasks..." << std::endl;
          cacheThreadPool->waitForAll();
          std::cout << "MSDF: Cache generation complete" << std::endl;
        }
        
        // Sleep in small intervals to allow quick shutdown
        for (int i = 0; i < DISCOVERY_INTERVAL_SECONDS && !stopDiscovery; ++i) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (!stopDiscovery) {
          int newFonts = scanSystemFonts();
          if (newFonts > 0) {
            std::cout << "MSDF: Discovered " << newFonts << " new fonts" << std::endl;
            // Try GPU caching first, fall back to CPU
            int gpuResult = generateCachesWithGPU();
            if (gpuResult < 0) {
              preCacheNewFonts(false);
            }
          }
        }
      }
      
      // Wait for any remaining cache tasks on shutdown
      if (cacheThreadPool && cacheThreadPool->isBusy()) {
        std::cout << "MSDF: Waiting for cache tasks to complete before shutdown..." << std::endl;
        cacheThreadPool->waitForAll();
      }
    });
    
    std::cout << "MSDF: Started background font discovery (interval: " 
              << DISCOVERY_INTERVAL_SECONDS << "s)" << std::endl;
  }
  
  void stopBackgroundDiscovery() {
    stopDiscovery = true;
    if (discoveryThread.joinable()) {
      discoveryThread.join();
    }
  }
  
  // Set callback for font discovery events
  void setOnFontsDiscovered(std::function<void()> callback) {
    onFontsDiscovered = callback;
  }
  
  std::string makeFontKey(const std::string& family, MSDFFontWeight weight = MSDFFontWeight::Normal, 
                          MSDFFontStyle style = MSDFFontStyle::Normal) {
    std::string key = toLower(family);
    key += (weight == MSDFFontWeight::Bold || weight == MSDFFontWeight::Bolder) ? ":bold" : ":normal";
    key += (style == MSDFFontStyle::Italic || style == MSDFFontStyle::Oblique) ? ":italic" : ":normal";
    return key;
  }
  
  // Register a font path without loading it (lazy loading)
  void registerFontPath(const std::string& name, MSDFFontWeight weight, MSDFFontStyle style, const std::string& path) {
    std::lock_guard<std::mutex> lock(fontsMutex);
    std::string key = makeFontKey(name, weight, style);
    
    // Check if cache exists
    std::string cacheDir = getMSDFCacheDirectory();
    std::string cacheFile = cacheDir + "/" + getCacheFilename(path);
    bool cached = std::filesystem::exists(cacheFile);
    
    fonts[key] = FontEntry{path, nullptr, false, cached};
    knownFontPaths.insert(path);
  }
  
  // Old interface for compatibility
  bool loadFont(const std::string& name, const std::string& path) {
    return loadFontVariant(name, MSDFFontWeight::Normal, MSDFFontStyle::Normal, path);
  }
  
  bool loadFontVariant(const std::string& name, MSDFFontWeight weight, MSDFFontStyle style, const std::string& path) {
    registerFontPath(name, weight, style, path);
    return true;
  }
  
  void registerAlias(const std::string& alias, const std::string& existingName) {
    std::lock_guard<std::mutex> lock(fontsMutex);
    std::string lowerAlias = toLower(alias);
    std::string lowerExisting = toLower(existingName);
    
    MSDFFontWeight weights[] = {MSDFFontWeight::Normal, MSDFFontWeight::Bold};
    MSDFFontStyle styles[] = {MSDFFontStyle::Normal, MSDFFontStyle::Italic};
    
    for (auto w : weights) {
      for (auto s : styles) {
        std::string existingKey = makeFontKey(lowerExisting, w, s);
        auto it = fonts.find(existingKey);
        if (it != fonts.end()) {
          std::string aliasKey = makeFontKey(lowerAlias, w, s);
          fonts[aliasKey] = FontEntry{it->second.path, nullptr, false, it->second.isCached};
        }
      }
    }
  }
  
  // Get list of all registered font families
  std::vector<std::string> getRegisteredFamilies() {
    std::lock_guard<std::mutex> lock(fontsMutex);
    std::set<std::string> families;
    for (const auto& pair : fonts) {
      // Extract family name from key (everything before first ':')
      size_t colonPos = pair.first.find(':');
      if (colonPos != std::string::npos) {
        families.insert(pair.first.substr(0, colonPos));
      }
    }
    return std::vector<std::string>(families.begin(), families.end());
  }
  
  // Get count of registered fonts
  size_t getRegisteredFontCount() {
    std::lock_guard<std::mutex> lock(fontsMutex);
    return knownFontPaths.size();
  }
  
  // Get count of cached fonts
  size_t getCachedFontCount() {
    std::lock_guard<std::mutex> lock(fontsMutex);
    size_t count = 0;
    std::set<std::string> countedPaths;
    for (const auto& pair : fonts) {
      if (pair.second.isCached && countedPaths.find(pair.second.path) == countedPaths.end()) {
        count++;
        countedPaths.insert(pair.second.path);
      }
    }
    return count;
  }
  
private:
  // Scan system font directories and register discovered fonts
  int scanSystemFonts() {
    std::vector<SystemFontInfo> discovered;
    
    #ifdef _WIN32
    // Windows font directories
    std::vector<std::string> fontDirs = {
      "C:\\Windows\\Fonts"
    };
    
    // Also check user fonts folder
    char userPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, userPath))) {
      fontDirs.push_back(std::string(userPath) + "\\Microsoft\\Windows\\Fonts");
    }
    #else
    std::vector<std::string> fontDirs = {
      "/usr/share/fonts",
      "/usr/local/share/fonts",
      "~/.fonts",
      "~/.local/share/fonts"
    };
    #endif
    
    for (const auto& dir : fontDirs) {
      scanFontDirectory(dir, discovered);
    }
    
    // Register newly discovered fonts
    int newCount = 0;
    for (const auto& info : discovered) {
      std::lock_guard<std::mutex> lock(fontsMutex);
      if (knownFontPaths.find(info.path) == knownFontPaths.end()) {
        std::string key = makeFontKey(info.familyName, info.weight, info.style);
        
        // Check if cache exists
        std::string cacheDir = getMSDFCacheDirectory();
        std::string cacheFile = cacheDir + "/" + getCacheFilename(info.path);
        bool cached = std::filesystem::exists(cacheFile);
        
        fonts[key] = FontEntry{info.path, nullptr, false, cached};
        knownFontPaths.insert(info.path);
        newCount++;
      }
    }
    
    std::cout << "MSDF: System font scan complete - found " << discovered.size() 
              << " fonts, " << newCount << " new" << std::endl;
    
    return newCount;
  }
  
  void scanFontDirectory(const std::string& dirPath, std::vector<SystemFontInfo>& discovered) {
    try {
      if (!std::filesystem::exists(dirPath)) return;
      
      for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        // Only process TrueType and OpenType fonts
        if (ext != ".ttf" && ext != ".otf" && ext != ".ttc") continue;
        
        std::string fontPath = entry.path().string();
        
        // Skip if already known
        {
          std::lock_guard<std::mutex> lock(fontsMutex);
          if (knownFontPaths.find(fontPath) != knownFontPaths.end()) continue;
        }
        
        // Extract font info from filename (fast) instead of reading file
        SystemFontInfo info = extractFontInfoFast(fontPath);
        if (!info.familyName.empty()) {
          discovered.push_back(info);
        }
      }
    } catch (const std::exception& e) {
      // Ignore directory access errors
    }
  }
  
  // Fast font info extraction using only filename (no file reading)
  SystemFontInfo extractFontInfoFast(const std::string& fontPath) {
    SystemFontInfo info;
    info.path = fontPath;
    info.weight = MSDFFontWeight::Normal;
    info.style = MSDFFontStyle::Normal;
    
    // Use filename as family name
    std::filesystem::path p(fontPath);
    std::string filename = p.stem().string();
    
    // Common naming patterns: Arial, ArialBold, Arial-Bold, Arial_Bold, arialbd
    std::string lowerName = toLower(filename);
    
    // Detect weight from filename
    if (lowerName.find("bold") != std::string::npos || 
        lowerName.find("bd") != std::string::npos ||
        lowerName.find("-b") != std::string::npos) {
      info.weight = MSDFFontWeight::Bold;
    }
    
    // Detect style from filename
    if (lowerName.find("italic") != std::string::npos || 
        lowerName.find("oblique") != std::string::npos ||
        lowerName.find("-i") != std::string::npos ||
        lowerName.find("i.") != std::string::npos) {
      info.style = MSDFFontStyle::Italic;
    }
    
    // Remove common suffixes to get base family name
    std::string baseName = filename;
    // Remove weight/style indicators
    std::vector<std::string> suffixes = {"Bold", "bold", "BD", "bd", "Italic", "italic", "IT", "it", 
                                          "BI", "bi", "Regular", "regular", "-", "_"};
    for (const auto& suffix : suffixes) {
      size_t pos;
      while ((pos = baseName.find(suffix)) != std::string::npos) {
        baseName.erase(pos, suffix.length());
      }
    }
    
    // Trim and use as family name
    while (!baseName.empty() && (baseName.back() == ' ' || baseName.back() == '-' || baseName.back() == '_')) {
      baseName.pop_back();
    }
    
    info.familyName = baseName.empty() ? filename : baseName;
    return info;
  }
  
  // Full font info extraction (reads file - slow, used for detailed info if needed)
  SystemFontInfo extractFontInfo(const std::string& fontPath) {
    SystemFontInfo info;
    info.path = fontPath;
    info.weight = MSDFFontWeight::Normal;
    info.style = MSDFFontStyle::Normal;
    
    // Load font file to extract name
    std::ifstream file(fontPath, std::ios::binary);
    if (!file) return info;
    
    std::vector<unsigned char> fontData(std::istreambuf_iterator<char>(file), {});
    
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {
      return info;
    }
    
    // Try to get font family name from name table
    // Name ID 1 = Font Family, Name ID 2 = Font Subfamily
    int nameLength;
    const char* familyName = stbtt_GetFontNameString(&fontInfo, &nameLength, 
      STBTT_PLATFORM_ID_MICROSOFT, STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 1);
    
    if (familyName && nameLength > 0) {
      // Convert from UTF-16BE to ASCII (simple conversion for common fonts)
      std::string name;
      for (int i = 1; i < nameLength; i += 2) {
        char c = familyName[i];
        if (c >= 32 && c < 127) name += c;
      }
      info.familyName = name;
    }
    
    if (info.familyName.empty()) {
      // Fallback: use filename
      std::filesystem::path p(fontPath);
      info.familyName = p.stem().string();
    }
    
    // Try to determine weight/style from subfamily name or filename
    const char* subfamilyName = stbtt_GetFontNameString(&fontInfo, &nameLength,
      STBTT_PLATFORM_ID_MICROSOFT, STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 2);
    
    std::string subfamily;
    if (subfamilyName && nameLength > 0) {
      for (int i = 1; i < nameLength; i += 2) {
        char c = subfamilyName[i];
        if (c >= 32 && c < 127) subfamily += (char)tolower(c);
      }
    }
    
    // Also check filename for hints
    std::string filename = toLower(std::filesystem::path(fontPath).stem().string());
    std::string combined = subfamily + " " + filename;
    
    if (combined.find("bold") != std::string::npos) {
      info.weight = MSDFFontWeight::Bold;
    }
    if (combined.find("italic") != std::string::npos || 
        combined.find("oblique") != std::string::npos) {
      info.style = MSDFFontStyle::Italic;
    }
    
    return info;
  }
  
  // Pre-cache fonts that don't have cache files yet
  // If essentialOnly is true, only cache serif, sans-serif, and monospace
  // Uses thread pool for parallel generation
  void preCacheNewFonts(bool essentialOnly = false) {
    if (!cacheThreadPool) return;
    
    std::vector<std::string> toCachePaths;  // unique paths to cache
    std::set<std::string> pathsToCache;     // for deduplication
    
    // Essential font keys (the core fonts we need for startup)
    std::set<std::string> essentialKeys = {
      "serif:normal:normal", "serif:bold:normal", "serif:normal:italic", "serif:bold:italic",
      "sans-serif:normal:normal", "sans-serif:bold:normal", "sans-serif:normal:italic", "sans-serif:bold:italic",
      "monospace:normal:normal", "monospace:bold:normal", "monospace:normal:italic", "monospace:bold:italic"
    };
    
    // Collect unique paths that need caching
    {
      std::lock_guard<std::mutex> lock(fontsMutex);
      std::lock_guard<std::mutex> cacheLock(cachingMutex);
      
      for (auto& pair : fonts) {
        if (pair.second.isCached) continue;
        if (pair.second.loadAttempted) continue;
        
        // If essentialOnly, skip non-essential fonts
        if (essentialOnly && essentialKeys.find(pair.first) == essentialKeys.end()) {
          continue;
        }
        
        const std::string& path = pair.second.path;
        
        // Skip if already being cached or already queued
        if (pathsBeingCached.find(path) != pathsBeingCached.end()) continue;
        if (pathsToCache.find(path) != pathsToCache.end()) continue;
        
        // Check if cache already exists
        std::string cacheDir = getMSDFCacheDirectory();
        std::string cacheFile = cacheDir + "/" + getCacheFilename(path);
        if (std::filesystem::exists(cacheFile)) {
          pair.second.isCached = true;
          continue;
        }
        
        pathsToCache.insert(path);
        toCachePaths.push_back(path);
      }
      
      // Mark all paths as being cached
      for (const auto& path : toCachePaths) {
        pathsBeingCached.insert(path);
      }
    }
    
    if (toCachePaths.empty()) {
      return;
    }
    
    std::cout << "MSDF: Queuing " << toCachePaths.size() << " fonts for parallel caching (" 
              << cacheThreadPool->threadCount() << " threads)..." << std::endl;
    
    // Submit cache tasks to thread pool
    for (const auto& path : toCachePaths) {
      if (stopDiscovery) break;
      
      cacheThreadPool->submit([this, path]() {
        if (stopDiscovery) {
          // Remove from being cached set
          std::lock_guard<std::mutex> cacheLock(cachingMutex);
          pathsBeingCached.erase(path);
          return;
        }
        
        // Double-check cache doesn't exist (another thread might have created it)
        std::string cacheDir = getMSDFCacheDirectory();
        std::string cacheFile = cacheDir + "/" + getCacheFilename(path);
        if (std::filesystem::exists(cacheFile)) {
          markPathAsCached(path);
          return;
        }
        
        // Generate cache (thread-safe, no OpenGL)
        std::cout << "MSDF: [Thread] Caching: " << std::filesystem::path(path).filename().string() << std::endl;
        auto font = std::make_unique<MSDFFont>();
        bool success = font->generateCacheOnly(path);
        
        if (success) {
          markPathAsCached(path);
        } else {
          // Remove from being cached set on failure
          std::lock_guard<std::mutex> cacheLock(cachingMutex);
          pathsBeingCached.erase(path);
        }
      });
    }
  }
  
  // Use external GPU-based MSDF generator for fast font caching
  // Returns number of fonts successfully cached
  int generateCachesWithGPU() {
    // Collect uncached font paths
    std::vector<std::string> uncachedPaths;
    {
      std::lock_guard<std::mutex> lock(fontsMutex);
      std::set<std::string> seenPaths;
      
      for (auto& pair : fonts) {
        if (pair.second.isCached) continue;
        
        const std::string& path = pair.second.path;
        if (seenPaths.find(path) != seenPaths.end()) continue;
        
        // Check if cache already exists
        std::string cacheDir = getMSDFCacheDirectory();
        std::string cacheFile = cacheDir + "/" + getCacheFilename(path);
        if (std::filesystem::exists(cacheFile)) {
          pair.second.isCached = true;
          continue;
        }
        
        seenPaths.insert(path);
        uncachedPaths.push_back(path);
      }
    }
    
    if (uncachedPaths.empty()) {
      return 0;
    }
    
    // Find the msdf-gpu executable (relative to current executable)
    std::string exeDir = getExecutableDirectory();
    std::filesystem::path gpuToolPath = std::filesystem::path(exeDir) / "msdf-gpu.exe";
    
    if (!std::filesystem::exists(gpuToolPath)) {
      // Try tools directory (during development)
      gpuToolPath = std::filesystem::path(exeDir) / ".." / ".." / "tools" / "msdf-gpu" / "build" / "Release" / "msdf-gpu.exe";
      if (!std::filesystem::exists(gpuToolPath)) {
        std::cout << "MSDF: GPU tool not found, falling back to CPU caching" << std::endl;
        return -1;  // Signal to use CPU fallback
      }
    }
    
    // Create cache directory
    std::string cacheDir = getMSDFCacheDirectory();
    std::filesystem::create_directories(cacheDir);
    
    // Create temporary file with font paths
    std::filesystem::path tempFile = std::filesystem::path(cacheDir) / "_gpu_batch.txt";
    
    {
      std::ofstream file(tempFile);
      for (const auto& path : uncachedPaths) {
        file << path << "\n";
      }
    }
    
    std::cout << "MSDF: Running GPU caching for " << uncachedPaths.size() << " fonts..." << std::endl;
    
    // Execute GPU tool
    #ifdef _WIN32
    // Use _spawnl which handles Windows paths better
    intptr_t result = _spawnl(_P_WAIT, 
                              gpuToolPath.string().c_str(),
                              gpuToolPath.string().c_str(),
                              "--batch",
                              tempFile.string().c_str(),
                              cacheDir.c_str(),
                              NULL);
    #else
    // Unix fallback
    std::string command = "\"" + gpuToolPath.string() + "\" --batch \"" + 
                          tempFile.string() + "\" \"" + cacheDir + "\"";
    int result = system(command.c_str());
    #endif
    
    // Clean up temp file
    std::filesystem::remove(tempFile);
    
    // Regardless of exit code, check which caches were created
    // (GPU tool may partially succeed even if some fonts fail)
    int cachedCount = 0;
    {
      std::lock_guard<std::mutex> lock(fontsMutex);
      for (auto& pair : fonts) {
        if (!pair.second.isCached) {
          std::string cacheFile = cacheDir + "/" + getCacheFilename(pair.second.path);
          if (std::filesystem::exists(cacheFile)) {
            pair.second.isCached = true;
            cachedCount++;
          }
        }
      }
    }
    
    if (result == 0) {
      std::cout << "MSDF: GPU caching complete (" << cachedCount << " new caches)" << std::endl;
    } else {
      // Some fonts may have failed (e.g. symbol fonts), but that's expected
      // Those fonts will also fail on CPU, so no point in falling back
      std::cout << "MSDF: GPU caching done (exit " << result << ", " << cachedCount << " cached)" << std::endl;
    }
    // Return count of newly cached fonts (0 is fine - means all done or all failed)
    // Only return -1 if GPU tool wasn't found (handled above)
    return cachedCount;
  }
  
  // Mark a font path as cached (called from thread pool)
  void markPathAsCached(const std::string& path) {
    std::lock_guard<std::mutex> lock(fontsMutex);
    std::lock_guard<std::mutex> cacheLock(cachingMutex);
    
    // Update all font entries with this path
    for (auto& pair : fonts) {
      if (pair.second.path == path) {
        pair.second.isCached = true;
      }
    }
    
    pathsBeingCached.erase(path);
  }
  
  // Internal: ensure a font entry is actually loaded (lazy load)
  MSDFFont* ensureLoaded(FontEntry& entry) {
    if (entry.font && entry.font->isLoaded()) {
      return entry.font.get();
    }
    if (entry.loadAttempted) {
      return nullptr;  // Already tried and failed
    }
    entry.loadAttempted = true;
    entry.font = std::make_unique<MSDFFont>();
    
    // First try fast cache-only load (no CPU generation)
    if (entry.font->loadFromCacheOnly(entry.path)) {
      entry.isCached = true;
      return entry.font.get();
    }
    
    // Fall back to full load (may generate atlas with CPU if not cached)
    entry.font->loadFont(entry.path);
    if (entry.font->isLoaded()) {
      entry.isCached = true;  // It's now cached (loadFont saves to cache)
      return entry.font.get();
    }
    entry.font.reset();
    return nullptr;
  }
  
public:
  // Convert from old Font.hpp enum types
  MSDFFont* getFont(const std::string& fontFamily, int fontWeight = 0, int fontStyle = 0) {
    MSDFFontWeight w = (fontWeight == 1 || fontWeight == 3) ? MSDFFontWeight::Bold : MSDFFontWeight::Normal;
    MSDFFontStyle s = (fontStyle == 1 || fontStyle == 2) ? MSDFFontStyle::Italic : MSDFFontStyle::Normal;
    return getFontInternal(fontFamily, w, s);
  }
  
  MSDFFont* getFontInternal(const std::string& fontFamily, MSDFFontWeight weight, MSDFFontStyle style) {
    std::lock_guard<std::mutex> lock(fontsMutex);
    std::vector<std::string> families = parseFontFamily(fontFamily);
    
    for (const auto& family : families) {
      std::string key = makeFontKey(family, weight, style);
      auto it = fonts.find(key);
      if (it != fonts.end()) {
        MSDFFont* font = ensureLoaded(it->second);
        if (font) return font;
      }
      
      // Fallback without italic
      if (style != MSDFFontStyle::Normal) {
        key = makeFontKey(family, weight, MSDFFontStyle::Normal);
        it = fonts.find(key);
        if (it != fonts.end()) {
          MSDFFont* font = ensureLoaded(it->second);
          if (font) return font;
        }
      }
      
      // Fallback to regular
      key = makeFontKey(family, MSDFFontWeight::Normal, MSDFFontStyle::Normal);
      it = fonts.find(key);
      if (it != fonts.end()) {
        MSDFFont* font = ensureLoaded(it->second);
        if (font) return font;
      }
    }
    
    // Fallback to serif
    std::string key = makeFontKey("serif", MSDFFontWeight::Normal, MSDFFontStyle::Normal);
    auto it = fonts.find(key);
    if (it != fonts.end()) {
      MSDFFont* font = ensureLoaded(it->second);
      if (font) return font;
    }
    
    // Last resort: try any registered font
    for (auto& pair : fonts) {
      MSDFFont* font = ensureLoaded(pair.second);
      if (font) return font;
    }
    
    return nullptr;
  }
  
  MSDFFont* getDefaultFont() {
    return getFont("serif");
  }

private:
  std::string toLower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
  }
  
  std::vector<std::string> parseFontFamily(const std::string& fontFamily) const {
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;
    char quoteChar = 0;
    
    for (size_t i = 0; i < fontFamily.length(); ++i) {
      char c = fontFamily[i];
      
      if (!inQuotes && (c == '"' || c == '\'')) {
        inQuotes = true;
        quoteChar = c;
      } else if (inQuotes && c == quoteChar) {
        inQuotes = false;
        quoteChar = 0;
      } else if (!inQuotes && c == ',') {
        std::string trimmed = trim(current);
        if (!trimmed.empty()) {
          result.push_back(trimmed);
        }
        current.clear();
      } else {
        current += c;
      }
    }
    
    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
      result.push_back(trimmed);
    }
    
    return result;
  }
  
  std::string trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
  }
};

} // namespace skene
