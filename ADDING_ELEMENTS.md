# Adding New Element Types to Skene

This document provides instructions for adding support for new HTML element types to the Skene renderer.

## Current Supported Elements
- `<div>` - Block container (Red border)
- `<h1>` - Heading (Green border)
- `<p>` - Paragraph (Blue border)
- Text nodes

## Steps to Add a New Element Type

### 1. Parser Support (Already Generic)
The `HtmlParser` already supports any tag name. No changes needed unless you need special parsing logic.

### 2. Add Visual Styling in `main.cpp`

**Location:** `paint()` function in `src/main.cpp`

**Add color mapping:**
```cpp
if (tag == "div") {
  r = 0.8f; g = 0.2f; b = 0.2f;  // Red
} else if (tag == "h1") {
  r = 0.2f; g = 0.8f; b = 0.2f;  // Green
} else if (tag == "p") {
  r = 0.2f; g = 0.2f; b = 0.8f;  // Blue
} else if (tag == "span") {      // NEW ELEMENT
  r = 0.8f; g = 0.8f; b = 0.2f;  // Yellow
}
```

### 3. Add Layout Behavior (if needed)

**Location:** `RenderBox::layout()` in `src/layout/RenderTree.hpp`

**Current behavior:** All elements use block layout with CSS padding support.

**For inline elements (like `<span>`):**
- Will require implementing inline layout logic
- Need to track line wrapping
- Currently not supported

**For special elements:**
```cpp
// In layout() function
if (node->tagName == "hr") {
  // Horizontal rule: fixed height, full width
  frame.height = 2.0f;
  return; // Skip children
}
```

### 4. Add Special Rendering (if needed)

**Location:** `paint()` function in `src/main.cpp`

**Example - Image placeholder:**
```cpp
if (box->node->tagName == "img") {
  // Draw placeholder rectangle
  renderer.drawRect(box->frame.x, box->frame.y, 
                   box->frame.width, box->frame.height,
                   0.5f, 0.5f, 0.5f, 1.0f);
  
  // Draw "IMG" text
  renderer.drawText(box->frame.x + 5, box->frame.y + 20,
                   "IMG", font, 1.0f, 1.0f, 1.0f, 1.0f);
}
```

### 5. Test the New Element

**Create test HTML:**
```html
<div style="padding:20px">
  <your-new-element>Content</your-new-element>
</div>
```

**Verify:**
- Element appears in inspector tree
- Border color is correct
- Layout behaves as expected
- Attributes are parsed correctly

## Element Type Reference

### Block Elements (Recommended to add first)
- `<h2>`, `<h3>`, `<h4>`, `<h5>`, `<h6>` - Headings
- `<section>`, `<article>`, `<aside>` - Semantic containers
- `<header>`, `<footer>`, `<nav>` - Page structure
- `<ul>`, `<ol>`, `<li>` - Lists
- `<blockquote>` - Quoted text
- `<pre>` - Preformatted text
- `<hr>` - Horizontal rule

### Inline Elements (Requires inline layout implementation)
- `<span>` - Inline container
- `<a>` - Links
- `<strong>`, `<em>` - Text emphasis
- `<code>` - Inline code

### Special Elements (Require custom rendering)
- `<img>` - Images (placeholder)
- `<input>` - Form inputs
- `<button>` - Buttons
- `<textarea>` - Multi-line input

## Color Palette Suggestions

Use distinct colors for easy debugging:
- Orange: `r=1.0f, g=0.5f, b=0.0f`
- Purple: `r=0.5f, g=0.0f, b=0.5f`
- Cyan: `r=0.0f, g=0.8f, b=0.8f`
- Yellow: `r=0.8f, g=0.8f, b=0.2f`
- Magenta: `r=0.8f, g=0.2f, b=0.8f`

## Next Steps for Better Architecture

See `REFACTORING.md` for plans to improve code structure as we add more features.
