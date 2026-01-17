// STB implementation - define the implementation flag only if not already done
// This must be before any includes that use stb_truetype.h
#define STB_TRUETYPE_IMPLEMENTATION
#include "render/stb/stb_truetype.h"
// Undefine after include to prevent the implementation from being compiled again
#undef STB_TRUETYPE_IMPLEMENTATION

#include "layout/RenderTree.hpp"
#include "parser/HtmlParser.hpp"
#include "render/Renderer.hpp"
#include "style/StyleSheet.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

int screenWidth = 1024;
int screenHeight = 600;
const int INSPECTOR_WIDTH = 300; // Wider inspector

// Global pointers for resize event watcher
SDL_Window* g_window = nullptr;
skene::Renderer* g_renderer = nullptr;
skene::RenderTree* g_renderTree = nullptr;
skene::StyleSheet* g_styleSheet = nullptr;
skene::MSDFFontManager* g_fontManager = nullptr;  // MSDF font manager for sharp text
std::shared_ptr<skene::Node> g_dom = nullptr;
bool g_needsRender = false;

// Forward declaration
void doRender();

struct InspectorLine {
  float y, h;
  std::shared_ptr<skene::Node> node;
};
std::vector<InspectorLine> inspectorLines; // Rebuilt every frame/paint? Or just
                                           // computed during paintInspector

std::shared_ptr<skene::Node> selectedNode = nullptr;

// Text selection state
skene::TextSelection textSelection;

// Selection mode for word/line-wise selection during drag
enum class SelectionMode { Character, Word, Line };
SelectionMode selectionMode = SelectionMode::Character;

// Store the anchor word boundaries for word-wise selection
size_t anchorWordStart = 0;
size_t anchorWordEnd = 0;

// Click tracking for double/triple click detection
Uint32 lastClickTime = 0;
int lastClickX = 0, lastClickY = 0;
int clickCount = 0;
const Uint32 DOUBLE_CLICK_TIME = 500; // ms
const int DOUBLE_CLICK_DISTANCE = 5; // pixels

// Scroll state
float scrollY = 0.0f;
float maxScrollY = 0.0f;

// Debug/FPS tracking
Uint32 fpsLastTime = 0;
int fpsFrameCount = 0;
float fpsCurrent = 0.0f;
float frameTimeMs = 0.0f;
Uint32 frameStartTime = 0;
const float SCROLL_SPEED = 40.0f;

// Sidebar tab state
enum class SidebarTab { Inspector, Performance };
SidebarTab currentSidebarTab = SidebarTab::Inspector;
const float TAB_HEIGHT = 30.0f;

// VSync state
bool vsyncEnabled = true;

// Checkbox bounds for click detection in Performance panel
struct CheckboxBounds {
  float x, y, width, height;
  bool isValid = false;
};
CheckboxBounds vsyncCheckbox;

// Slider bounds for click/drag detection
struct SliderBounds {
  float x, y, width, height;
  float minVal, maxVal;
  float* valuePtr;
  bool isValid = false;
  bool isDragging = false;
};
SliderBounds edgeLowSlider;
SliderBounds edgeHighSlider;
SliderBounds* activeSlider = nullptr;

// Helper: check if character is a word boundary
// Apostrophe is NOT a boundary when between letters (for "don't", "it's", etc.)
bool isWordBoundaryAt(const std::string &text, size_t idx) {
  if (idx >= text.length()) return true;
  char c = text[idx];
  
  // Whitespace is always a boundary
  if (std::isspace((unsigned char)c)) return true;
  
  // Apostrophe: check if it's within a word (letter before AND after)
  if (c == '\'' || c == '\xe2') { // \xe2 is start of UTF-8 curly apostrophe
    bool hasBefore = idx > 0 && std::isalpha((unsigned char)text[idx - 1]);
    bool hasAfter = idx + 1 < text.length() && std::isalpha((unsigned char)text[idx + 1]);
    if (hasBefore && hasAfter) return false; // Part of word like "don't"
  }
  
  // Other punctuation is a boundary
  if (std::ispunct((unsigned char)c)) return true;
  
  return false;
}

// Find word boundaries around a character index in text
std::pair<size_t, size_t> findWordBoundaries(const std::string &text, size_t charIdx) {
  if (text.empty()) return {0, 0};
  if (charIdx > text.length()) charIdx = text.length();
  
  // If at end or on a boundary, adjust
  if (charIdx == text.length()) charIdx = text.length() - 1;
  
  // If clicking on whitespace/punct, select just that
  if (isWordBoundaryAt(text, charIdx)) {
    return {charIdx, charIdx + 1};
  }
  
  // Find start of word
  size_t start = charIdx;
  while (start > 0 && !isWordBoundaryAt(text, start - 1)) {
    --start;
  }
  
  // Find end of word
  size_t end = charIdx;
  while (end < text.length() && !isWordBoundaryAt(text, end)) {
    ++end;
  }
  
  // Include trailing whitespace (browser behavior)
  while (end < text.length() && std::isspace((unsigned char)text[end])) {
    ++end;
  }
  
  return {start, end};
}

// Find word boundaries across adjacent text boxes in inline flow
// This handles the case where double-clicking on an inline element should
// consider adjacent text as part of the word selection (Chrome behavior)
struct CrossBoxWordSelection {
  std::shared_ptr<skene::RenderBox> startBox;
  std::shared_ptr<skene::RenderBox> endBox;
  size_t startLineIdx;
  size_t endLineIdx;
  size_t startCharIdx;
  size_t endCharIdx;
};

CrossBoxWordSelection findWordBoundariesAcrossBoxes(
    std::shared_ptr<skene::RenderBox> clickedBox,
    size_t lineIdx, size_t charIdx,
    const std::vector<std::shared_ptr<skene::RenderBox>> &allTextBoxes) {
  
  CrossBoxWordSelection result;
  result.startBox = clickedBox;
  result.endBox = clickedBox;
  result.startLineIdx = lineIdx;
  result.endLineIdx = lineIdx;
  
  if (clickedBox->textLines.empty() || lineIdx >= clickedBox->textLines.size()) {
    result.startCharIdx = 0;
    result.endCharIdx = 0;
    return result;
  }
  
  const auto &line = clickedBox->textLines[lineIdx];
  
  // Find word boundaries within the current text box only
  // Chrome behavior: double-click selects only the word, not adjacent punctuation
  auto [wordStart, wordEnd] = findWordBoundaries(line.text, charIdx);
  result.startCharIdx = wordStart;
  result.endCharIdx = wordEnd;
  
  return result;
}

// Find the block-level ancestor of a node (p, div, h1-h6, li, etc.)
std::shared_ptr<skene::Node> findBlockAncestor(std::shared_ptr<skene::Node> node) {
  if (!node) return nullptr;
  
  auto current = node->parent.lock();
  while (current) {
    // Check if this is a block-level element
    const std::string &tag = current->tagName;
    if (tag == "p" || tag == "div" || tag == "li" || tag == "td" || tag == "th" ||
        tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6" ||
        tag == "blockquote" || tag == "pre" || tag == "article" || tag == "section" ||
        tag == "header" || tag == "footer" || tag == "main" || tag == "nav" || tag == "aside") {
      return current;
    }
    current = current->parent.lock();
  }
  return nullptr;
}

// Check if a node is a descendant of another node
bool isDescendantOf(std::shared_ptr<skene::Node> node, std::shared_ptr<skene::Node> ancestor) {
  if (!node || !ancestor) return false;
  
  auto current = node->parent.lock();
  while (current) {
    if (current == ancestor) return true;
    current = current->parent.lock();
  }
  return false;
}

// Find all text boxes that belong to the same block-level element
std::pair<std::shared_ptr<skene::RenderBox>, std::shared_ptr<skene::RenderBox>> 
findBlockTextBoxRange(std::shared_ptr<skene::RenderBox> clickedBox,
                      const std::vector<std::shared_ptr<skene::RenderBox>> &allTextBoxes) {
  if (!clickedBox || !clickedBox->node) {
    return {clickedBox, clickedBox};
  }
  
  // Find the block-level ancestor
  auto blockAncestor = findBlockAncestor(clickedBox->node);
  if (!blockAncestor) {
    // No block ancestor found, just select this box
    return {clickedBox, clickedBox};
  }
  
  // Find all text boxes that are descendants of this block ancestor
  std::shared_ptr<skene::RenderBox> firstBox = nullptr;
  std::shared_ptr<skene::RenderBox> lastBox = nullptr;
  
  for (const auto &box : allTextBoxes) {
    if (box->node && isDescendantOf(box->node, blockAncestor)) {
      if (!firstBox) firstBox = box;
      lastBox = box;
    }
  }
  
  if (!firstBox) firstBox = clickedBox;
  if (!lastBox) lastBox = clickedBox;
  
  return {firstBox, lastBox};
}

// Find link (<a>) ancestor of a node and return its href
std::string findLinkHref(std::shared_ptr<skene::Node> node) {
  if (!node) return "";
  
  auto current = node;
  while (current) {
    if (current->type == skene::NodeType::Element) {
      std::string tag = current->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      if (tag == "a") {
        auto it = current->attributes.find("href");
        if (it != current->attributes.end()) {
          return it->second;
        }
        return "#";  // Link without href
      }
    }
    current = current->parent.lock();
  }
  return "";  // Not a link
}

// Check if a RenderBox (or its node) is inside a link
bool isInsideLink(std::shared_ptr<skene::RenderBox> box) {
  if (!box || !box->node) return false;
  return !findLinkHref(box->node).empty();
}

// Find the RenderBox at a point (for hover detection)
std::shared_ptr<skene::RenderBox> findBoxAtPoint(
    std::shared_ptr<skene::RenderBox> box, float x, float y, float scrollOffsetY = 0) {
  if (!box) return nullptr;
  
  float adjustedY = y + scrollOffsetY;
  skene::Rect borderBox = box->box.borderBox();
  
  bool inBounds = x >= borderBox.x && x < borderBox.x + borderBox.width &&
                  adjustedY >= borderBox.y && adjustedY < borderBox.y + borderBox.height;
  
  if (!inBounds) return nullptr;
  
  // Check children first (reverse order for z-order)
  for (auto it = box->children.rbegin(); it != box->children.rend(); ++it) {
    float childScrollY = scrollOffsetY + box->scrollY;
    auto result = findBoxAtPoint(*it, x, y, childScrollY);
    if (result) return result;
  }
  
  // Return this box if we're in bounds
  return box;
}

// Collect all text boxes in document order (recursive)
void collectTextBoxes(std::shared_ptr<skene::RenderBox> box, 
                      std::vector<std::shared_ptr<skene::RenderBox>> &textBoxes,
                      bool debug = false) {
  if (!box) return;
  
  if (box->node && box->node->type == skene::NodeType::Text) {
    if (debug) {
      auto parent = box->node->parent.lock();
      std::string parentTag = parent ? parent->tagName : "none";
      std::cout << "Text node: \"" << box->node->textContent.substr(0, 30) 
                << "\" parent=<" << parentTag << "> textLines=" << box->textLines.size()
                << " frame=[" << box->frame.x << "," << box->frame.y << ","
                << box->frame.width << "," << box->frame.height << "]";
      if (!box->textLines.empty()) {
        auto& line = box->textLines[0];
        std::cout << " line0=[" << line.x << "," << line.y << "," 
                  << line.width << "," << line.height << "]";
      }
      std::cout << std::endl;
    }
    if (!box->textLines.empty()) {
      textBoxes.push_back(box);
    }
  }
  
  for (auto &child : box->children) {
    collectTextBoxes(child, textBoxes, debug);
  }
}

// Helper function to find text box at exact point (recursive)
std::shared_ptr<skene::RenderBox> findTextBoxAtExact(
    std::shared_ptr<skene::RenderBox> box, float x, float y, skene::MSDFFontManager &fontManager,
    size_t &lineIndex, size_t &charIndex) {
  if (!box) return nullptr;
  
  // Check children first (front-to-back, but reversed for proper z-order)
  for (auto it = box->children.rbegin(); it != box->children.rend(); ++it) {
    auto result = findTextBoxAtExact(*it, x, y, fontManager, lineIndex, charIndex);
    if (result) return result;
  }
  
  // Check this box's text
  if (box->node && box->node->type == skene::NodeType::Text && !box->textLines.empty()) {
    float fontSize = box->computedStyle.fontSize;
    skene::MSDFFont* font = fontManager.getFont(box->computedStyle.fontFamily, 
        static_cast<int>(box->computedStyle.fontWeight), static_cast<int>(box->computedStyle.fontStyle));
    if (!font) return nullptr;
    
    // Check if point is within any text line's bounding area
    for (size_t i = 0; i < box->textLines.size(); ++i) {
      const auto &line = box->textLines[i];
      // Check both vertical AND horizontal bounds
      float lineTop = line.y;
      float lineBottom = line.y + line.height;
      float lineLeft = line.x;
      float lineRight = line.x + line.width;
      
      if (y >= lineTop && y < lineBottom && x >= lineLeft && x < lineRight) {
        lineIndex = i;
        float localX = x - line.x;
        charIndex = font->hitTestText(line.text, std::max(0.0f, localX), fontSize);
        return box;
      }
    }
  }
  
  return nullptr;
}

// Find text box at vertical position during drag selection
// Prioritizes Y coordinate - finds the text box at that row regardless of X
// This ensures dragging far left/right still selects the text at that Y position
std::shared_ptr<skene::RenderBox> findTextBoxAtY(
    float x, float y, skene::MSDFFontManager &fontManager,
    size_t &lineIndex, size_t &charIndex) {
  
  if (textSelection.allTextBoxes.empty()) return nullptr;
  
  // First, collect all text lines that intersect this Y position
  struct LineCandidate {
    std::shared_ptr<skene::RenderBox> box;
    size_t lineIdx;
    float x, width;
  };
  std::vector<LineCandidate> candidatesAtY;
  
  for (auto &box : textSelection.allTextBoxes) {
    if (box->textLines.empty()) continue;
    
    for (size_t i = 0; i < box->textLines.size(); ++i) {
      const auto &line = box->textLines[i];
      float lineTop = line.y;
      float lineBottom = line.y + line.height;
      
      // Check if Y is within this line's vertical bounds
      if (y >= lineTop && y < lineBottom) {
        candidatesAtY.push_back({box, i, line.x, line.width});
      }
    }
  }
  
  // If we have candidates at this Y, find the one at X position
  if (!candidatesAtY.empty()) {
    // Sort by X position
    std::sort(candidatesAtY.begin(), candidatesAtY.end(), 
              [](const LineCandidate &a, const LineCandidate &b) { return a.x < b.x; });
    
    // Find which text box the X falls into (or the gap before/after it)
    for (size_t i = 0; i < candidatesAtY.size(); ++i) {
      const auto &cand = candidatesAtY[i];
      float lineLeft = cand.x;
      float lineRight = cand.x + cand.width;
      
      // If X is within this box's horizontal bounds, use it
      if (x >= lineLeft && x < lineRight) {
        lineIndex = cand.lineIdx;
        const auto &line = cand.box->textLines[cand.lineIdx];
        float fontSize = cand.box->computedStyle.fontSize;
        skene::MSDFFont* font = fontManager.getFont(cand.box->computedStyle.fontFamily, 
            static_cast<int>(cand.box->computedStyle.fontWeight), static_cast<int>(cand.box->computedStyle.fontStyle));
        float localX = x - line.x;
        charIndex = font ? font->hitTestText(line.text, localX, fontSize) : 0;
        return cand.box;
      }
      
      // If X is in the gap before this box (but after previous box)
      if (x < lineLeft && i > 0) {
        const auto &prev = candidatesAtY[i - 1];
        float prevRight = prev.x + prev.width;
        float gapMidpoint = (prevRight + lineLeft) / 2.0f;
        
        // Use midpoint to decide: before midpoint = end of prev, after = start of next
        if (x < gapMidpoint) {
          lineIndex = prev.lineIdx;
          charIndex = prev.box->textLines[prev.lineIdx].text.length();
          return prev.box;
        } else {
          lineIndex = cand.lineIdx;
          charIndex = 0;
          return cand.box;
        }
      }
    }
    
    // X is outside all boxes - use leftmost or rightmost
    if (x < candidatesAtY.front().x) {
      // Before first box - select start of first box
      const auto &first = candidatesAtY.front();
      lineIndex = first.lineIdx;
      charIndex = 0;
      return first.box;
    } else {
      // After last box - select end of last box
      const auto &last = candidatesAtY.back();
      lineIndex = last.lineIdx;
      charIndex = last.box->textLines[last.lineIdx].text.length();
      return last.box;
    }
  }
  
  // No line at this exact Y - find nearest by vertical distance only
  float bestDist = std::numeric_limits<float>::max();
  std::shared_ptr<skene::RenderBox> bestBox = nullptr;
  size_t bestLineIdx = 0;
  bool isBelowNearest = false;
  
  for (auto &box : textSelection.allTextBoxes) {
    if (box->textLines.empty()) continue;
    
    for (size_t i = 0; i < box->textLines.size(); ++i) {
      const auto &line = box->textLines[i];
      float lineTop = line.y;
      float lineBottom = line.y + line.height;
      float lineMidY = (lineTop + lineBottom) / 2.0f;
      
      float dist = std::abs(y - lineMidY);
      if (dist < bestDist) {
        bestDist = dist;
        bestBox = box;
        bestLineIdx = i;
        isBelowNearest = (y > lineBottom);
      }
    }
  }
  
  if (bestBox) {
    lineIndex = bestLineIdx;
    const auto &line = bestBox->textLines[bestLineIdx];
    float fontSize = bestBox->computedStyle.fontSize;
    skene::MSDFFont* font = fontManager.getFont(bestBox->computedStyle.fontFamily, 
        static_cast<int>(bestBox->computedStyle.fontWeight), static_cast<int>(bestBox->computedStyle.fontStyle));
    
    // If below the nearest line, anchor at end; if above, at start
    if (isBelowNearest) {
      charIndex = line.text.length();
    } else if (y < line.y) {
      charIndex = 0;
    } else if (x <= line.x) {
      charIndex = 0;
    } else if (x >= line.x + line.width) {
      charIndex = line.text.length();
    } else {
      float localX = x - line.x;
      charIndex = font ? font->hitTestText(line.text, localX, fontSize) : 0;
    }
    return bestBox;
  }
  
  return nullptr;
}

// Find the nearest text box to a point (for starting selection in empty space)
// The anchor position depends on where the click is relative to the nearest text:
// - Above text → anchor at START of that text (char 0)
// - Below text → anchor at END of that text (last char)
// - Left of text → anchor at START of line
// - Right of text → anchor at END of line
std::shared_ptr<skene::RenderBox> findNearestTextBox(
    float x, float y, skene::MSDFFontManager &fontManager,
    size_t &lineIndex, size_t &charIndex) {
  
  if (textSelection.allTextBoxes.empty()) return nullptr;
  
  std::shared_ptr<skene::RenderBox> bestBox = nullptr;
  float bestDistance = std::numeric_limits<float>::max();
  size_t bestLineIdx = 0;
  bool isAbove = false;
  bool isBelow = false;
  bool isLeft = false;
  bool isRight = false;
  
  for (auto &box : textSelection.allTextBoxes) {
    if (box->textLines.empty()) continue;
    
    for (size_t i = 0; i < box->textLines.size(); ++i) {
      const auto &line = box->textLines[i];
      
      // Calculate distance to this line
      float lineTop = line.y;
      float lineBottom = line.y + line.height;
      float lineLeft = line.x;
      float lineRight = line.x + line.width;
      
      // Vertical distance
      float dy = 0;
      bool above = false, below = false;
      if (y < lineTop) { dy = lineTop - y; above = true; }
      else if (y > lineBottom) { dy = y - lineBottom; below = true; }
      
      // Horizontal distance  
      float dx = 0;
      bool left = false, right = false;
      if (x < lineLeft) { dx = lineLeft - x; left = true; }
      else if (x > lineRight) { dx = x - lineRight; right = true; }
      
      float distance = dx * dx + dy * dy;
      
      if (distance < bestDistance) {
        bestDistance = distance;
        bestBox = box;
        bestLineIdx = i;
        isAbove = above;
        isBelow = below;
        isLeft = left;
        isRight = right;
      }
    }
  }
  
  if (!bestBox || bestBox->textLines.empty()) return nullptr;
  
  lineIndex = bestLineIdx;
  const auto &bestLine = bestBox->textLines[bestLineIdx];
  float fontSize = bestBox->computedStyle.fontSize;
  skene::MSDFFont* font = fontManager.getFont(bestBox->computedStyle.fontFamily, 
      static_cast<int>(bestBox->computedStyle.fontWeight), static_cast<int>(bestBox->computedStyle.fontStyle));
  
  // Determine character index based on position relative to nearest text
  if (isAbove) {
    // Click is above the nearest text - anchor at START of line
    charIndex = 0;
  } else if (isBelow) {
    // Click is below the nearest text - anchor at END of line  
    charIndex = bestLine.text.length();
  } else if (isLeft) {
    // Click is left of text - anchor at START
    charIndex = 0;
  } else if (isRight) {
    // Click is right of text - anchor at END
    charIndex = bestLine.text.length();
  } else {
    // Click is within bounds - use hit test
    float localX = x - bestLine.x;
    charIndex = font ? font->hitTestText(bestLine.text, std::max(0.0f, localX), fontSize) : 0;
  }
  
  return bestBox;
}

// Helper function to find text box at point, falling back to nearest
std::shared_ptr<skene::RenderBox> findTextBoxAt(
    std::shared_ptr<skene::RenderBox> box, float x, float y, skene::MSDFFontManager &fontManager,
    size_t &lineIndex, size_t &charIndex, bool allowNearest = false) {
  
  // First try exact match
  auto result = findTextBoxAtExact(box, x, y, fontManager, lineIndex, charIndex);
  if (result) return result;
  
  // If allowNearest, find the closest text box
  if (allowNearest) {
    return findNearestTextBox(x, y, fontManager, lineIndex, charIndex);
  }
  
  return nullptr;
}

// Get selected text from current selection (supports cross-element)
std::string getSelectedText(skene::TextSelection &sel) {
  if (!sel.hasSelection || !sel.anchorBox || !sel.focusBox) {
    return "";
  }
  
  std::string result;
  
  // Find start and end box indices
  int anchorIdx = sel.getBoxIndex(sel.anchorBox);
  int focusIdx = sel.getBoxIndex(sel.focusBox);
  
  if (anchorIdx < 0 || focusIdx < 0) return "";
  
  int startIdx = std::min(anchorIdx, focusIdx);
  int endIdx = std::max(anchorIdx, focusIdx);
  bool anchorFirst = anchorIdx <= focusIdx;
  
  for (int boxIdx = startIdx; boxIdx <= endIdx; ++boxIdx) {
    auto box = sel.allTextBoxes[boxIdx];
    if (box->textLines.empty()) continue;
    
    // Add newline between block elements (different boxes)
    if (boxIdx > startIdx && !result.empty()) {
      result += "\n";
    }
    
    for (size_t lineIdx = 0; lineIdx < box->textLines.size(); ++lineIdx) {
      const auto &line = box->textLines[lineIdx];
      auto [selStart, selEnd] = sel.getSelectionRangeForLine(box, lineIdx, line.text.length());
      
      if (selStart < selEnd && selStart < line.text.length()) {
        // Add space between wrapped lines within same box
        if (lineIdx > 0 && !result.empty() && result.back() != '\n') {
          result += " ";
        }
        result += line.text.substr(selStart, selEnd - selStart);
      }
    }
  }
  
  return result;
}

void paintInspector(skene::Renderer &renderer,
                    std::shared_ptr<skene::Node> node, skene::MSDFFontManager &fontManager,
                    float x, float &y, int depth) {
  if (!node)
    return;

  float lineHeight = 18.0f;
  skene::MSDFFont* font = fontManager.getFont("sans-serif", 0, 0);  // Use sans-serif for UI

  // Store line for hit testing
  InspectorLine line;
  line.y = y;
  line.h = lineHeight;
  line.node = node;
  inspectorLines.push_back(line);

  // Highlight if selected
  if (node == selectedNode) {
    renderer.drawRect(x, y, (float)INSPECTOR_WIDTH, lineHeight, 0.3f, 0.3f,
                      0.6f, 1.0f); // Blue highlight
  }

  // Draw Node text
  std::string display;
  if (node->type == skene::NodeType::Element) {
    display = "<" + node->tagName + ">";
  } else if (node->type == skene::NodeType::Text) {
    display = "\"text\""; // display text content truncated?
    if (node->textContent.length() > 10)
      display += " " + node->textContent.substr(0, 10) + "...";
    else
      display += " " + node->textContent;
  } else if (node->type == skene::NodeType::Document) {
    display = "Document";
  }

  float indent = depth * 15.0f;
  if (font) {
    renderer.drawText(x + indent + 5, y + 14, display, *font, 0.0f, 0.0f, 0.0f, 1.0f);
  }
  y += lineHeight;

  for (auto &child : node->children) {
    paintInspector(renderer, child, fontManager, x, y, depth + 1);
  }
}

// Editor State
bool isEditing = false;
int cursorTimer = 0;

void paintStyles(skene::Renderer &renderer, skene::MSDFFontManager &fontManager, float x,
                 float y) {
  if (!selectedNode)
    return;
  
  skene::MSDFFont* font = fontManager.getFont("sans-serif", 0, 0);

  // Draw background for styles section
  const float STYLES_SECTION_HEIGHT = screenHeight * 0.4f - 20;
  renderer.drawRect(x, y, INSPECTOR_WIDTH, STYLES_SECTION_HEIGHT, 0.95f, 0.95f,
                    0.95f, 1.0f); // Light gray background

  float currentY = y + 20;

  if (font) {
    renderer.drawText(x + 5, currentY, "Computed / Attributes:", *font, 0.0f, 0.0f,
                      0.0f, 1.0f);
  }
  currentY += 20;

  std::string typeStr =
      (selectedNode->type == skene::NodeType::Element) ? "Element" : "Text";
  if (font) {
    renderer.drawText(x + 10, currentY, "Type: " + typeStr, *font, 0.2f, 0.2f,
                      0.2f, 1.0f);
  }
  currentY += 18;

  if (selectedNode->type == skene::NodeType::Element) {
    if (font) {
      renderer.drawText(x + 10, currentY, "Tag: " + selectedNode->tagName, *font,
                        0.2f, 0.2f, 0.2f, 1.0f);
    }
    currentY += 18;

    // Style Editor
    if (font) {
      renderer.drawText(x + 10, currentY, "Style (Type to edit):", *font, 0.0f,
                        0.0f, 0.5f, 1.0f);
    }
    currentY += 20;

    // Draw Input Box Background
    renderer.drawRect(x + 10, currentY, INSPECTOR_WIDTH - 20, 24, 1.0f, 1.0f,
                      1.0f, 1.0f);
    renderer.drawRectOutline(x + 10, currentY, INSPECTOR_WIDTH - 20, 24, 0.0f,
                             0.0f, 0.0f, 1.0f);

    // Draw Style Content
    std::string styleStr = selectedNode->attributes["style"];
    if (font) {
      renderer.drawText(x + 15, currentY + 16, styleStr, *font, 0.0f, 0.0f, 0.0f,
                        1.0f);
    }

    // Draw Cursor
    if (cursorTimer < 30) { // Blink every ~30 frames (at 60fps)
      // Better approximation: use font size (16px) * 0.5 as avg char width
      float txtW = styleStr.length() * 7.5f;
      renderer.drawRect(x + 15 + txtW, currentY + 5, 2, 14, 0.0f, 0.0f, 0.0f,
                        1.0f);
    }

    currentY += 30;

    // Attributes (Read Only)
    for (auto const &[key, val] : selectedNode->attributes) {
      if (key == "style")
        continue; // Skip style as it's above
      std::string attrDisplay = key + ": " + val;
      if (font) {
        renderer.drawText(x + 10, currentY, attrDisplay, *font, 0.4f, 0.4f, 0.4f,
                          1.0f);
      }
      currentY += 18;
    }
  }
}

// Paint sidebar tab bar
void paintSidebarTabs(skene::Renderer &renderer, skene::MSDFFontManager &fontManager, float x, float y) {
  skene::MSDFFont* font = fontManager.getFont("sans-serif", 0, 0);
  float tabWidth = INSPECTOR_WIDTH / 2.0f;
  
  // Tab 1: Inspector
  bool isInspectorActive = (currentSidebarTab == SidebarTab::Inspector);
  renderer.drawRect(x, y, tabWidth, TAB_HEIGHT, 
                    isInspectorActive ? 0.95f : 0.8f, 
                    isInspectorActive ? 0.95f : 0.8f, 
                    isInspectorActive ? 0.95f : 0.8f, 1.0f);
  renderer.drawRectOutline(x, y, tabWidth, TAB_HEIGHT, 0.6f, 0.6f, 0.6f, 1.0f);
  if (font) {
    renderer.drawText(x + 10, y + 20, "Inspector", *font, 
                      isInspectorActive ? 0.0f : 0.4f, 
                      isInspectorActive ? 0.0f : 0.4f, 
                      isInspectorActive ? 0.0f : 0.4f, 1.0f);
  }
  
  // Tab 2: Performance
  bool isPerfActive = (currentSidebarTab == SidebarTab::Performance);
  renderer.drawRect(x + tabWidth, y, tabWidth, TAB_HEIGHT, 
                    isPerfActive ? 0.95f : 0.8f, 
                    isPerfActive ? 0.95f : 0.8f, 
                    isPerfActive ? 0.95f : 0.8f, 1.0f);
  renderer.drawRectOutline(x + tabWidth, y, tabWidth, TAB_HEIGHT, 0.6f, 0.6f, 0.6f, 1.0f);
  if (font) {
    renderer.drawText(x + tabWidth + 10, y + 20, "Performance", *font, 
                      isPerfActive ? 0.0f : 0.4f, 
                      isPerfActive ? 0.0f : 0.4f, 
                      isPerfActive ? 0.0f : 0.4f, 1.0f);
  }
}

// Paint performance/debug view in sidebar
void paintPerformanceView(skene::Renderer &renderer, skene::MSDFFontManager &fontManager, 
                          float x, float y, float availableHeight) {
  skene::MSDFFont* font = fontManager.getFont("sans-serif", 0, 0);
  if (!font) return;
  
  float fontSize = 14.0f;
  float lineHeight = 20.0f;
  float currentY = y + 20;
  float labelX = x + 15;
  float valueX = x + 130;
  
  // Background
  renderer.drawRect(x, y, INSPECTOR_WIDTH, availableHeight, 0.95f, 0.95f, 0.95f, 1.0f);
  
  // Section: Frame Stats
  renderer.drawText(labelX - 5, currentY, "Frame Statistics", *font, 0.0f, 0.0f, 0.5f, 1.0f);
  currentY += lineHeight + 5;
  
  // FPS
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.1f", fpsCurrent);
  renderer.drawText(labelX, currentY, "FPS:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  float fpsColorR = fpsCurrent >= 60 ? 0.0f : (fpsCurrent >= 30 ? 0.8f : 1.0f);
  float fpsColorG = fpsCurrent >= 60 ? 0.7f : (fpsCurrent >= 30 ? 0.6f : 0.0f);
  renderer.drawText(valueX, currentY, buffer, *font, fpsColorR, fpsColorG, 0.0f, 1.0f, fontSize);
  currentY += lineHeight;
  
  // Frame time
  snprintf(buffer, sizeof(buffer), "%.2f ms", frameTimeMs);
  renderer.drawText(labelX, currentY, "Frame Time:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight;
  
  // Target frame time (16.67ms for 60fps)
  float targetMs = 1000.0f / 60.0f;
  snprintf(buffer, sizeof(buffer), "%.2f ms (60fps)", targetMs);
  renderer.drawText(labelX, currentY, "Target:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.5f, 0.5f, 0.5f, 1.0f, fontSize);
  currentY += lineHeight + 15;
  
  // Section: Layout Stats
  renderer.drawText(labelX - 5, currentY, "Layout Statistics", *font, 0.0f, 0.0f, 0.5f, 1.0f);
  currentY += lineHeight + 5;
  
  // Text boxes count
  snprintf(buffer, sizeof(buffer), "%zu", textSelection.allTextBoxes.size());
  renderer.drawText(labelX, currentY, "Text Boxes:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight;
  
  // Inspector lines (DOM nodes visible)
  snprintf(buffer, sizeof(buffer), "%zu", inspectorLines.size());
  renderer.drawText(labelX, currentY, "DOM Nodes:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight + 15;
  
  // Section: Scroll & Window
  renderer.drawText(labelX - 5, currentY, "Viewport", *font, 0.0f, 0.0f, 0.5f, 1.0f);
  currentY += lineHeight + 5;
  
  // Window size
  snprintf(buffer, sizeof(buffer), "%d x %d", screenWidth, screenHeight);
  renderer.drawText(labelX, currentY, "Window:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight;
  
  // Content area
  snprintf(buffer, sizeof(buffer), "%d x %d", screenWidth - INSPECTOR_WIDTH, screenHeight);
  renderer.drawText(labelX, currentY, "Content:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight;
  
  // Scroll position
  snprintf(buffer, sizeof(buffer), "%.0f / %.0f", scrollY, maxScrollY);
  renderer.drawText(labelX, currentY, "Scroll Y:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, buffer, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight + 15;
  
  // Section: Selection
  renderer.drawText(labelX - 5, currentY, "Selection", *font, 0.0f, 0.0f, 0.5f, 1.0f);
  currentY += lineHeight + 5;
  
  // Has selection
  renderer.drawText(labelX, currentY, "Active:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  renderer.drawText(valueX, currentY, textSelection.hasSelection ? "Yes" : "No", *font, 
                    textSelection.hasSelection ? 0.0f : 0.5f, 
                    textSelection.hasSelection ? 0.6f : 0.5f, 
                    0.0f, 1.0f, fontSize);
  currentY += lineHeight + 15;
  
  // Section: Settings
  renderer.drawText(labelX - 5, currentY, "Settings", *font, 0.0f, 0.0f, 0.5f, 1.0f);
  currentY += lineHeight + 5;
  
  // VSync checkbox
  float checkboxSize = 16.0f;
  float checkboxX = labelX;
  float checkboxY = currentY;
  
  // Store checkbox bounds for click detection
  vsyncCheckbox.x = checkboxX;
  vsyncCheckbox.y = checkboxY;
  vsyncCheckbox.width = checkboxSize;
  vsyncCheckbox.height = checkboxSize;
  vsyncCheckbox.isValid = true;
  
  // Draw checkbox background
  renderer.drawRect(checkboxX, checkboxY, checkboxSize, checkboxSize, 1.0f, 1.0f, 1.0f, 1.0f);
  renderer.drawRectOutline(checkboxX, checkboxY, checkboxSize, checkboxSize, 0.4f, 0.4f, 0.4f, 1.0f);
  
  // Draw checkmark if VSync is enabled
  if (vsyncEnabled) {
    // Simple checkmark using two lines (drawn as thin rects)
    renderer.drawRect(checkboxX + 3, checkboxY + 8, 5, 2, 0.0f, 0.5f, 0.0f, 1.0f);
    renderer.drawRect(checkboxX + 6, checkboxY + 4, 2, 8, 0.0f, 0.5f, 0.0f, 1.0f);
  }
  
  renderer.drawText(checkboxX + checkboxSize + 8, currentY + 12, "VSync", *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  renderer.drawText(checkboxX + checkboxSize + 55, currentY + 12, vsyncEnabled ? "(On)" : "(Off)", *font,
                    vsyncEnabled ? 0.0f : 0.5f, vsyncEnabled ? 0.5f : 0.5f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight + 15;
  
  // Section: Text Rendering
  renderer.drawText(labelX - 5, currentY, "Text Rendering", *font, 0.0f, 0.0f, 0.5f, 1.0f);
  currentY += lineHeight + 5;
  
  // Edge Low slider
  float sliderWidth = 120.0f;
  float sliderHeight = 8.0f;
  float sliderX = valueX;
  float sliderY = currentY + 4;
  float knobWidth = 10.0f;
  
  renderer.drawText(labelX, currentY + 10, "Edge Low:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  
  // Store slider bounds
  edgeLowSlider.x = sliderX;
  edgeLowSlider.y = sliderY;
  edgeLowSlider.width = sliderWidth;
  edgeLowSlider.height = sliderHeight;
  edgeLowSlider.minVal = -1.0f;
  edgeLowSlider.maxVal = 0.0f;
  edgeLowSlider.isValid = true;
  
  // Draw slider track
  renderer.drawRect(sliderX, sliderY, sliderWidth, sliderHeight, 0.8f, 0.8f, 0.8f, 1.0f);
  renderer.drawRectOutline(sliderX, sliderY, sliderWidth, sliderHeight, 0.5f, 0.5f, 0.5f, 1.0f);
  
  // Draw slider knob
  float edgeLow = renderer.getMsdfEdgeLow();
  float knobPosLow = sliderX + ((edgeLow - edgeLowSlider.minVal) / (edgeLowSlider.maxVal - edgeLowSlider.minVal)) * (sliderWidth - knobWidth);
  renderer.drawRect(knobPosLow, sliderY - 2, knobWidth, sliderHeight + 4, 0.3f, 0.5f, 0.8f, 1.0f);
  
  // Show value
  char valBuf[32];
  snprintf(valBuf, sizeof(valBuf), "%.2f", edgeLow);
  renderer.drawText(sliderX + sliderWidth + 10, currentY + 10, valBuf, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
  currentY += lineHeight + 5;
  
  // Edge High slider
  sliderY = currentY + 4;
  
  renderer.drawText(labelX, currentY + 10, "Edge High:", *font, 0.3f, 0.3f, 0.3f, 1.0f, fontSize);
  
  // Store slider bounds
  edgeHighSlider.x = sliderX;
  edgeHighSlider.y = sliderY;
  edgeHighSlider.width = sliderWidth;
  edgeHighSlider.height = sliderHeight;
  edgeHighSlider.minVal = 0.0f;
  edgeHighSlider.maxVal = 1.0f;
  edgeHighSlider.isValid = true;
  
  // Draw slider track
  renderer.drawRect(sliderX, sliderY, sliderWidth, sliderHeight, 0.8f, 0.8f, 0.8f, 1.0f);
  renderer.drawRectOutline(sliderX, sliderY, sliderWidth, sliderHeight, 0.5f, 0.5f, 0.5f, 1.0f);
  
  // Draw slider knob
  float edgeHigh = renderer.getMsdfEdgeHigh();
  float knobPosHigh = sliderX + ((edgeHigh - edgeHighSlider.minVal) / (edgeHighSlider.maxVal - edgeHighSlider.minVal)) * (sliderWidth - knobWidth);
  renderer.drawRect(knobPosHigh, sliderY - 2, knobWidth, sliderHeight + 4, 0.3f, 0.5f, 0.8f, 1.0f);
  
  // Show value
  snprintf(valBuf, sizeof(valBuf), "%.2f", edgeHigh);
  renderer.drawText(sliderX + sliderWidth + 10, currentY + 10, valBuf, *font, 0.0f, 0.0f, 0.0f, 1.0f, fontSize);
}

// Draw selection highlights across all text boxes, filling gaps between inline elements
void paintSelectionHighlights(skene::Renderer &renderer, skene::MSDFFontManager &fontManager) {
  if (!textSelection.hasSelection) {
    return;
  }
  
  // Collect all selection segments with their positions
  struct SelectionSegment {
    float x, y, width, height;
  };
  
  // Group segments by Y position (same line)
  std::map<int, std::vector<SelectionSegment>> segmentsByLine;
  
  for (size_t boxIdx = 0; boxIdx < textSelection.allTextBoxes.size(); ++boxIdx) {
    auto &box = textSelection.allTextBoxes[boxIdx];
    if (box->textLines.empty()) continue;
    
    skene::MSDFFont* font = fontManager.getFont(box->computedStyle.fontFamily, 
        static_cast<int>(box->computedStyle.fontWeight), static_cast<int>(box->computedStyle.fontStyle));
    if (!font) font = fontManager.getDefaultFont();
    if (!font) continue;
    
    float fontSize = box->computedStyle.fontSize;
    
    for (size_t lineIdx = 0; lineIdx < box->textLines.size(); ++lineIdx) {
      const auto &line = box->textLines[lineIdx];
      
      auto [selStart, selEnd] = textSelection.getSelectionRangeForLine(
          box, lineIdx, line.text.length());
      
      if (selStart < selEnd) {
        float startX = line.x + font->getPositionAtIndex(line.text, selStart, fontSize);
        float endX = line.x + font->getPositionAtIndex(line.text, selEnd, fontSize);
        
        // Use Y position rounded to int as key for grouping lines
        int lineKey = (int)(line.y * 10); // Multiply by 10 for sub-pixel grouping
        segmentsByLine[lineKey].push_back({startX, line.y, endX - startX, line.height});
      }
    }
  }
  
  // For each line, sort segments by X and fill gaps
  for (auto &[lineKey, segments] : segmentsByLine) {
    if (segments.empty()) continue;
    
    // Sort by X position
    std::sort(segments.begin(), segments.end(), 
              [](const SelectionSegment &a, const SelectionSegment &b) { return a.x < b.x; });
    
    // Draw each segment and fill gaps to next segment
    for (size_t i = 0; i < segments.size(); ++i) {
      const auto &seg = segments[i];
      
      // Calculate width - extend to fill gap to next segment if on same line
      float drawWidth = seg.width;
      if (i + 1 < segments.size()) {
        float gapEnd = segments[i + 1].x;
        // Fill gap: extend this segment's highlight to touch the next one
        drawWidth = gapEnd - seg.x;
      }
      
      // Draw selection background (bright blue highlight)
      renderer.drawRect(seg.x, seg.y, drawWidth, seg.height,
                       0.2f, 0.4f, 0.9f, 1.0f);
    }
  }
}

// Find the deepest scrollable element containing the given point
// Also returns the chain of scrollable ancestors for scroll propagation
std::shared_ptr<skene::RenderBox> findScrollableElementAt(
    std::shared_ptr<skene::RenderBox> box, float px, float py, float parentScrollX = 0, float parentScrollY = 0,
    std::vector<std::shared_ptr<skene::RenderBox>>* scrollableChain = nullptr) {
  if (!box) return nullptr;
  
  skene::Rect borderBox = box->box.borderBox();
  
  // Adjust point for parent scroll
  float localPx = px + parentScrollX;
  float localPy = py + parentScrollY;
  
  // Check if point is within this box's bounds
  bool inBounds = localPx >= borderBox.x && localPx < borderBox.x + borderBox.width &&
                  localPy >= borderBox.y && localPy < borderBox.y + borderBox.height;
  
  if (!inBounds) return nullptr;
  
  // Track if this element is scrollable (for building the chain)
  bool thisIsScrollable = box->isScrollable();
  
  // Check children first (depth-first, inner elements take priority)
  float childScrollX = parentScrollX + box->scrollX;
  float childScrollY = parentScrollY + box->scrollY;
  
  for (auto it = box->children.rbegin(); it != box->children.rend(); ++it) {
    auto result = findScrollableElementAt(*it, px, py, childScrollX, childScrollY, scrollableChain);
    if (result) {
      // Add this element to chain if it's scrollable (builds chain from inner to outer)
      if (thisIsScrollable && scrollableChain) {
        scrollableChain->push_back(box);
      }
      return result;
    }
  }
  
  // If this element is scrollable, return it (and add to chain as the innermost)
  if (thisIsScrollable) {
    if (scrollableChain) {
      scrollableChain->push_back(box);
    }
    return box;
  }
  
  return nullptr;
}

// Paint Logic - with off-screen culling
void paint(skene::Renderer &renderer, std::shared_ptr<skene::RenderBox> box,
           skene::MSDFFontManager &fontManager, skene::StyleSheet &styleSheet,
           float viewportTop, float viewportBottom) {
  if (!box->node)
    return;

  auto &style = box->computedStyle;
  skene::Rect borderBox = box->box.borderBox();

  // Skip if not visible (zero size)
  if (borderBox.width <= 0 || borderBox.height <= 0) {
    // Still paint children (they might be positioned)
    for (auto &child : box->children) {
      paint(renderer, child, fontManager, styleSheet, viewportTop, viewportBottom);
    }
    return;
  }
  
  // Off-screen culling: skip elements completely outside viewport
  float elementTop = borderBox.y;
  float elementBottom = borderBox.y + borderBox.height;
  
  if (elementBottom < viewportTop || elementTop > viewportBottom) {
    // Element is completely off-screen, but children might be visible (positioned elements)
    // Only recurse if this is a container that might have absolutely positioned children
    if (box->children.empty()) {
      return; // Leaf node, safe to skip entirely
    }
    // For containers, still check children (they might be positioned differently)
    for (auto &child : box->children) {
      paint(renderer, child, fontManager, styleSheet, viewportTop, viewportBottom);
    }
    return;
  }

  // Set opacity
  renderer.setOpacity(style.opacity);

  // Checkbox inputs are custom-painted later; skip the generic background/border
  // pass so they don't look like wide text inputs.
  bool isCheckboxInput = false;
  if (box->node->type == skene::NodeType::Element) {
    std::string tag = box->node->tagName;
    std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
    if (tag == "input") {
      auto typeIt = box->node->attributes.find("type");
      std::string inputType = "text";
      if (typeIt != box->node->attributes.end()) {
        inputType = typeIt->second;
        std::transform(inputType.begin(), inputType.end(), inputType.begin(), ::tolower);
      }
      isCheckboxInput = (inputType == "checkbox");
    }
  }

  // 1. Draw background
  if (!isCheckboxInput && style.backgroundColor.a > 0) {
    if (style.borderRadius > 0) {
      renderer.drawRoundedRect(borderBox.x, borderBox.y, borderBox.width,
                               borderBox.height, style.borderRadius,
                               style.backgroundColor.r, style.backgroundColor.g,
                               style.backgroundColor.b, style.backgroundColor.a);
    } else {
      renderer.drawRect(borderBox.x, borderBox.y, borderBox.width,
                        borderBox.height, style.backgroundColor.r,
                        style.backgroundColor.g, style.backgroundColor.b,
                        style.backgroundColor.a);
    }
  }

  // 2. Draw selection highlight
  if (box->node == selectedNode) {
    renderer.drawRect(borderBox.x, borderBox.y, borderBox.width,
                      borderBox.height, 0.5f, 0.5f, 1.0f, 0.15f);
  }

  // 3. Draw borders
  if (!isCheckboxInput && box->node->type == skene::NodeType::Element) {
    float borderTop = style.getBorderTopWidth();
    float borderRight = style.getBorderRightWidth();
    float borderBottom = style.getBorderBottomWidth();
    float borderLeft = style.getBorderLeftWidth();

    if (borderTop > 0 || borderRight > 0 || borderBottom > 0 ||
        borderLeft > 0) {
      // Use per-side colors
      renderer.drawBorderPerSide(borderBox.x, borderBox.y, borderBox.width,
                          borderBox.height, borderTop, borderRight,
                          borderBottom, borderLeft,
                          style.borderTopColor.r, style.borderTopColor.g, 
                          style.borderTopColor.b, style.borderTopColor.a,
                          style.borderRightColor.r, style.borderRightColor.g,
                          style.borderRightColor.b, style.borderRightColor.a,
                          style.borderBottomColor.r, style.borderBottomColor.g,
                          style.borderBottomColor.b, style.borderBottomColor.a,
                          style.borderLeftColor.r, style.borderLeftColor.g,
                          style.borderLeftColor.b, style.borderLeftColor.a);
    }
    // No debug borders - only draw explicit borders
  }

  // 4. Draw list markers (bullets/numbers) for <li> elements
  if (box->node->type == skene::NodeType::Element) {
    std::string tag = box->node->tagName;
    std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
    if (tag == "li" && style.listStyleType != skene::ListStyleType::None) {
      // Get font for the marker
      skene::MSDFFont* font = fontManager.getFont(style.fontFamily, 
          static_cast<int>(style.fontWeight), static_cast<int>(style.fontStyle));
      if (!font) font = fontManager.getDefaultFont();
      
      if (font) {
        float fontSize = style.fontSize;
        
        // The <li>'s children contain the text. Look at the first child's textLines
        float markerY = box->box.content.y;  // Default fallback
        
        // Children of the <li> RenderBox - look for first with textLines
        for (auto& child : box->children) {
          if (!child->textLines.empty()) {
            markerY = child->textLines[0].y;
            break;
          }
        }
        
        // Build the marker string first, then right-align all markers to same edge
        std::string marker;
        if (style.listStyleType == skene::ListStyleType::Disc) {
          marker = "\xe2\x80\xa2";  // UTF-8 bullet character (•)
        } else if (style.listStyleType == skene::ListStyleType::Circle) {
          marker = "\xe2\x97\x8b";  // UTF-8 white circle (○)
        } else if (style.listStyleType == skene::ListStyleType::Square) {
          marker = "\xe2\x96\xaa";  // UTF-8 black small square (▪)
        } else if (style.listStyleType == skene::ListStyleType::Decimal) {
          marker = std::to_string(style.listItemIndex) + ".";
        }
        
        if (!marker.empty()) {
          // Position all markers with their right edge at the same distance from content
          float markerWidth = font->getTextWidth(marker, fontSize);
          // For bullets, center them at a fixed position; for numbers, right-align
          float markerX;
          if (style.listStyleType == skene::ListStyleType::Decimal) {
            // Numbers: right-align so the period is at a consistent position
            markerX = box->box.content.x - 6.0f - markerWidth;
          } else {
            // Bullets: center at a fixed position (12px from content)
            markerX = box->box.content.x - 12.0f - markerWidth / 2.0f;
          }
          
          // Note: text rendering uses line.y + fontSize for baseline positioning
          renderer.drawText(markerX, markerY + fontSize, marker, *font,
                           style.color.r, style.color.g, style.color.b, style.color.a,
                           fontSize);
        }
      }
    }
    
    // <blockquote> element: render left border for visual indication
    if (tag == "blockquote") {
      skene::Rect content = box->box.content;
      
      // Draw left border (4px wide, light gray)
      float borderWidth = 4.0f;
      float borderX = content.x - 8.0f; // Position border slightly left of content
      renderer.drawRect(borderX, content.y, borderWidth, content.height,
                       0.7f, 0.7f, 0.7f, 1.0f); // Light gray color
    }
    
    // <pre> element: ensure text is rendered with monospace font
    if (tag == "pre") {
      // The CSS already sets font-family: monospace, but we can ensure 
      // all child text nodes get the monospace font by updating their style
      // This is handled by the text rendering code which respects font-family
    }
    
    // <input> element: render based on type
    if (tag == "input") {
      auto typeIt = box->node->attributes.find("type");
      std::string inputType = "text"; // default
      if (typeIt != box->node->attributes.end()) {
        inputType = typeIt->second;
        std::transform(inputType.begin(), inputType.end(), inputType.begin(), ::tolower);
      }
      
      if (inputType == "checkbox") {
        skene::Rect content = box->box.content;
        
        // Fixed checkbox size regardless of content size
        float checkboxSize = 16.0f;
        float checkboxX = content.x;
        float checkboxY = content.y + (content.height - checkboxSize) / 2.0f;
        
        // Background
        renderer.drawRect(checkboxX, checkboxY, checkboxSize, checkboxSize,
                         1.0f, 1.0f, 1.0f, 1.0f); // White background
        
        // Border
        renderer.drawRectOutline(checkboxX, checkboxY, checkboxSize, checkboxSize,
                               0.5f, 0.5f, 0.5f, 1.0f); // Gray border
        
        // Check if checked
        auto checkedIt = box->node->attributes.find("checked");
        bool isChecked = (checkedIt != box->node->attributes.end());
        
        if (isChecked) {
          // Draw proper checkmark (✓) shape using lines
          // First part: short line going down-right from left
          float x1 = checkboxX + 3, y1 = checkboxY + 8;
          float x2 = checkboxX + 7, y2 = checkboxY + 11;
          
          // Second part: longer line going up-right
          float x3 = x2, y3 = y2;
          float x4 = checkboxX + 13, y4 = checkboxY + 5;
          
          // Draw checkmark lines with thickness
          renderer.drawLine(x1, y1, x2, y2, 1.5f, 0.2f, 0.2f, 0.2f, 1.0f); // Dark gray
          renderer.drawLine(x3, y3, x4, y4, 1.5f, 0.2f, 0.2f, 0.2f, 1.0f); // Dark gray
        }
      } else {
        // Text/password/email inputs: render placeholder text
        auto it = box->node->attributes.find("placeholder");
        if (it != box->node->attributes.end() && !it->second.empty()) {
          skene::MSDFFont* font = fontManager.getFont(style.fontFamily, 
              static_cast<int>(style.fontWeight), static_cast<int>(style.fontStyle));
          if (!font) font = fontManager.getDefaultFont();
          
          if (font) {
            float fontSize = style.fontSize;
            // Placeholder text is gray
            renderer.drawText(box->box.content.x + 2, box->box.content.y + fontSize,
                             it->second, *font,
                             0.6f, 0.6f, 0.6f, 1.0f,  // Gray color
                             fontSize);
          }
        }
      }
    }
    
    // <img> element: render actual image or fallback placeholder
    if (tag == "img") {
      skene::Rect content = box->box.content;
      
      // Get src attribute
      auto srcAttr = box->node->attributes.find("src");
      bool imageLoaded = false;
      
      if (srcAttr != box->node->attributes.end() && !srcAttr->second.empty()) {
        std::string imagePath = srcAttr->second;
        // Try to load and draw the image with CSS properties
        imageLoaded = renderer.loadImage(imagePath);
        if (imageLoaded) {
          renderer.drawImage(content.x, content.y, content.width, content.height, imagePath,
                            style.objectFit, style.objectPosition, style.imageRendering);
        }
      }
      
      // Draw placeholder if image not loaded
      if (!imageLoaded) {
        // Draw light gray background
        renderer.drawRect(content.x, content.y, content.width, content.height,
                         0.9f, 0.9f, 0.9f, 1.0f);
        
        // Draw border
        renderer.drawRectOutline(content.x, content.y, content.width, content.height,
                                0.7f, 0.7f, 0.7f, 1.0f);
        
        // Draw image icon (simple mountain/sun icon)
        float iconSize = std::min(content.width, content.height) * 0.4f;
        float iconX = content.x + (content.width - iconSize) / 2.0f;
        float iconY = content.y + (content.height - iconSize) / 2.0f;
        
        // Sun (circle in top-right)
        float sunRadius = iconSize * 0.15f;
        float sunX = iconX + iconSize * 0.7f;
        float sunY = iconY + iconSize * 0.25f;
        renderer.drawRect(sunX - sunRadius, sunY - sunRadius, 
                         sunRadius * 2, sunRadius * 2,
                         0.5f, 0.5f, 0.5f, 1.0f);
        
        // Mountain (triangle shape - simplified as rectangles)
        float mtnBaseY = iconY + iconSize * 0.8f;
        float mtnHeight = iconSize * 0.5f;
        // Left mountain
        renderer.drawRect(iconX + iconSize * 0.1f, mtnBaseY - mtnHeight * 0.6f,
                         iconSize * 0.3f, mtnHeight * 0.6f,
                         0.5f, 0.5f, 0.5f, 1.0f);
        // Right mountain (taller)
        renderer.drawRect(iconX + iconSize * 0.35f, mtnBaseY - mtnHeight,
                         iconSize * 0.4f, mtnHeight,
                         0.6f, 0.6f, 0.6f, 1.0f);
        
        // Draw alt text or "IMG" if no alt
        skene::MSDFFont* font = fontManager.getFont("sans-serif", 
            static_cast<int>(skene::FontWeight::Normal), 
            static_cast<int>(skene::FontStyle::Normal));
        if (!font) font = fontManager.getDefaultFont();
        
        if (font) {
          std::string altText = "IMG";
          auto altAttr = box->node->attributes.find("alt");
          if (altAttr != box->node->attributes.end() && !altAttr->second.empty()) {
            altText = altAttr->second;
          }
          float fontSize = std::min(12.0f, content.height * 0.15f);
          float textWidth = font->getTextWidth(altText, fontSize);
          float textX = content.x + (content.width - textWidth) / 2.0f;
          float textY = content.y + content.height - 4.0f;
          
          renderer.drawText(textX, textY, altText, *font,
                           0.5f, 0.5f, 0.5f, 1.0f, fontSize);
        }
      }
    }
    
    // <textarea> element: render with placeholder
    if (tag == "textarea") {
      skene::Rect content = box->box.content;
      
      // Draw placeholder text if no content
      std::string placeholder;
      auto placeholderAttr = box->node->attributes.find("placeholder");
      if (placeholderAttr != box->node->attributes.end()) {
        placeholder = placeholderAttr->second;
      }
      
      if (!placeholder.empty()) {
        skene::MSDFFont* font = fontManager.getFont(style.fontFamily, 
            static_cast<int>(style.fontWeight), static_cast<int>(style.fontStyle));
        if (!font) font = fontManager.getDefaultFont();
        
        if (font) {
          float fontSize = style.fontSize;
          renderer.drawText(content.x + 2, content.y + fontSize,
                           placeholder, *font,
                           0.6f, 0.6f, 0.6f, 1.0f, fontSize);
        }
      }
    }
    
    // <select> element: render dropdown appearance
    if (tag == "select") {
      skene::Rect content = box->box.content;
      
      // Draw dropdown arrow
      float arrowSize = 8.0f;
      float arrowX = content.x + content.width - arrowSize - 4;
      float arrowY = content.y + (content.height - arrowSize) / 2.0f;
      
      // Simple down arrow (triangle)
      renderer.drawRect(arrowX, arrowY, arrowSize, 2, 0.4f, 0.4f, 0.4f, 1.0f);
      renderer.drawRect(arrowX + 1, arrowY + 2, arrowSize - 2, 2, 0.4f, 0.4f, 0.4f, 1.0f);
      renderer.drawRect(arrowX + 2, arrowY + 4, arrowSize - 4, 2, 0.4f, 0.4f, 0.4f, 1.0f);
      renderer.drawRect(arrowX + 3, arrowY + 6, arrowSize - 6, 2, 0.4f, 0.4f, 0.4f, 1.0f);
    }
  }

  // 5. Draw text
  if (box->node->type == skene::NodeType::Text) {
    // Get MSDF font for this element's font-family with weight and style
    skene::MSDFFont* font = fontManager.getFont(style.fontFamily, 
        static_cast<int>(style.fontWeight), static_cast<int>(style.fontStyle));
    if (!font) font = fontManager.getDefaultFont();
    
    // Use wrapped text lines if available
    if (!box->textLines.empty() && font) {
      float fontSize = style.fontSize;
      
      // Check for sub/sup vertical offset based on parent element
      float verticalOffset = 0.0f;
      auto parentBox = box->parent.lock();
      if (parentBox && parentBox->node && parentBox->node->type == skene::NodeType::Element) {
        std::string parentTag = parentBox->node->tagName;
        std::transform(parentTag.begin(), parentTag.end(), parentTag.begin(), ::tolower);
        if (parentTag == "sub") {
          verticalOffset = fontSize * 0.4f + 4.0f;  // Move down + extra offset
        } else if (parentTag == "sup") {
          verticalOffset = -fontSize * 0.4f + 4.0f;  // Move up + extra offset
        }
      }
      
      for (size_t lineIdx = 0; lineIdx < box->textLines.size(); ++lineIdx) {
        const auto &line = box->textLines[lineIdx];
        
        // Check for text selection on this line
        size_t selStart = 0, selEnd = 0;
        if (textSelection.hasSelection) {
          auto [ss, se] = textSelection.getSelectionRangeForLine(
              box, lineIdx, line.text.length());
          selStart = ss;
          selEnd = se;
          
          if (selStart < selEnd) {
            float startX = line.x + font->getPositionAtIndex(line.text, selStart, fontSize);
            float endX = line.x + font->getPositionAtIndex(line.text, selEnd, fontSize);
            
            // If entire line is selected, extend highlight to fill gap to next text box
            // BUT only if this element (or its parent) doesn't have right padding
            float myPaddingRight = style.padding.right.toPx();
            // Check parent's padding too (text nodes inherit from parent element)
            auto parentBox = box->parent.lock();
            if (parentBox && parentBox->node && parentBox->node->type == skene::NodeType::Element) {
              myPaddingRight = std::max(myPaddingRight, parentBox->computedStyle.padding.right.toPx());
            }
            
            if (selEnd == line.text.length() && myPaddingRight < 0.5f) {
              // Find next text box on the same line
              int boxIdx = textSelection.getBoxIndex(box);
              if (boxIdx >= 0 && boxIdx + 1 < (int)textSelection.allTextBoxes.size()) {
                auto nextBox = textSelection.allTextBoxes[boxIdx + 1];
                if (!nextBox->textLines.empty()) {
                  const auto &nextLine = nextBox->textLines[0];
                  // Check if on same visual line (similar Y position)
                  if (std::abs(nextLine.y - line.y) < line.height * 0.5f) {
                    // Check if next box is also selected
                    auto [nextSelStart, nextSelEnd] = textSelection.getSelectionRangeForLine(
                        nextBox, 0, nextLine.text.length());
                    if (nextSelStart < nextSelEnd) {
                      // Check next element's (or its parent's) left padding
                      float nextPaddingLeft = nextBox->computedStyle.padding.left.toPx();
                      auto nextParentBox = nextBox->parent.lock();
                      if (nextParentBox && nextParentBox->node && nextParentBox->node->type == skene::NodeType::Element) {
                        nextPaddingLeft = std::max(nextPaddingLeft, nextParentBox->computedStyle.padding.left.toPx());
                      }
                      
                      if (nextPaddingLeft < 0.5f) {
                        // Extend to the start of next box's text
                        endX = nextLine.x;
                      }
                    }
                  }
                }
              }
            }
            
            // Draw selection background (bright blue highlight)
            renderer.drawRect(startX, line.y, endX - startX, line.height,
                             0.2f, 0.4f, 0.9f, 1.0f);
          }
        }
        
        // Draw text with consistent positioning using single-pass rendering
        // This draws all characters in one glBegin/glEnd, avoiding jitter from multiple passes
        float drawY = line.y + fontSize + verticalOffset;
        if (selStart < selEnd && selEnd > 0) {
          // Draw text with selection - single pass with color switching
          renderer.drawTextWithSelectionMSDF(line.x, drawY, line.text, *font,
                                         style.color.r, style.color.g, style.color.b,
                                         style.color.a, fontSize, selStart, selEnd,
                                         1.0f, 1.0f, 1.0f, 1.0f);  // White for selected
        } else {
          // No selection - draw full text normally
          renderer.drawText(line.x, drawY, line.text, *font,
                            style.color.r, style.color.g, style.color.b,
                            style.color.a, fontSize);
        }

        // Draw text decoration
        if (style.textDecoration == skene::TextDecoration::Underline) {
          renderer.drawLine(line.x, drawY + 2,
                            line.x + line.width, drawY + 2,
                            1.0f, style.color.r, style.color.g, style.color.b,
                            style.color.a);
        } else if (style.textDecoration == skene::TextDecoration::LineThrough) {
          float midY = line.y + fontSize * 0.5f + verticalOffset;
          renderer.drawLine(line.x, midY, line.x + line.width, midY, 1.0f,
                            style.color.r, style.color.g, style.color.b,
                            style.color.a);
        }
      }
    } else if (font) {
      // Fallback: render text content directly
      renderer.drawText(box->box.content.x,
                        box->box.content.y + style.fontSize,
                        box->node->textContent, *font, style.color.r,
                        style.color.g, style.color.b, style.color.a, style.fontSize);
    }
  }

  // 5. Handle overflow clipping and scrolling
  bool hasClipping = style.overflow == skene::Overflow::Hidden ||
                     style.overflow == skene::Overflow::Scroll ||
                     style.overflow == skene::Overflow::Auto;
  bool hasScrolling = box->isScrollable();
  
  if (hasClipping) {
    // Flush any pending draws before setting clip rect
    renderer.flushRects();
    renderer.setClipRect(box->box.content.x, box->box.content.y,
                         box->box.content.width, box->box.content.height);
  }
  
  // Apply scroll offset for scrollable elements
  if (hasScrolling) {
    renderer.pushTranslate(-box->scrollX, -box->scrollY);
  }

  // 6. Paint children
  for (auto &child : box->children) {
    paint(renderer, child, fontManager, styleSheet, viewportTop, viewportBottom);
  }
  
  // Pop scroll translation
  if (hasScrolling) {
    renderer.popTranslate(-box->scrollX, -box->scrollY);
  }

  // 7. Draw scrollbar BEFORE clearing clip rect (so it's not clipped by parent)
  // The scrollbar should be drawn within our own clip rect, not the parent's
  if (hasScrolling && box->scrollableHeight > 0) {
    float contentX = box->box.content.x;
    float contentY = box->box.content.y;
    float contentW = box->box.content.width;
    float contentH = box->box.content.height;
    float totalHeight = contentH + box->scrollableHeight;
    
    // Scrollbar track
    float scrollbarWidth = 8.0f;
    float scrollbarX = contentX + contentW - scrollbarWidth;
    renderer.drawRect(scrollbarX, contentY, scrollbarWidth, contentH, 0.9f, 0.9f, 0.9f, 0.5f);
    
    // Scrollbar thumb
    float thumbHeight = (contentH / totalHeight) * contentH;
    thumbHeight = std::max(thumbHeight, 20.0f);  // Minimum thumb size
    float thumbY = contentY + (box->scrollY / box->maxScrollY()) * (contentH - thumbHeight);
    renderer.drawRect(scrollbarX, thumbY, scrollbarWidth, thumbHeight, 0.5f, 0.5f, 0.5f, 0.8f);
  }

  // 8. Clear clipping after drawing scrollbar
  if (hasClipping) {
    renderer.flushRects();
    renderer.clearClipRect();
  }

  // Reset opacity
  renderer.setOpacity(1.0f);
}

// Reload function for Ctrl+R
void reloadPage() {
  if (!g_renderTree || !g_styleSheet || !g_fontManager || !g_dom) return;
  
  // Save current scroll position before reloading
  float savedScrollY = scrollY;
  
  std::string filename = "index.html";
  std::string html;
  
  std::ifstream htmlFile(filename);
  if (htmlFile) {
    std::stringstream buffer;
    buffer << htmlFile.rdbuf();
    html = buffer.str();
    std::cout << "Reloading: " << filename << std::endl;
  } else {
    std::cerr << "Error: Could not reload " << filename << std::endl;
    return;
  }
  
  // Parse HTML
  skene::HtmlParser parser;
  auto parseResult = parser.parseWithStyles(html);
  g_dom = parseResult.document;
  
  // Reset stylesheet
  g_styleSheet->rules.clear();
  
  // Load user agent stylesheet
  std::ifstream uaFile("src/style/userAgent.css");
  if (uaFile) {
    std::stringstream uaBuffer;
    uaBuffer << uaFile.rdbuf();
    g_styleSheet->loadUserAgentStylesheet(uaBuffer.str());
  }
  
  // Load CSS from <style> tags
  for (const auto& cssContent : parseResult.styleContents) {
    g_styleSheet->addStylesheet(cssContent);
  }
  
  // Rebuild layout
  g_renderTree->buildAndLayout(g_dom, (float)(screenWidth - INSPECTOR_WIDTH),
                              *g_styleSheet, g_fontManager);
  
  // Reset text selection
  textSelection.allTextBoxes.clear();
  textSelection.hasSelection = false;
  selectedNode = nullptr;
  
  // Restore scroll position (clamped to new max scroll)
  scrollY = savedScrollY;
  if (scrollY < 0) scrollY = 0;
  if (scrollY > maxScrollY) scrollY = maxScrollY;
  
  std::cout << "Scroll restored to: " << scrollY << " (max: " << maxScrollY << ")" << std::endl;
  
  g_needsRender = true;
}

// Render function that can be called during resize
void doRender() {
  if (!g_renderer || !g_renderTree || !g_styleSheet || !g_fontManager || !g_window) return;
  
  auto& renderer = *g_renderer;
  auto& renderTree = *g_renderTree;
  auto& styleSheet = *g_styleSheet;
  auto& fontManager = *g_fontManager;
  
  // Re-layout with new size
  renderTree.relayout((float)(screenWidth - INSPECTOR_WIDTH), (float)screenHeight,
                      styleSheet, &fontManager);

  // Calculate max scroll based on content height
  if (renderTree.root) {
    float contentHeight = renderTree.root->box.borderBox().height;
    maxScrollY = std::max(0.0f, contentHeight - (float)screenHeight);
    if (scrollY > maxScrollY) scrollY = maxScrollY;
  }

  // Rebuild text boxes list
  textSelection.allTextBoxes.clear();
  collectTextBoxes(renderTree.root, textSelection.allTextBoxes, false);

  renderer.clear();

  // Set up clipping for content area
  glEnable(GL_SCISSOR_TEST);
  glScissor(0, 0, screenWidth - INSPECTOR_WIDTH, screenHeight);

  // Apply scroll offset
  renderer.pushTranslate(0, -scrollY);
  
  // Calculate viewport bounds in content space (accounting for scroll)
  float viewportTop = scrollY;
  float viewportBottom = scrollY + screenHeight;
  
  // Draw selection highlights first (fills gaps between inline elements)
  paintSelectionHighlights(renderer, fontManager);
  
  paint(renderer, renderTree.root, fontManager, styleSheet, viewportTop, viewportBottom);
  renderer.popTranslate(0, -scrollY);

  glDisable(GL_SCISSOR_TEST);

  // Draw scrollbar if needed
  if (maxScrollY > 0) {
    float viewportHeight = (float)screenHeight;
    float contentHeight = viewportHeight + maxScrollY;
    float scrollbarHeight = (viewportHeight / contentHeight) * viewportHeight;
    float scrollbarY = (scrollY / maxScrollY) * (viewportHeight - scrollbarHeight);
    float scrollbarX = (float)(screenWidth - INSPECTOR_WIDTH - 10);
    renderer.drawRect(scrollbarX, 0, 8, viewportHeight, 0.9f, 0.9f, 0.9f, 0.5f);
    renderer.drawRect(scrollbarX, scrollbarY, 8, scrollbarHeight, 0.6f, 0.6f, 0.6f, 0.8f);
  }

  // Inspector
  renderer.drawRect((screenWidth - INSPECTOR_WIDTH), 0, INSPECTOR_WIDTH, screenHeight,
                    0.9f, 0.9f, 0.9f, 1.0f);
  renderer.drawRectOutline((screenWidth - INSPECTOR_WIDTH), 0, INSPECTOR_WIDTH,
                           screenHeight, 0.5f, 0.5f, 0.5f, 1.0f);

  float sidebarX = (float)(screenWidth - INSPECTOR_WIDTH);
  paintSidebarTabs(renderer, fontManager, sidebarX, 0);

  inspectorLines.clear();
  float inspectY = TAB_HEIGHT;
  const float INSPECTOR_TREE_HEIGHT = (screenHeight - TAB_HEIGHT) * 0.6f;
  const float INSPECTOR_STYLES_START = TAB_HEIGHT + INSPECTOR_TREE_HEIGHT + 10;

  if (currentSidebarTab == SidebarTab::Inspector) {
    paintInspector(renderer, g_dom, fontManager, sidebarX, inspectY, 0);
    renderer.drawRect(sidebarX, TAB_HEIGHT + INSPECTOR_TREE_HEIGHT, INSPECTOR_WIDTH,
                      2, 0.5f, 0.5f, 0.5f, 1.0f);
    paintStyles(renderer, fontManager, sidebarX, INSPECTOR_STYLES_START);
  } else {
    paintPerformanceView(renderer, fontManager, sidebarX, TAB_HEIGHT, screenHeight - TAB_HEIGHT);
  }

  renderer.endFrame();
  SDL_GL_SwapWindow(g_window);
}

// Event watcher for real-time resize rendering
int resizeEventWatcher(void* data, SDL_Event* event) {
  if (event->type == SDL_WINDOWEVENT) {
    if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
        event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
      screenWidth = event->window.data1;
      screenHeight = event->window.data2;
      if (g_renderer) {
        g_renderer->resize(screenWidth, screenHeight);
      }
      doRender();
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL IO Error" << std::endl;
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

  SDL_Window *window = SDL_CreateWindow(
      "Skene Browser", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      screenWidth, screenHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(1);

  // Create system cursors
  SDL_Cursor* arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
  SDL_Cursor* ibeamCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
  SDL_Cursor* handCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
  SDL_Cursor* currentCursor = arrowCursor;

  // Enable Text Input for Editor
  SDL_StartTextInput();

  skene::Renderer renderer(screenWidth, screenHeight);
  skene::MSDFFontManager fontManager;  // MSDF font manager for sharp text
  
  // Initialize core fonts synchronously (GPU-accelerated) before rendering
  fontManager.initializeCoreFonts();
  
  // Start background font discovery thread for additional system fonts
  fontManager.startBackgroundDiscovery();
  
  std::cout << "MSDF: Discovered " << fontManager.getRegisteredFontCount() 
            << " system fonts (" << fontManager.getCachedFontCount() << " cached)" << std::endl;

  std::string filename = "index.html";
  if (argc > 1)
    filename = argv[1];

  std::string html;
  std::ifstream htmlFile(filename);
  if (htmlFile) {
    std::stringstream buffer;
    buffer << htmlFile.rdbuf();
    html = buffer.str();
  } else {
    html = "<div><h1>Error</h1><p>No index.html</p></div>";
  }

  skene::HtmlParser parser;
  auto parseResult = parser.parseWithStyles(html);
  auto dom = parseResult.document;

  skene::RenderTree renderTree;
  skene::StyleSheet styleSheet;

  // Load user agent stylesheet (browser defaults)
  std::ifstream uaFile("src/style/userAgent.css");
  if (uaFile) {
    std::stringstream uaBuffer;
    uaBuffer << uaFile.rdbuf();
    styleSheet.loadUserAgentStylesheet(uaBuffer.str());
    std::cout << "Loaded user agent stylesheet" << std::endl;
  } else {
    std::cerr << "Warning: Could not load userAgent.css" << std::endl;
  }

  // Load CSS from <style> tags (author stylesheet)
  for (const auto& cssContent : parseResult.styleContents) {
    styleSheet.addStylesheet(cssContent);
  }

  // Initial Layout
  renderTree.buildAndLayout(dom, (float)(screenWidth - INSPECTOR_WIDTH),
                            styleSheet, &fontManager);

  // Set up global pointers for resize event watcher
  g_window = window;
  g_renderer = &renderer;
  g_renderTree = &renderTree;
  g_styleSheet = &styleSheet;
  g_fontManager = &fontManager;
  g_dom = dom;
  
  // Add event watcher for real-time resize rendering
  SDL_AddEventWatch(resizeEventWatcher, nullptr);

  // Initialize FPS tracking
  fpsLastTime = SDL_GetTicks();

  bool quit = false;
  SDL_Event e;

  while (!quit) {
    // Track frame time
    frameStartTime = SDL_GetTicks();
    
    // Calculate FPS every second
    fpsFrameCount++;
    Uint32 currentTime = SDL_GetTicks();
    if (currentTime - fpsLastTime >= 1000) {
      fpsCurrent = fpsFrameCount * 1000.0f / (currentTime - fpsLastTime);
      fpsFrameCount = 0;
      fpsLastTime = currentTime;
    }
    
    cursorTimer = (cursorTimer + 1) % 60;

    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
      } else if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
            e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          screenWidth = e.window.data1;
          screenHeight = e.window.data2;
          renderer.resize(screenWidth, screenHeight);
          // Reset scroll if needed
          scrollY = 0;
        }
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        int mx = e.button.x;
        int my = e.button.y;
        Uint32 currentTime = SDL_GetTicks();
        
        // Detect double/triple click
        if (currentTime - lastClickTime < DOUBLE_CLICK_TIME &&
            std::abs(mx - lastClickX) < DOUBLE_CLICK_DISTANCE &&
            std::abs(my - lastClickY) < DOUBLE_CLICK_DISTANCE) {
          clickCount++;
        } else {
          clickCount = 1;
        }
        lastClickTime = currentTime;
        lastClickX = mx;
        lastClickY = my;

        // Check inspector click
        if (mx >= (screenWidth - INSPECTOR_WIDTH)) {
          // Check if clicking on tabs
          if (my < TAB_HEIGHT) {
            float tabWidth = INSPECTOR_WIDTH / 2.0f;
            float relativeX = mx - (screenWidth - INSPECTOR_WIDTH);
            if (relativeX < tabWidth) {
              currentSidebarTab = SidebarTab::Inspector;
            } else {
              currentSidebarTab = SidebarTab::Performance;
            }
          } else if (currentSidebarTab == SidebarTab::Inspector) {
            // Only process inspector line clicks when on Inspector tab
            // Find which line
            for (const auto &line : inspectorLines) {
              if (my >= line.y && my < line.y + line.h) {
                selectedNode = line.node;
                std::cout << "Selected: " << selectedNode->tagName << std::endl;
                break;
              }
            }
          } else if (currentSidebarTab == SidebarTab::Performance) {
            // Check VSync checkbox click
            if (vsyncCheckbox.isValid &&
                mx >= vsyncCheckbox.x && mx < vsyncCheckbox.x + vsyncCheckbox.width + 80 &&
                my >= vsyncCheckbox.y && my < vsyncCheckbox.y + vsyncCheckbox.height) {
              vsyncEnabled = !vsyncEnabled;
              SDL_GL_SetSwapInterval(vsyncEnabled ? 1 : 0);
              std::cout << "VSync: " << (vsyncEnabled ? "ON" : "OFF") << std::endl;
            }
            
            // Check slider clicks
            auto checkSlider = [&](SliderBounds& slider) {
              if (slider.isValid &&
                  mx >= slider.x && mx < slider.x + slider.width &&
                  my >= slider.y - 4 && my < slider.y + slider.height + 4) {
                activeSlider = &slider;
                // Update value based on click position
                float ratio = (mx - slider.x) / slider.width;
                ratio = std::max(0.0f, std::min(1.0f, ratio));
                float newVal = slider.minVal + ratio * (slider.maxVal - slider.minVal);
                if (&slider == &edgeLowSlider) {
                  renderer.setMsdfEdgeLow(newVal);
                } else {
                  renderer.setMsdfEdgeHigh(newVal);
                }
              }
            };
            checkSlider(edgeLowSlider);
            checkSlider(edgeHighSlider);
          }
        } else {
          // Content area - adjust for scroll and handle text selection based on click count
          float contentX = (float)mx;
          float contentY = (float)my + scrollY;  // Adjust for scroll
          
          // Check if clicking on a link first
          auto clickedBox = findBoxAtPoint(renderTree.root, contentX, (float)my, scrollY);
          if (clickedBox) {
            std::string href = findLinkHref(clickedBox->node);
            if (!href.empty() && href != "#" && clickCount == 1) {
              // Open link on single click
              std::cout << "Opening link: " << href << std::endl;
              #ifdef _WIN32
              ShellExecuteA(NULL, "open", href.c_str(), NULL, NULL, SW_SHOWNORMAL);
              #else
              std::string cmd = "xdg-open \"" + href + "\" &";
              system(cmd.c_str());
              #endif
              continue;  // Don't start text selection on link click
            }
          }
          
          size_t lineIdx = 0, charIdx = 0;
          auto textBox = findTextBoxAt(renderTree.root, contentX, contentY, fontManager, lineIdx, charIdx, true);
          
          // Check for Shift+Click to extend selection
          bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
          
          if (textBox && !textBox->textLines.empty()) {
            // Check user-select CSS property
            const std::string &userSelect = textBox->computedStyle.userSelect;
            
            if (userSelect == "none") {
              // Selection not allowed on this element
              // Don't start selection, but don't clear existing selection either
            } else if (userSelect == "all") {
              // Select entire element on single click
              textSelection.anchorBox = textBox;
              textSelection.focusBox = textBox;
              textSelection.anchorLineIndex = 0;
              textSelection.anchorCharIndex = 0;
              textSelection.focusLineIndex = textBox->textLines.size() - 1;
              textSelection.focusCharIndex = textBox->textLines.back().text.length();
              textSelection.hasSelection = true;
              textSelection.isSelecting = false;
              selectionMode = SelectionMode::Character;
            } else {
              // Normal selection (auto or text)
              const auto &line = textBox->textLines[lineIdx];
            
              if (shiftHeld && textSelection.hasSelection) {
                // Shift+Click: extend selection from anchor to clicked position
                textSelection.focusBox = textBox;
                textSelection.focusLineIndex = lineIdx;
                textSelection.focusCharIndex = charIdx;
                textSelection.hasSelection = true;
                textSelection.isSelecting = false;
                selectionMode = SelectionMode::Character;
              } else if (clickCount >= 3) {
                // Triple-click or more: select entire block element (paragraph, etc.)
                // This matches Chrome behavior where triple-click selects the whole paragraph
                auto [firstBox, lastBox] = findBlockTextBoxRange(textBox, textSelection.allTextBoxes);
                
                textSelection.anchorBox = firstBox;
                textSelection.focusBox = lastBox;
                textSelection.anchorLineIndex = 0;
                textSelection.anchorCharIndex = 0;
                textSelection.focusLineIndex = lastBox->textLines.size() - 1;
                textSelection.focusCharIndex = lastBox->textLines.back().text.length();
                textSelection.hasSelection = true;
                textSelection.isSelecting = false; // No drag expansion for paragraph select
                selectionMode = SelectionMode::Line;
              } else if (clickCount == 2) {
                // Double-click: select word across text boxes (Chrome behavior)
                // This allows selecting "padding," when clicking on <strong>padding</strong>
                auto crossBoxSel = findWordBoundariesAcrossBoxes(textBox, lineIdx, charIdx, textSelection.allTextBoxes);
                textSelection.anchorBox = crossBoxSel.startBox;
                textSelection.focusBox = crossBoxSel.endBox;
                textSelection.anchorLineIndex = crossBoxSel.startLineIdx;
                textSelection.anchorCharIndex = crossBoxSel.startCharIdx;
                textSelection.focusLineIndex = crossBoxSel.endLineIdx;
                textSelection.focusCharIndex = crossBoxSel.endCharIdx;
                textSelection.hasSelection = true;
                textSelection.isSelecting = true; // Enable drag for word-wise selection
                selectionMode = SelectionMode::Word;
                // Remember anchor word boundaries for expansion (use the original box's boundaries)
                auto [wordStart, wordEnd] = findWordBoundaries(line.text, charIdx);
                anchorWordStart = wordStart;
                anchorWordEnd = wordEnd;
              } else {
                // Single click: start character-wise drag selection
                textSelection.startSelection(textBox, lineIdx, charIdx);
                selectionMode = SelectionMode::Character;
              }
            }
          } else {
            textSelection.clear();
            selectionMode = SelectionMode::Character;
          }
        }
      } else if (e.type == SDL_MOUSEMOTION) {
        int mx = e.motion.x;
        int my = e.motion.y;
        
        // Handle slider dragging
        if (activeSlider) {
          float ratio = (mx - activeSlider->x) / activeSlider->width;
          ratio = std::max(0.0f, std::min(1.0f, ratio));
          float newVal = activeSlider->minVal + ratio * (activeSlider->maxVal - activeSlider->minVal);
          if (activeSlider == &edgeLowSlider) {
            renderer.setMsdfEdgeLow(newVal);
          } else {
            renderer.setMsdfEdgeHigh(newVal);
          }
        }
        // Update selection while dragging
        else if (textSelection.isSelecting) {
          
          // Adjust for scroll when in content area
          float contentY = (float)my + (mx < (screenWidth - INSPECTOR_WIDTH) ? scrollY : 0);
          
          size_t lineIdx = 0, charIdx = 0;
          // Use findTextBoxAtY for drag selection - prioritizes vertical position
          // This ensures dragging far left/right still selects text at that Y row
          auto textBox = findTextBoxAtY((float)mx, contentY, fontManager, lineIdx, charIdx);
          if (textBox && !textBox->textLines.empty()) {
            if (selectionMode == SelectionMode::Word) {
              // Word-wise selection: snap to word boundaries
              const auto &line = textBox->textLines[lineIdx];
              auto [wordStart, wordEnd] = findWordBoundaries(line.text, charIdx);
              
              // Determine direction from anchor
              bool isSameBox = (textBox == textSelection.anchorBox);
              bool isAfterAnchor = false;
              
              if (isSameBox && lineIdx == textSelection.anchorLineIndex) {
                isAfterAnchor = (charIdx >= anchorWordEnd);
              } else {
                // Compare box positions in document order
                auto it1 = std::find(textSelection.allTextBoxes.begin(), textSelection.allTextBoxes.end(), textSelection.anchorBox);
                auto it2 = std::find(textSelection.allTextBoxes.begin(), textSelection.allTextBoxes.end(), textBox);
                isAfterAnchor = (it2 > it1) || (it2 == it1 && lineIdx > textSelection.anchorLineIndex);
              }
              
              if (isAfterAnchor) {
                // Extending forward: anchor at word start, focus at current word end
                textSelection.anchorCharIndex = anchorWordStart;
                textSelection.focusBox = textBox;
                textSelection.focusLineIndex = lineIdx;
                textSelection.focusCharIndex = wordEnd;
              } else {
                // Extending backward: anchor at word end, focus at current word start
                textSelection.anchorCharIndex = anchorWordEnd;
                textSelection.focusBox = textBox;
                textSelection.focusLineIndex = lineIdx;
                textSelection.focusCharIndex = wordStart;
              }
              textSelection.hasSelection = true;
            } else if (selectionMode == SelectionMode::Line) {
              // Line-wise selection: select entire lines
              textSelection.focusBox = textBox;
              textSelection.focusLineIndex = lineIdx;
              
              // Determine direction
              auto it1 = std::find(textSelection.allTextBoxes.begin(), textSelection.allTextBoxes.end(), textSelection.anchorBox);
              auto it2 = std::find(textSelection.allTextBoxes.begin(), textSelection.allTextBoxes.end(), textBox);
              bool isAfterAnchor = (it2 > it1) || (it2 == it1 && lineIdx > textSelection.anchorLineIndex);
              
              if (isAfterAnchor) {
                textSelection.anchorCharIndex = 0;
                textSelection.focusCharIndex = textBox->textLines[lineIdx].text.length();
              } else {
                textSelection.anchorCharIndex = textSelection.anchorBox->textLines[textSelection.anchorLineIndex].text.length();
                textSelection.focusCharIndex = 0;
              }
              textSelection.hasSelection = true;
            } else {
              // Character-wise selection (default)
              textSelection.updateSelection(textBox, lineIdx, charIdx);
            }
          }
        }
        
        // Update cursor based on mouse position (even when not dragging)
        // (mx, my already declared above)
        
        // Check if over text in content area (not inspector)
        if (mx < (screenWidth - INSPECTOR_WIDTH)) {
          float contentY = (float)my + scrollY;  // Adjust for scroll
          
          // First check if hovering over a link
          auto hoverBox = findBoxAtPoint(renderTree.root, (float)mx, (float)my, scrollY);
          bool isOverLink = hoverBox && isInsideLink(hoverBox);
          
          SDL_Cursor* desiredCursor;
          if (isOverLink) {
            desiredCursor = handCursor;
          } else {
            // Check if over text
            size_t dummyLine = 0, dummyChar = 0;
            auto textHoverBox = findTextBoxAtExact(renderTree.root, (float)mx, contentY, fontManager, dummyLine, dummyChar);
            desiredCursor = textHoverBox ? ibeamCursor : arrowCursor;
          }
          
          if (currentCursor != desiredCursor) {
            currentCursor = desiredCursor;
            SDL_SetCursor(currentCursor);
          }
        } else {
          // Inspector area - always arrow cursor
          if (currentCursor != arrowCursor) {
            currentCursor = arrowCursor;
            SDL_SetCursor(currentCursor);
          }
        }
      } else if (e.type == SDL_MOUSEBUTTONUP) {
        if (textSelection.isSelecting) {
          textSelection.endSelection();
        }
        // Release slider
        activeSlider = nullptr;
      } else if (e.type == SDL_MOUSEWHEEL) {
        // Handle scrolling - only in content area
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mx < (screenWidth - INSPECTOR_WIDTH)) {
          // Check if hovering over a scrollable element
          float contentX = (float)mx;
          float contentY = (float)my + scrollY;  // Account for page scroll
          
          // Get the scrollable element chain (innermost first, then ancestors)
          std::vector<std::shared_ptr<skene::RenderBox>> scrollableChain;
          auto scrollableBox = findScrollableElementAt(g_renderTree->root, contentX, contentY, 0, 0, &scrollableChain);
          
          float scrollDelta = e.wheel.y * SCROLL_SPEED;
          bool scrollConsumed = false;
          
          // Try to scroll each element in the chain, propagating overflow
          for (size_t i = 0; i < scrollableChain.size() && !scrollConsumed; ++i) {
            auto& box = scrollableChain[i];
            float oldScrollY = box->scrollY;
            
            // Apply scroll delta
            box->scrollY -= scrollDelta;
            box->clampScroll();
            
            // Calculate how much was actually scrolled
            float actualScroll = oldScrollY - box->scrollY;
            
            // If we scrolled the full amount, consume the event
            if (std::abs(actualScroll - scrollDelta) < 0.01f) {
              scrollConsumed = true;
            } else {
              // Calculate remaining scroll delta to propagate
              scrollDelta -= actualScroll;
            }
          }
          
          // If no element consumed the scroll (or no scrollable elements), scroll the page
          if (!scrollConsumed) {
            scrollY -= scrollDelta;
            if (scrollY < 0) scrollY = 0;
            if (scrollY > maxScrollY) scrollY = maxScrollY;
          }
        }
      } else if (e.type == SDL_TEXTINPUT) {
        if (selectedNode && selectedNode->type == skene::NodeType::Element) {
          selectedNode->attributes["style"] += e.text.text;
        }
      } else if (e.type == SDL_KEYDOWN) {
        // Ctrl+R to reload page
        if (e.key.keysym.sym == SDLK_r && (e.key.keysym.mod & KMOD_CTRL)) {
          reloadPage();
        }
        if (e.key.keysym.sym == SDLK_BACKSPACE && selectedNode) {
          std::string &style = selectedNode->attributes["style"];
          if (!style.empty()) {
            style.pop_back();
          }
        }
        // Ctrl+C to copy selection
        if (e.key.keysym.sym == SDLK_c && (e.key.keysym.mod & KMOD_CTRL)) {
          std::string selected = getSelectedText(textSelection);
          if (!selected.empty()) {
            SDL_SetClipboardText(selected.c_str());
            std::cout << "Copied to clipboard: \"" << selected << "\"" << std::endl;
          }
        }
        // Ctrl+A to select all (entire document)
        if (e.key.keysym.sym == SDLK_a && (e.key.keysym.mod & KMOD_CTRL)) {
          // Select from first text box to last text box
          if (!textSelection.allTextBoxes.empty()) {
            auto firstBox = textSelection.allTextBoxes.front();
            auto lastBox = textSelection.allTextBoxes.back();
            
            if (firstBox && !firstBox->textLines.empty() && 
                lastBox && !lastBox->textLines.empty()) {
              textSelection.anchorBox = firstBox;
              textSelection.focusBox = lastBox;
              textSelection.anchorLineIndex = 0;
              textSelection.anchorCharIndex = 0;
              textSelection.focusLineIndex = lastBox->textLines.size() - 1;
              textSelection.focusCharIndex = lastBox->textLines.back().text.length();
              textSelection.hasSelection = true;
            }
          }
        }
        
        // Shift+Arrow to extend selection
        bool shiftHeld = (e.key.keysym.mod & KMOD_SHIFT) != 0;
        bool ctrlHeld = (e.key.keysym.mod & KMOD_CTRL) != 0;
        
        if (shiftHeld && (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_RIGHT ||
                         e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_DOWN)) {
          // Initialize selection if none exists
          if (!textSelection.hasSelection && textSelection.focusBox) {
            textSelection.anchorBox = textSelection.focusBox;
            textSelection.anchorLineIndex = textSelection.focusLineIndex;
            textSelection.anchorCharIndex = textSelection.focusCharIndex;
            textSelection.hasSelection = true;
          }
          
          if (textSelection.focusBox && !textSelection.focusBox->textLines.empty()) {
            const auto &line = textSelection.focusBox->textLines[textSelection.focusLineIndex];
            
            if (e.key.keysym.sym == SDLK_RIGHT) {
              if (ctrlHeld) {
                // Ctrl+Shift+Right: move to end of next word
                auto [wordStart, wordEnd] = findWordBoundaries(line.text, textSelection.focusCharIndex);
                if (wordEnd < line.text.length()) {
                  // Find next word after current
                  size_t nextStart = wordEnd;
                  while (nextStart < line.text.length() && isWordBoundaryAt(line.text, nextStart)) {
                    ++nextStart;
                  }
                  if (nextStart < line.text.length()) {
                    auto [nextWordStart, nextWordEnd] = findWordBoundaries(line.text, nextStart);
                    textSelection.focusCharIndex = nextWordEnd;
                  } else {
                    textSelection.focusCharIndex = line.text.length();
                  }
                } else {
                  textSelection.focusCharIndex = line.text.length();
                }
              } else {
                // Shift+Right: move one character
                if (textSelection.focusCharIndex < line.text.length()) {
                  textSelection.focusCharIndex++;
                } else {
                  // At end of current box, move to next text box
                  int currentIdx = -1;
                  for (int i = 0; i < (int)textSelection.allTextBoxes.size(); ++i) {
                    if (textSelection.allTextBoxes[i] == textSelection.focusBox) {
                      currentIdx = i;
                      break;
                    }
                  }
                  if (currentIdx >= 0 && currentIdx + 1 < (int)textSelection.allTextBoxes.size()) {
                    textSelection.focusBox = textSelection.allTextBoxes[currentIdx + 1];
                    textSelection.focusLineIndex = 0;
                    // Skip leading whitespace in the new box
                    textSelection.focusCharIndex = 0;
                    if (!textSelection.focusBox->textLines.empty()) {
                      const auto &newLine = textSelection.focusBox->textLines[0].text;
                      while (textSelection.focusCharIndex < newLine.length() && 
                             (newLine[textSelection.focusCharIndex] == ' ' || newLine[textSelection.focusCharIndex] == '\t')) {
                        textSelection.focusCharIndex++;
                      }
                    }
                  }
                }
              }
              // Reset goalX when moving horizontally
              textSelection.resetGoalX();
            } else if (e.key.keysym.sym == SDLK_LEFT) {
              if (ctrlHeld) {
                // Ctrl+Shift+Left: move to start of previous word
                if (textSelection.focusCharIndex > 0) {
                  size_t pos = textSelection.focusCharIndex - 1;
                  // Skip whitespace/punctuation
                  while (pos > 0 && isWordBoundaryAt(line.text, pos)) {
                    --pos;
                  }
                  // Find start of word
                  auto [wordStart, wordEnd] = findWordBoundaries(line.text, pos);
                  textSelection.focusCharIndex = wordStart;
                }
              } else {
                // Shift+Left: move one character
                if (textSelection.focusCharIndex > 0) {
                  textSelection.focusCharIndex--;
                } else {
                  // At start of current box, move to previous text box
                  int currentIdx = -1;
                  for (int i = 0; i < (int)textSelection.allTextBoxes.size(); ++i) {
                    if (textSelection.allTextBoxes[i] == textSelection.focusBox) {
                      currentIdx = i;
                      break;
                    }
                  }
                  if (currentIdx > 0) {
                    textSelection.focusBox = textSelection.allTextBoxes[currentIdx - 1];
                    textSelection.focusLineIndex = 0;
                    if (!textSelection.focusBox->textLines.empty()) {
                      const auto &newLine = textSelection.focusBox->textLines[0].text;
                      // Go to end of text, skipping trailing whitespace
                      textSelection.focusCharIndex = newLine.length();
                      while (textSelection.focusCharIndex > 0 && 
                             (newLine[textSelection.focusCharIndex - 1] == ' ' || newLine[textSelection.focusCharIndex - 1] == '\t')) {
                        textSelection.focusCharIndex--;
                      }
                    } else {
                      textSelection.focusCharIndex = 0;
                    }
                  }
                }
              }
              // Reset goalX when moving horizontally
              textSelection.resetGoalX();
            } else if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_DOWN) {
              // Shift+Up/Down: move selection to line above/below
              // Chrome-style: use sticky goalX to maintain column position across lines
              
              // Calculate current cursor X position
              float cursorX = textSelection.focusBox->textLines[textSelection.focusLineIndex].x;
              if (textSelection.focusCharIndex > 0) {
                float fontSize = textSelection.focusBox->computedStyle.fontSize;
                const auto &currentLine = textSelection.focusBox->textLines[textSelection.focusLineIndex];
                std::string beforeCursor = currentLine.text.substr(0, textSelection.focusCharIndex);
                cursorX += fontManager.getDefaultFont()->getTextWidth(beforeCursor, fontSize);
              }
              
              // Set goalX on first up/down press, keep it for subsequent presses
              if (textSelection.goalX < 0) {
                textSelection.goalX = cursorX;
              }
              // Use goalX for positioning (maintains column across short lines)
              float targetX = textSelection.goalX;
              
              float currentY = textSelection.focusBox->textLines[textSelection.focusLineIndex].y;
              
              // Collect all visual lines from all text boxes
              struct VisualLine {
                std::shared_ptr<skene::RenderBox> box;
                size_t lineIndex;
                float y;
                float x;
                float width;
              };
              std::vector<VisualLine> allLines;
              
              for (const auto &box : textSelection.allTextBoxes) {
                for (size_t li = 0; li < box->textLines.size(); ++li) {
                  const auto &tl = box->textLines[li];
                  allLines.push_back({box, li, tl.y, tl.x, tl.width});
                }
              }
              
              // Sort by Y position, then X for lines at same Y
              std::sort(allLines.begin(), allLines.end(), [](const VisualLine &a, const VisualLine &b) {
                if (std::abs(a.y - b.y) < 1.0f) return a.x < b.x;
                return a.y < b.y;
              });
              
              // Find current line index
              int currentLineIdx = -1;
              for (int i = 0; i < (int)allLines.size(); ++i) {
                if (allLines[i].box == textSelection.focusBox && 
                    allLines[i].lineIndex == textSelection.focusLineIndex) {
                  currentLineIdx = i;
                  break;
                }
              }
              
              if (currentLineIdx >= 0) {
                int targetLineIdx = -1;
                
                if (e.key.keysym.sym == SDLK_UP) {
                  // Find line above: first line with Y significantly less than current
                  for (int i = currentLineIdx - 1; i >= 0; --i) {
                    if (allLines[i].y < currentY - 1.0f) {
                      float targetY = allLines[i].y;
                      // Find the line at this Y that contains or is closest to targetX
                      float bestDist = FLT_MAX;
                      for (int j = i; j >= 0 && allLines[j].y >= targetY - 1.0f; --j) {
                        // Prefer line that contains targetX
                        if (targetX >= allLines[j].x && targetX <= allLines[j].x + allLines[j].width) {
                          targetLineIdx = j;
                          bestDist = -1; // Can't beat this
                        } else if (bestDist >= 0) {
                          float dist = std::min(std::abs(allLines[j].x - targetX), 
                                               std::abs(allLines[j].x + allLines[j].width - targetX));
                          if (dist < bestDist) {
                            bestDist = dist;
                            targetLineIdx = j;
                          }
                        }
                      }
                      break;
                    }
                  }
                  // If no line above, go to start of first line
                  if (targetLineIdx < 0 && !allLines.empty()) {
                    targetLineIdx = 0;
                    // Move to start of document
                    textSelection.focusBox = allLines[0].box;
                    textSelection.focusLineIndex = allLines[0].lineIndex;
                    textSelection.focusCharIndex = 0;
                  }
                } else { // SDLK_DOWN
                  // Find line below: first line with Y significantly greater than current
                  for (int i = currentLineIdx + 1; i < (int)allLines.size(); ++i) {
                    if (allLines[i].y > currentY + 1.0f) {
                      float targetY = allLines[i].y;
                      // Find the line at this Y that contains or is closest to targetX
                      float bestDist = FLT_MAX;
                      for (int j = i; j < (int)allLines.size() && allLines[j].y <= targetY + 1.0f; ++j) {
                        // Prefer line that contains targetX
                        if (targetX >= allLines[j].x && targetX <= allLines[j].x + allLines[j].width) {
                          targetLineIdx = j;
                          bestDist = -1;
                        } else if (bestDist >= 0) {
                          float dist = std::min(std::abs(allLines[j].x - targetX), 
                                               std::abs(allLines[j].x + allLines[j].width - targetX));
                          if (dist < bestDist) {
                            bestDist = dist;
                            targetLineIdx = j;
                          }
                        }
                      }
                      break;
                    }
                  }
                  // If no line below, go to end of last line
                  if (targetLineIdx < 0 && !allLines.empty()) {
                    int lastIdx = (int)allLines.size() - 1;
                    const auto &lastLine = allLines[lastIdx];
                    textSelection.focusBox = lastLine.box;
                    textSelection.focusLineIndex = lastLine.lineIndex;
                    textSelection.focusCharIndex = lastLine.box->textLines[lastLine.lineIndex].text.length();
                  }
                }
                
                if (targetLineIdx >= 0) {
                  const auto &targetLine = allLines[targetLineIdx];
                  textSelection.focusBox = targetLine.box;
                  textSelection.focusLineIndex = targetLine.lineIndex;
                  
                  // Find character position at targetX (goalX)
                  const auto &tl = targetLine.box->textLines[targetLine.lineIndex];
                  float fontSize = targetLine.box->computedStyle.fontSize;
                  size_t charIdx = 0;
                  float x = tl.x;
                  
                  // If targetX is before line start, position at start
                  if (targetX <= tl.x) {
                    charIdx = 0;
                  } 
                  // If targetX is after line end, position at end
                  else if (targetX >= tl.x + tl.width) {
                    charIdx = tl.text.length();
                  }
                  // Otherwise find the closest character
                  else {
                    for (size_t i = 0; i < tl.text.length(); ++i) {
                      float charW = fontManager.getDefaultFont()->getTextWidth(tl.text.substr(i, 1), fontSize);
                      if (targetX < x + charW / 2) {
                        break;
                      }
                      x += charW;
                      charIdx = i + 1;
                    }
                  }
                  textSelection.focusCharIndex = charIdx;
                }
              }
            }
          }
        }
      }
    }

    // Live Re-Layout!
    // Since we modifying DOM attributes directly, we just need to re-run layout
    renderTree.relayout((float)(screenWidth - INSPECTOR_WIDTH), (float)screenHeight,
                        styleSheet, &fontManager);

    // Calculate max scroll based on content height
    if (renderTree.root) {
      float contentHeight = renderTree.root->box.borderBox().height;
      maxScrollY = std::max(0.0f, contentHeight - (float)screenHeight);
      // Clamp scrollY if content shrunk
      if (scrollY > maxScrollY) scrollY = maxScrollY;
    }

    // Rebuild text boxes list for selection (must be done after layout)
    textSelection.allTextBoxes.clear();
    static bool debugOnce = true;
    collectTextBoxes(renderTree.root, textSelection.allTextBoxes, debugOnce);
    if (debugOnce) {
      std::cout << "Total text boxes collected: " << textSelection.allTextBoxes.size() << std::endl;
      debugOnce = false;
    }

    renderer.clear();

    // Set up clipping for content area (exclude inspector)
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, screenWidth - INSPECTOR_WIDTH, screenHeight);

    // Apply scroll offset for content rendering
    renderer.pushTranslate(0, -scrollY);

    // Calculate viewport bounds in content space (accounting for scroll)
    float viewportTop = scrollY;
    float viewportBottom = scrollY + screenHeight;

    // Paint content with scroll and culling
    paint(renderer, renderTree.root, fontManager, styleSheet, viewportTop, viewportBottom);

    // Remove scroll offset
    renderer.popTranslate(0, -scrollY);

    // Disable clipping before drawing inspector
    glDisable(GL_SCISSOR_TEST);

    // Draw scrollbar if content overflows
    if (maxScrollY > 0) {
      float viewportHeight = (float)screenHeight;
      float contentHeight = viewportHeight + maxScrollY;
      float scrollbarHeight = (viewportHeight / contentHeight) * viewportHeight;
      float scrollbarY = (scrollY / maxScrollY) * (viewportHeight - scrollbarHeight);
      
      // Scrollbar track
      float scrollbarX = (float)(screenWidth - INSPECTOR_WIDTH - 10);
      renderer.drawRect(scrollbarX, 0, 8, viewportHeight, 0.9f, 0.9f, 0.9f, 0.5f);
      
      // Scrollbar thumb
      renderer.drawRect(scrollbarX, scrollbarY, 8, scrollbarHeight, 0.6f, 0.6f, 0.6f, 0.8f);
    }

    // Inspector BG
    renderer.drawRect((screenWidth - INSPECTOR_WIDTH), 0, INSPECTOR_WIDTH, screenHeight,
                      0.9f, 0.9f, 0.9f, 1.0f);
    renderer.drawRectOutline((screenWidth - INSPECTOR_WIDTH), 0, INSPECTOR_WIDTH,
                             screenHeight, 0.5f, 0.5f, 0.5f, 1.0f);

    // Draw sidebar tabs at top
    float sidebarX = (float)(screenWidth - INSPECTOR_WIDTH);
    paintSidebarTabs(renderer, fontManager, sidebarX, 0);

    // Reset and rebuild inspector lines for current frame (simple immediate
    // mode detection)
    inspectorLines.clear();
    float inspectY = TAB_HEIGHT; // Start below tabs

    const float INSPECTOR_TREE_HEIGHT = (screenHeight - TAB_HEIGHT) * 0.6f; // 60% for tree
    const float INSPECTOR_STYLES_START = TAB_HEIGHT + INSPECTOR_TREE_HEIGHT + 10;

    if (currentSidebarTab == SidebarTab::Inspector) {
      // 1. Tree View (Top 60%)
      paintInspector(renderer, dom, fontManager, sidebarX, inspectY, 0);

      // 2. Divider
      renderer.drawRect(sidebarX, TAB_HEIGHT + INSPECTOR_TREE_HEIGHT, INSPECTOR_WIDTH,
                        2, 0.5f, 0.5f, 0.5f, 1.0f);

      // 3. Styles View (Bottom 40%)
      paintStyles(renderer, fontManager, sidebarX, INSPECTOR_STYLES_START);
    } else {
      // Performance view
      paintPerformanceView(renderer, fontManager, sidebarX, TAB_HEIGHT, screenHeight - TAB_HEIGHT);
    }

    // Calculate frame time at end of frame
    frameTimeMs = (float)(SDL_GetTicks() - frameStartTime);

    renderer.endFrame();
    SDL_GL_SwapWindow(window);
  }

  // Remove event watcher and clean up global pointers
  SDL_DelEventWatch(resizeEventWatcher, nullptr);
  g_window = nullptr;
  g_renderer = nullptr;
  g_renderTree = nullptr;
  g_styleSheet = nullptr;
  g_fontManager = nullptr;
  g_dom = nullptr;

  SDL_StopTextInput();
  SDL_FreeCursor(arrowCursor);
  SDL_FreeCursor(ibeamCursor);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
