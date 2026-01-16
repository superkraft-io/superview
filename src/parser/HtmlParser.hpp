#pragma once

#include "dom/Node.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <stack>
#include <string>
#include <vector>

namespace skene {

// Result of parsing HTML - includes both DOM and extracted styles
struct ParseResult {
  std::shared_ptr<Node> document;
  std::vector<std::string> styleContents; // Contents of all <style> tags
};

class HtmlParser {
public:
  // Self-closing (void) elements that don't need closing tags
  static const std::set<std::string> &voidElements() {
    static const std::set<std::string> elements = {
        "area", "base", "br",    "col",   "embed",  "hr",    "img",
        "input", "link", "meta",  "param", "source", "track", "wbr",
        "!doctype", "style"}; // style is handled specially
    return elements;
  }

  // Raw text elements (content is not parsed as HTML)
  static const std::set<std::string> &rawTextElements() {
    static const std::set<std::string> elements = {"script", "style"};
    return elements;
  }

  // Helper to convert codepoint to UTF-8 string (used for entity map)
  static std::string utf8Char(int cp) {
    std::string result;
    if (cp < 0x80) {
      result += static_cast<char>(cp);
    } else if (cp < 0x800) {
      result += static_cast<char>(0xC0 | (cp >> 6));
      result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      result += static_cast<char>(0xE0 | (cp >> 12));
      result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
      result += static_cast<char>(0xF0 | (cp >> 18));
      result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return result;
  }

  // HTML entity map - using explicit UTF-8 encoding for all characters
  static const std::map<std::string, std::string> &htmlEntities() {
    static std::map<std::string, std::string> entities;
    static bool initialized = false;
    if (!initialized) {
      entities["amp"] = "&";
      entities["lt"] = "<";
      entities["gt"] = ">";
      entities["quot"] = "\"";
      entities["apos"] = "'";
      entities["nbsp"] = utf8Char(0x00A0);
      entities["copy"] = utf8Char(0x00A9);  // ©
      entities["reg"] = utf8Char(0x00AE);   // ®
      entities["trade"] = utf8Char(0x2122); // ™
      entities["euro"] = utf8Char(0x20AC);  // €
      entities["pound"] = utf8Char(0x00A3); // £
      entities["yen"] = utf8Char(0x00A5);   // ¥
      entities["cent"] = utf8Char(0x00A2);  // ¢
      entities["deg"] = utf8Char(0x00B0);   // °
      entities["plusmn"] = utf8Char(0x00B1); // ±
      entities["times"] = utf8Char(0x00D7); // ×
      entities["divide"] = utf8Char(0x00F7); // ÷
      entities["frac12"] = utf8Char(0x00BD); // ½
      entities["frac14"] = utf8Char(0x00BC); // ¼
      entities["frac34"] = utf8Char(0x00BE); // ¾
      entities["hellip"] = utf8Char(0x2026); // …
      entities["mdash"] = utf8Char(0x2014); // —
      entities["ndash"] = utf8Char(0x2013); // –
      entities["lsquo"] = utf8Char(0x2018); // '
      entities["rsquo"] = utf8Char(0x2019); // '
      entities["ldquo"] = utf8Char(0x201C); // "
      entities["rdquo"] = utf8Char(0x201D); // "
      entities["bull"] = utf8Char(0x2022);  // •
      entities["middot"] = utf8Char(0x00B7); // ·
      entities["para"] = utf8Char(0x00B6);  // ¶
      entities["sect"] = utf8Char(0x00A7);  // §
      entities["laquo"] = utf8Char(0x00AB); // «
      entities["raquo"] = utf8Char(0x00BB); // »
      entities["iexcl"] = utf8Char(0x00A1); // ¡
      entities["iquest"] = utf8Char(0x00BF); // ¿
      entities["acute"] = utf8Char(0x00B4); // ´
      entities["cedil"] = utf8Char(0x00B8); // ¸
      entities["macr"] = utf8Char(0x00AF);  // ¯
      entities["uml"] = utf8Char(0x00A8);   // ¨
      entities["ordf"] = utf8Char(0x00AA);  // ª
      entities["ordm"] = utf8Char(0x00BA);  // º
      entities["sup1"] = utf8Char(0x00B9);  // ¹
      entities["sup2"] = utf8Char(0x00B2);  // ²
      entities["sup3"] = utf8Char(0x00B3);  // ³
      entities["not"] = utf8Char(0x00AC);   // ¬
      entities["shy"] = utf8Char(0x00AD);   // soft hyphen
      initialized = true;
    }
    return entities;
  }

  // Parse and return both DOM and styles
  ParseResult parseWithStyles(const std::string &html) {
    ParseResult result;
    result.document = std::make_shared<Node>(NodeType::Document);
    std::stack<std::shared_ptr<Node>> nodeStack;
    nodeStack.push(result.document);

    size_t pos = 0;
    size_t len = html.length();

    while (pos < len) {
      // Find next tag
      size_t lt = html.find('<', pos);

      if (lt == std::string::npos) {
        // No more tags, process remaining text
        addTextNode(nodeStack, html.substr(pos));
        break;
      }

      // Process text before tag
      if (lt > pos) {
        addTextNode(nodeStack, html.substr(pos, lt - pos));
      }

      // Skip malformed tags
      if (lt + 1 >= len) {
        break;
      }

      // Check for comment
      if (html.substr(lt, 4) == "<!--") {
        size_t commentEnd = html.find("-->", lt + 4);
        if (commentEnd != std::string::npos) {
          pos = commentEnd + 3;
          continue;
        } else {
          break;
        }
      }

      // Check for DOCTYPE
      if (html.substr(lt, 9) == "<!DOCTYPE" ||
          html.substr(lt, 9) == "<!doctype") {
        size_t doctypeEnd = html.find('>', lt);
        if (doctypeEnd != std::string::npos) {
          pos = doctypeEnd + 1;
          continue;
        }
      }

      // Find end of tag
      size_t gt = findTagEnd(html, lt);
      if (gt == std::string::npos) {
        break;
      }

      std::string tagContent = html.substr(lt + 1, gt - lt - 1);
      
      // Check for style tag
      std::string tagName = extractTagName(tagContent);
      toLowerCase(tagName);
      
      if (tagName == "style" && tagContent[0] != '/') {
        // Find closing </style> tag
        size_t styleEnd = html.find("</style>", gt + 1);
        if (styleEnd == std::string::npos) {
          styleEnd = html.find("</STYLE>", gt + 1);
        }
        if (styleEnd != std::string::npos) {
          std::string styleContent = html.substr(gt + 1, styleEnd - gt - 1);
          result.styleContents.push_back(styleContent);
          pos = styleEnd + 8; // Skip past </style>
          continue;
        }
      }
      
      // Check for script tag (skip it entirely)
      if (tagName == "script" && tagContent[0] != '/') {
        size_t scriptEnd = html.find("</script>", gt + 1);
        if (scriptEnd == std::string::npos) {
          scriptEnd = html.find("</SCRIPT>", gt + 1);
        }
        if (scriptEnd != std::string::npos) {
          pos = scriptEnd + 9; // Skip past </script>
          continue;
        }
      }

      // Process tag
      processTag(tagContent, nodeStack);

      pos = gt + 1;
    }

    return result;
  }

  // Legacy parse method (just returns DOM)
  std::shared_ptr<Node> parse(const std::string &html) {
    return parseWithStyles(html).document;
  }

private:
  // Find the actual end of a tag (handling quoted attributes)
  size_t findTagEnd(const std::string &html, size_t start) {
    size_t pos = start + 1;
    bool inQuote = false;
    char quoteChar = 0;

    while (pos < html.length()) {
      char c = html[pos];

      if (inQuote) {
        if (c == quoteChar) {
          inQuote = false;
        }
      } else {
        if (c == '"' || c == '\'') {
          inQuote = true;
          quoteChar = c;
        } else if (c == '>') {
          return pos;
        }
      }
      pos++;
    }

    return std::string::npos;
  }

  void processTag(const std::string &tagContent,
                  std::stack<std::shared_ptr<Node>> &nodeStack) {
    if (tagContent.empty()) {
      return;
    }

    // Closing tag
    if (tagContent[0] == '/') {
      std::string tagName = extractTagName(tagContent.substr(1));
      toLowerCase(tagName);

      // Pop stack until we find matching tag or reach root
      while (nodeStack.size() > 1) {
        auto current = nodeStack.top();
        if (current->type == NodeType::Element &&
            current->tagName == tagName) {
          nodeStack.pop();
          break;
        }
        // Auto-close mismatched tags (error recovery)
        nodeStack.pop();
      }
      return;
    }

    // Self-closing tag syntax (e.g., <br/>)
    bool selfClosingSyntax =
        !tagContent.empty() && tagContent.back() == '/';
    std::string content = selfClosingSyntax
                              ? tagContent.substr(0, tagContent.length() - 1)
                              : tagContent;

    // Extract tag name
    std::string tagName = extractTagName(content);
    toLowerCase(tagName);

    if (tagName.empty()) {
      return;
    }

    // Create element
    auto element = Node::createElement(tagName);

    // Parse attributes
    parseAttributes(content, element);

    // Add to parent
    if (!nodeStack.empty()) {
      nodeStack.top()->appendChild(element);
    }

    // Don't push void elements or self-closing tags onto stack
    bool isVoid = voidElements().count(tagName) > 0;
    if (!isVoid && !selfClosingSyntax) {
      nodeStack.push(element);
    }
  }

  std::string extractTagName(const std::string &tagContent) {
    size_t end = 0;
    while (end < tagContent.length() && !std::isspace(static_cast<unsigned char>(tagContent[end])) &&
           tagContent[end] != '/' && tagContent[end] != '>') {
      end++;
    }
    return tagContent.substr(0, end);
  }

  void parseAttributes(const std::string &tagContent,
                       std::shared_ptr<Node> &element) {
    // Find where attributes start (after tag name)
    size_t start = 0;
    while (start < tagContent.length() && !std::isspace(static_cast<unsigned char>(tagContent[start]))) {
      start++;
    }

    std::string attrsStr = tagContent.substr(start);
    size_t pos = 0;
    size_t len = attrsStr.length();

    while (pos < len) {
      // Skip whitespace
      while (pos < len && std::isspace(static_cast<unsigned char>(attrsStr[pos]))) {
        pos++;
      }
      if (pos >= len) break;

      // Read attribute name
      std::string key;
      while (pos < len && !std::isspace(static_cast<unsigned char>(attrsStr[pos])) && 
             attrsStr[pos] != '=' && attrsStr[pos] != '>' && attrsStr[pos] != '/') {
        key += attrsStr[pos++];
      }
      if (key.empty()) break;

      // Skip whitespace
      while (pos < len && std::isspace(static_cast<unsigned char>(attrsStr[pos]))) {
        pos++;
      }

      std::string val;
      if (pos < len && attrsStr[pos] == '=') {
        pos++; // Skip '='
        
        // Skip whitespace
        while (pos < len && std::isspace(static_cast<unsigned char>(attrsStr[pos]))) {
          pos++;
        }

        if (pos < len) {
          char quote = attrsStr[pos];
          if (quote == '"' || quote == '\'') {
            pos++; // Skip opening quote
            while (pos < len && attrsStr[pos] != quote) {
              val += attrsStr[pos++];
            }
            if (pos < len) pos++; // Skip closing quote
          } else {
            // Unquoted value
            while (pos < len && !std::isspace(static_cast<unsigned char>(attrsStr[pos])) && 
                   attrsStr[pos] != '>' && attrsStr[pos] != '/') {
              val += attrsStr[pos++];
            }
          }
        }
      } else {
        val = key; // Boolean attribute
      }

      toLowerCase(key);
      element->attributes[key] = decodeEntities(val);
    }
  }

  void addTextNode(std::stack<std::shared_ptr<Node>> &nodeStack,
                   const std::string &text) {
    std::string processed = decodeEntities(text);

    // Collapse whitespace but preserve leading/trailing single space
    std::string collapsed;
    bool lastWasSpace = false;
    bool hadLeadingSpace = false;
    bool hadTrailingSpace = false;

    // Check for leading whitespace
    if (!processed.empty() && std::isspace(static_cast<unsigned char>(processed[0]))) {
      hadLeadingSpace = true;
    }
    // Check for trailing whitespace
    if (!processed.empty() && std::isspace(static_cast<unsigned char>(processed.back()))) {
      hadTrailingSpace = true;
    }

    for (char c : processed) {
      if (std::isspace(static_cast<unsigned char>(c))) {
        if (!lastWasSpace) {
          collapsed += ' ';
          lastWasSpace = true;
        }
      } else {
        collapsed += c;
        lastWasSpace = false;
      }
    }

    // Trim internal whitespace but preserve boundary spaces for inline flow
    size_t start = 0;
    size_t end = collapsed.length();
    
    // Find first non-space
    while (start < collapsed.length() && collapsed[start] == ' ') start++;
    // Find last non-space  
    while (end > start && collapsed[end - 1] == ' ') end--;
    
    if (start >= end) {
      // Text is only whitespace - only create a space node for inline parent elements
      // Block elements like div, section, etc. don't need whitespace-only text nodes
      if (!nodeStack.empty()) {
        std::string parentTag = nodeStack.top()->tagName;
        // Check if parent is an inline element (not block)
        bool isInlineParent = (parentTag == "span" || parentTag == "a" || 
                               parentTag == "strong" || parentTag == "em" ||
                               parentTag == "b" || parentTag == "i" || 
                               parentTag == "code" || parentTag == "p" ||
                               parentTag == "h1" || parentTag == "h2" || 
                               parentTag == "h3" || parentTag == "h4" ||
                               parentTag == "h5" || parentTag == "h6" ||
                               parentTag == "li" || parentTag == "td" ||
                               parentTag == "th" || parentTag == "label");
        // Don't create whitespace-only nodes for block containers
        // Only create them when inside a text-containing element
        if (!isInlineParent) {
          return;
        }
      }
      return; // Skip whitespace-only nodes entirely
    }
    
    // Build result with preserved boundary spaces
    std::string result;
    if (hadLeadingSpace) result += ' ';
    result += collapsed.substr(start, end - start);
    if (hadTrailingSpace && !hadLeadingSpace) result += ' ';
    else if (hadTrailingSpace && hadLeadingSpace && end < collapsed.length()) result += ' ';

    if (!result.empty() && !nodeStack.empty()) {
      auto textNode = Node::createText(result);
      nodeStack.top()->appendChild(textNode);
    }
  }

  std::string decodeEntities(const std::string &text) {
    std::string result;
    size_t pos = 0;

    while (pos < text.length()) {
      if (text[pos] == '&') {
        size_t semicolon = text.find(';', pos);
        if (semicolon != std::string::npos && semicolon - pos < 12) {
          std::string entity = text.substr(pos + 1, semicolon - pos - 1);

          // Numeric character reference
          if (!entity.empty() && entity[0] == '#') {
            try {
              int codePoint;
              if (entity.length() > 1 &&
                  (entity[1] == 'x' || entity[1] == 'X')) {
                // Hex
                codePoint = std::stoi(entity.substr(2), nullptr, 16);
              } else {
                // Decimal
                codePoint = std::stoi(entity.substr(1));
              }

              // Convert code point to UTF-8
              result += codePointToUtf8(codePoint);
              pos = semicolon + 1;
              continue;
            } catch (...) {
            }
          }

          // Named entity
          auto &entities = htmlEntities();
          auto it = entities.find(entity);
          if (it != entities.end()) {
            result += it->second;
            pos = semicolon + 1;
            continue;
          }
        }
      }

      result += text[pos];
      pos++;
    }

    return result;
  }

  std::string codePointToUtf8(int cp) {
    std::string result;

    if (cp < 0x80) {
      result += static_cast<char>(cp);
    } else if (cp < 0x800) {
      result += static_cast<char>(0xC0 | (cp >> 6));
      result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      result += static_cast<char>(0xE0 | (cp >> 12));
      result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
      result += static_cast<char>(0xF0 | (cp >> 18));
      result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (cp & 0x3F));
    }

    return result;
  }

  void toLowerCase(std::string &str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  }
};

} // namespace skene
