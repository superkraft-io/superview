#pragma once

// Prevent Windows macro conflicts
#ifdef NONE
#undef NONE
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

#include "Color.hpp"
#include "CssParser.hpp"
#include "dom/Node.hpp"
#include <map>
#include <string>
#include <sstream>

namespace skene {

// Font style enums (CSS values)
enum class FontWeight { Normal, Bold, Lighter, Bolder };
enum class FontStyle { Normal, Italic, Oblique };

enum class DisplayType { Block, Inline, InlineBlock, Flex, Grid, Hidden, Table, TableRowGroup, TableRow, TableCell };

enum class TextAlign { Left, Center, Right, Justify };

// FontWeight and FontStyle are defined in render/Font.hpp

enum class TextDecoration { None, Underline, Overline, LineThrough };

enum class Overflow { Visible, Hidden, Scroll, Auto };

enum class Position { Static, Relative, Absolute, Fixed, Sticky };

enum class BoxSizing { ContentBox, BorderBox };

enum class ListStyleType { None, Disc, Circle, Square, Decimal, DecimalLeadingZero, LowerAlpha, UpperAlpha, LowerRoman, UpperRoman };

struct EdgeValues {
  CssValue top, right, bottom, left;

  EdgeValues() = default;
  EdgeValues(CssValue all) : top(all), right(all), bottom(all), left(all) {}
  EdgeValues(CssValue tb, CssValue rl)
      : top(tb), right(rl), bottom(tb), left(rl) {}
  EdgeValues(CssValue t, CssValue rl, CssValue b)
      : top(t), right(rl), bottom(b), left(rl) {}
  EdgeValues(CssValue t, CssValue r, CssValue b, CssValue l)
      : top(t), right(r), bottom(b), left(l) {}
};

class StyleSheet {
public:
  // Viewport dimensions for percentage calculations
  float viewportWidth = 1024.0f;
  float viewportHeight = 768.0f;

  // CSS rules from <style> tags
  std::vector<CssParser::CssRule> rules;

  struct ComputedStyle {
    // Box model
    EdgeValues padding;
    EdgeValues margin;
    EdgeValues borderWidth;

    // Dimensions
    CssValue width{-1, CssUnit::Auto};
    CssValue height{-1, CssUnit::Auto};
    CssValue minWidth{0, CssUnit::Px};
    CssValue minHeight{0, CssUnit::Px};
    CssValue maxWidth{-1, CssUnit::Auto};
    CssValue maxHeight{-1, CssUnit::Auto};

    // Colors
    Color color = Color::Black();
    Color backgroundColor = Color::Transparent();
    Color borderColor = Color::Black();
    Color borderTopColor = Color::Black();
    Color borderRightColor = Color::Black();
    Color borderBottomColor = Color::Black();
    Color borderLeftColor = Color::Black();

    // Typography
    float fontSize = 16.0f;
    float lineHeight = 1.2f;  // Chrome's "normal" is typically ~1.2 for serif fonts
    FontWeight fontWeight = FontWeight::Normal;
    FontStyle fontStyle = FontStyle::Normal;
    TextDecoration textDecoration = TextDecoration::None;
    TextAlign textAlign = TextAlign::Left;
    std::string fontFamily = "serif";  // Default browser font (Times New Roman)

    // Layout
    DisplayType display = DisplayType::Block;
    Position position = Position::Static;
    BoxSizing boxSizing = BoxSizing::ContentBox;
    Overflow overflow = Overflow::Visible;

    // Positioning
    CssValue top{0, CssUnit::Auto};
    CssValue right_{0, CssUnit::Auto}; // right is reserved
    CssValue bottom{0, CssUnit::Auto};
    CssValue left{0, CssUnit::Auto};
    int zIndex = 0;

    // Border radius
    float borderRadius = 0.0f;
    float borderTopLeftRadius = 0.0f;
    float borderTopRightRadius = 0.0f;
    float borderBottomLeftRadius = 0.0f;
    float borderBottomRightRadius = 0.0f;

    // Opacity
    float opacity = 1.0f;

    // Flexbox
    std::string flexDirection = "row";
    std::string flexWrap = "nowrap";  // nowrap, wrap, wrap-reverse
    std::string justifyContent = "flex-start";
    std::string alignItems = "stretch";
    float flexGrow = 0.0f;
    float flexShrink = 1.0f;
    CssValue flexBasis{-1, CssUnit::Auto};
    float gap = 0.0f;

    // Text selection
    std::string userSelect = "auto"; // auto, none, text, all
    
    // List styling
    ListStyleType listStyleType = ListStyleType::None;
    int listItemIndex = 0;  // For ordered lists, the item number

    // Image styling
    std::string objectFit = "fill";  // fill, contain, cover, none, scale-down
    std::string objectPosition = "50% 50%";  // x y position (center center by default)
    std::string imageRendering = "auto";  // auto, pixelated, crisp-edges

    // Vertical alignment for inline elements
    std::string verticalAlign = "baseline";  // baseline, top, middle, bottom, text-top, text-bottom, sub, super

    // Helper to get total padding in pixels
    float getPaddingTop(float parentWidth = 0, float fontSize = 16.0f) const {
      return padding.top.toPx(parentWidth, fontSize);
    }
    float getPaddingRight(float parentWidth = 0, float fontSize = 16.0f) const {
      return padding.right.toPx(parentWidth, fontSize);
    }
    float getPaddingBottom(float parentWidth = 0,
                           float fontSize = 16.0f) const {
      return padding.bottom.toPx(parentWidth, fontSize);
    }
    float getPaddingLeft(float parentWidth = 0, float fontSize = 16.0f) const {
      return padding.left.toPx(parentWidth, fontSize);
    }

    float getMarginTop(float parentWidth = 0, float fontSize = 16.0f) const {
      return margin.top.toPx(parentWidth, fontSize);
    }
    float getMarginRight(float parentWidth = 0, float fontSize = 16.0f) const {
      return margin.right.toPx(parentWidth, fontSize);
    }
    float getMarginBottom(float parentWidth = 0, float fontSize = 16.0f) const {
      return margin.bottom.toPx(parentWidth, fontSize);
    }
    float getMarginLeft(float parentWidth = 0, float fontSize = 16.0f) const {
      return margin.left.toPx(parentWidth, fontSize);
    }

    float getBorderTopWidth() const { return borderWidth.top.toPx(); }
    float getBorderRightWidth() const { return borderWidth.right.toPx(); }
    float getBorderBottomWidth() const { return borderWidth.bottom.toPx(); }
    float getBorderLeftWidth() const { return borderWidth.left.toPx(); }
  };

  // User agent stylesheet rules (lowest priority)
  std::vector<CssParser::CssRule> uaRules;

  // Add CSS rules from a stylesheet string
  void addStylesheet(const std::string& css) {
    auto newRules = CssParser::parseStylesheet(css);
    rules.insert(rules.end(), newRules.begin(), newRules.end());
  }

  // Load the user agent stylesheet (should be called first, before author styles)
  void loadUserAgentStylesheet(const std::string& css) {
    uaRules = CssParser::parseStylesheet(css);
  }

  // Clear all rules
  void clearRules() {
    rules.clear();
  }

  // Check if a selector matches a node
  bool selectorMatches(const CssParser::SimpleSelector& sel, const Node& node) const {
    if (node.type != NodeType::Element) {
      return false;
    }

    // Check tag (if specified and not universal)
    if (!sel.tag.empty() && sel.tag != "*" && sel.tag != node.tagName) {
      return false;
    }

    // Check id (if specified)
    if (!sel.id.empty() && sel.id != node.getId()) {
      return false;
    }

    // Check all classes (if any specified)
    for (const auto& cls : sel.classes) {
      if (!node.hasClass(cls)) {
        return false;
      }
    }

    return true;
  }

  // Check if a compound selector matches a node with its ancestors
  bool compoundSelectorMatches(const CssParser::CompoundSelector& compound, 
                               const Node& node,
                               const std::vector<const Node*>& ancestors) const {
    if (compound.parts.empty()) return false;
    
    // Last part must match the target node
    if (!selectorMatches(compound.parts.back(), node)) {
      return false;
    }
    
    // If only one part, we're done (simple selector)
    if (compound.parts.size() == 1) {
      return true;
    }
    
    // For descendant selectors, check that each part matches an ancestor
    // Working backwards from the second-to-last part
    int partIdx = (int)compound.parts.size() - 2;
    
    for (auto it = ancestors.rbegin(); it != ancestors.rend() && partIdx >= 0; ++it) {
      if (selectorMatches(compound.parts[partIdx], **it)) {
        partIdx--;
      }
    }
    
    // All ancestor parts must have matched
    return partIdx < 0;
  }

  // Build ancestor list from node's parent chain
  std::vector<const Node*> getAncestors(const Node& node) const {
    std::vector<const Node*> ancestors;
    auto parent = node.parent.lock();
    while (parent) {
      ancestors.push_back(parent.get());
      parent = parent->parent.lock();
    }
    // Reverse so ancestors are from root to immediate parent
    std::reverse(ancestors.begin(), ancestors.end());
    return ancestors;
  }

  ComputedStyle computeStyle(const Node &node, const std::vector<const Node*>& ancestors = {}) {
    ComputedStyle style;

    if (node.type == NodeType::Element) {
      // Build ancestor list if not provided
      std::vector<const Node*> nodeAncestors = ancestors.empty() ? getAncestors(node) : ancestors;

      // 1. Apply user agent stylesheet rules (lowest priority)
      for (const auto& rule : uaRules) {
        bool matches = false;
        if (rule.compoundSelector.parts.size() > 1) {
          matches = compoundSelectorMatches(rule.compoundSelector, node, nodeAncestors);
        } else {
          matches = selectorMatches(rule.selector, node);
        }
        if (matches) {
          applyDeclarations(rule.declarations, style);
        }
      }

      // 2. Apply author stylesheet rules (from <style> tags)
      // Collect matching rules with specificity for proper cascade
      std::vector<std::pair<std::tuple<int,int,int>, const CssParser::CssRule*>> matchingRules;
      
      for (const auto& rule : rules) {
        bool matches = false;
        if (rule.compoundSelector.parts.size() > 1) {
          matches = compoundSelectorMatches(rule.compoundSelector, node, nodeAncestors);
        } else {
          matches = selectorMatches(rule.selector, node);
        }
        
        if (matches) {
          matchingRules.push_back({rule.specificity(), &rule});
        }
      }

      // Sort by specificity (lower first, so higher specificity wins when applied later)
      std::sort(matchingRules.begin(), matchingRules.end(),
                [](const auto& a, const auto& b) {
                  return a.first < b.first;
                });

      // Apply matching author rules in specificity order
      for (const auto& [spec, rule] : matchingRules) {
        applyDeclarations(rule->declarations, style);
      }

      // 3. Inline styles have highest specificity - apply last
      if (node.attributes.find("style") != node.attributes.end()) {
        parseStyleAttribute(node.attributes.at("style"), style);
      }

      // 4. Runtime/DOM-dependent logic that can't be expressed in CSS
      std::string tag = node.tagName;
      std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
      
      // <li> elements: set list-style-type based on parent (ul/ol)
      // This requires DOM traversal and can't be done in static CSS
      if (tag == "li") {
        auto parent = node.parent.lock();
        if (parent) {
          std::string parentTag = parent->tagName;
          std::transform(parentTag.begin(), parentTag.end(), parentTag.begin(), ::tolower);
          if (parentTag == "ul") {
            style.listStyleType = ListStyleType::Disc;
          } else if (parentTag == "ol") {
            style.listStyleType = ListStyleType::Decimal;
            // Count this item's position among siblings
            int index = 1;
            for (const auto& sibling : parent->children) {
              if (sibling.get() == &node) break;
              if (sibling->type == NodeType::Element) {
                std::string sibTag = sibling->tagName;
                std::transform(sibTag.begin(), sibTag.end(), sibTag.begin(), ::tolower);
                if (sibTag == "li") index++;
              }
            }
            style.listItemIndex = index;
          }
        }
      }
    }

    return style;
  }

  void setViewport(float w, float h) {
    viewportWidth = w;
    viewportHeight = h;
  }

private:
  // Apply a set of declarations to a style
  void applyDeclarations(const std::map<std::string, std::string>& declarations, ComputedStyle& style) {
    for (const auto& [property, value] : declarations) {
      applyProperty(property, value, style);
    }
  }

  // Apply a single property
  void applyProperty(const std::string& property, const std::string& value, ComputedStyle& style) {
    // Padding
    if (property == "padding") {
      CssParser::parse4ValueShorthand(value, style.padding.top,
                                      style.padding.right,
                                      style.padding.bottom, style.padding.left);
    } else if (property == "padding-top") {
      style.padding.top = CssParser::parseValue(value);
    } else if (property == "padding-right") {
      style.padding.right = CssParser::parseValue(value);
    } else if (property == "padding-bottom") {
      style.padding.bottom = CssParser::parseValue(value);
    } else if (property == "padding-left") {
      style.padding.left = CssParser::parseValue(value);
    }
    // CSS Logical Properties for padding (maps to physical in horizontal-tb writing mode)
    else if (property == "padding-block-start") {
      style.padding.top = CssParser::parseValue(value);
    } else if (property == "padding-block-end") {
      style.padding.bottom = CssParser::parseValue(value);
    } else if (property == "padding-inline-start") {
      style.padding.left = CssParser::parseValue(value);
    } else if (property == "padding-inline-end") {
      style.padding.right = CssParser::parseValue(value);
    } else if (property == "padding-block") {
      CssValue top, bottom;
      CssParser::parse2ValueShorthand(value, top, bottom);
      style.padding.top = top;
      style.padding.bottom = bottom;
    } else if (property == "padding-inline") {
      CssValue left, right;
      CssParser::parse2ValueShorthand(value, left, right);
      style.padding.left = left;
      style.padding.right = right;
    }
    // Margin
    else if (property == "margin") {
      CssParser::parse4ValueShorthand(value, style.margin.top,
                                      style.margin.right, style.margin.bottom,
                                      style.margin.left);
    } else if (property == "margin-top") {
      style.margin.top = CssParser::parseValue(value);
    } else if (property == "margin-right") {
      style.margin.right = CssParser::parseValue(value);
    } else if (property == "margin-bottom") {
      style.margin.bottom = CssParser::parseValue(value);
    } else if (property == "margin-left") {
      style.margin.left = CssParser::parseValue(value);
    }
    // CSS Logical Properties for margins (maps to physical in horizontal-tb writing mode)
    else if (property == "margin-block-start") {
      style.margin.top = CssParser::parseValue(value);
    } else if (property == "margin-block-end") {
      style.margin.bottom = CssParser::parseValue(value);
    } else if (property == "margin-inline-start") {
      style.margin.left = CssParser::parseValue(value);
    } else if (property == "margin-inline-end") {
      style.margin.right = CssParser::parseValue(value);
    } else if (property == "margin-block") {
      // Shorthand: margin-block: <block-start> [<block-end>]
      CssValue top, bottom;
      CssParser::parse2ValueShorthand(value, top, bottom);
      style.margin.top = top;
      style.margin.bottom = bottom;
    } else if (property == "margin-inline") {
      // Shorthand: margin-inline: <inline-start> [<inline-end>]
      CssValue left, right;
      CssParser::parse2ValueShorthand(value, left, right);
      style.margin.left = left;
      style.margin.right = right;
    }
    // Border width
    else if (property == "border-width") {
      CssParser::parse4ValueShorthand(value, style.borderWidth.top,
                                      style.borderWidth.right,
                                      style.borderWidth.bottom,
                                      style.borderWidth.left);
    } else if (property == "border-top-width") {
      style.borderWidth.top = CssParser::parseValue(value);
    } else if (property == "border-right-width") {
      style.borderWidth.right = CssParser::parseValue(value);
    } else if (property == "border-bottom-width") {
      style.borderWidth.bottom = CssParser::parseValue(value);
    } else if (property == "border-left-width") {
      style.borderWidth.left = CssParser::parseValue(value);
    }
    // Border shorthand
    else if (property == "border") {
      parseBorderShorthand(value, style);
    } else if (property == "border-top") {
      parseBorderSideShorthand(value, style.borderWidth.top, style.borderTopColor);
    } else if (property == "border-right") {
      parseBorderSideShorthand(value, style.borderWidth.right, style.borderRightColor);
    } else if (property == "border-bottom") {
      parseBorderSideShorthand(value, style.borderWidth.bottom, style.borderBottomColor);
    } else if (property == "border-left") {
      parseBorderSideShorthand(value, style.borderWidth.left, style.borderLeftColor);
    }
    // Border color
    else if (property == "border-color") {
      auto color = CssParser::parseColor(value);
      if (color) {
        style.borderColor = *color;
        style.borderTopColor = *color;
        style.borderRightColor = *color;
        style.borderBottomColor = *color;
        style.borderLeftColor = *color;
      }
    } else if (property == "border-top-color") {
      auto color = CssParser::parseColor(value);
      if (color) style.borderTopColor = *color;
    } else if (property == "border-right-color") {
      auto color = CssParser::parseColor(value);
      if (color) style.borderRightColor = *color;
    } else if (property == "border-bottom-color") {
      auto color = CssParser::parseColor(value);
      if (color) style.borderBottomColor = *color;
    } else if (property == "border-left-color") {
      auto color = CssParser::parseColor(value);
      if (color) style.borderLeftColor = *color;
    }
    // Border radius
    else if (property == "border-radius") {
      style.borderRadius = CssParser::parseValue(value).toPx();
      style.borderTopLeftRadius = style.borderRadius;
      style.borderTopRightRadius = style.borderRadius;
      style.borderBottomLeftRadius = style.borderRadius;
      style.borderBottomRightRadius = style.borderRadius;
    } else if (property == "border-top-left-radius") {
      style.borderTopLeftRadius = CssParser::parseValue(value).toPx();
    } else if (property == "border-top-right-radius") {
      style.borderTopRightRadius = CssParser::parseValue(value).toPx();
    } else if (property == "border-bottom-left-radius") {
      style.borderBottomLeftRadius = CssParser::parseValue(value).toPx();
    } else if (property == "border-bottom-right-radius") {
      style.borderBottomRightRadius = CssParser::parseValue(value).toPx();
    }
    // Dimensions
    else if (property == "width") {
      style.width = CssParser::parseValue(value);
    } else if (property == "height") {
      style.height = CssParser::parseValue(value);
    } else if (property == "min-width") {
      style.minWidth = CssParser::parseValue(value);
    } else if (property == "max-width") {
      style.maxWidth = CssParser::parseValue(value);
    } else if (property == "min-height") {
      style.minHeight = CssParser::parseValue(value);
    } else if (property == "max-height") {
      style.maxHeight = CssParser::parseValue(value);
    }
    // Colors
    else if (property == "color") {
      auto color = CssParser::parseColor(value);
      if (color) style.color = *color;
    } else if (property == "background-color" || property == "background") {
      auto color = CssParser::parseColor(value);
      if (color) style.backgroundColor = *color;
    }
    // Typography
    else if (property == "font-size") {
      style.fontSize = CssParser::parseValue(value).toPx(0, 16.0f);
    } else if (property == "line-height") {
      std::string v = CssParser::trim(value);
      if (v.find("px") != std::string::npos || v.find("em") != std::string::npos) {
        style.lineHeight = CssParser::parseValue(v).toPx(0, style.fontSize) / style.fontSize;
      } else {
        style.lineHeight = std::stof(v);
      }
    } else if (property == "font-weight") {
      std::string v = CssParser::trim(value);
      if (v == "bold" || v == "700" || v == "800" || v == "900") {
        style.fontWeight = FontWeight::Bold;
      } else if (v == "lighter") {
        style.fontWeight = FontWeight::Lighter;
      } else if (v == "bolder") {
        style.fontWeight = FontWeight::Bolder;
      } else {
        style.fontWeight = FontWeight::Normal;
      }
    } else if (property == "font-style") {
      std::string v = CssParser::trim(value);
      if (v == "italic") {
        style.fontStyle = FontStyle::Italic;
      } else if (v == "oblique") {
        style.fontStyle = FontStyle::Oblique;
      } else {
        style.fontStyle = FontStyle::Normal;
      }
    } else if (property == "text-decoration") {
      std::string v = CssParser::trim(value);
      if (v == "underline") {
        style.textDecoration = TextDecoration::Underline;
      } else if (v == "overline") {
        style.textDecoration = TextDecoration::Overline;
      } else if (v == "line-through") {
        style.textDecoration = TextDecoration::LineThrough;
      } else {
        style.textDecoration = TextDecoration::None;
      }
    } else if (property == "text-align") {
      std::string v = CssParser::trim(value);
      if (v == "center") {
        style.textAlign = TextAlign::Center;
      } else if (v == "right") {
        style.textAlign = TextAlign::Right;
      } else if (v == "justify") {
        style.textAlign = TextAlign::Justify;
      } else {
        style.textAlign = TextAlign::Left;
      }
    } else if (property == "font-family") {
      style.fontFamily = CssParser::trim(value);
      // Chrome quirk: monospace fonts default to 13px instead of 16px
      // Only apply if font-size hasn't been explicitly set
      if (style.fontFamily.find("monospace") != std::string::npos && 
          style.fontSize == 16.0f) {
        style.fontSize = 13.0f;
      }
    }
    // Layout
    else if (property == "display") {
      std::string v = CssParser::trim(value);
      if (v == "block") {
        style.display = DisplayType::Block;
      } else if (v == "inline") {
        style.display = DisplayType::Inline;
      } else if (v == "inline-block") {
        style.display = DisplayType::InlineBlock;
      } else if (v == "flex") {
        style.display = DisplayType::Flex;
      } else if (v == "grid") {
        style.display = DisplayType::Grid;
      } else if (v == "table") {
        style.display = DisplayType::Table;
      } else if (v == "table-row-group") {
        style.display = DisplayType::TableRowGroup;
      } else if (v == "table-row") {
        style.display = DisplayType::TableRow;
      } else if (v == "table-cell") {
        style.display = DisplayType::TableCell;
      } else if (v == "none") {
        style.display = DisplayType::Hidden;
      }
    } else if (property == "position") {
      std::string v = CssParser::trim(value);
      if (v == "relative") {
        style.position = Position::Relative;
      } else if (v == "absolute") {
        style.position = Position::Absolute;
      } else if (v == "fixed") {
        style.position = Position::Fixed;
      } else if (v == "sticky") {
        style.position = Position::Sticky;
      } else {
        style.position = Position::Static;
      }
    } else if (property == "box-sizing") {
      std::string v = CssParser::trim(value);
      if (v == "border-box") {
        style.boxSizing = BoxSizing::BorderBox;
      } else {
        style.boxSizing = BoxSizing::ContentBox;
      }
    } else if (property == "overflow") {
      std::string v = CssParser::trim(value);
      if (v == "hidden") {
        style.overflow = Overflow::Hidden;
      } else if (v == "scroll") {
        style.overflow = Overflow::Scroll;
      } else if (v == "auto") {
        style.overflow = Overflow::Auto;
      } else {
        style.overflow = Overflow::Visible;
      }
    }
    // Positioning
    else if (property == "top") {
      style.top = CssParser::parseValue(value);
    } else if (property == "right") {
      style.right_ = CssParser::parseValue(value);
    } else if (property == "bottom") {
      style.bottom = CssParser::parseValue(value);
    } else if (property == "left") {
      style.left = CssParser::parseValue(value);
    } else if (property == "z-index") {
      style.zIndex = std::stoi(CssParser::trim(value));
    }
    // Opacity
    else if (property == "opacity") {
      style.opacity = std::stof(CssParser::trim(value));
    }
    // Flexbox
    else if (property == "flex-direction") {
      style.flexDirection = CssParser::trim(value);
    } else if (property == "flex-wrap") {
      style.flexWrap = CssParser::trim(value);
    } else if (property == "justify-content") {
      style.justifyContent = CssParser::trim(value);
    } else if (property == "align-items") {
      style.alignItems = CssParser::trim(value);
    } else if (property == "flex-grow") {
      style.flexGrow = std::stof(CssParser::trim(value));
    } else if (property == "flex-shrink") {
      style.flexShrink = std::stof(CssParser::trim(value));
    } else if (property == "flex-basis") {
      style.flexBasis = CssParser::parseValue(value);
    } else if (property == "flex") {
      // Parse flex shorthand: flex: [flex-grow] [flex-shrink] [flex-basis]
      // Common values: flex: 1, flex: 0, flex: auto, flex: none, etc.
      std::string v = CssParser::trim(value);
      if (v == "auto") {
        style.flexGrow = 1;
        style.flexShrink = 1;
        style.flexBasis = CssParser::parseValue("auto");
      } else if (v == "none") {
        style.flexGrow = 0;
        style.flexShrink = 0;
        style.flexBasis = CssParser::parseValue("auto");
      } else {
        // Try to parse as numbers/percentages
        std::istringstream iss(v);
        std::string token;
        int tokenCount = 0;
        
        while (iss >> token && tokenCount < 3) {
          try {
            if (tokenCount == 0) {
              // First token is flex-grow
              style.flexGrow = std::stof(token);
            } else if (tokenCount == 1) {
              // Second token is flex-shrink
              style.flexShrink = std::stof(token);
            } else if (tokenCount == 2) {
              // Third token is flex-basis
              style.flexBasis = CssParser::parseValue(token);
            }
            tokenCount++;
          } catch (...) {
            // If first token fails, might be flex-basis only
            if (tokenCount == 0) {
              style.flexBasis = CssParser::parseValue(token);
              break;
            }
          }
        }
        
        // If only one number provided and it's not 0, it's flex-grow only
        // flex-shrink defaults to 1, flex-basis defaults to 0%
        if (tokenCount == 1) {
          if (style.flexGrow > 0) {
            style.flexShrink = 1;
            style.flexBasis = CssParser::parseValue("0%");
          }
        }
      }
    } else if (property == "gap") {
      style.gap = CssParser::parseValue(value).toPx();
    } else if (property == "user-select" || property == "-webkit-user-select" ||
               property == "-moz-user-select" || property == "-ms-user-select") {
      std::string v = CssParser::trim(value);
      if (v == "none" || v == "auto" || v == "text" || v == "all") {
        style.userSelect = v;
      }
    } else if (property == "object-fit") {
      std::string v = CssParser::trim(value);
      if (v == "fill" || v == "contain" || v == "cover" || v == "none" || v == "scale-down") {
        style.objectFit = v;
      }
    } else if (property == "object-position") {
      style.objectPosition = CssParser::trim(value);
    } else if (property == "image-rendering") {
      std::string v = CssParser::trim(value);
      if (v == "auto" || v == "pixelated" || v == "crisp-edges" || v == "-webkit-optimize-contrast") {
        style.imageRendering = v;
      }
    } else if (property == "vertical-align") {
      std::string v = CssParser::trim(value);
      if (v == "baseline" || v == "top" || v == "middle" || v == "bottom" ||
          v == "text-top" || v == "text-bottom" || v == "sub" || v == "super") {
        style.verticalAlign = v;
      }
    }
  }

  // Debug helper for element border colors
  Color getBorderColorForTag(const std::string &tag) {
    // Element-specific colors for debugging
    if (tag == "div")
      return Color::Red();
    if (tag == "h1")
      return Color::Green();
    if (tag == "h2")
      return Color::Orange();
    if (tag == "h3")
      return Color::Purple();
    if (tag == "h4")
      return Color::Cyan();
    if (tag == "h5")
      return Color::Yellow();
    if (tag == "h6")
      return Color::Magenta();
    if (tag == "p")
      return Color::Blue();
    if (tag == "section")
      return Color(0.9f, 0.5f, 0.1f);
    if (tag == "article")
      return Color(0.1f, 0.5f, 0.9f);
    if (tag == "header")
      return Color(0.5f, 0.9f, 0.1f);
    if (tag == "footer")
      return Color(0.9f, 0.1f, 0.5f);
    if (tag == "nav")
      return Color(0.5f, 0.1f, 0.9f);
    if (tag == "aside")
      return Color(0.1f, 0.9f, 0.5f);
    if (tag == "span")
      return Color(1.0f, 0.8f, 0.0f); // Gold
    if (tag == "a")
      return Color(0.0f, 0.5f, 1.0f); // Link blue

    return Color::Black(); // Default
  }

  void parseStyleAttribute(const std::string &cssText, ComputedStyle &style) {
    auto props = CssParser::parseDeclarations(cssText);
    applyDeclarations(props, style);
  }

  void parseBorderShorthand(const std::string &value, ComputedStyle &style) {
    std::istringstream stream(value);
    std::string part;
    while (stream >> part) {
      auto width = CssParser::parseValue(part);
      if (width.value > 0 && width.unit == CssUnit::Px) {
        style.borderWidth.top = style.borderWidth.right =
            style.borderWidth.bottom = style.borderWidth.left = width;
      }
      auto color = CssParser::parseColor(part);
      if (color) {
        style.borderColor = *color;
        style.borderTopColor = style.borderRightColor =
            style.borderBottomColor = style.borderLeftColor = *color;
      }
    }
  }

  void parseBorderSideShorthand(const std::string &value, CssValue &width, Color &color) {
    std::istringstream stream(value);
    std::string part;
    while (stream >> part) {
      auto w = CssParser::parseValue(part);
      if (w.value > 0 && w.unit == CssUnit::Px) {
        width = w;
      }
      auto c = CssParser::parseColor(part);
      if (c) {
        color = *c;
      }
    }
  }
};

} // namespace skene
