# Chrome Rendering Parity TODO

This document outlines features and behaviors to implement for Chrome-like rendering.

## 1. User-Agent Stylesheet (High Priority)

Chrome's default CSS rules for HTML elements. Source: [Chromium UA Stylesheet](https://chromium.googlesource.com/chromium/blink/+/master/Source/core/css/html.css)

### Block Elements
- [ ] `html, body` - margin: 8px for body
- [ ] `h1` - font-size: 2em; margin: 0.67em 0; font-weight: bold
- [ ] `h2` - font-size: 1.5em; margin: 0.83em 0; font-weight: bold
- [ ] `h3` - font-size: 1.17em; margin: 1em 0; font-weight: bold
- [ ] `h4` - font-size: 1em; margin: 1.33em 0; font-weight: bold
- [ ] `h5` - font-size: 0.83em; margin: 1.67em 0; font-weight: bold
- [ ] `h6` - font-size: 0.67em; margin: 2.33em 0; font-weight: bold
- [ ] `p` - margin: 1em 0
- [ ] `blockquote` - margin: 1em 40px
- [ ] `pre` - font-family: monospace; white-space: pre; margin: 1em 0
- [ ] `hr` - border-style: inset; border-width: 1px; margin: 0.5em auto

### Lists
- [ ] `ul, ol` - margin: 1em 0; padding-left: 40px
- [ ] `ul` - list-style-type: disc
- [ ] `ol` - list-style-type: decimal
- [ ] `li` - display: list-item
- [ ] Nested list style changes (disc → circle → square)

### Inline Elements
- [ ] `strong, b` - font-weight: bold
- [ ] `em, i` - font-style: italic
- [ ] `u` - text-decoration: underline
- [ ] `s, strike, del` - text-decoration: line-through
- [ ] `sub` - vertical-align: sub; font-size: smaller
- [ ] `sup` - vertical-align: super; font-size: smaller
- [ ] `small` - font-size: smaller
- [ ] `big` - font-size: larger
- [ ] `code, kbd, samp` - font-family: monospace
- [ ] `a:link` - color: #0000EE; text-decoration: underline
- [ ] `a:visited` - color: #551A8B

### Tables
- [ ] `table` - border-collapse: separate; border-spacing: 2px
- [ ] `td, th` - padding: 1px
- [ ] `th` - font-weight: bold; text-align: center

### Forms
- [ ] `input, textarea, select, button` - default styling
- [ ] Focus outlines

## 2. CSS Box Model (High Priority)

### Margin Collapsing
- [ ] Adjacent sibling margins collapse (take larger)
- [ ] Parent-child margin collapsing
- [ ] Empty block margin collapsing
- [ ] Margins don't collapse with padding/border between
- [ ] Margins don't collapse for floats, absolute/fixed, inline-block, flex items

### Box Sizing
- [ ] `box-sizing: content-box` (default)
- [ ] `box-sizing: border-box`

### Overflow
- [ ] `overflow: visible` (default)
- [ ] `overflow: hidden`
- [ ] `overflow: scroll`
- [ ] `overflow: auto`

## 3. Text & Typography (High Priority)

### Line Height
- [ ] Default line-height: normal (~1.2 × font-size)
- [ ] Numeric line-height (e.g., `1.5`)
- [ ] Length line-height (e.g., `24px`)
- [ ] Percentage line-height

### Vertical Alignment
- [ ] `vertical-align: baseline` (default)
- [ ] `vertical-align: top/middle/bottom`
- [ ] `vertical-align: text-top/text-bottom`
- [ ] `vertical-align: sub/super`

### Text Properties
- [ ] `text-align: left/center/right/justify`
- [ ] `text-indent`
- [ ] `letter-spacing`
- [ ] `word-spacing`
- [ ] `white-space: normal/nowrap/pre/pre-wrap/pre-line`
- [ ] `text-transform: uppercase/lowercase/capitalize`
- [ ] `text-overflow: ellipsis`

### Font Features
- [ ] `font-variant`
- [ ] `font-stretch`
- [ ] System font stack resolution

## 4. Layout (Medium Priority)

### Display Types
- [x] `display: block`
- [x] `display: inline`
- [x] `display: inline-block`
- [x] `display: flex`
- [ ] `display: inline-flex`
- [ ] `display: grid`
- [ ] `display: none`

### Positioning
- [x] `position: static` (default)
- [x] `position: relative`
- [ ] `position: absolute`
- [ ] `position: fixed`
- [ ] `position: sticky`
- [ ] `z-index` stacking

### Flexbox (partial)
- [x] `flex-direction`
- [x] `justify-content`
- [x] `align-items`
- [ ] `align-content`
- [ ] `flex-wrap`
- [ ] `flex-grow`
- [ ] `flex-shrink`
- [ ] `flex-basis`
- [ ] `align-self`
- [ ] `order`
- [ ] `gap` / `row-gap` / `column-gap`

### Float & Clear
- [ ] `float: left/right`
- [ ] `clear: left/right/both`

## 5. Visual Effects (Medium Priority)

### Backgrounds
- [x] `background-color`
- [ ] `background-image`
- [ ] `background-position`
- [ ] `background-size`
- [ ] `background-repeat`
- [ ] Multiple backgrounds
- [ ] Gradients (linear-gradient, radial-gradient)

### Borders
- [x] `border-width/style/color`
- [x] `border-radius`
- [ ] Individual corner radii
- [ ] `border-image`

### Shadows
- [ ] `box-shadow`
- [ ] `text-shadow`

### Opacity & Visibility
- [x] `opacity`
- [ ] `visibility: visible/hidden`

### Transforms
- [ ] `transform: translate/rotate/scale/skew`
- [ ] `transform-origin`

### Transitions & Animations
- [ ] `transition`
- [ ] `animation` / `@keyframes`

## 6. Selectors (Medium Priority)

### Currently Supported
- [x] Element selectors (`div`, `p`)
- [x] Class selectors (`.class`)
- [x] ID selectors (`#id`)
- [x] Descendant combinator (`div p`)

### To Implement
- [ ] Child combinator (`div > p`)
- [ ] Adjacent sibling (`h1 + p`)
- [ ] General sibling (`h1 ~ p`)
- [ ] Attribute selectors (`[attr]`, `[attr=value]`)
- [ ] Pseudo-classes (`:hover`, `:active`, `:focus`, `:first-child`, `:last-child`, `:nth-child()`)
- [ ] Pseudo-elements (`::before`, `::after`, `::first-letter`, `::first-line`)
- [ ] `:not()` selector
- [ ] Multiple selectors (`h1, h2, h3`)

### Specificity
- [ ] Proper specificity calculation
- [ ] `!important` handling
- [ ] Cascade order (user-agent → user → author)

## 7. Units & Values (Low Priority)

### Currently Supported
- [x] `px`
- [x] `em`
- [x] `%`
- [x] `vw`, `vh`

### To Implement
- [ ] `rem` (root em)
- [ ] `vmin`, `vmax`
- [ ] `ch`, `ex`
- [ ] `calc()`
- [ ] `min()`, `max()`, `clamp()`

## 8. Inherited vs Non-Inherited Properties

Ensure correct inheritance behavior:
- [ ] `color` - inherited
- [ ] `font-*` - inherited
- [ ] `line-height` - inherited
- [ ] `text-align` - inherited
- [ ] `background-*` - NOT inherited
- [ ] `border-*` - NOT inherited
- [ ] `margin/padding` - NOT inherited
- [ ] `inherit` keyword
- [ ] `initial` keyword
- [ ] `unset` keyword

## 9. Rendering Accuracy

### Subpixel Rendering
- [ ] Subpixel positioning for smoother layout
- [ ] Anti-aliased borders at subpixel positions

### Font Rendering
- [x] MSDF for sharp text at all sizes
- [ ] Proper baseline alignment
- [ ] Kerning support
- [ ] Ligatures (fi, fl, etc.)

### Color Accuracy
- [x] Hex colors
- [x] RGB/RGBA
- [x] Named colors
- [ ] HSL/HSLA
- [ ] `currentColor` keyword
- [ ] Color interpolation for gradients

## 10. Performance Optimizations

- [x] Font atlas caching
- [ ] Layout caching (avoid re-layout when unnecessary)
- [ ] Dirty rect rendering (only repaint changed areas)
- [ ] Layer compositing for transforms/opacity
- [ ] Text shaping cache

---

## Implementation Priority

### Phase 1 - Core Parity
1. User-Agent stylesheet defaults
2. Margin collapsing
3. Line-height calculations
4. Missing display types (none, inline-flex)

### Phase 2 - Advanced Layout
1. Absolute/fixed positioning
2. Float/clear
3. Full flexbox support
4. CSS Grid basics

### Phase 3 - Visual Polish
1. Box shadows
2. Gradients
3. Transforms
4. Transitions

### Phase 4 - Advanced Selectors
1. Pseudo-classes
2. Pseudo-elements
3. Complex selectors
4. Proper specificity
