#pragma once

#include "MSDFFont.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cmath>
#include <iostream>
#include <vector>

// OpenGL 2.0+ shader function types (not in SDL_opengl.h on Windows)
#ifdef _WIN32
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const* string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRY *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY *PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);

// Global function pointers (initialized once)
static PFNGLCREATESHADERPROC glCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
static PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
static PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC glAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
static PFNGLDELETESHADERPROC glDeleteShader = nullptr;
static PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
static PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
static PFNGLUNIFORM1IPROC glUniform1i = nullptr;
static PFNGLUNIFORM1FPROC glUniform1f = nullptr;
static PFNGLUNIFORM2FPROC glUniform2f = nullptr;
static PFNGLUNIFORM4FPROC glUniform4f = nullptr;
static PFNGLACTIVETEXTUREPROC glActiveTexture_ptr = nullptr;

static bool loadGLFunctions() {
  static bool loaded = false;
  if (loaded) return true;
  
  glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
  glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
  glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
  glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
  glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
  glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
  glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
  glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
  glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
  glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
  glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
  glDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram");
  glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
  glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
  glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
  glUniform1f = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");
  glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
  glUniform4f = (PFNGLUNIFORM4FPROC)SDL_GL_GetProcAddress("glUniform4f");
  glActiveTexture_ptr = (PFNGLACTIVETEXTUREPROC)SDL_GL_GetProcAddress("glActiveTexture");
  
  loaded = (glCreateShader && glShaderSource && glCompileShader && 
            glGetShaderiv && glCreateProgram && glAttachShader &&
            glLinkProgram && glUseProgram && glGetUniformLocation);
  
  return loaded;
}
#else
// On other platforms, these are typically available directly
static bool loadGLFunctions() { return true; }
#define glActiveTexture_ptr glActiveTexture
#endif

namespace skene {

// Vertex with position and color for batching
struct ColorVertex {
  float x, y;
  float r, g, b, a;
};

class Renderer {
  int screenWidth, screenHeight;
  float globalOpacity = 1.0f;
  float translateX = 0.0f;
  float translateY = 0.0f;
  
  // Clip rect stack for nested scrollable elements
  struct ClipRect {
    int x, y, w, h;
  };
  std::vector<ClipRect> clipStack;
  
  // Batched rect rendering
  std::vector<ColorVertex> rectBatch;
  bool batchingEnabled = true;
  
  // MSDF shader program
  GLuint msdfShaderProgram = 0;
  GLint msdfUniformTex = -1;
  GLint msdfUniformPxRange = -1;
  GLint msdfUniformColor = -1;
  GLint msdfUniformTexelSize = -1;
  GLint msdfUniformEdgeLow = -1;
  GLint msdfUniformEdgeHigh = -1;
  bool msdfShaderInitialized = false;
  
  // Smoothstep edge parameters (adjustable via UI)
float msdfEdgeLow = -0.5f;
    float msdfEdgeHigh = 0.42f;

public:
  Renderer(int w, int h) : screenWidth(w), screenHeight(h) {
    rectBatch.reserve(4096); // Pre-allocate for ~1000 rects
    initMSDFShader();
  }

  ~Renderer() {
    if (msdfShaderProgram) {
      glDeleteProgram(msdfShaderProgram);
    }
  }

  void setOpacity(float opacity) { globalOpacity = opacity; }
  
  // Translation for scrolling
  void pushTranslate(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    translateX += x;
    translateY += y;
  }
  
  void popTranslate(float x, float y) {
    glPopMatrix();
    translateX -= x;
    translateY -= y;
  }
  
  float getTranslateY() const { return translateY; }

  void clear() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);

    glViewport(0, 0, screenWidth, screenHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (GLdouble)screenWidth, (GLdouble)screenHeight, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Enable blending once for entire frame - don't toggle per draw call
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Clear batch for new frame
    rectBatch.clear();
  }
  
  // Flush all batched rects to GPU
  void flushRects() {
    if (rectBatch.empty()) return;
    
    glBegin(GL_QUADS);
    for (const auto& v : rectBatch) {
      glColor4f(v.r, v.g, v.b, v.a);
      glVertex2f(v.x, v.y);
    }
    glEnd();
    
    rectBatch.clear();
  }
  
  // Call at end of frame before swap
  void endFrame() {
    flushRects();
  }

  // MSDF text rendering - the only text rendering method
  void drawText(float x, float y, const std::string &text, MSDFFont &font,
                float r, float g, float b, float a, float fontSize = 16.0f) {
    drawTextRangeMSDF(x, y, text, font, r, g, b, a, fontSize, 0, text.length());
  }

  void drawRect(float x, float y, float w, float h, float r, float g, float b,
                float a) {
    if (a <= 0)
      return;

    float finalA = a * globalOpacity;
    
    // Add 4 vertices (quad) to batch
    rectBatch.push_back({x, y, r, g, b, finalA});
    rectBatch.push_back({x + w, y, r, g, b, finalA});
    rectBatch.push_back({x + w, y + h, r, g, b, finalA});
    rectBatch.push_back({x, y + h, r, g, b, finalA});
  }

  void drawRectOutline(float x, float y, float w, float h, float r, float g,
                       float b, float a) {
    if (a <= 0)
      return;

    // Flush batched rects before outline to maintain draw order
    flushRects();
    
    glColor4f(r, g, b, a * globalOpacity);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
  }

  // Draw a border with specified widths for each side (single color)
  void drawBorder(float x, float y, float w, float h, float topWidth,
                  float rightWidth, float bottomWidth, float leftWidth,
                  float r, float g, float b, float a) {
    // Call per-side color version with same color for all sides
    drawBorderPerSide(x, y, w, h, topWidth, rightWidth, bottomWidth, leftWidth,
                      r, g, b, a,  // top
                      r, g, b, a,  // right
                      r, g, b, a,  // bottom
                      r, g, b, a); // left
  }

  // Draw a border with per-side colors
  void drawBorderPerSide(float x, float y, float w, float h, 
                         float topWidth, float rightWidth, float bottomWidth, float leftWidth,
                         float topR, float topG, float topB, float topA,
                         float rightR, float rightG, float rightB, float rightA,
                         float bottomR, float bottomG, float bottomB, float bottomA,
                         float leftR, float leftG, float leftB, float leftA) {
    // Flush batched rects before borders to maintain draw order
    flushRects();
    
    // Top border
    if (topWidth > 0 && topA > 0) {
      glColor4f(topR, topG, topB, topA * globalOpacity);
      glBegin(GL_QUADS);
      glVertex2f(x, y);
      glVertex2f(x + w, y);
      glVertex2f(x + w, y + topWidth);
      glVertex2f(x, y + topWidth);
      glEnd();
    }

    // Bottom border
    if (bottomWidth > 0 && bottomA > 0) {
      glColor4f(bottomR, bottomG, bottomB, bottomA * globalOpacity);
      glBegin(GL_QUADS);
      glVertex2f(x, y + h - bottomWidth);
      glVertex2f(x + w, y + h - bottomWidth);
      glVertex2f(x + w, y + h);
      glVertex2f(x, y + h);
      glEnd();
    }

    // Left border
    if (leftWidth > 0 && leftA > 0) {
      glColor4f(leftR, leftG, leftB, leftA * globalOpacity);
      glBegin(GL_QUADS);
      glVertex2f(x, y + topWidth);
      glVertex2f(x + leftWidth, y + topWidth);
      glVertex2f(x + leftWidth, y + h - bottomWidth);
      glVertex2f(x, y + h - bottomWidth);
      glEnd();
    }

    // Right border
    if (rightWidth > 0 && rightA > 0) {
      glColor4f(rightR, rightG, rightB, rightA * globalOpacity);
      glBegin(GL_QUADS);
      glVertex2f(x + w - rightWidth, y + topWidth);
      glVertex2f(x + w, y + topWidth);
      glVertex2f(x + w, y + h - bottomWidth);
      glVertex2f(x + w - rightWidth, y + h - bottomWidth);
      glEnd();
    }
  }

  // Draw rounded rectangle
  void drawRoundedRect(float x, float y, float w, float h, float radius,
                       float r, float g, float b, float a) {
    if (a <= 0)
      return;

    // Flush batched rects before rounded rect to maintain draw order
    flushRects();
    
    const int segments = 16;
    radius = std::min(radius, std::min(w, h) / 2.0f);

    glColor4f(r, g, b, a * globalOpacity);

    // Draw main rectangle (without corners)
    glBegin(GL_QUADS);
    // Center
    glVertex2f(x + radius, y);
    glVertex2f(x + w - radius, y);
    glVertex2f(x + w - radius, y + h);
    glVertex2f(x + radius, y + h);
    // Left
    glVertex2f(x, y + radius);
    glVertex2f(x + radius, y + radius);
    glVertex2f(x + radius, y + h - radius);
    glVertex2f(x, y + h - radius);
    // Right
    glVertex2f(x + w - radius, y + radius);
    glVertex2f(x + w, y + radius);
    glVertex2f(x + w, y + h - radius);
    glVertex2f(x + w - radius, y + h - radius);
    glEnd();

    // Draw corners
    auto drawCorner = [&](float cx, float cy, float startAngle) {
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(cx, cy);
      for (int i = 0; i <= segments; i++) {
        float angle = startAngle + (3.14159f / 2.0f) * i / segments;
        glVertex2f(cx + std::cos(angle) * radius,
                   cy + std::sin(angle) * radius);
      }
      glEnd();
    };

    drawCorner(x + radius, y + radius, 3.14159f);          // Top-left
    drawCorner(x + w - radius, y + radius, -3.14159f / 2); // Top-right
    drawCorner(x + w - radius, y + h - radius, 0);         // Bottom-right
    drawCorner(x + radius, y + h - radius, 3.14159f / 2);  // Bottom-left
  }

  // Draw underline for text decoration
  void drawLine(float x1, float y1, float x2, float y2, float thickness,
                float r, float g, float b, float a) {
    // Flush batched rects before line to maintain draw order
    flushRects();
    
    glLineWidth(thickness);
    glColor4f(r, g, b, a * globalOpacity);

    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();

    glLineWidth(1.0f);
  }

  // Set scissor region for overflow clipping (supports nesting via stack)
  // Coordinates are in content space, will be transformed to screen space
  void setClipRect(float x, float y, float w, float h) {
    // Transform from content space to screen space using current translation
    float screenX = x + translateX;
    float screenY = y + translateY;
    
    // Convert to OpenGL scissor coordinates (bottom-left origin)
    int clipX = (int)screenX;
    int clipY = screenHeight - (int)(screenY + h);
    int clipW = (int)w;
    int clipH = (int)h;
    
    // If there's a parent clip rect, intersect with it
    if (!clipStack.empty()) {
      const auto& parent = clipStack.back();
      int newX = std::max(clipX, parent.x);
      int newY = std::max(clipY, parent.y);
      int newRight = std::min(clipX + clipW, parent.x + parent.w);
      int newTop = std::min(clipY + clipH, parent.y + parent.h);
      clipX = newX;
      clipY = newY;
      clipW = std::max(0, newRight - newX);
      clipH = std::max(0, newTop - newY);
    }
    
    // Push to stack and apply
    clipStack.push_back({clipX, clipY, clipW, clipH});
    glEnable(GL_SCISSOR_TEST);
    glScissor(clipX, clipY, clipW, clipH);
  }

  void clearClipRect() {
    if (!clipStack.empty()) {
      clipStack.pop_back();
    }
    
    if (clipStack.empty()) {
      glDisable(GL_SCISSOR_TEST);
    } else {
      // Restore parent clip rect
      const auto& parent = clipStack.back();
      glScissor(parent.x, parent.y, parent.w, parent.h);
    }
  }

  void resize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
  }

  int getWidth() const { return screenWidth; }
  int getHeight() const { return screenHeight; }

  // MSDF text rendering
  void drawTextMSDF(float x, float y, const std::string &text, MSDFFont &font,
                    float r, float g, float b, float a, float fontSize) {
    drawTextRangeMSDF(x, y, text, font, r, g, b, a, fontSize, 0, text.length());
  }

  void drawTextRangeMSDF(float x, float y, const std::string &text, MSDFFont &font,
                         float r, float g, float b, float a, float fontSize,
                         size_t startIdx, size_t endIdx) {
    if (!msdfShaderInitialized || text.empty()) return;
    
    // Flush batched rects before text to maintain draw order
    flushRects();
    
    float scale = fontSize / font.getGlyphSize();
    float pxRange = font.getPixelRange();
    
    // Calculate screen pixel range for MSDF rendering
    // For crisp text, we need at least ~2px range on screen
    float screenPxRange = std::max(2.0f, pxRange * scale);
    
    // Snap baseline to pixel boundary for sharp text rendering
    // In OpenGL, pixel centers are at 0.5 offsets, so we round to integers
    float snappedX = std::floor(x + 0.5f);
    float snappedY = std::floor(y + 0.5f);
    
    // Use MSDF shader
    glUseProgram(msdfShaderProgram);
    glUniform1i(msdfUniformTex, 0);
    glUniform1f(msdfUniformPxRange, screenPxRange);
    glUniform4f(msdfUniformColor, r, g, b, a * globalOpacity);
    glUniform1f(msdfUniformEdgeLow, msdfEdgeLow);
    glUniform1f(msdfUniformEdgeHigh, msdfEdgeHigh);
    
    // Enable blending for smooth MSDF edges
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_TEXTURE_2D);
    if (glActiveTexture_ptr) glActiveTexture_ptr(GL_TEXTURE0);
    font.bind();
    
    glBegin(GL_QUADS);
    float currentX = 0;
    size_t charIndex = 0;
    
    for (size_t i = 0; i < text.length(); ++i) {
      int cp = MSDFFont::decodeUTF8(text, i);
      if (cp < 32) {
        charIndex++;
        continue;
      }
      
      const MSDFGlyph* glyph = font.getGlyph(cp);
      if (!glyph || !glyph->valid) {
        charIndex++;
        continue;
      }
      
      // Only draw if within the specified range
      if (charIndex >= startIdx && charIndex < endIdx && glyph->width > 0) {
        float x0 = snappedX + currentX + glyph->xoff * scale;
        float y0 = snappedY + glyph->yoff * scale;
        float x1 = x0 + glyph->width * scale;
        float y1 = y0 + glyph->height * scale;
        
        glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(x0, y0);
        glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(x1, y0);
        glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(x1, y1);
        glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(x0, y1);
      }
      
      currentX += glyph->advance * scale;
      charIndex++;
    }
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glUseProgram(0);
  }

  // Draw MSDF text with selection highlighting - two-pass for correct coloring
  void drawTextWithSelectionMSDF(float x, float y, const std::string &text, MSDFFont &font,
                                  float r, float g, float b, float a, float fontSize,
                                  size_t selStart, size_t selEnd,
                                  float selR, float selG, float selB, float selA) {
    if (!msdfShaderInitialized || text.empty()) return;
    
    // Flush batched rects before text to maintain draw order
    flushRects();
    
    float scale = fontSize / font.getGlyphSize();
    float pxRange = font.getPixelRange();
    
    // Calculate screen pixel range for MSDF rendering
    float screenPxRange = std::max(2.0f, pxRange * scale);
    
    // Snap baseline to pixel boundary for sharp text rendering
    float snappedX = std::floor(x + 0.5f);
    float snappedY = std::floor(y + 0.5f);
    
    // Use MSDF shader
    glUseProgram(msdfShaderProgram);
    glUniform1i(msdfUniformTex, 0);
    glUniform1f(msdfUniformPxRange, screenPxRange);
    
    // Enable blending for smooth MSDF edges
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_TEXTURE_2D);
    if (glActiveTexture_ptr) glActiveTexture_ptr(GL_TEXTURE0);
    font.bind();
    
    // First pass: draw non-selected characters
    glUniform4f(msdfUniformColor, r, g, b, a * globalOpacity);
    glBegin(GL_QUADS);
    float currentX = 0;
    size_t charIndex = 0;
    
    for (size_t i = 0; i < text.length(); ++i) {
      int cp = MSDFFont::decodeUTF8(text, i);
      if (cp < 32) {
        charIndex++;
        continue;
      }
      
      const MSDFGlyph* glyph = font.getGlyph(cp);
      if (!glyph || !glyph->valid) {
        charIndex++;
        continue;
      }
      
      // Only draw non-selected characters in first pass
      if (glyph->width > 0 && !(charIndex >= selStart && charIndex < selEnd)) {
        float x0 = snappedX + currentX + glyph->xoff * scale;
        float y0 = snappedY + glyph->yoff * scale;
        float x1 = x0 + glyph->width * scale;
        float y1 = y0 + glyph->height * scale;
        
        glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(x0, y0);
        glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(x1, y0);
        glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(x1, y1);
        glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(x0, y1);
      }
      
      currentX += glyph->advance * scale;
      charIndex++;
    }
    glEnd();
    
    // Second pass: draw selected characters with selection color
    if (selStart < selEnd) {
      glUniform4f(msdfUniformColor, selR, selG, selB, selA * globalOpacity);
      glBegin(GL_QUADS);
      currentX = 0;
      charIndex = 0;
      
      for (size_t i = 0; i < text.length(); ++i) {
        int cp = MSDFFont::decodeUTF8(text, i);
        if (cp < 32) {
          charIndex++;
          continue;
        }
        
        const MSDFGlyph* glyph = font.getGlyph(cp);
        if (!glyph || !glyph->valid) {
          charIndex++;
          continue;
        }
        
        // Only draw selected characters in second pass
        if (glyph->width > 0 && (charIndex >= selStart && charIndex < selEnd)) {
          float x0 = snappedX + currentX + glyph->xoff * scale;
          float y0 = snappedY + glyph->yoff * scale;
          float x1 = x0 + glyph->width * scale;
          float y1 = y0 + glyph->height * scale;
          
          glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(x0, y0);
          glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(x1, y0);
          glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(x1, y1);
          glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(x0, y1);
        }
        
        currentX += glyph->advance * scale;
        charIndex++;
      }
      glEnd();
    }
    
    glDisable(GL_TEXTURE_2D);
    glUseProgram(0);
  }

private:
  void initMSDFShader() {
    // Load OpenGL 2.0+ functions
    if (!loadGLFunctions()) {
      std::cerr << "MSDF: Could not load OpenGL shader functions" << std::endl;
      return;
    }
    
    // Check for shader support (OpenGL 2.0+)
    const char* version = (const char*)glGetString(GL_VERSION);
    if (!version) {
      std::cerr << "MSDF: Could not get OpenGL version" << std::endl;
      return;
    }
    
    // MSDF vertex shader
    const char* vertexShaderSrc = R"(
      #version 120
      varying vec2 vTexCoord;
      void main() {
        vTexCoord = gl_MultiTexCoord0.xy;
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
      }
    )";
    
    // MSDF fragment shader
    const char* fragmentShaderSrc = R"(
      #version 120
      uniform sampler2D msdfTex;
      uniform float pxRange;
      uniform vec4 textColor;
      uniform float edgeLow;
      uniform float edgeHigh;
      varying vec2 vTexCoord;
      
      float median(float r, float g, float b) {
        return max(min(r, g), min(max(r, g), b));
      }
      
      void main() {
        vec3 msd = texture2D(msdfTex, vTexCoord).rgb;
        float sd = median(msd.r, msd.g, msd.b);
        float screenPxDistance = pxRange * (sd - 0.5);
        // Crisp but smooth edge - adjustable via uniforms
        float opacity = smoothstep(edgeLow, edgeHigh, screenPxDistance);
        gl_FragColor = vec4(textColor.rgb, textColor.a * opacity);
      }
    )";
    
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vertexShader);
    
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
      std::cerr << "MSDF: Vertex shader error: " << infoLog << std::endl;
      glDeleteShader(vertexShader);
      return;
    }
    
    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
      std::cerr << "MSDF: Fragment shader error: " << infoLog << std::endl;
      glDeleteShader(vertexShader);
      glDeleteShader(fragmentShader);
      return;
    }
    
    // Link program
    msdfShaderProgram = glCreateProgram();
    glAttachShader(msdfShaderProgram, vertexShader);
    glAttachShader(msdfShaderProgram, fragmentShader);
    glLinkProgram(msdfShaderProgram);
    
    glGetProgramiv(msdfShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetProgramInfoLog(msdfShaderProgram, 512, nullptr, infoLog);
      std::cerr << "MSDF: Shader link error: " << infoLog << std::endl;
      glDeleteShader(vertexShader);
      glDeleteShader(fragmentShader);
      glDeleteProgram(msdfShaderProgram);
      msdfShaderProgram = 0;
      return;
    }
    
    // Clean up shaders (linked into program)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Get uniform locations
    msdfUniformTex = glGetUniformLocation(msdfShaderProgram, "msdfTex");
    msdfUniformPxRange = glGetUniformLocation(msdfShaderProgram, "pxRange");
    msdfUniformColor = glGetUniformLocation(msdfShaderProgram, "textColor");
    msdfUniformTexelSize = glGetUniformLocation(msdfShaderProgram, "texelSize");
    msdfUniformEdgeLow = glGetUniformLocation(msdfShaderProgram, "edgeLow");
    msdfUniformEdgeHigh = glGetUniformLocation(msdfShaderProgram, "edgeHigh");
    
    msdfShaderInitialized = true;
    std::cout << "MSDF: Shader initialized successfully" << std::endl;
  }
  
public:
  // Getters/setters for edge parameters
  float getMsdfEdgeLow() const { return msdfEdgeLow; }
  float getMsdfEdgeHigh() const { return msdfEdgeHigh; }
  void setMsdfEdgeLow(float val) { msdfEdgeLow = val; }
  void setMsdfEdgeHigh(float val) { msdfEdgeHigh = val; }
};

} // namespace skene
