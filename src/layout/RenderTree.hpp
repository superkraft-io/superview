#pragma once

#include "dom/Node.hpp"
#include "render/MSDFFont.hpp"
#include "style/StyleSheet.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace skene {

struct Rect {
  float x = 0, y = 0, width = 0, height = 0;

  float right() const { return x + width; }
  float bottom() const { return y + height; }
};

// Box model dimensions
struct BoxDimensions {
  Rect content;
  EdgeValues padding;
  EdgeValues border;
  EdgeValues margin;

  // Get the full margin box
  Rect marginBox() const {
    return Rect{content.x - padding.left.toPx() - border.left.toPx() -
                    margin.left.toPx(),
                content.y - padding.top.toPx() - border.top.toPx() -
                    margin.top.toPx(),
                content.width + padding.left.toPx() + padding.right.toPx() +
                    border.left.toPx() + border.right.toPx() +
                    margin.left.toPx() + margin.right.toPx(),
                content.height + padding.top.toPx() + padding.bottom.toPx() +
                    border.top.toPx() + border.bottom.toPx() +
                    margin.top.toPx() + margin.bottom.toPx()};
  }

  // Get the border box (content + padding + border)
  Rect borderBox() const {
    return Rect{content.x - padding.left.toPx() - border.left.toPx(),
                content.y - padding.top.toPx() - border.top.toPx(),
                content.width + padding.left.toPx() + padding.right.toPx() +
                    border.left.toPx() + border.right.toPx(),
                content.height + padding.top.toPx() + padding.bottom.toPx() +
                    border.top.toPx() + border.bottom.toPx()};
  }

  // Get the padding box (content + padding)
  Rect paddingBox() const {
    return Rect{content.x - padding.left.toPx(),
                content.y - padding.top.toPx(),
                content.width + padding.left.toPx() + padding.right.toPx(),
                content.height + padding.top.toPx() + padding.bottom.toPx()};
  }
};

// Text selection state - supports cross-element selection
struct TextSelection {
  // Selection anchor (where selection started)
  std::shared_ptr<class RenderBox> anchorBox = nullptr;
  size_t anchorLineIndex = 0;
  size_t anchorCharIndex = 0;
  
  // Selection focus (where selection currently ends)
  std::shared_ptr<class RenderBox> focusBox = nullptr;
  size_t focusLineIndex = 0;
  size_t focusCharIndex = 0;
  
  bool isSelecting = false;  // Currently dragging
  bool hasSelection = false; // Selection exists
  
  // Sticky X position for up/down navigation (like Chrome)
  // -1 means not set, will be calculated on first up/down press
  float goalX = -1.0f;
  
  // Document-order list of all text boxes (rebuilt each frame)
  std::vector<std::shared_ptr<class RenderBox>> allTextBoxes;
  
  void clear() {
    anchorBox = nullptr;
    focusBox = nullptr;
    anchorLineIndex = anchorCharIndex = 0;
    focusLineIndex = focusCharIndex = 0;
    isSelecting = false;
    hasSelection = false;
    goalX = -1.0f;
  }
  
  void startSelection(std::shared_ptr<class RenderBox> box, size_t lineIdx, size_t charIdx) {
    anchorBox = focusBox = box;
    anchorLineIndex = focusLineIndex = lineIdx;
    anchorCharIndex = focusCharIndex = charIdx;
    isSelecting = true;
    hasSelection = false;
    goalX = -1.0f;  // Reset goalX when starting new selection
  }
  
  void updateSelection(std::shared_ptr<class RenderBox> box, size_t lineIdx, size_t charIdx) {
    focusBox = box;
    focusLineIndex = lineIdx;
    focusCharIndex = charIdx;
    hasSelection = (anchorBox != focusBox || anchorLineIndex != focusLineIndex || anchorCharIndex != focusCharIndex);
  }
  
  // Reset goalX when user moves horizontally (left/right keys or mouse click)
  void resetGoalX() {
    goalX = -1.0f;
  }
  
  void endSelection() {
    isSelecting = false;
  }
  
  // Get index of a box in the document order (-1 if not found)
  int getBoxIndex(std::shared_ptr<class RenderBox> box) const {
    for (size_t i = 0; i < allTextBoxes.size(); ++i) {
      if (allTextBoxes[i].get() == box.get()) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }
  
  // Check if a box is within the selection range
  // Returns: -1 = before selection, 0 = within selection, 1 = after selection
  int getBoxSelectionState(std::shared_ptr<class RenderBox> box) const {
    if (!hasSelection || !anchorBox || !focusBox) return -1;
    
    int boxIdx = getBoxIndex(box);
    int anchorIdx = getBoxIndex(anchorBox);
    int focusIdx = getBoxIndex(focusBox);
    
    if (boxIdx < 0 || anchorIdx < 0 || focusIdx < 0) return -1;
    
    int startIdx = std::min(anchorIdx, focusIdx);
    int endIdx = std::max(anchorIdx, focusIdx);
    
    if (boxIdx < startIdx) return -1;
    if (boxIdx > endIdx) return 1;
    return 0;  // Within selection
  }
  
  // Check if this is the start box of the selection
  bool isStartBox(std::shared_ptr<class RenderBox> box) const {
    if (!hasSelection) return false;
    int anchorIdx = getBoxIndex(anchorBox);
    int focusIdx = getBoxIndex(focusBox);
    int boxIdx = getBoxIndex(box);
    
    if (anchorIdx <= focusIdx) {
      return boxIdx == anchorIdx;
    } else {
      return boxIdx == focusIdx;
    }
  }
  
  // Check if this is the end box of the selection
  bool isEndBox(std::shared_ptr<class RenderBox> box) const {
    if (!hasSelection) return false;
    int anchorIdx = getBoxIndex(anchorBox);
    int focusIdx = getBoxIndex(focusBox);
    int boxIdx = getBoxIndex(box);
    
    if (anchorIdx <= focusIdx) {
      return boxIdx == focusIdx;
    } else {
      return boxIdx == anchorIdx;
    }
  }
  
  // Get selection range for a specific box
  // Returns (startChar, endChar) for the given line, or (-1, -1) if not selected
  std::pair<size_t, size_t> getSelectionRangeForLine(
      std::shared_ptr<class RenderBox> box, size_t lineIdx, size_t lineLength) const {
    
    int state = getBoxSelectionState(box);
    if (state != 0) return {0, 0};  // Not in selection
    
    bool isStart = isStartBox(box);
    bool isEnd = isEndBox(box);
    
    // Determine anchor/focus order
    int anchorIdx = getBoxIndex(anchorBox);
    int focusIdx = getBoxIndex(focusBox);
    bool anchorFirst = anchorIdx <= focusIdx;
    
    size_t startLine, startChar, endLine, endChar;
    if (anchorFirst) {
      startLine = anchorLineIndex;
      startChar = anchorCharIndex;
      endLine = focusLineIndex;
      endChar = focusCharIndex;
    } else {
      startLine = focusLineIndex;
      startChar = focusCharIndex;
      endLine = anchorLineIndex;
      endChar = anchorCharIndex;
    }
    
    if (isStart && isEnd) {
      // Same box - original behavior
      if (startLine > endLine || (startLine == endLine && startChar > endChar)) {
        std::swap(startLine, endLine);
        std::swap(startChar, endChar);
      }
      if (lineIdx < startLine || lineIdx > endLine) return {0, 0};
      size_t selStart = (lineIdx == startLine) ? startChar : 0;
      size_t selEnd = (lineIdx == endLine) ? endChar : lineLength;
      return {selStart, selEnd};
    } else if (isStart) {
      // Start box - select from anchor/focus to end of box
      if (lineIdx < startLine) return {0, 0};
      size_t selStart = (lineIdx == startLine) ? startChar : 0;
      return {selStart, lineLength};
    } else if (isEnd) {
      // End box - select from start of box to anchor/focus
      if (lineIdx > endLine) return {0, 0};
      size_t selEnd = (lineIdx == endLine) ? endChar : lineLength;
      return {0, selEnd};
    } else {
      // Middle box - select entire line
      return {0, lineLength};
    }
  }
};

class RenderBox : public std::enable_shared_from_this<RenderBox> {
public:
  std::shared_ptr<Node> node;
  Rect frame;            // Legacy - for compatibility
  BoxDimensions box;     // Full box model
  StyleSheet::ComputedStyle computedStyle;
  std::vector<std::shared_ptr<RenderBox>> children;
  std::weak_ptr<RenderBox> parent;

  // Text layout info
  struct TextLine {
    std::string text;
    float x, y;
    float width, height;
    size_t startIndex = 0;  // Character offset in original text
  };
  std::vector<TextLine> textLines;

  // Scroll state for overflow:scroll/auto elements
  float scrollX = 0.0f;
  float scrollY = 0.0f;
  float scrollableWidth = 0.0f;   // Content width beyond container
  float scrollableHeight = 0.0f;  // Content height beyond container
  
  // Returns true if this element has scrollable overflow
  bool isScrollable() const {
    return (computedStyle.overflow == Overflow::Scroll || computedStyle.overflow == Overflow::Auto) &&
           (scrollableHeight > 0 || scrollableWidth > 0);
  }
  
  // Returns max scroll values
  float maxScrollX() const { return std::max(0.0f, scrollableWidth); }
  float maxScrollY() const { return std::max(0.0f, scrollableHeight); }
  
  // Clamp scroll to valid range
  void clampScroll() {
    scrollX = std::max(0.0f, std::min(scrollX, maxScrollX()));
    scrollY = std::max(0.0f, std::min(scrollY, maxScrollY()));
  }

  RenderBox(std::shared_ptr<Node> n) : node(n) {}

  void addChild(std::shared_ptr<RenderBox> child) {
    children.push_back(child);
    child->parent = weak_from_this();
  }

  // New layout with StyleSheet and FontManager support
  void layout(float x, float y, float availableWidth, StyleSheet &styleSheet,
              MSDFFontManager *fontManager, float viewportWidth = 1024.0f,
              float viewportHeight = 768.0f, bool inInlineFlow = false) {
    // Compute style for this node
    computedStyle = styleSheet.computeStyle(*node);
    
    // CSS Inheritance: Certain properties inherit from parent by default
    // This applies to both text nodes AND element nodes
    auto parentBox = parent.lock();
    if (parentBox) {
      // Text nodes inherit all text-related properties
      if (node->type == NodeType::Text) {
        computedStyle.color = parentBox->computedStyle.color;
        computedStyle.fontSize = parentBox->computedStyle.fontSize;
        computedStyle.fontWeight = parentBox->computedStyle.fontWeight;
        computedStyle.fontStyle = parentBox->computedStyle.fontStyle;
        computedStyle.fontFamily = parentBox->computedStyle.fontFamily;
        computedStyle.textDecoration = parentBox->computedStyle.textDecoration;
        computedStyle.textAlign = parentBox->computedStyle.textAlign;
        computedStyle.lineHeight = parentBox->computedStyle.lineHeight;
      }
      // Element nodes: inherit text properties if not explicitly set
      // color inherits by default in CSS (unless overridden)
      else if (node->type == NodeType::Element) {
        // Check if color was explicitly set via CSS rules or inline style
        // For now, inherit if color is still the default black and parent isn't black
        // A more complete solution would track which properties were explicitly set
        bool hasInlineStyle = node->attributes.find("style") != node->attributes.end();
        std::string inlineStyle = hasInlineStyle ? node->attributes.at("style") : "";
        bool colorExplicitlySet = inlineStyle.find("color") != std::string::npos;
        
        // Also check if any CSS rule set the color for this element
        // For simplicity, we inherit color if it's still the default
        if (!colorExplicitlySet && computedStyle.color == Color::Black()) {
          computedStyle.color = parentBox->computedStyle.color;
        }
        
        // Inherit text-align, font-family, line-height from parent
        // unless explicitly set via inline style
        bool textAlignExplicitlySet = inlineStyle.find("text-align") != std::string::npos;
        if (!textAlignExplicitlySet) {
          computedStyle.textAlign = parentBox->computedStyle.textAlign;
        }
        
        bool fontFamilyExplicitlySet = inlineStyle.find("font-family") != std::string::npos;
        if (!fontFamilyExplicitlySet) {
          computedStyle.fontFamily = parentBox->computedStyle.fontFamily;
        }
        
        bool lineHeightExplicitlySet = inlineStyle.find("line-height") != std::string::npos;
        if (!lineHeightExplicitlySet) {
          computedStyle.lineHeight = parentBox->computedStyle.lineHeight;
        }
      }
    }
    
    auto &style = computedStyle;

    // Skip if display:none
    if (style.display == DisplayType::Hidden) {
      frame = {x, y, 0, 0};
      return;
    }

    // Get the correct font for this element's style
    MSDFFont* font = fontManager->getFont(style.fontFamily, 
        static_cast<int>(style.fontWeight), static_cast<int>(style.fontStyle));
    if (!font) font = fontManager->getDefaultFont();

    float fontSize = style.fontSize;
    float parentWidth = availableWidth;

    // Get margin values
    float marginTop = style.getMarginTop(parentWidth, fontSize);
    float marginRight = style.getMarginRight(parentWidth, fontSize);
    float marginBottom = style.getMarginBottom(parentWidth, fontSize);
    float marginLeft = style.getMarginLeft(parentWidth, fontSize);

    // Get padding values
    float paddingTop = style.getPaddingTop(parentWidth, fontSize);
    float paddingRight = style.getPaddingRight(parentWidth, fontSize);
    float paddingBottom = style.getPaddingBottom(parentWidth, fontSize);
    float paddingLeft = style.getPaddingLeft(parentWidth, fontSize);

    // Get border widths
    float borderTop = style.getBorderTopWidth();
    float borderRight = style.getBorderRightWidth();
    float borderBottom = style.getBorderBottomWidth();
    float borderLeft = style.getBorderLeftWidth();

    // Special-case: checkbox/radio inputs should not behave like text inputs.
    // (Attribute selectors may not be applied yet, so use the DOM "type" attribute.)
    bool isCheckableInput = false;
    bool isCheckboxInput = false;
    if (node->type == NodeType::Element) {
      std::string tag = node->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      if (tag == "input") {
        auto typeIt = node->attributes.find("type");
        std::string inputType = "text";
        if (typeIt != node->attributes.end()) {
          inputType = typeIt->second;
          std::transform(inputType.begin(), inputType.end(), inputType.begin(), ::tolower);
        }
        isCheckboxInput = (inputType == "checkbox");
        isCheckableInput = isCheckboxInput || (inputType == "radio");
      }
    }

    if (isCheckableInput) {
      // Remove default input padding/border from layout so the control is a true 16x16 box.
      paddingTop = paddingRight = paddingBottom = paddingLeft = 0.0f;
      borderTop = borderRight = borderBottom = borderLeft = 0.0f;

      box.padding = EdgeValues(CssValue{0, CssUnit::Px});
      box.border = EdgeValues(CssValue{0, CssUnit::Px});

      // Give checkboxes a little gap before label text.
      if (isCheckboxInput) {
        marginRight += 4.0f;
        box.margin = style.margin;
        box.margin.right = CssValue{4, CssUnit::Px};
      }
    }

    // Store in box model
    if (!isCheckableInput) {
      box.margin = style.margin;
      box.padding = style.padding;
      box.border = style.borderWidth;
    }

    // Calculate position (accounting for margin)
    float contentStartX = x + marginLeft + borderLeft + paddingLeft;
    float contentStartY = y + marginTop + borderTop + paddingTop;

    // Calculate available content width
    float totalHorizontalSpace =
        marginLeft + borderLeft + paddingLeft + paddingRight + borderRight + marginRight;

    // Determine width
    float contentWidth;
    if (!style.width.isAuto() && style.width.value >= 0) {
      contentWidth =
          style.width.toPx(parentWidth, fontSize, viewportWidth, viewportHeight);
      if (style.boxSizing == BoxSizing::BorderBox) {
        contentWidth -= (paddingLeft + paddingRight + borderLeft + borderRight);
      }
    } else if (style.display == DisplayType::Inline ||
               style.display == DisplayType::InlineBlock ||
               style.display == DisplayType::Table) {
      // For tables, measure actual column widths
      if (style.display == DisplayType::Table) {
        contentWidth = measureTableIntrinsicWidth(font, fontSize) - (paddingLeft + paddingRight + borderLeft + borderRight);
      } else {
        contentWidth = measureIntrinsicWidth(font, fontSize);
      }
    } else if (node->type == NodeType::Text && inInlineFlow) {
      // Text nodes in inline flow use intrinsic width (parent handles wrapping)
      contentWidth = measureIntrinsicWidth(font, fontSize);
    } else {
      // Block or standalone text: fill available width (allows wrapping)
      contentWidth = availableWidth - totalHorizontalSpace;
    }
    
    // Ensure non-negative content width
    if (contentWidth < 0) contentWidth = 0;

    // Apply min/max width constraints
    if (!style.minWidth.isAuto() && style.minWidth.value > 0) {
      contentWidth = std::max(
          contentWidth,
          style.minWidth.toPx(parentWidth, fontSize, viewportWidth, viewportHeight));
    }
    if (!style.maxWidth.isAuto() && style.maxWidth.value > 0) {
      contentWidth = std::min(
          contentWidth,
          style.maxWidth.toPx(parentWidth, fontSize, viewportWidth, viewportHeight));
    }

    // Set content box position
    box.content.x = contentStartX;
    box.content.y = contentStartY;
    box.content.width = contentWidth;

    // Layout children or text
    float contentHeight = 0;

    if (node->type == NodeType::Text) {
      // Text node: perform text wrapping
      contentHeight =
          layoutText(contentStartX, contentStartY, contentWidth, font, style);
    } else if (style.display == DisplayType::Flex) {
      contentHeight = layoutFlexChildren(contentStartX, contentStartY,
                                         contentWidth, styleSheet, fontManager,
                                         viewportWidth, viewportHeight);
    } else if (style.display == DisplayType::Table) {
      contentHeight = layoutTableChildren(contentStartX, contentStartY,
                                         contentWidth, styleSheet, fontManager,
                                         viewportWidth, viewportHeight);
    } else if (style.display == DisplayType::Block) {
      contentHeight = layoutBlockChildren(contentStartX, contentStartY,
                                          contentWidth, styleSheet, fontManager,
                                          viewportWidth, viewportHeight);
    } else if (style.display == DisplayType::TableRowGroup ||
               style.display == DisplayType::TableRow ||
               style.display == DisplayType::TableCell) {
      // Table structural elements: handled by parent table layout
      // Just layout their children as blocks
      contentHeight = layoutBlockChildren(contentStartX, contentStartY,
                                          contentWidth, styleSheet, fontManager,
                                          viewportWidth, viewportHeight);
    } else {
      // For inline elements that sized to intrinsic width, use a large width
      // to prevent internal wrapping (the parent handles line wrapping)
      float layoutWidth = contentWidth;
      if (style.display == DisplayType::Inline && style.width.isAuto()) {
        // Use a very large width to prevent wrapping inside inline elements
        layoutWidth = 100000.0f;
      }
      contentHeight = layoutInlineChildren(contentStartX, contentStartY,
                                           layoutWidth, styleSheet, fontManager,
                                           viewportWidth, viewportHeight);
    }

    // Store natural content height for scroll calculation
    float naturalContentHeight = contentHeight;
    
    // Form elements: ensure minimum dimensions
    if (node->type == NodeType::Element) {
      std::string tag = node->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      if (tag == "input") {
        auto typeIt = node->attributes.find("type");
        std::string inputType = "text";
        if (typeIt != node->attributes.end()) {
          inputType = typeIt->second;
          std::transform(inputType.begin(), inputType.end(), inputType.begin(), ::tolower);
        }

        if (inputType == "checkbox" || inputType == "radio") {
          // Fixed-size checkable controls
          if (style.width.isAuto()) {
            contentWidth = 16.0f;
            box.content.width = contentWidth;
          }
          if (style.height.isAuto()) {
            contentHeight = 16.0f;
            naturalContentHeight = contentHeight;
          }
        } else if (contentHeight < fontSize + 4) {
          contentHeight = fontSize + 4;  // Minimum height based on font size
          naturalContentHeight = contentHeight;
        }
      }
      // <textarea> element: use rows/cols attributes
      if (tag == "textarea") {
        int rows = 2;  // Default rows
        int cols = 20; // Default cols
        auto rowsAttr = node->attributes.find("rows");
        auto colsAttr = node->attributes.find("cols");
        if (rowsAttr != node->attributes.end()) {
          try { rows = std::stoi(rowsAttr->second); } catch (...) {}
        }
        if (colsAttr != node->attributes.end()) {
          try { cols = std::stoi(colsAttr->second); } catch (...) {}
        }
        // Approximate character width for monospace
        float charWidth = fontSize * 0.6f;
        float lineHeight = fontSize * 1.2f;
        
        if (style.width.isAuto()) {
          contentWidth = cols * charWidth;
          box.content.width = contentWidth;
        }
        if (style.height.isAuto()) {
          contentHeight = rows * lineHeight;
          naturalContentHeight = contentHeight;
        }
      }
      // <select> element: default dimensions
      if (tag == "select") {
        if (style.width.isAuto()) {
          contentWidth = 150.0f;
          box.content.width = contentWidth;
        }
        if (style.height.isAuto()) {
          contentHeight = fontSize + 8;
          naturalContentHeight = contentHeight;
        }
      }
      // <img> element: use width/height attributes if no CSS dimensions
      if (tag == "img") {
        // If no specified width, use attribute or default
        if (style.width.isAuto()) {
          auto widthAttr = node->attributes.find("width");
          if (widthAttr != node->attributes.end()) {
            try {
              contentWidth = std::stof(widthAttr->second);
              box.content.width = contentWidth;
            } catch (...) {}
          } else {
            // Default placeholder size
            contentWidth = 150.0f;
            box.content.width = contentWidth;
          }
        }
        // If no specified height, use attribute or default
        if (style.height.isAuto()) {
          auto heightAttr = node->attributes.find("height");
          if (heightAttr != node->attributes.end()) {
            try {
              contentHeight = std::stof(heightAttr->second);
            } catch (...) {}
          } else {
            // Default placeholder size
            contentHeight = 150.0f;
          }
          naturalContentHeight = contentHeight;
        }
      }
    }

    // Determine height
    float specifiedHeight = -1;
    if (!style.height.isAuto() && style.height.value >= 0) {
      specifiedHeight =
          style.height.toPx(parentWidth, fontSize, viewportWidth, viewportHeight);
      if (style.boxSizing == BoxSizing::BorderBox) {
        specifiedHeight -= (paddingTop + paddingBottom + borderTop + borderBottom);
      }
      contentHeight = specifiedHeight;
    }

    // Apply min/max height
    if (!style.minHeight.isAuto() && style.minHeight.value > 0) {
      contentHeight = std::max(
          contentHeight,
          style.minHeight.toPx(parentWidth, fontSize, viewportWidth, viewportHeight));
    }
    if (!style.maxHeight.isAuto() && style.maxHeight.value > 0) {
      float maxH = style.maxHeight.toPx(parentWidth, fontSize, viewportWidth, viewportHeight);
      if (contentHeight > maxH) {
        contentHeight = maxH;
      }
    }

    box.content.height = contentHeight;
    
    // Calculate scrollable area for overflow:scroll/auto elements
    if (style.overflow == Overflow::Scroll || style.overflow == Overflow::Auto) {
      scrollableHeight = std::max(0.0f, naturalContentHeight - contentHeight);
      scrollableWidth = 0.0f; // TODO: horizontal scrolling
      clampScroll();
    } else {
      scrollableHeight = 0.0f;
      scrollableWidth = 0.0f;
    }

    // Update legacy frame to border box for backward compatibility
    Rect borderBox = box.borderBox();
    frame.x = borderBox.x;
    frame.y = borderBox.y;
    frame.width = borderBox.width;
    frame.height = borderBox.height;
  }

  // Hit test for text selection
  // Returns true if point is within this box's text, sets lineIndex and charIndex
  bool hitTestText(float px, float py, MSDFFont *font, size_t &lineIndex, size_t &charIndex) {
    if (node->type != NodeType::Text || textLines.empty() || !font) {
      return false;
    }
    
    float fontSize = computedStyle.fontSize;
    
    // Check each text line
    for (size_t i = 0; i < textLines.size(); ++i) {
      const auto &line = textLines[i];
      float lineTop = line.y;
      float lineBottom = line.y + line.height;
      
      // Check if point is within this line's vertical bounds
      if (py >= lineTop && py < lineBottom) {
        lineIndex = i;
        float localX = px - line.x;
        charIndex = font->hitTestText(line.text, localX, fontSize);
        return true;
      }
    }
    
    // If above all lines, return start of first line
    if (py < textLines[0].y) {
      lineIndex = 0;
      charIndex = 0;
      return true;
    }
    
    // If below all lines, return end of last line
    if (py >= textLines.back().y) {
      lineIndex = textLines.size() - 1;
      charIndex = textLines.back().text.length();
      return true;
    }
    
    return false;
  }

  // Check if point is within box bounds
  bool containsPoint(float px, float py) const {
    Rect bbox = box.borderBox();
    return px >= bbox.x && px < bbox.x + bbox.width &&
           py >= bbox.y && py < bbox.y + bbox.height;
  }

private:
  float measureIntrinsicWidth(MSDFFont *font, float fontSize) {
    if (node->type == NodeType::Text && font) {
      return font->getTextWidth(node->textContent, fontSize);
    }
    
    // Form elements have minimum intrinsic widths
    if (node->type == NodeType::Element) {
      std::string tag = node->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      if (tag == "input") {
        auto typeIt = node->attributes.find("type");
        std::string inputType = "text";
        if (typeIt != node->attributes.end()) {
          inputType = typeIt->second;
          std::transform(inputType.begin(), inputType.end(), inputType.begin(), ::tolower);
        }
        if (inputType == "checkbox" || inputType == "radio") {
          return 16.0f + 4.0f;  // 16px checkbox + 4px right margin
        }
        return 150.0f;  // Default input width
      }
      if (tag == "button") {
        // Button width based on text content or minimum
        float textWidth = 0;
        for (auto &child : children) {
          textWidth += child->measureIntrinsicWidth(font, fontSize);
        }
        return std::max(textWidth, 40.0f);  // Minimum 40px
      }
      if (tag == "img") {
        // Use width attribute if specified, otherwise default
        auto widthAttr = node->attributes.find("width");
        if (widthAttr != node->attributes.end()) {
          try {
            return std::stof(widthAttr->second);
          } catch (...) {}
        }
        return 150.0f;  // Default placeholder width
      }
      if (tag == "textarea") {
        // Use cols attribute if specified
        int cols = 20;
        auto colsAttr = node->attributes.find("cols");
        if (colsAttr != node->attributes.end()) {
          try { cols = std::stoi(colsAttr->second); } catch (...) {}
        }
        return cols * fontSize * 0.6f;  // Approximate char width
      }
      if (tag == "select") {
        return 150.0f;  // Default select width
      }
    }

    // For block elements, use max width of children (they stack vertically)
    // For inline elements, sum widths (they flow horizontally)
    auto &style = computedStyle;
    bool isBlockElement = (style.display == DisplayType::Block || 
                          style.display == DisplayType::Flex ||
                          style.display == DisplayType::TableRow ||
                          style.display == DisplayType::Table);
    
    float padding = style.getPaddingLeft() + style.getPaddingRight();
    
    if (isBlockElement) {
      // Block elements: width is max of children (they stack vertically)
      float maxWidth = 0;
      for (auto &child : children) {
        maxWidth = std::max(maxWidth, child->measureIntrinsicWidth(font, fontSize));
      }
      return maxWidth + padding;
    } else {
      // Inline/inline-block elements: sum widths (they flow horizontally)
      float totalWidth = 0;
      for (auto &child : children) {
        totalWidth += child->measureIntrinsicWidth(font, fontSize);
      }
      return totalWidth + padding;
    }
  }

  // Measure intrinsic width for tables by calculating column widths
  float measureTableIntrinsicWidth(MSDFFont *font, float fontSize) {
    // Find all table rows (including through tbody/thead/tfoot)
    std::vector<std::vector<std::shared_ptr<RenderBox>>> cellsByRow;
    
    for (auto& child : children) {
      std::string tag = child->node->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      
      // Handle tbody, thead, tfoot
      if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
        for (auto& rowChild : child->children) {
          std::string rowTag = rowChild->node->tagName;
          std::transform(rowTag.begin(), rowTag.end(), rowTag.begin(), ::tolower);
          if (rowTag == "tr") {
            std::vector<std::shared_ptr<RenderBox>> cells;
            for (auto& cellChild : rowChild->children) {
              std::string cellTag = cellChild->node->tagName;
              std::transform(cellTag.begin(), cellTag.end(), cellTag.begin(), ::tolower);
              if (cellTag == "td" || cellTag == "th") {
                cells.push_back(cellChild);
              }
            }
            cellsByRow.push_back(cells);
          }
        }
      }
      // Direct TR children (no tbody)
      else if (tag == "tr") {
        std::vector<std::shared_ptr<RenderBox>> cells;
        for (auto& cellChild : child->children) {
          std::string cellTag = cellChild->node->tagName;
          std::transform(cellTag.begin(), cellTag.end(), cellTag.begin(), ::tolower);
          if (cellTag == "td" || cellTag == "th") {
            cells.push_back(cellChild);
          }
        }
        cellsByRow.push_back(cells);
      }
    }
    
    if (cellsByRow.empty()) return 0;
    
    // Determine number of columns
    size_t numColumns = 0;
    for (auto& rowCells : cellsByRow) {
      numColumns = std::max(numColumns, rowCells.size());
    }
    
    if (numColumns == 0) return 0;
    
    // Measure all cells to determine column widths
    std::vector<float> columnWidths(numColumns, 0);
    for (size_t rowIdx = 0; rowIdx < cellsByRow.size(); rowIdx++) {
      auto& rowCells = cellsByRow[rowIdx];
      for (size_t colIdx = 0; colIdx < rowCells.size(); colIdx++) {
        auto cell = rowCells[colIdx];
        auto& cellStyle = cell->computedStyle;
        float cellFontSize = cellStyle.fontSize;
        MSDFFont* cellFont = font;
        
        // Get cell padding and border
        float cellPaddingLeft = cellStyle.getPaddingLeft(1000, cellFontSize);
        float cellPaddingRight = cellStyle.getPaddingRight(1000, cellFontSize);
        float cellBorderLeft = cellStyle.getBorderLeftWidth();
        float cellBorderRight = cellStyle.getBorderRightWidth();
        float cellHorizontalSpace = cellPaddingLeft + cellPaddingRight + cellBorderLeft + cellBorderRight;
        
        // For text cells, measure just the text content (excluding padding)
        float cellContentWidth = 0;
        if (cell->node->type == NodeType::Text && cellFont) {
          cellContentWidth = cellFont->getTextWidth(cell->node->textContent, cellFontSize);
        } else {
          // For non-text cells, use children's intrinsic width without their padding
          for (auto& child : cell->children) {
            if (child->node->type == NodeType::Text && cellFont) {
              cellContentWidth += cellFont->getTextWidth(child->node->textContent, cellFontSize);
            }
          }
        }
        
        float cellTotalWidth = cellContentWidth + cellHorizontalSpace;
        columnWidths[colIdx] = std::max(columnWidths[colIdx], cellTotalWidth);
      }
    }
    
    // Sum up all column widths
    float totalTableWidth = 0;
    for (float w : columnWidths) {
      totalTableWidth += w;
    }
    
    // Add table padding and border
    auto &style = computedStyle;
    float tablePadding = style.getPaddingLeft() + style.getPaddingRight();
    float tableBorder = style.getBorderLeftWidth() + style.getBorderRightWidth();
    
    return totalTableWidth + tablePadding + tableBorder;
  }

  float layoutText(float x, float y, float maxWidth, MSDFFont *font,
                   const StyleSheet::ComputedStyle &style) {
    textLines.clear();

    if (!font || node->textContent.empty()) {
      return 0;
    }

    // Ensure positive width for text layout
    if (maxWidth <= 0) {
      maxWidth = 10000.0f; // Very large width - don't wrap
    }

    float fontSize = style.fontSize;
    float lineHeight = fontSize * style.lineHeight;
    float currentY = y;

    std::string text = node->textContent;
    
    // Check if text fits on one line
    float totalWidth = font->getTextWidth(text, fontSize);
    
    // Helper lambda to calculate line X based on text-align
    auto getLineX = [&](float lineWidth, float availWidth) -> float {
      switch (style.textAlign) {
        case TextAlign::Center:
          return x + (availWidth - lineWidth) / 2.0f;
        case TextAlign::Right:
          return x + availWidth - lineWidth;
        case TextAlign::Left:
        case TextAlign::Justify:
        default:
          return x;
      }
    };
    
    // If text fits on one line, don't wrap
    if (totalWidth <= maxWidth) {
      TextLine line;
      line.text = text;
      line.x = getLineX(totalWidth, maxWidth);
      line.y = currentY;
      line.width = totalWidth;
      line.height = lineHeight;
      textLines.push_back(line);
      return lineHeight;
    }

    // Word wrap algorithm - preserve spaces between words
    std::vector<std::string> words;
    std::string currentWord;
    
    for (size_t i = 0; i < text.length(); ++i) {
      char c = text[i];
      if (c == ' ') {
        if (!currentWord.empty()) {
          words.push_back(currentWord);
          currentWord.clear();
        }
        words.push_back(" "); // Preserve space as separate token
      } else {
        currentWord += c;
      }
    }
    if (!currentWord.empty()) {
      words.push_back(currentWord);
    }

    if (words.empty()) {
      return 0;
    }

    std::string currentLine;
    float currentLineWidth = 0;

    for (size_t i = 0; i < words.size(); ++i) {
      const std::string &word = words[i];
      float wordWidth = font->getTextWidth(word, fontSize);

      float testWidth = currentLineWidth + wordWidth;

      if (testWidth <= maxWidth || currentLine.empty()) {
        // Word fits on current line
        currentLine += word;
        currentLineWidth += wordWidth;
      } else {
        // Start new line - trim trailing space from current line
        while (!currentLine.empty() && currentLine.back() == ' ') {
          currentLine.pop_back();
          currentLineWidth = font->getTextWidth(currentLine, fontSize);
        }
        
        if (!currentLine.empty()) {
          TextLine line;
          line.text = currentLine;
          line.x = getLineX(currentLineWidth, maxWidth);
          line.y = currentY;
          line.width = currentLineWidth;
          line.height = lineHeight;
          textLines.push_back(line);
          currentY += lineHeight;
        }

        // Skip leading space on new line
        if (word == " ") {
          currentLine.clear();
          currentLineWidth = 0;
        } else {
          currentLine = word;
          currentLineWidth = wordWidth;
        }
      }
    }

    // Add remaining text
    // Trim trailing space
    while (!currentLine.empty() && currentLine.back() == ' ') {
      currentLine.pop_back();
    }
    
    if (!currentLine.empty()) {
      currentLineWidth = font->getTextWidth(currentLine, fontSize);
      TextLine line;
      line.text = currentLine;
      line.x = getLineX(currentLineWidth, maxWidth);
      line.y = currentY;
      line.width = currentLineWidth;
      line.height = lineHeight;
      textLines.push_back(line);
      currentY += lineHeight;
    }

    return currentY - y;
  }

  float layoutBlockChildren(float x, float y, float width,
                            StyleSheet &styleSheet, MSDFFontManager *fontManager,
                            float viewportWidth, float viewportHeight) {
    // Check if all children are inline and count inline elements vs text nodes
    bool allInline = true;
    int inlineElementCount = 0;
    int textNodeCount = 0;
    
    for (const auto &child : children) {
      StyleSheet::ComputedStyle childStyle = styleSheet.computeStyle(*child->node);
      bool isInlineElement = (childStyle.display == DisplayType::Inline ||
                              childStyle.display == DisplayType::InlineBlock);
      bool isTextNode = (child->node->type == NodeType::Text);
      
      if (isInlineElement) {
        inlineElementCount++;
      } else if (isTextNode) {
        textNodeCount++;
      } else {
        allInline = false;
        break;
      }
    }
    
    // If all children are inline AND there are inline elements mixed with text,
    // use inline layout (which handles wrapping at element boundaries)
    // If it's just text nodes, let them wrap normally with block layout
    bool hasInlineElements = inlineElementCount > 0;
    
    if (allInline && !children.empty() && hasInlineElements) {
      return layoutInlineChildren(x, y, width, styleSheet, fontManager, viewportWidth,
                                 viewportHeight);
    }
    
    // Otherwise, use block layout with CSS margin collapsing
    float currentY = y;
    float prevMarginBottom = 0.0f;  // Track previous block's bottom margin
    size_t i = 0;

    while (i < children.size()) {
      auto &child = children[i];
      
      // Compute style to determine display type
      StyleSheet::ComputedStyle childStyle = styleSheet.computeStyle(*child->node);
      
      // Check if this child is inline or text
      bool isInlineElement = (childStyle.display == DisplayType::Inline ||
                              childStyle.display == DisplayType::InlineBlock);
      bool isTextNode = (child->node->type == NodeType::Text);
      bool isInlineContext = isInlineElement || isTextNode;
      
      if (isInlineContext) {
        // Inline content (elements or text) - no margin collapsing
        std::vector<size_t> inlineGroup;
        while (i < children.size()) {
          auto &c = children[i];
          StyleSheet::ComputedStyle cStyle = styleSheet.computeStyle(*c->node);
          bool isInline = (cStyle.display == DisplayType::Inline ||
                           cStyle.display == DisplayType::InlineBlock ||
                           c->node->type == NodeType::Text);
          if (isInline) {
            inlineGroup.push_back(i);
            i++;
          } else {
            break;
          }
        }
        
        currentY += layoutInlineGroup(inlineGroup, x, currentY, width, styleSheet,
                                     fontManager, viewportWidth, viewportHeight);
        prevMarginBottom = 0.0f;  // Reset after inline content
      } else {
        // Block element - apply CSS margin collapsing
        float childMarginTop = childStyle.getMarginTop(width, childStyle.fontSize);
        float childMarginBottom = childStyle.getMarginBottom(width, childStyle.fontSize);
        
        // CSS Margin Collapsing: adjacent vertical margins collapse to the larger
        // Instead of adding both margins, use: max(prevBottom, currentTop)
        float collapsedMargin = std::max(prevMarginBottom, childMarginTop);
        
        // Position: currentY already includes prevMarginBottom, so subtract it 
        // and add the collapsed margin instead
        float childY = currentY - prevMarginBottom + collapsedMargin;
        
        // Layout child - but we need to tell it NOT to add its top margin
        // since we're handling it externally. We do this by passing a Y that
        // already accounts for the margin, then the child adds marginTop again.
        // Fix: pass Y position for where the margin-box top should be
        float marginBoxY = currentY - prevMarginBottom + collapsedMargin - childMarginTop;
        
        child->layout(x, marginBoxY, width, styleSheet, fontManager, viewportWidth,
                     viewportHeight, false);
        
        // Move currentY to after this child's border box, then add its bottom margin
        Rect borderBox = child->box.borderBox();
        currentY = borderBox.bottom() + childMarginBottom;
        
        // Store for next iteration
        prevMarginBottom = childMarginBottom;
        i++;
      }
    }

    // Return content height - keep trailing margin for proper spacing within padded containers
    return currentY - y;
  }
  
  // Helper to tokenize text into words for inline wrapping
  // Break points: space (discardable), after comma, after dash (for hyphenated words)
  // Examples: "padding, margin" -> ["padding,", " ", "margin"]
  //           "background-color" -> ["background-", "color"]
  std::vector<std::string> tokenizeForInlineLayout(const std::string &text) {
    std::vector<std::string> tokens;
    std::string currentToken;
    
    for (size_t i = 0; i < text.length(); ++i) {
      char c = text[i];
      
      if (c == ' ') {
        // Space is a break point - push current token and the space separately
        if (!currentToken.empty()) {
          tokens.push_back(currentToken);
          currentToken.clear();
        }
        tokens.push_back(" ");
      } else if (c == ',') {
        // Comma stays with previous word, break point is AFTER comma
        currentToken += c;
        // Only push if there's more text after this
        if (i + 1 < text.length()) {
          tokens.push_back(currentToken);
          currentToken.clear();
        }
      } else if (c == '-') {
        // Dash allows breaking - stays with previous part
        // Only break if there's text before AND after the dash
        if (!currentToken.empty() && i + 1 < text.length() && text[i + 1] != ' ') {
          currentToken += c;
          tokens.push_back(currentToken);
          currentToken.clear();
        } else {
          currentToken += c;
        }
      } else {
        currentToken += c;
      }
    }
    
    if (!currentToken.empty()) {
      tokens.push_back(currentToken);
    }
    
    return tokens;
  }
  
  // Check if a render box is an inline element with only text content
  bool isInlineWithTextOnly(const std::shared_ptr<RenderBox> &box) {
    if (box->node->type != NodeType::Element) return false;
    if (box->children.size() != 1) return false;
    return box->children[0]->node->type == NodeType::Text;
  }
  
  // Get the text content from an inline element with a single text child
  std::string getInlineTextContent(const std::shared_ptr<RenderBox> &box) {
    if (box->children.size() == 1 && box->children[0]->node->type == NodeType::Text) {
      return box->children[0]->node->textContent;
    }
    return "";
  }
  
  // Check if a string is punctuation-only (should not start a new line)
  bool isPunctuationOnly(const std::string &s) {
    if (s.empty()) return false;
    for (char c : s) {
      if (c != ',' && c != '.' && c != ';' && c != ':' && c != '!' && 
          c != '?' && c != ')' && c != ']' && c != '}' && c != '"' && 
          c != '\'' && c != '-') {
        return false;
      }
    }
    return true;
  }
  
  // Helper to layout text tokens with inline styling
  // Used for both bare text nodes and inline elements containing only text
  // Returns the new currentX position
  void layoutTextTokensInline(
      std::shared_ptr<RenderBox> &child,
      const std::string &text,
      float &currentX, float &currentY, float &maxLineHeight,
      float x, float width, MSDFFont *font,
      const StyleSheet::ComputedStyle &style) {
    
    float fontSize = style.fontSize;
    float textLineHeight = fontSize * style.lineHeight;
    
    // Tokenize text for word-level wrapping
    std::vector<std::string> tokens = tokenizeForInlineLayout(text);
    
    // Clear text lines - we'll build them manually
    child->textLines.clear();
    
    std::string currentLineText;
    float lineStartX = currentX;
    
    for (size_t t = 0; t < tokens.size(); ++t) {
      const std::string &token = tokens[t];
      float tokenWidth = font->getTextWidth(token, fontSize);
      
      // Check if token fits on current line
      // Don't wrap before punctuation - it should stay at end of previous line
      bool shouldWrap = (currentX + tokenWidth > x + width && currentX > x);
      if (shouldWrap && isPunctuationOnly(token)) {
        shouldWrap = false;  // Keep punctuation on current line even if it overflows slightly
      }
      
      if (shouldWrap) {
        // Doesn't fit - save current line text if any (trim trailing space)
        while (!currentLineText.empty() && currentLineText.back() == ' ') {
          currentLineText.pop_back();
        }
        if (!currentLineText.empty()) {
          float lineWidth = font->getTextWidth(currentLineText, fontSize);
          float lineX = lineStartX;  // Default to where text started
          // Apply text-align centering if needed
          if (style.textAlign == TextAlign::Center) {
            lineX = x + (width - lineWidth) / 2.0f;
          } else if (style.textAlign == TextAlign::Right) {
            lineX = x + width - lineWidth;
          }
          TextLine line;
          line.text = currentLineText;
          line.x = lineX;
          line.y = currentY;
          line.width = lineWidth;
          line.height = textLineHeight;
          child->textLines.push_back(line);
        }
        
        // Start new line
        currentX = x;
        currentY += maxLineHeight;
        maxLineHeight = textLineHeight;
        currentLineText.clear();
        lineStartX = currentX;
        
        // Skip leading space on new line
        if (token == " ") {
          continue;
        }
      }
      
      currentLineText += token;
      currentX += tokenWidth;
      maxLineHeight = std::max(maxLineHeight, textLineHeight);
    }
    
    // Save remaining text (trim trailing space)
    while (!currentLineText.empty() && currentLineText.back() == ' ') {
      currentLineText.pop_back();
    }
    if (!currentLineText.empty()) {
      float lineWidth = font->getTextWidth(currentLineText, fontSize);
      float lineX = lineStartX;  // Default to where text started
      // Apply text-align centering if needed
      if (style.textAlign == TextAlign::Center) {
        lineX = x + (width - lineWidth) / 2.0f;
      } else if (style.textAlign == TextAlign::Right) {
        lineX = x + width - lineWidth;
      }
      TextLine line;
      line.text = currentLineText;
      line.x = lineX;
      line.y = currentY;
      line.width = lineWidth;
      line.height = textLineHeight;
      child->textLines.push_back(line);
    }
    
    // Set child's frame to encompass all its text lines
    if (!child->textLines.empty()) {
      float minX = child->textLines[0].x;
      float minY = child->textLines[0].y;
      float maxX = minX;
      float maxY = minY;
      for (const auto &line : child->textLines) {
        minX = std::min(minX, line.x);
        maxX = std::max(maxX, line.x + line.width);
        maxY = std::max(maxY, line.y + line.height);
      }
      child->frame = {minX, minY, maxX - minX, maxY - minY};
      child->box.content = child->frame;
    } else {
      child->frame = {currentX, currentY, 0, 0};
      child->box.content = child->frame;
    }
  }
  
  // Helper to apply vertical-align adjustments to a line of inline elements
  void applyVerticalAlign(const std::vector<size_t>& lineIndices, float lineTop, float lineHeight) {
    for (size_t idx : lineIndices) {
      auto& child = children[idx];
      std::string vAlign = child->computedStyle.verticalAlign;
      float childHeight = child->frame.height;
      
      // Calculate the current Y position relative to line top
      float currentRelY = child->frame.y - lineTop;
      float desiredRelY = 0.0f;  // Default: top alignment
      
      if (vAlign == "baseline" || vAlign == "text-bottom" || vAlign == "bottom") {
        // Align bottom of element with bottom of line box
        desiredRelY = lineHeight - childHeight;
      } else if (vAlign == "middle") {
        // Align middle of element with middle of line box
        desiredRelY = (lineHeight - childHeight) / 2.0f;
      } else if (vAlign == "top" || vAlign == "text-top") {
        // Align top of element with top of line box
        desiredRelY = 0.0f;
      } else if (vAlign == "sub") {
        // Subscript - lower by 0.2em approximately
        desiredRelY = lineHeight - childHeight + child->computedStyle.fontSize * 0.2f;
      } else if (vAlign == "super") {
        // Superscript - raise by 0.4em approximately
        desiredRelY = -child->computedStyle.fontSize * 0.4f;
      }
      
      // Calculate the delta we need to move
      float yDelta = desiredRelY - currentRelY;
      
      if (std::abs(yDelta) > 0.01f) {
        // Adjust frame
        child->frame.y += yDelta;
        // Adjust box model positions
        child->box.content.y += yDelta;
        // Adjust text lines if any
        for (auto& textLine : child->textLines) {
          textLine.y += yDelta;
        }
        // For elements with children, adjust their frames too
        std::function<void(std::shared_ptr<RenderBox>&, float)> adjustChildren;
        adjustChildren = [&adjustChildren](std::shared_ptr<RenderBox>& box, float offset) {
          for (auto& c : box->children) {
            c->frame.y += offset;
            c->box.content.y += offset;
            for (auto& tl : c->textLines) {
              tl.y += offset;
            }
            adjustChildren(c, offset);
          }
        };
        adjustChildren(child, yDelta);
      }
    }
  }
  
  // Helper function to layout a group of inline elements on the same line(s)
  float layoutInlineGroup(const std::vector<size_t> &indices, float x, float y,
                         float width, StyleSheet &styleSheet, MSDFFontManager *fontManager,
                         float viewportWidth, float viewportHeight) {
    float currentX = x;
    float currentY = y;
    float lineHeight = 20.0f;
    float maxLineHeight = lineHeight;
    float lineStartY = y;
    std::vector<size_t> currentLineIndices;  // Track children on current line
    
    for (size_t idx : indices) {
      auto &child = children[idx];
      
      // Handle <br> element - force line break
      if (child->node->type == NodeType::Element) {
        std::string tag = child->node->tagName;
        std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        if (tag == "br") {
          // Apply vertical-align to current line before breaking
          if (!currentLineIndices.empty()) {
            applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
            currentLineIndices.clear();
          }
          // Force line break
          child->frame = {currentX, currentY, 0, maxLineHeight};
          child->box.content = child->frame;
          currentX = x;
          currentY += maxLineHeight;
          lineStartY = currentY;
          maxLineHeight = lineHeight;
          continue;
        }
      }
      
      // For text nodes, do word-level wrapping
      if (child->node->type == NodeType::Text) {
        // Compute style first to get font size
        child->computedStyle = styleSheet.computeStyle(*child->node);
        auto parentBox = child->parent.lock();
        if (parentBox) {
          child->computedStyle.color = parentBox->computedStyle.color;
          child->computedStyle.fontSize = parentBox->computedStyle.fontSize;
          child->computedStyle.fontWeight = parentBox->computedStyle.fontWeight;
          child->computedStyle.fontStyle = parentBox->computedStyle.fontStyle;
          child->computedStyle.fontFamily = parentBox->computedStyle.fontFamily;
          child->computedStyle.textDecoration = parentBox->computedStyle.textDecoration;
          child->computedStyle.textAlign = parentBox->computedStyle.textAlign;
          child->computedStyle.lineHeight = parentBox->computedStyle.lineHeight;
        }
        
        MSDFFont* font = fontManager->getFont(child->computedStyle.fontFamily,
            static_cast<int>(child->computedStyle.fontWeight), static_cast<int>(child->computedStyle.fontStyle));
        if (!font) font = fontManager->getDefaultFont();
        
        if (font) {
          layoutTextTokensInline(child, child->node->textContent, currentX, currentY,
                               maxLineHeight, x, width, font, child->computedStyle);
        }
        
      } else if (isInlineWithTextOnly(child)) {
        // Inline element with only text content (e.g., <code>, <strong>)
        // Tokenize and wrap the text, but apply the element's styling
        child->computedStyle = styleSheet.computeStyle(*child->node);
        
        // Apply text alignment inheritance for inline elements
        // Check if text-align is explicitly set in inline style
        bool hasInlineStyle = child->node->type == NodeType::Element && 
                             child->node->attributes.find("style") != child->node->attributes.end();
        std::string inlineStyle = hasInlineStyle ? child->node->attributes.at("style") : "";
        bool textAlignExplicitlySet = inlineStyle.find("text-align") != std::string::npos;
        
        if (!textAlignExplicitlySet) {
          // Inherit text-align from parent (this container)
          child->computedStyle.textAlign = computedStyle.textAlign;
        }
        
        // Only inherit font-size if the element doesn't have its own default size
        // (e.g., <code> has 13px default, should not inherit parent's 16px)
        std::string tag = "";
        if (child->node->type == NodeType::Element) {
          tag = child->node->tagName;
          std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        }
        bool hasOwnFontSize = (tag == "code" || tag == "pre" || tag == "kbd" || 
                               tag == "samp" || tag == "tt" || tag == "small" ||
                               tag == "sub" || tag == "sup" ||
                               tag.substr(0, 1) == "h");  // h1-h6
        if (!hasOwnFontSize) {
          child->computedStyle.fontSize = computedStyle.fontSize;
        }
        // Also inherit line-height if not explicitly set
        child->computedStyle.lineHeight = computedStyle.lineHeight;
        
        // Get the text child
        auto &textChild = child->children[0];
        textChild->computedStyle = styleSheet.computeStyle(*textChild->node);
        // Inherit styles from parent inline element
        textChild->computedStyle.color = child->computedStyle.color;
        textChild->computedStyle.fontSize = child->computedStyle.fontSize;
        textChild->computedStyle.fontWeight = child->computedStyle.fontWeight;
        textChild->computedStyle.fontStyle = child->computedStyle.fontStyle;
        textChild->computedStyle.fontFamily = child->computedStyle.fontFamily;
        textChild->computedStyle.textDecoration = child->computedStyle.textDecoration;
        textChild->computedStyle.textAlign = child->computedStyle.textAlign;
        textChild->computedStyle.lineHeight = child->computedStyle.lineHeight;
        
        // Get box model values
        float paddingLeft = child->computedStyle.padding.left.toPx();
        float paddingRight = child->computedStyle.padding.right.toPx();
        float borderLeft = child->computedStyle.borderWidth.left.toPx();
        float borderRight = child->computedStyle.borderWidth.right.toPx();
        float marginLeft = child->computedStyle.margin.left.toPx();
        float marginRight = child->computedStyle.margin.right.toPx();
        
        // Apply margin + border + padding before text
        currentX += marginLeft + borderLeft + paddingLeft;
        
        MSDFFont* font = fontManager->getFont(textChild->computedStyle.fontFamily,
            static_cast<int>(textChild->computedStyle.fontWeight), static_cast<int>(textChild->computedStyle.fontStyle));
        if (!font) font = fontManager->getDefaultFont();
        
        std::string text = getInlineTextContent(child);
        if (font) {
          layoutTextTokensInline(textChild, text, currentX, currentY,
                               maxLineHeight, x, width, font, textChild->computedStyle);
        }
        
        // Apply padding + border + margin after text
        currentX += paddingRight + borderRight + marginRight;
        
        // Update parent element's frame to include the full box model
        // box.content is the inner content area (where text goes)
        child->box.content = textChild->frame;
        child->box.padding = child->computedStyle.padding;
        child->box.border = child->computedStyle.borderWidth;
        child->box.margin = child->computedStyle.margin;
        // frame is the border box for compatibility
        child->frame = child->box.borderBox();
        
      } else {
        // Complex inline element - layout as a unit

        // Pre-measure intrinsic width to avoid wrapping inside the element just
        // because we're near the end of the line (common for <label><input> text).
        StyleSheet::ComputedStyle preStyle = styleSheet.computeStyle(*child->node);
        child->computedStyle = preStyle;
        MSDFFont* preFont = fontManager->getFont(preStyle.fontFamily,
            static_cast<int>(preStyle.fontWeight), static_cast<int>(preStyle.fontStyle));
        if (!preFont) preFont = fontManager->getDefaultFont();
        float idealWidth = child->measureIntrinsicWidth(preFont, preStyle.fontSize);
        float idealMarginLeft = preStyle.getMarginLeft(width, preStyle.fontSize);
        float idealMarginRight = preStyle.getMarginRight(width, preStyle.fontSize);
        float idealBorderLeft = preStyle.getBorderLeftWidth();
        float idealBorderRight = preStyle.getBorderRightWidth();
        float idealTotal = idealWidth + idealMarginLeft + idealMarginRight + idealBorderLeft + idealBorderRight;

        if (currentX > x && currentX + idealTotal > x + width) {
          // Apply vertical-align to current line before wrapping
          if (!currentLineIndices.empty()) {
            applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
            currentLineIndices.clear();
          }
          currentX = x;
          currentY += maxLineHeight;
          lineStartY = currentY;
          maxLineHeight = lineHeight;
        }

        child->layout(currentX, currentY, width - (currentX - x), styleSheet, fontManager,
                     viewportWidth, viewportHeight, true);
        
        Rect childBox = child->box.borderBox();
        
        // For inline elements, reposition to eliminate margin-induced gaps
        // The margin is already in the box model, we just need to position the border box
        float marginLeft = child->computedStyle.margin.left.toPx();
        child->box.content.x = currentX + marginLeft + child->computedStyle.borderWidth.left.toPx() + 
                               child->computedStyle.padding.left.toPx();
        
        // If child doesn't fit on this line and we've already placed something, wrap
        if (currentX + childBox.width > x + width && currentX > x) {
          // Apply vertical-align to current line before wrapping
          if (!currentLineIndices.empty()) {
            applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
            currentLineIndices.clear();
          }
          currentX = x;
          currentY += maxLineHeight;
          lineStartY = currentY;
          maxLineHeight = lineHeight;
          
          // Re-layout child on new line
          child->layout(currentX, currentY, width, styleSheet, fontManager,
                       viewportWidth, viewportHeight, true);
          childBox = child->box.borderBox();
          
          // Reposition again after wrapping
          marginLeft = child->computedStyle.margin.left.toPx();
          child->box.content.x = currentX + marginLeft + child->computedStyle.borderWidth.left.toPx() + 
                                 child->computedStyle.padding.left.toPx();
        }
        
        currentX += childBox.width;
        maxLineHeight = std::max(maxLineHeight, childBox.height);
        currentLineIndices.push_back(idx);  // Track this child for vertical-align
      }
    }
    
    // Apply vertical-align to final line
    if (!currentLineIndices.empty()) {
      applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
    }
    
    return (currentY - y) + maxLineHeight;
  }

  float layoutInlineChildren(float x, float y, float width,
                             StyleSheet &styleSheet, MSDFFontManager *fontManager,
                             float viewportWidth, float viewportHeight) {
    float currentX = x;
    float currentY = y;
    float lineHeight = 20.0f;
    float maxLineHeight = lineHeight;
    float lineStartY = y;
    std::vector<size_t> currentLineIndices;  // Track children on current line for vertical-align

    for (size_t childIdx = 0; childIdx < children.size(); ++childIdx) {
      auto &child = children[childIdx];
      // Handle <br> element - force line break
      if (child->node->type == NodeType::Element) {
        std::string tag = child->node->tagName;
        std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        if (tag == "br") {
          // Apply vertical-align before line break
          if (!currentLineIndices.empty()) {
            applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
            currentLineIndices.clear();
          }
          // Force line break
          child->frame = {currentX, currentY, 0, maxLineHeight};
          child->box.content = child->frame;
          currentX = x;
          currentY += maxLineHeight;
          lineStartY = currentY;
          maxLineHeight = lineHeight;
          continue;
        }
      }
      
      // For text nodes, do word-level wrapping
      if (child->node->type == NodeType::Text) {
        // Compute style first to get font size
        child->computedStyle = styleSheet.computeStyle(*child->node);
        auto parentBox = child->parent.lock();
        if (parentBox) {
          child->computedStyle.color = parentBox->computedStyle.color;
          child->computedStyle.fontSize = parentBox->computedStyle.fontSize;
          child->computedStyle.fontWeight = parentBox->computedStyle.fontWeight;
          child->computedStyle.fontStyle = parentBox->computedStyle.fontStyle;
          child->computedStyle.fontFamily = parentBox->computedStyle.fontFamily;
          child->computedStyle.textDecoration = parentBox->computedStyle.textDecoration;
          child->computedStyle.textAlign = parentBox->computedStyle.textAlign;
          child->computedStyle.lineHeight = parentBox->computedStyle.lineHeight;
        }
        
        MSDFFont* font = fontManager->getFont(child->computedStyle.fontFamily,
            static_cast<int>(child->computedStyle.fontWeight), static_cast<int>(child->computedStyle.fontStyle));
        if (!font) font = fontManager->getDefaultFont();
        
        if (font) {
          layoutTextTokensInline(child, child->node->textContent, currentX, currentY,
                                 maxLineHeight, x, width, font, child->computedStyle);
        }
        
      } else if (isInlineWithTextOnly(child)) {
        // Inline element with only text content (e.g., <code>, <strong>)
        child->computedStyle = styleSheet.computeStyle(*child->node);
        
        // Apply text alignment inheritance for inline elements
        // Check if text-align is explicitly set in inline style
        bool hasInlineStyle = child->node->type == NodeType::Element && 
                             child->node->attributes.find("style") != child->node->attributes.end();
        std::string inlineStyle = hasInlineStyle ? child->node->attributes.at("style") : "";
        bool textAlignExplicitlySet = inlineStyle.find("text-align") != std::string::npos;
        
        if (!textAlignExplicitlySet) {
          // Inherit text-align from parent (this container)
          child->computedStyle.textAlign = computedStyle.textAlign;
        }
        
        // Only inherit font-size if the element doesn't have its own default size
        // (e.g., <code> has 13px default, should not inherit parent's 16px)
        std::string tag = "";
        if (child->node->type == NodeType::Element) {
          tag = child->node->tagName;
          std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        }
        bool hasOwnFontSize = (tag == "code" || tag == "pre" || tag == "kbd" || 
                               tag == "samp" || tag == "tt" || tag == "small" ||
                               tag == "sub" || tag == "sup" ||
                               tag.substr(0, 1) == "h");  // h1-h6
        if (!hasOwnFontSize) {
          child->computedStyle.fontSize = computedStyle.fontSize;
        }
        child->computedStyle.lineHeight = computedStyle.lineHeight;
        
        // Get the text child
        auto &textChild = child->children[0];
        textChild->computedStyle = styleSheet.computeStyle(*textChild->node);
        // Inherit styles from parent inline element
        textChild->computedStyle.color = child->computedStyle.color;
        textChild->computedStyle.fontSize = child->computedStyle.fontSize;
        textChild->computedStyle.fontWeight = child->computedStyle.fontWeight;
        textChild->computedStyle.fontStyle = child->computedStyle.fontStyle;
        textChild->computedStyle.fontFamily = child->computedStyle.fontFamily;
        textChild->computedStyle.textDecoration = child->computedStyle.textDecoration;
        textChild->computedStyle.textAlign = child->computedStyle.textAlign;
        textChild->computedStyle.lineHeight = child->computedStyle.lineHeight;
        
        // Get box model values
        float paddingLeft = child->computedStyle.padding.left.toPx();
        float paddingRight = child->computedStyle.padding.right.toPx();
        float borderLeft = child->computedStyle.borderWidth.left.toPx();
        float borderRight = child->computedStyle.borderWidth.right.toPx();
        float marginLeft = child->computedStyle.margin.left.toPx();
        float marginRight = child->computedStyle.margin.right.toPx();
        
        // Apply margin + border + padding before text
        currentX += marginLeft + borderLeft + paddingLeft;
        
        MSDFFont* font = fontManager->getFont(textChild->computedStyle.fontFamily,
            static_cast<int>(textChild->computedStyle.fontWeight), static_cast<int>(textChild->computedStyle.fontStyle));
        if (!font) font = fontManager->getDefaultFont();
        
        std::string text = getInlineTextContent(child);
        if (font) {
          layoutTextTokensInline(textChild, text, currentX, currentY,
                                 maxLineHeight, x, width, font, textChild->computedStyle);
        }
        
        // Apply padding + border + margin after text
        currentX += paddingRight + borderRight + marginRight;
        
        // Update parent element's frame to include the full box model
        // box.content is the inner content area (where text goes)
        child->box.content = textChild->frame;
        child->box.padding = child->computedStyle.padding;
        child->box.border = child->computedStyle.borderWidth;
        child->box.margin = child->computedStyle.margin;
        // frame is the border box for compatibility
        child->frame = child->box.borderBox();
        
      } else {
        // Complex inline element - layout as a unit

        // Same pre-measure logic as layoutInlineGroup: if the whole element
        // doesn't fit, move it to the next line before laying it out.
        StyleSheet::ComputedStyle preStyle = styleSheet.computeStyle(*child->node);
        child->computedStyle = preStyle;
        MSDFFont* preFont = fontManager->getFont(preStyle.fontFamily,
            static_cast<int>(preStyle.fontWeight), static_cast<int>(preStyle.fontStyle));
        if (!preFont) preFont = fontManager->getDefaultFont();
        float idealWidth = child->measureIntrinsicWidth(preFont, preStyle.fontSize);
        float idealMarginLeft = preStyle.getMarginLeft(width, preStyle.fontSize);
        float idealMarginRight = preStyle.getMarginRight(width, preStyle.fontSize);
        float idealBorderLeft = preStyle.getBorderLeftWidth();
        float idealBorderRight = preStyle.getBorderRightWidth();
        float idealTotal = idealWidth + idealMarginLeft + idealMarginRight + idealBorderLeft + idealBorderRight;

        if (currentX > x && currentX + idealTotal > x + width) {
          // Apply vertical-align before wrapping
          if (!currentLineIndices.empty()) {
            applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
            currentLineIndices.clear();
          }
          currentX = x;
          currentY += maxLineHeight;
          lineStartY = currentY;
          maxLineHeight = lineHeight;
        }

        child->layout(currentX, currentY, width - (currentX - x), styleSheet,
                      fontManager, viewportWidth, viewportHeight, true);

        Rect childBox = child->box.borderBox();

        // Line break if needed
        if (currentX + childBox.width > x + width && currentX > x) {
          // Apply vertical-align before wrapping
          if (!currentLineIndices.empty()) {
            applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
            currentLineIndices.clear();
          }
          currentX = x;
          currentY += maxLineHeight;
          lineStartY = currentY;
          maxLineHeight = lineHeight;

          child->layout(currentX, currentY, width, styleSheet, fontManager,
                        viewportWidth, viewportHeight, true);
          childBox = child->box.borderBox();
        }

        currentX += childBox.width;
        maxLineHeight = std::max(maxLineHeight, childBox.height);
        currentLineIndices.push_back(childIdx);  // Track for vertical-align
      }
    }

    // Apply vertical-align to final line
    if (!currentLineIndices.empty()) {
      applyVerticalAlign(currentLineIndices, lineStartY, maxLineHeight);
    }

    return (currentY - y) + maxLineHeight;
  }

  float layoutFlexChildren(float x, float y, float width,
                           StyleSheet &styleSheet, MSDFFontManager *fontManager,
                           float viewportWidth, float viewportHeight) {
    auto &style = computedStyle;
    bool isRow = (style.flexDirection == "row" ||
                  style.flexDirection == "row-reverse");
    bool canWrap = (style.flexWrap == "wrap" || style.flexWrap == "wrap-reverse");
    float gap = style.gap;

    // First pass: measure intrinsic sizes of all children
    std::vector<float> intrinsicSizes;
    float totalFlexGrow = 0;
    
    for (auto &child : children) {
      MSDFFont* font = fontManager->getDefaultFont();
      float fontSize = child->computedStyle.fontSize;
      float intrinsicSize = 0;

      if (isRow) {
        if (child->computedStyle.flexGrow > 0) {
          // For flex growing items, use minimal size
          auto &childStyle = child->computedStyle;
          float padding = childStyle.getPaddingLeft() + childStyle.getPaddingRight();
          float border = childStyle.getBorderLeftWidth() + childStyle.getBorderRightWidth();
          intrinsicSize = padding + border;
        } else {
          intrinsicSize = child->measureIntrinsicWidth(font, fontSize);
        }
      } else {
        intrinsicSize = 0; // Column layout measures during positioning
      }
      
      intrinsicSizes.push_back(intrinsicSize);
      totalFlexGrow += child->computedStyle.flexGrow;
    }

    // For wrapping flex containers, we need to organize children into lines
    struct FlexLine {
      std::vector<size_t> childIndices;
      float totalSize = 0;
      float totalFlexGrow = 0;
      float crossSize = 0;  // Height for row, width for column
    };
    
    std::vector<FlexLine> lines;
    
    if (canWrap && isRow) {
      // Organize children into lines that fit within width
      FlexLine currentLine;
      float lineSize = 0;
      
      for (size_t i = 0; i < children.size(); i++) {
        float childSize = intrinsicSizes[i];
        float sizeWithGap = childSize + (currentLine.childIndices.empty() ? 0 : gap);
        
        // Check if child fits on current line
        if (!currentLine.childIndices.empty() && lineSize + sizeWithGap > width) {
          // Start a new line
          currentLine.totalSize = lineSize;
          lines.push_back(currentLine);
          currentLine = FlexLine{};
          lineSize = 0;
          sizeWithGap = childSize;  // No gap at start of new line
        }
        
        currentLine.childIndices.push_back(i);
        currentLine.totalFlexGrow += children[i]->computedStyle.flexGrow;
        lineSize += sizeWithGap;
      }
      
      // Add last line
      if (!currentLine.childIndices.empty()) {
        currentLine.totalSize = lineSize;
        lines.push_back(currentLine);
      }
    } else {
      // No wrapping - single line with all children
      FlexLine singleLine;
      float totalSize = 0;
      for (size_t i = 0; i < children.size(); i++) {
        singleLine.childIndices.push_back(i);
        totalSize += intrinsicSizes[i] + (i > 0 ? gap : 0);
      }
      singleLine.totalSize = totalSize;
      singleLine.totalFlexGrow = totalFlexGrow;
      lines.push_back(singleLine);
    }

    // Layout each line
    float currentY_line = y;
    float maxWidth = 0;
    
    for (auto &line : lines) {
      float freeSpace = width - line.totalSize;
      if (freeSpace < 0) freeSpace = 0;
      
      float currentPos = 0;
      float maxCrossSize = 0;
      
      // Handle justify-content for this line
      if (style.justifyContent == "center") {
        currentPos = freeSpace / 2;
      } else if (style.justifyContent == "flex-end") {
        currentPos = freeSpace;
      } else if (style.justifyContent == "space-between" && line.childIndices.size() > 1) {
        gap = freeSpace / (line.childIndices.size() - 1);
      } else if (style.justifyContent == "space-around" && !line.childIndices.empty()) {
        float spacing = freeSpace / line.childIndices.size();
        currentPos = spacing / 2;
        gap = style.gap + spacing;
      }
      
      for (size_t idx : line.childIndices) {
        auto &child = children[idx];
        
        // Distribute free space based on flex-grow within this line
        float extraSize = 0;
        if (line.totalFlexGrow > 0) {
          extraSize = (freeSpace * child->computedStyle.flexGrow) / line.totalFlexGrow;
        }
        
        if (isRow) {
          float childWidth = intrinsicSizes[idx] + extraSize;
          child->layout(x + currentPos, currentY_line, childWidth, styleSheet, fontManager,
                        viewportWidth, viewportHeight);
          currentPos += child->frame.width + gap;
          maxCrossSize = std::max(maxCrossSize, child->frame.height);
        } else {
          child->layout(x, currentY_line + currentPos, width, styleSheet, fontManager,
                        viewportWidth, viewportHeight);
          currentPos += child->frame.height + gap;
          maxCrossSize = std::max(maxCrossSize, child->frame.width);
        }
      }
      
      line.crossSize = maxCrossSize;
      
      if (isRow) {
        currentY_line += maxCrossSize + gap;
        maxWidth = std::max(maxWidth, currentPos - gap);
      }
    }

    // Return total height for row direction, or total width for column
    if (isRow) {
      float totalHeight = currentY_line - y;
      if (!lines.empty()) totalHeight -= gap;  // Remove trailing gap
      return totalHeight;
    } else {
      return lines.empty() ? 0 : lines[0].crossSize;
    }
  }

  // Table layout algorithm
  float layoutTableChildren(float x, float y, float width, StyleSheet &styleSheet,
                           MSDFFontManager *fontManager, float viewportWidth,
                           float viewportHeight) {
    // Find all table rows (including through tbody/thead/tfoot)
    std::vector<std::shared_ptr<RenderBox>> rows;
    std::vector<std::vector<std::shared_ptr<RenderBox>>> cellsByRow;
    
    for (auto& child : children) {
      auto& childStyle = child->computedStyle;
      std::string tag = child->node->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      
      // Handle tbody, thead, tfoot
      if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
        for (auto& rowChild : child->children) {
          auto& rowStyle = rowChild->computedStyle;
          std::string rowTag = rowChild->node->tagName;
          std::transform(rowTag.begin(), rowTag.end(), rowTag.begin(), ::tolower);
          if (rowTag == "tr") {
            rows.push_back(rowChild);
            std::vector<std::shared_ptr<RenderBox>> cells;
            for (auto& cellChild : rowChild->children) {
              auto& cellStyle = cellChild->computedStyle;
              std::string cellTag = cellChild->node->tagName;
              std::transform(cellTag.begin(), cellTag.end(), cellTag.begin(), ::tolower);
              if (cellTag == "td" || cellTag == "th") {
                cells.push_back(cellChild);
              }
            }
            cellsByRow.push_back(cells);
          }
        }
      }
      // Direct TR children (no tbody)
      else if (tag == "tr") {
        rows.push_back(child);
        std::vector<std::shared_ptr<RenderBox>> cells;
        for (auto& cellChild : child->children) {
          auto& cellStyle = cellChild->computedStyle;
          std::string cellTag = cellChild->node->tagName;
          std::transform(cellTag.begin(), cellTag.end(), cellTag.begin(), ::tolower);
          if (cellTag == "td" || cellTag == "th") {
            cells.push_back(cellChild);
          }
        }
        cellsByRow.push_back(cells);
      }
    }
    
    if (rows.empty()) return 0;
    
    // Get table properties
    float fontSize = computedStyle.fontSize;
    float parentWidth = width;
    float paddingLeft = computedStyle.getPaddingLeft(parentWidth, fontSize);
    float paddingRight = computedStyle.getPaddingRight(parentWidth, fontSize);
    float borderLeft = computedStyle.getBorderLeftWidth();
    float borderRight = computedStyle.getBorderRightWidth();
    float tableContentWidth = width - paddingLeft - paddingRight - borderLeft - borderRight;
    
    // Determine number of columns (max cells in any row)
    size_t numColumns = 0;
    for (auto& rowCells : cellsByRow) {
      numColumns = std::max(numColumns, rowCells.size());
    }
    
    if (numColumns == 0) return 0;
    
    // FIRST PASS: Measure all cells to determine column widths
    std::vector<float> columnWidths(numColumns, 0);
    for (size_t rowIdx = 0; rowIdx < cellsByRow.size(); rowIdx++) {
      auto& rowCells = cellsByRow[rowIdx];
      for (size_t colIdx = 0; colIdx < rowCells.size(); colIdx++) {
        auto cell = rowCells[colIdx];
        auto& cellStyle = cell->computedStyle;
        float cellFontSize = cellStyle.fontSize;
        MSDFFont* cellFont = fontManager->getFont(cellStyle.fontFamily,
            static_cast<int>(cellStyle.fontWeight), static_cast<int>(cellStyle.fontStyle));
        if (!cellFont) cellFont = fontManager->getDefaultFont();
        
        // Get cell padding and border
        float cellPaddingLeft = cellStyle.getPaddingLeft(tableContentWidth, cellFontSize);
        float cellPaddingRight = cellStyle.getPaddingRight(tableContentWidth, cellFontSize);
        float cellBorderLeft = cellStyle.getBorderLeftWidth();
        float cellBorderRight = cellStyle.getBorderRightWidth();
        float cellHorizontalSpace = cellPaddingLeft + cellPaddingRight + cellBorderLeft + cellBorderRight;
        
        // Measure just the text content (tightly)
        float cellContentWidth = 0;
        if (cell->node->type == NodeType::Text && cellFont) {
          cellContentWidth = cellFont->getTextWidth(cell->node->textContent, cellFontSize);
        } else {
          // For non-text cells, measure text children
          for (auto& child : cell->children) {
            if (child->node->type == NodeType::Text && cellFont) {
              cellContentWidth += cellFont->getTextWidth(child->node->textContent, cellFontSize);
            }
          }
        }
        
        float cellTotalWidth = cellContentWidth + cellHorizontalSpace;
        columnWidths[colIdx] = std::max(columnWidths[colIdx], cellTotalWidth);
      }
    }
    
    // Distribute column widths: sum up and scale if needed
    float totalColumnWidth = 0;
    for (float w : columnWidths) {
      totalColumnWidth += w;
    }
    
    // If columns exceed available width, scale them proportionally
    if (totalColumnWidth > tableContentWidth) {
      float scale = tableContentWidth / totalColumnWidth;
      for (float& w : columnWidths) {
        w *= scale;
      }
    }
    
    // SECOND PASS: Layout rows and cells with calculated column widths
    float currentY = y;
    for (size_t rowIdx = 0; rowIdx < rows.size(); rowIdx++) {
      auto row = rows[rowIdx];
      auto& rowCells = cellsByRow[rowIdx];
      
      float currentX = x;
      float maxRowHeight = 0;
      
      // First, layout all cells in this row with their column widths
      for (size_t colIdx = 0; colIdx < rowCells.size(); colIdx++) {
        auto cell = rowCells[colIdx];
        float cellWidth = columnWidths[colIdx];
        
        // Layout cell with its column width
        cell->layout(currentX, currentY, cellWidth, styleSheet, fontManager, viewportWidth, viewportHeight);
        
        maxRowHeight = std::max(maxRowHeight, cell->frame.height);
        currentX += cellWidth;
      }
      
      // Position row's frame for rendering purposes
      row->frame = {x, currentY, currentX - x, maxRowHeight};
      
      // Adjust row's children to align properly
      currentX = x;
      for (size_t colIdx = 0; colIdx < rowCells.size(); colIdx++) {
        auto cell = rowCells[colIdx];
        cell->frame.x = currentX;
        cell->frame.y = currentY;
        cell->frame.width = columnWidths[colIdx];
        currentX += columnWidths[colIdx];
      }
      
      currentY += maxRowHeight;
    }
    
    // Calculate total table height
    float totalHeight = currentY - y;
    
    // Also layout any tbody/thead/tfoot groups (update their frames)
    for (auto& child : children) {
      auto& childStyle = child->computedStyle;
      std::string tag = child->node->tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      if (tag == "tbody" || tag == "thead" || tag == "tfoot") {
        float groupStartY = y;
        float groupHeight = 0;
        // Calculate group height based on its rows
        for (auto& rowChild : child->children) {
          groupHeight += rowChild->frame.height;
        }
        child->frame = {x, groupStartY, columnWidths.empty() ? 0 : (x + columnWidths[0] + (columnWidths.size() > 1 ? columnWidths[1] : 0)), groupHeight};
        groupStartY += groupHeight;
      }
    }
    
    return totalHeight;
  }

public:
  void print(int indent = 0) {
    std::string pad(indent * 2, ' ');
    std::cout << pad << "Box [" << frame.x << ", " << frame.y << ", "
              << frame.width << ", " << frame.height << "] ";
    if (node->type == NodeType::Element)
      std::cout << "<" << node->tagName << ">";
    else if (node->type == NodeType::Text)
      std::cout << "\"" << node->textContent.substr(0, 20) << "...\"";
    std::cout << std::endl;

    for (auto &child : children)
      child->print(indent + 1);
  }

  std::shared_ptr<RenderBox> getptr() { return shared_from_this(); }
};

class RenderTree {
public:
  std::shared_ptr<RenderBox> root;
  float viewportWidth = 1024.0f;
  float viewportHeight = 768.0f;

  std::shared_ptr<RenderBox> build(std::shared_ptr<Node> node) {
    auto box = std::make_shared<RenderBox>(node);

    for (auto &child : node->children) {
      box->addChild(build(child));
    }
    return box;
  }

  void buildAndLayout(std::shared_ptr<Node> domRoot, float screenWidth,
                      StyleSheet &styleSheet, MSDFFontManager *fontManager) {
    viewportWidth = screenWidth;
    styleSheet.setViewport(viewportWidth, viewportHeight);
    root = build(domRoot);
    root->layout(0, 0, screenWidth, styleSheet, fontManager, viewportWidth,
                 viewportHeight);
  }

  void relayout(float screenWidth, float screenHeight, StyleSheet &styleSheet, MSDFFontManager *fontManager) {
    if (root) {
      viewportWidth = screenWidth;
      viewportHeight = screenHeight;
      styleSheet.setViewport(viewportWidth, viewportHeight);
      root->layout(0, 0, screenWidth, styleSheet, fontManager, viewportWidth,
                   viewportHeight);
    }
  }
};

} // namespace skene

