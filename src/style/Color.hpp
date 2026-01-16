#pragma once

namespace skene {

struct Color {
  float r, g, b, a;

  Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
  Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

  // Comparison operators
  bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  bool operator!=(const Color& other) const {
    return !(*this == other);
  }

  // Predefined colors for element types
  static Color Red() { return Color(0.8f, 0.2f, 0.2f); }
  static Color Green() { return Color(0.2f, 0.8f, 0.2f); }
  static Color Blue() { return Color(0.2f, 0.2f, 0.8f); }
  static Color Orange() { return Color(1.0f, 0.5f, 0.0f); }
  static Color Purple() { return Color(0.5f, 0.0f, 0.5f); }
  static Color Cyan() { return Color(0.0f, 0.8f, 0.8f); }
  static Color Yellow() { return Color(0.8f, 0.8f, 0.2f); }
  static Color Magenta() { return Color(0.8f, 0.2f, 0.8f); }
  static Color Black() { return Color(0.0f, 0.0f, 0.0f); }
  static Color White() { return Color(1.0f, 1.0f, 1.0f); }
  static Color Gray() { return Color(0.5f, 0.5f, 0.5f); }
  static Color Transparent() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
};

} // namespace skene
