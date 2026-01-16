#pragma once

// Prevent Windows macro conflicts
#ifdef NONE
#undef NONE
#endif

#include "Color.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace skene {

// CSS Units
enum class CssUnit { Px, Em, Rem, Percent, Vw, Vh, Auto, None };

struct CssValue {
  float value = 0.0f;
  CssUnit unit = CssUnit::Px;

  CssValue() = default;
  CssValue(float v, CssUnit u = CssUnit::Px) : value(v), unit(u) {}

  // Resolve to pixels given context
  float toPx(float parentSize = 0.0f, float fontSize = 16.0f,
             float viewportWidth = 1024.0f,
             float viewportHeight = 768.0f) const {
    switch (unit) {
    case CssUnit::Px:
      return value;
    case CssUnit::Em:
      return value * fontSize;
    case CssUnit::Rem:
      return value * 16.0f; // Root font size
    case CssUnit::Percent:
      return (value / 100.0f) * parentSize;
    case CssUnit::Vw:
      return (value / 100.0f) * viewportWidth;
    case CssUnit::Vh:
      return (value / 100.0f) * viewportHeight;
    case CssUnit::Auto:
    case CssUnit::None:
      return -1.0f;
    }
    return value;
  }

  bool isAuto() const { return unit == CssUnit::Auto; }
};

class CssParser {
public:
  // Named colors map
  static const std::map<std::string, Color> &namedColors() {
    static const std::map<std::string, Color> colors = {
        {"black", Color(0.0f, 0.0f, 0.0f)},
        {"white", Color(1.0f, 1.0f, 1.0f)},
        {"red", Color(1.0f, 0.0f, 0.0f)},
        {"green", Color(0.0f, 0.5f, 0.0f)},
        {"blue", Color(0.0f, 0.0f, 1.0f)},
        {"yellow", Color(1.0f, 1.0f, 0.0f)},
        {"cyan", Color(0.0f, 1.0f, 1.0f)},
        {"magenta", Color(1.0f, 0.0f, 1.0f)},
        {"orange", Color(1.0f, 0.647f, 0.0f)},
        {"purple", Color(0.5f, 0.0f, 0.5f)},
        {"pink", Color(1.0f, 0.753f, 0.796f)},
        {"brown", Color(0.647f, 0.165f, 0.165f)},
        {"gray", Color(0.5f, 0.5f, 0.5f)},
        {"grey", Color(0.5f, 0.5f, 0.5f)},
        {"silver", Color(0.753f, 0.753f, 0.753f)},
        {"navy", Color(0.0f, 0.0f, 0.5f)},
        {"teal", Color(0.0f, 0.5f, 0.5f)},
        {"olive", Color(0.5f, 0.5f, 0.0f)},
        {"maroon", Color(0.5f, 0.0f, 0.0f)},
        {"lime", Color(0.0f, 1.0f, 0.0f)},
        {"aqua", Color(0.0f, 1.0f, 1.0f)},
        {"fuchsia", Color(1.0f, 0.0f, 1.0f)},
        {"transparent", Color(0.0f, 0.0f, 0.0f, 0.0f)},
        {"lightgray", Color(0.827f, 0.827f, 0.827f)},
        {"lightgrey", Color(0.827f, 0.827f, 0.827f)},
        {"darkgray", Color(0.663f, 0.663f, 0.663f)},
        {"darkgrey", Color(0.663f, 0.663f, 0.663f)},
        {"lightblue", Color(0.678f, 0.847f, 0.902f)},
        {"lightgreen", Color(0.565f, 0.933f, 0.565f)},
        {"lightyellow", Color(1.0f, 1.0f, 0.878f)},
        {"darkblue", Color(0.0f, 0.0f, 0.545f)},
        {"darkgreen", Color(0.0f, 0.392f, 0.0f)},
        {"darkred", Color(0.545f, 0.0f, 0.0f)},
        {"coral", Color(1.0f, 0.498f, 0.314f)},
        {"crimson", Color(0.863f, 0.078f, 0.235f)},
        {"gold", Color(1.0f, 0.843f, 0.0f)},
        {"indigo", Color(0.294f, 0.0f, 0.51f)},
        {"ivory", Color(1.0f, 1.0f, 0.941f)},
        {"khaki", Color(0.941f, 0.902f, 0.549f)},
        {"lavender", Color(0.902f, 0.902f, 0.98f)},
        {"salmon", Color(0.98f, 0.502f, 0.447f)},
        {"skyblue", Color(0.529f, 0.808f, 0.922f)},
        {"tomato", Color(1.0f, 0.388f, 0.278f)},
        {"turquoise", Color(0.251f, 0.878f, 0.816f)},
        {"violet", Color(0.933f, 0.51f, 0.933f)},
        {"wheat", Color(0.961f, 0.871f, 0.702f)},
    };
    return colors;
  }

  // Parse CSS declarations into property map
  static std::map<std::string, std::string>
  parseDeclarations(const std::string &css) {
    std::map<std::string, std::string> props;

    std::string cleaned = css;
    // Remove comments
    size_t commentStart;
    while ((commentStart = cleaned.find("/*")) != std::string::npos) {
      size_t commentEnd = cleaned.find("*/", commentStart);
      if (commentEnd != std::string::npos) {
        cleaned.erase(commentStart, commentEnd - commentStart + 2);
      } else {
        break;
      }
    }

    // Split by semicolons
    std::istringstream stream(cleaned);
    std::string declaration;

    while (std::getline(stream, declaration, ';')) {
      size_t colonPos = declaration.find(':');
      if (colonPos != std::string::npos) {
        std::string property = trim(declaration.substr(0, colonPos));
        std::string value = trim(declaration.substr(colonPos + 1));

        // Convert property to lowercase
        std::transform(property.begin(), property.end(), property.begin(),
                       ::tolower);

        if (!property.empty() && !value.empty()) {
          props[property] = value;
        }
      }
    }

    return props;
  }

  // Parse a CSS value with unit
  static CssValue parseValue(const std::string &valueStr) {
    std::string str = trim(valueStr);

    if (str.empty()) {
      return CssValue(0, CssUnit::Px);
    }

    // Check for 'auto'
    if (str == "auto") {
      return CssValue(0, CssUnit::Auto);
    }

    // Check for 'none'
    if (str == "none" || str == "0") {
      return CssValue(0, CssUnit::None);
    }

    // Extract numeric part and unit
    size_t i = 0;
    bool negative = false;

    if (i < str.length() && str[i] == '-') {
      negative = true;
      i++;
    }

    size_t numStart = i;
    while (i < str.length() && (std::isdigit(static_cast<unsigned char>(str[i])) || str[i] == '.')) {
      i++;
    }

    if (i == numStart) {
      return CssValue(0, CssUnit::Px);
    }

    float value = std::stof(str.substr(numStart, i - numStart));
    if (negative)
      value = -value;

    std::string unitStr = trim(str.substr(i));
    std::transform(unitStr.begin(), unitStr.end(), unitStr.begin(), ::tolower);

    CssUnit unit = CssUnit::Px;
    if (unitStr.empty() || unitStr == "px") {
      unit = CssUnit::Px;
    } else if (unitStr == "em") {
      unit = CssUnit::Em;
    } else if (unitStr == "rem") {
      unit = CssUnit::Rem;
    } else if (unitStr == "%") {
      unit = CssUnit::Percent;
    } else if (unitStr == "vw") {
      unit = CssUnit::Vw;
    } else if (unitStr == "vh") {
      unit = CssUnit::Vh;
    }

    return CssValue(value, unit);
  }

  // Parse color from string
  static std::optional<Color> parseColor(const std::string &colorStr) {
    std::string str = trim(colorStr);
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);

    if (str.empty()) {
      return std::nullopt;
    }

    // Named color
    auto &colors = namedColors();
    auto it = colors.find(str);
    if (it != colors.end()) {
      return it->second;
    }

    // Hex color
    if (str[0] == '#') {
      return parseHexColor(str);
    }

    // rgb() or rgba()
    if (str.substr(0, 4) == "rgb(" || str.substr(0, 5) == "rgba(") {
      return parseRgbColor(str);
    }

    // hsl() or hsla()
    if (str.substr(0, 4) == "hsl(" || str.substr(0, 5) == "hsla(") {
      return parseHslColor(str);
    }

    return std::nullopt;
  }

  // Parse 2-value shorthand (margin-block, margin-inline, padding-block, etc.)
  static void parse2ValueShorthand(const std::string &valueStr, CssValue &first,
                                   CssValue &second) {
    std::vector<std::string> parts = splitValues(valueStr);

    if (parts.empty()) {
      first = second = CssValue(0, CssUnit::Px);
      return;
    }

    if (parts.size() == 1) {
      first = second = parseValue(parts[0]);
    } else {
      first = parseValue(parts[0]);
      second = parseValue(parts[1]);
    }
  }

  // Parse 4-value shorthand (margin, padding, border-width)
  static void parse4ValueShorthand(const std::string &valueStr, CssValue &top,
                                   CssValue &right, CssValue &bottom,
                                   CssValue &left) {
    std::vector<std::string> parts = splitValues(valueStr);

    if (parts.empty()) {
      top = right = bottom = left = CssValue(0, CssUnit::Px);
      return;
    }

    if (parts.size() == 1) {
      top = right = bottom = left = parseValue(parts[0]);
    } else if (parts.size() == 2) {
      top = bottom = parseValue(parts[0]);
      right = left = parseValue(parts[1]);
    } else if (parts.size() == 3) {
      top = parseValue(parts[0]);
      right = left = parseValue(parts[1]);
      bottom = parseValue(parts[2]);
    } else {
      top = parseValue(parts[0]);
      right = parseValue(parts[1]);
      bottom = parseValue(parts[2]);
      left = parseValue(parts[3]);
    }
  }

  // Helper to split space-separated values
  static std::vector<std::string> splitValues(const std::string &str) {
    std::vector<std::string> parts;
    std::istringstream stream(str);
    std::string part;

    while (stream >> part) {
      parts.push_back(part);
    }

    return parts;
  }

private:
  static std::optional<Color> parseHexColor(const std::string &hex) {
    std::string h = hex.substr(1); // Remove #

    int r, g, b, a = 255;

    if (h.length() == 3) {
      // #RGB -> #RRGGBB
      r = std::stoi(std::string(2, h[0]), nullptr, 16);
      g = std::stoi(std::string(2, h[1]), nullptr, 16);
      b = std::stoi(std::string(2, h[2]), nullptr, 16);
    } else if (h.length() == 4) {
      // #RGBA
      r = std::stoi(std::string(2, h[0]), nullptr, 16);
      g = std::stoi(std::string(2, h[1]), nullptr, 16);
      b = std::stoi(std::string(2, h[2]), nullptr, 16);
      a = std::stoi(std::string(2, h[3]), nullptr, 16);
    } else if (h.length() == 6) {
      // #RRGGBB
      r = std::stoi(h.substr(0, 2), nullptr, 16);
      g = std::stoi(h.substr(2, 2), nullptr, 16);
      b = std::stoi(h.substr(4, 2), nullptr, 16);
    } else if (h.length() == 8) {
      // #RRGGBBAA
      r = std::stoi(h.substr(0, 2), nullptr, 16);
      g = std::stoi(h.substr(2, 2), nullptr, 16);
      b = std::stoi(h.substr(4, 2), nullptr, 16);
      a = std::stoi(h.substr(6, 2), nullptr, 16);
    } else {
      return std::nullopt;
    }

    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  }

  static std::optional<Color> parseRgbColor(const std::string &rgb) {
    // Extract values between parentheses
    size_t start = rgb.find('(');
    size_t end = rgb.find(')');
    if (start == std::string::npos || end == std::string::npos) {
      return std::nullopt;
    }

    std::string values = rgb.substr(start + 1, end - start - 1);

    // Split by comma or space
    std::vector<float> parts;
    std::istringstream stream(values);
    std::string part;

    while (std::getline(stream, part, ',')) {
      std::string trimmed = trim(part);
      if (trimmed.back() == '%') {
        parts.push_back(std::stof(trimmed.substr(0, trimmed.length() - 1)) /
                        100.0f);
      } else {
        float val = std::stof(trimmed);
        // If > 1, assume 0-255 range
        if (val > 1.0f)
          val /= 255.0f;
        parts.push_back(val);
      }
    }

    if (parts.size() < 3) {
      return std::nullopt;
    }

    float a = parts.size() >= 4 ? parts[3] : 1.0f;
    return Color(parts[0], parts[1], parts[2], a);
  }

  static std::optional<Color> parseHslColor(const std::string &hsl) {
    size_t start = hsl.find('(');
    size_t end = hsl.find(')');
    if (start == std::string::npos || end == std::string::npos) {
      return std::nullopt;
    }

    std::string values = hsl.substr(start + 1, end - start - 1);
    std::vector<float> parts;
    std::istringstream stream(values);
    std::string part;

    int idx = 0;
    while (std::getline(stream, part, ',')) {
      std::string trimmed = trim(part);
      if (idx == 0) {
        // Hue: degrees
        parts.push_back(std::stof(trimmed));
      } else {
        // Saturation and lightness: percentages
        if (trimmed.back() == '%') {
          trimmed = trimmed.substr(0, trimmed.length() - 1);
        }
        parts.push_back(std::stof(trimmed) / 100.0f);
      }
      idx++;
    }

    if (parts.size() < 3) {
      return std::nullopt;
    }

    float h = std::fmod(parts[0], 360.0f) / 360.0f;
    float s = parts[1];
    float l = parts[2];
    float a = parts.size() >= 4 ? parts[3] : 1.0f;

    // HSL to RGB conversion
    float r, g, b;
    if (s == 0) {
      r = g = b = l;
    } else {
      auto hue2rgb = [](float p, float q, float t) {
        if (t < 0)
          t += 1;
        if (t > 1)
          t -= 1;
        if (t < 1.0f / 6.0f)
          return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f)
          return q;
        if (t < 2.0f / 3.0f)
          return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
      };

      float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
      float p = 2 * l - q;
      r = hue2rgb(p, q, h + 1.0f / 3.0f);
      g = hue2rgb(p, q, h);
      b = hue2rgb(p, q, h - 1.0f / 3.0f);
    }

    return Color(r, g, b, a);
  }

public:
  // ============================================================
  // CSS Selector & Rule Parsing (Public API)
  // ============================================================

  // Trim helper (public)
  static std::string trim(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
      return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
  }

  // Represents a simple selector (tag, class, or id)
  struct SimpleSelector {
    std::string tag;               // e.g., "div", "*" for universal
    std::string id;                // e.g., "myId" (without #)
    std::vector<std::string> classes; // e.g., {"btn", "primary"} (without .)
    
    // Calculate specificity: (id count, class count, tag count)
    std::tuple<int, int, int> specificity() const {
      int idCount = id.empty() ? 0 : 1;
      int classCount = static_cast<int>(classes.size());
      int tagCount = (tag.empty() || tag == "*") ? 0 : 1;
      return {idCount, classCount, tagCount};
    }
  };

  // A compound selector: list of simple selectors for descendant matching
  // e.g., "footer p" becomes [SimpleSelector("footer"), SimpleSelector("p")]
  struct CompoundSelector {
    std::vector<SimpleSelector> parts;  // From ancestor to target (last is the target)
    
    std::tuple<int, int, int> specificity() const {
      int ids = 0, classes = 0, tags = 0;
      for (const auto& part : parts) {
        auto [i, c, t] = part.specificity();
        ids += i;
        classes += c;
        tags += t;
      }
      return {ids, classes, tags};
    }
  };

  // A CSS rule: selector + declarations
  struct CssRule {
    std::string selectorText;
    SimpleSelector selector;         // For simple selectors (backward compat)
    CompoundSelector compoundSelector;  // For descendant selectors
    std::map<std::string, std::string> declarations;
    
    // For sorting by specificity
    std::tuple<int, int, int> specificity() const {
      if (compoundSelector.parts.size() > 1) {
        return compoundSelector.specificity();
      }
      return selector.specificity();
    }
  };

  // Parse a simple selector string like "div", ".class", "#id", "div.class#id"
  static SimpleSelector parseSimpleSelector(const std::string& selectorStr) {
    SimpleSelector sel;
    std::string str = trim(selectorStr);
    
    size_t i = 0;
    std::string current;
    char mode = 't'; // 't' = tag, '.' = class, '#' = id
    
    while (i <= str.length()) {
      char c = (i < str.length()) ? str[i] : '\0';
      
      if (c == '.' || c == '#' || c == '\0') {
        // Save current token
        if (!current.empty()) {
          if (mode == 't') {
            sel.tag = current;
          } else if (mode == '.') {
            sel.classes.push_back(current);
          } else if (mode == '#') {
            sel.id = current;
          }
        }
        current.clear();
        mode = c;
      } else {
        current += c;
      }
      i++;
    }
    
    return sel;
  }

  // Parse a compound selector (e.g., "footer p", "div.class p.text")
  static CompoundSelector parseCompoundSelector(const std::string& selectorStr) {
    CompoundSelector compound;
    std::string str = trim(selectorStr);
    
    // Split by whitespace (descendant combinator)
    std::istringstream iss(str);
    std::string part;
    while (iss >> part) {
      // Skip combinators like > + ~ for now (treat as descendant)
      if (part == ">" || part == "+" || part == "~") continue;
      compound.parts.push_back(parseSimpleSelector(part));
    }
    
    return compound;
  }

  // Parse a full CSS stylesheet (multiple rules)
  static std::vector<CssRule> parseStylesheet(const std::string& css) {
    std::vector<CssRule> rules;
    std::string content = css;
    
    // Remove CSS comments
    size_t commentStart;
    while ((commentStart = content.find("/*")) != std::string::npos) {
      size_t commentEnd = content.find("*/", commentStart);
      if (commentEnd != std::string::npos) {
        content.erase(commentStart, commentEnd - commentStart + 2);
      } else {
        content.erase(commentStart);
      }
    }
    
    size_t pos = 0;
    while (pos < content.length()) {
      // Find selector (before {)
      size_t braceOpen = content.find('{', pos);
      if (braceOpen == std::string::npos) break;
      
      std::string selectorText = trim(content.substr(pos, braceOpen - pos));
      if (selectorText.empty()) {
        pos = braceOpen + 1;
        continue;
      }
      
      // Find closing brace
      size_t braceClose = content.find('}', braceOpen);
      if (braceClose == std::string::npos) break;
      
      std::string declarationBlock = content.substr(braceOpen + 1, braceClose - braceOpen - 1);
      
      // Handle multiple selectors separated by comma
      std::vector<std::string> selectors;
      std::stringstream ss(selectorText);
      std::string singleSelector;
      while (std::getline(ss, singleSelector, ',')) {
        std::string trimmed = trim(singleSelector);
        if (!trimmed.empty()) {
          selectors.push_back(trimmed);
        }
      }
      
      // Parse declarations
      auto declarations = parseDeclarations(declarationBlock);
      
      // Create a rule for each selector
      for (const auto& sel : selectors) {
        CssRule rule;
        rule.selectorText = sel;
        rule.compoundSelector = parseCompoundSelector(sel);
        // Also store simple selector for backward compatibility (use last part)
        if (!rule.compoundSelector.parts.empty()) {
          rule.selector = rule.compoundSelector.parts.back();
        } else {
          rule.selector = parseSimpleSelector(sel);
        }
        rule.declarations = declarations;
        rules.push_back(rule);
      }
      
      pos = braceClose + 1;
    }
    
    return rules;
  }
};

} // namespace skene
