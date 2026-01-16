#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>


namespace skene {

enum class NodeType { Element, Text, Document };

struct Node : public std::enable_shared_from_this<Node> {
  NodeType type;
  std::string tagName;     // e.g. "div", "h1" (empty if Text/Document)
  std::string textContent; // Only for Text nodes
  std::map<std::string, std::string> attributes;
  std::vector<std::shared_ptr<Node>> children;
  std::weak_ptr<Node> parent;

  Node(NodeType t) : type(t) {}

  static std::shared_ptr<Node> createElement(const std::string &tag) {
    auto node = std::make_shared<Node>(NodeType::Element);
    node->tagName = tag;
    return node;
  }

  static std::shared_ptr<Node> createText(const std::string &text) {
    auto node = std::make_shared<Node>(NodeType::Text);
    node->textContent = text;
    return node;
  }

  void appendChild(std::shared_ptr<Node> child) {
    children.push_back(child);
    child->parent = shared_from_this();
  }

  // Get the id attribute
  std::string getId() const {
    auto it = attributes.find("id");
    return (it != attributes.end()) ? it->second : "";
  }

  // Get class list as vector
  std::vector<std::string> getClassList() const {
    std::vector<std::string> classes;
    auto it = attributes.find("class");
    if (it != attributes.end()) {
      std::istringstream iss(it->second);
      std::string cls;
      while (iss >> cls) {
        classes.push_back(cls);
      }
    }
    return classes;
  }

  // Check if element has a specific class
  bool hasClass(const std::string& className) const {
    auto classes = getClassList();
    for (const auto& c : classes) {
      if (c == className) return true;
    }
    return false;
  }

  // Helper to print tree
  void print(int indent = 0) const {
    std::string padding(indent * 2, ' ');
    if (type == NodeType::Document) {
      std::cout << padding << "Document" << std::endl;
    } else if (type == NodeType::Element) {
      std::cout << padding << "<" << tagName << ">" << std::endl;
    } else if (type == NodeType::Text) {
      std::cout << padding << "\"" << textContent << "\"" << std::endl;
    }

    for (const auto &child : children) {
      child->print(indent + 1);
    }
  }
};

} // namespace skene
