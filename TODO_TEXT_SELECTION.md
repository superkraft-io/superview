# Text Selection - Implementation Checklist

## Single Click
- [x] Click on text places cursor/starts selection
- [x] Click on empty space finds nearest text
- [x] Click position determines anchor (above/below/left/right of text)
- [x] Click in inspector area selects DOM node (separate behavior)

## Drag Selection (Single Click + Drag)
- [x] Drag selects character by character
- [x] Selection updates in real-time during drag
- [x] Cross-element selection (across multiple text nodes)
- [x] Selection works in flexbox layouts (horizontal bounds check)
- [ ] Auto-scroll when dragging near viewport edges
- [ ] Selection extends when dragging outside the content area

## Double Click
- [x] Double-click selects word under cursor
- [x] Word boundaries: whitespace and punctuation
- [ ] Apostrophe within word NOT a boundary ("don't" = one word)
- [ ] Hyphenated words handling ("self-aware" - browser varies)
- [ ] Double-click on punctuation selects just the punctuation
- [ ] Double-click on whitespace selects whitespace run (or nothing)
- [ ] Double-click at exact word boundary selects word to the right
- [ ] Double-click at line start selects first word
- [ ] Double-click at line end selects last word

## Double Click + Drag (Word-wise Selection)
- [ ] After double-click, drag extends selection by whole words
- [ ] Original double-clicked word is always included (anchor word)
- [ ] Dragging backward still includes anchor word
- [ ] Word-wise selection across line breaks
- [ ] Word-wise selection across elements

## Triple Click
- [x] Triple-click selects line
- [ ] Triple-click should select entire paragraph/block element, not just visual line
- [ ] Triple-click on multi-line paragraph selects all lines of that paragraph

## Triple Click + Drag (Paragraph-wise Selection)
- [ ] After triple-click, drag extends selection by whole paragraphs
- [ ] Original paragraph always included

## Click Count Behavior
- [x] Consecutive clicks within 500ms increment click count
- [x] Clicks beyond triple-click maintain selection (don't deselect)
- [ ] Click count resets after timeout OR if mouse moves too far
- [ ] Consider increasing timeout to 700ms (browser standard)

## Keyboard Modifiers
- [x] Ctrl+C copies selection to clipboard
- [x] Ctrl+A selects all (currently just current text box)
- [ ] Ctrl+A should select all text in document
- [ ] Shift+Click extends selection from anchor to click point
- [ ] Shift+Arrow keys extend selection character by character
- [ ] Ctrl+Shift+Arrow extends selection word by word
- [ ] Shift+Home/End extends selection to line start/end
- [ ] Ctrl+Shift+Home/End extends selection to document start/end

## Selection Appearance
- [x] Blue highlight background on selected text
- [ ] Selection color should match system/CSS `::selection` pseudo-element
- [ ] Inactive window selection should have different color (gray)
- [ ] Cursor blink when selection is collapsed (caret)

## Special Content
- [ ] Numbers with decimals ("123.456" - select as one or split?)
- [ ] URLs (double-click may select path segments)
- [ ] Email addresses
- [ ] CamelCase words (most browsers: one word)
- [ ] snake_case words (most browsers: one word)
- [ ] Unicode characters (proper grapheme cluster handling)
- [ ] Emoji (👍 should select as one unit)
- [ ] Multi-codepoint emoji (👨‍👩‍👧 family emoji)
- [ ] Non-breaking spaces (&nbsp;) - word boundary or not?
- [ ] Zero-width characters

## Edge Cases
- [ ] Empty text nodes
- [ ] Single character words ("I", "a")
- [ ] Very long words without breaks
- [ ] Right-to-left text (RTL)
- [ ] Mixed LTR/RTL content
- [ ] Vertical text (future)

## Selection API (for JavaScript)
- [ ] window.getSelection() equivalent
- [ ] Selection events (onselect, onselectstart)
- [ ] Programmatic selection (select(), setSelectionRange())
- [ ] Selection.toString()

## Platform Considerations
- [ ] Windows: standard behavior
- [ ] macOS: Option+double-click variations
- [ ] Touch devices: long-press to select, handles to adjust

## Performance
- [ ] Efficient hit-testing for large documents
- [ ] Minimize repaints during selection
- [ ] Cache text measurements

## Accessibility
- [ ] Selection announced by screen readers
- [ ] High contrast mode support

## Mouse Button Variations
- [ ] Middle-click paste (Linux/X11 primary selection)
- [ ] Right-click context menu (should preserve selection)
- [ ] Right-click on selected text vs unselected text (different context menus)

## Selection Persistence
- [ ] Selection persists when window loses focus
- [ ] Selection persists when scrolling
- [ ] Selection clears when clicking elsewhere (but not on right-click)
- [ ] Selection clears when starting to type (in editable content)

## Scrolling Interaction
- [ ] Mouse wheel during selection drag should scroll AND extend selection
- [ ] Page Up/Down with Shift extends selection
- [ ] Scrolling doesn't change selection position

## Touch/Pointer Events
- [ ] Touch-and-hold shows magnifier (mobile)
- [ ] Selection handles (draggable endpoints on mobile)
- [ ] Stylus/pen input behavior
- [ ] Pointer events (unified mouse/touch/pen API)

## Undo/Redo
- [ ] Track selection changes in undo history (optional)
- [ ] Undo restores previous selection state

## Find/Search Integration
- [ ] Ctrl+F with selection pre-fills search box
- [ ] Find highlights vs selection highlights (different colors)
- [ ] "Find next" changes/moves selection
- [ ] Search match highlighting separate from selection

## Drag and Drop
- [ ] Dragging selected text initiates drag-and-drop
- [ ] Dropping text into selection replaces it
- [ ] Drag preview shows selected text
- [ ] Drag cursor feedback (copy vs move)
- [ ] Cancel drag with Escape

## Multiple Selections
- [ ] Ctrl+Click to add multiple selection ranges (advanced)
- [ ] Alt+Drag for column/block selection (text editors)
- [ ] Multiple carets/cursors

## Form Elements
- [ ] Input/textarea have their own selection model
- [ ] Select all in input only selects that input's content
- [ ] Tab focus moves between form elements
- [ ] Selection in disabled/readonly elements

## Contenteditable
- [ ] Selection in editable vs non-editable content
- [ ] Caret (blinking cursor) positioning when editable
- [ ] Caret movement with arrow keys
- [ ] Insert text at caret position

## Copy Behavior
- [ ] Copy includes formatting (rich text) by default
- [ ] Ctrl+Shift+C for plain text copy (some apps)
- [ ] Copy preserves newlines from line breaks and block elements
- [ ] Copy from multiple elements maintains structure
- [ ] Cut (Ctrl+X) in editable content

## Timing and Events
- [ ] Mousedown vs mouseup timing for selection finalization
- [ ] Debouncing rapid selection changes
- [ ] Selection change events fired appropriately
- [ ] Prevent default on certain events

## Visual Feedback / Cursors
- [ ] I-beam cursor over selectable text
- [ ] Arrow cursor elsewhere
- [ ] Cursor changes during drag operations
- [ ] Cursor hotspot position accuracy
- [ ] Custom cursor support (CSS cursor property)

## Selection Direction/Affinity
- [ ] Selection has a direction (forward/backward) affecting keyboard extension
- [ ] Caret affinity at line breaks (end of prev line or start of next?)
- [ ] Preserve selection direction when extending

## Text Boundaries / Special Characters
- [ ] Soft hyphens (­) - invisible until line break
- [ ] Word joiners / non-breaking hyphens
- [ ] Tab characters (selection width matches tab stop)
- [ ] Line break types (LF, CR, CRLF) handling
- [ ] Combining characters (diacritics)
- [ ] Ligatures (fi, fl) - select as individual chars or together?

## Bidi (Bidirectional) Text
- [ ] Logical vs visual cursor movement
- [ ] Selection in mixed LTR/RTL spans
- [ ] Bidi algorithm integration
- [ ] Direction attribute handling (dir="rtl")
- [ ] Selection highlight for RTL text

## CSS user-select Property
- [ ] `user-select: none` - prevents selection entirely
- [ ] `user-select: all` - single click selects entire element
- [ ] `user-select: text` - default behavior
- [ ] `user-select: contain` - selection can't extend outside element
- [ ] Inherited vs non-inherited behavior
- [ ] Selection skips `user-select: none` elements

## CSS pointer-events
- [ ] `pointer-events: none` - click passes through element
- [ ] Selection through transparent elements

## CSS ::selection Pseudo-element
- [ ] Custom selection background color
- [ ] Custom selection text color
- [ ] Per-element ::selection styles
- [ ] Inherited ::selection styles

## Overflow/Clipping
- [ ] Selection in `overflow: hidden` containers
- [ ] Selection in scrollable containers (nested scrolling)
- [ ] Selection across `position: fixed` elements
- [ ] Selection across `position: sticky` elements
- [ ] Selection visibility when text is clipped
- [ ] Selection extends into clipped/hidden areas

## CSS Transforms
- [ ] Hit-testing on `transform: rotate()` elements
- [ ] Hit-testing on `transform: scale()` elements
- [ ] Hit-testing on `transform: skew()` elements
- [ ] Selection highlight respects transforms
- [ ] Nested transforms

## Z-Index/Stacking Context
- [ ] Selection through overlapping elements
- [ ] Which element "wins" for selection when stacked
- [ ] Selection in different stacking contexts
- [ ] `isolation: isolate` behavior

## Shadow DOM (Future)
- [ ] Selection crossing shadow boundaries
- [ ] Slot content selection
- [ ] Closed vs open shadow root selection access

## Replaced/Special Elements
- [ ] Images in selection (copy includes image reference?)
- [ ] Iframes - selection doesn't cross iframe boundary
- [ ] Canvas text selection (if applicable)
- [ ] SVG text selection
- [ ] Video/audio elements in selection
- [ ] Object/embed elements

## CSS white-space Property
- [ ] `white-space: normal` - collapse whitespace
- [ ] `white-space: pre` - preserve all whitespace
- [ ] `white-space: nowrap` - selection on single long line
- [ ] `white-space: pre-wrap` - preserve but wrap
- [ ] `white-space: pre-line` - collapse spaces, preserve newlines
- [ ] `white-space: break-spaces` behavior

## Text Truncation
- [ ] `text-overflow: ellipsis` - selection of truncated text
- [ ] `-webkit-line-clamp` - multi-line truncation
- [ ] Selection of text hidden by ellipsis
- [ ] Copy behavior for truncated text

## Floats and Positioned Elements
- [ ] Selection around floated elements
- [ ] Text flow around floats
- [ ] Selection in absolutely positioned elements
- [ ] Selection across float boundaries

## Printing
- [ ] Selection state when printing (usually hidden)
- [ ] Print selection only feature

## CSS Text Properties Effects
- [ ] `word-break` property effects on word selection
- [ ] `overflow-wrap` / `word-wrap` effects
- [ ] `hyphens` property (auto-hyphenation)
- [ ] `letter-spacing` - affects hit-testing positions
- [ ] `word-spacing` - affects word boundaries
- [ ] `text-indent` - first line indentation
- [ ] `line-height` - affects selection box height
- [ ] `vertical-align` - affects text baseline/positioning
- [ ] Font kerning effects on character positions

## CSS Text Decoration in Selection
- [ ] `text-decoration: underline` visibility in selection
- [ ] `text-decoration: line-through` in selection
- [ ] `text-decoration: overline` in selection
- [ ] `text-shadow` - hidden or visible during selection?

## Opacity and Visibility
- [ ] Selection on `opacity: 0.5` text
- [ ] `visibility: hidden` - no selection (but takes space)
- [ ] `display: none` - not in selection flow
- [ ] `color: transparent` text selection

## CSS Multi-Column Layout
- [ ] Selection across column breaks
- [ ] Selection spanning multiple columns
- [ ] Column-gap handling
- [ ] Column-rule interaction

## Writing Modes
- [ ] `writing-mode: vertical-rl` (vertical right-to-left)
- [ ] `writing-mode: vertical-lr` (vertical left-to-right)
- [ ] `text-orientation` property
- [ ] Sideways text selection
- [ ] Mixed horizontal/vertical content

## Ruby Annotations (CJK)
- [ ] Ruby text selection
- [ ] Ruby base selection
- [ ] Combined ruby + base selection

## Pseudo-Element Selection
- [ ] `::first-letter` pseudo-element selection
- [ ] `::first-line` pseudo-element selection
- [ ] `::before` generated content selection (usually not selectable)
- [ ] `::after` generated content selection (usually not selectable)
- [ ] `::marker` (list bullets) selection

## Focus Management
- [ ] Focus vs selection distinction
- [ ] `tabindex` affects focusability
- [ ] Focus ring vs selection highlight
- [ ] Blur event clears selection? (depends on context)
- [ ] Programmatic focus with selection

## Keyboard Navigation (Non-Selection)
- [ ] Arrow keys move caret (when editable)
- [ ] Home/End move to line start/end
- [ ] Ctrl+Home/End move to document start/end
- [ ] Page Up/Down scrolls (vs Shift+Page for selection)

## IME (Input Method Editor)
- [ ] Composition mode (for CJK input)
- [ ] Composition string highlighting
- [ ] Selection during IME composition
- [ ] Candidate window interaction

## Spell Check / Grammar
- [ ] Spell check underline interaction with selection
- [ ] Grammar check underline
- [ ] Right-click on misspelled word context menu
- [ ] Replace misspelled word clears selection

## Modern Web APIs
- [ ] CSS Custom Highlight API (Highlight Registry)
- [ ] Text Fragments (`#:~:text=` URL highlighting)
- [ ] Annotation/highlight ranges (separate from selection)

## Clipboard APIs
- [ ] Async Clipboard API (`navigator.clipboard`)
- [ ] Clipboard formats (text/plain, text/html, text/rtf)
- [ ] ClipboardItem for multiple formats
- [ ] Paste event handling
- [ ] Clipboard permissions

## DOM Range API
- [ ] `document.createRange()` equivalent
- [ ] Range objects (startContainer, endContainer, etc.)
- [ ] `Selection.getRangeAt()` method
- [ ] `Selection.addRange()` for multiple ranges
- [ ] Range.getBoundingClientRect() for selection bounds
- [ ] Collapsed range = caret position

## Selection State
- [ ] Anchor node vs focus node distinction
- [ ] Selection.anchorOffset, focusOffset
- [ ] Selection.isCollapsed (caret, no highlight)
- [ ] Selection.type ("None", "Caret", "Range")
- [ ] Selection containment queries (is element fully selected?)

## Selection Events (JavaScript)
- [ ] `selectstart` event (cancelable)
- [ ] `selectionchange` event
- [ ] Event firing timing and frequency
- [ ] Event bubbling behavior

## DOM Mutation During Selection
- [ ] Selection when selected text is deleted
- [ ] Selection when DOM node is removed
- [ ] Selection when text is inserted
- [ ] Selection normalization after mutations

## Selection Serialization
- [ ] Save selection state (for undo/history)
- [ ] Restore selection state
- [ ] Selection across page reload (session storage?)
- [ ] Selection in browser history navigation

## Selection Constraints
- [ ] Minimum selection size (prevent accidental micro-select)
- [ ] Maximum selection limit (some apps)
- [ ] Selection throttling for performance

## Tab/Window Behavior
- [ ] Selection persists when tab loses focus
- [ ] Selection state when returning to tab
- [ ] Selection across browser windows
- [ ] Selection in popup windows

## Table Selection
- [ ] Selection within a single table cell
- [ ] Selection across table cells
- [ ] Selection across table rows
- [ ] Selection across table columns
- [ ] Table cell selection mode (click selects cell)
- [ ] Copy table structure (maintains grid in paste)
- [ ] Selection across nested tables
- [ ] Selection with colspan/rowspan cells

## List Selection
- [ ] Selection of list item markers (bullets/numbers)
- [ ] Selection across list items
- [ ] Selection in nested lists
- [ ] Selection in definition lists (dl, dt, dd)
- [ ] Ordered list number copying

## HTML5 Semantic Elements
- [ ] Selection in <details>/<summary> elements
- [ ] Selection when details is collapsed vs expanded
- [ ] Selection in <figure>/<figcaption>
- [ ] Selection in <blockquote>
- [ ] Selection in <aside>
- [ ] Selection in <article>/<section>/<nav>
- [ ] Selection in <mark> (highlighted text)
- [ ] Selection in <time>, <address>, <cite>

## Interactive Elements
- [ ] Selection behavior on links (<a href>)
- [ ] Click vs drag differentiation on links
- [ ] Selection in <button> element text
- [ ] Selection in <label> elements
- [ ] Selection affecting label-input association

## Code/Preformatted Content
- [ ] Selection in <pre> (preserves whitespace)
- [ ] Selection in <code> elements
- [ ] Selection in <samp>, <kbd>, <var>
- [ ] Monospace font character width consistency
- [ ] Tab character width in pre/code

## CSS Grid Layout
- [ ] Selection across grid items
- [ ] Selection with grid gaps
- [ ] Selection with grid-area spanning
- [ ] Selection in overlapping grid items
- [ ] Implicit vs explicit grid selection

## Dynamic Content
- [ ] Selection during CSS animations
- [ ] Selection during CSS transitions
- [ ] Selection during JavaScript animations (requestAnimationFrame)
- [ ] Selection anchor persistence across relayouts
- [ ] Selection during resize events

## Viewport/Zoom
- [ ] Selection at different browser zoom levels
- [ ] Pinch-to-zoom effects on selection
- [ ] Responsive layout changes during selection
- [ ] Selection when viewport size changes

## Font Loading
- [ ] Selection during font loading (FOUT - Flash of Unstyled Text)
- [ ] Selection recalculation after font swap
- [ ] `font-display` property effects
- [ ] Variable font axis changes

## High DPI / Display
- [ ] Subpixel rendering effects on selection bounds
- [ ] Retina/HiDPI display selection accuracy
- [ ] Selection at fractional pixel positions
- [ ] Device pixel ratio considerations

## Virtual/Lazy Content
- [ ] Selection in virtual scrolling lists
- [ ] Selection with lazy-loaded content
- [ ] Selection in infinite scroll
- [ ] Selection across paginated content
- [ ] Windowed/virtualized list selection

## Sticky/Fixed During Selection
- [ ] Selection across sticky headers
- [ ] Selection with sticky columns
- [ ] Fixed position elements during scroll-selection

## Framework Considerations
- [ ] Selection during DOM reconciliation (React/Vue/Angular)
- [ ] Selection in virtual DOM updates
- [ ] Selection in web components
- [ ] Selection with client-side hydration

## Browser Extension Interference
- [ ] Selection modified by extensions
- [ ] Highlighting extensions (Hypothesis, etc.)
- [ ] Translation extensions modifying text
- [ ] Ad blockers removing selected content

## Inline Element Boundaries
- [ ] Selection crossing <span> boundaries
- [ ] Selection crossing <a> boundaries
- [ ] Selection crossing <strong>/<em> boundaries
- [ ] Selection crossing nested inline elements
- [ ] Inline-block boundary handling

## Editable Variations
- [ ] contenteditable="true" selection
- [ ] contenteditable="plaintext-only"
- [ ] designMode="on" (document-level editing)
- [ ] Mixed editable/non-editable content
- [ ] Selection entering/leaving editable regions

## MathML/Scientific Content
- [ ] MathML formula selection
- [ ] Selection of math operators/symbols
- [ ] Selection across math fractions
- [ ] Scientific notation selection

## Internationalization (i18n)
- [ ] Selection in languages with no word boundaries (Chinese, Japanese)
- [ ] Thai/Khmer text (no spaces between words)
- [ ] ICU word break rules
- [ ] Locale-specific word selection
- [ ] Right-to-left numeral handling

## Testing/QA
- [ ] Selection state in automated tests
- [ ] Selection screenshot comparisons
- [ ] Selection in headless browser mode
- [ ] Selection timing in e2e tests

## Security/Privacy
- [ ] Selection in password fields (shows dots/asterisks, not actual text)
- [ ] Selection leaking sensitive information
- [ ] Cross-origin selection restrictions
- [ ] Selection in sandboxed iframes
- [ ] Clipboard security (sensitive data)

## Cross-Origin Iframes
- [ ] Selection doesn't cross cross-origin iframe boundaries
- [ ] Same-origin iframe selection can cross
- [ ] postMessage for selection coordination
- [ ] Iframe sandbox attribute effects

## Fullscreen Mode
- [ ] Selection in fullscreen elements
- [ ] Exit fullscreen during selection drag
- [ ] Fullscreen API interaction with selection

## CSS Filters
- [ ] Selection on `filter: blur()` text
- [ ] Selection on `filter: opacity()` text
- [ ] `backdrop-filter` effects on selection highlight

## CSS Masks and Clip-path
- [ ] Selection on `clip-path` clipped text
- [ ] Selection on masked content (`mask-image`)
- [ ] Hit-testing through clipped areas

## CSS mix-blend-mode
- [ ] Selection highlight blending
- [ ] Inverted text selection visibility
- [ ] Blend mode effect on selection color

## CSS Shapes
- [ ] Selection in text wrapping around `shape-outside`
- [ ] Selection with `shape-inside` (future)
- [ ] Float shape selection boundaries

## CSS Containment
- [ ] `contain: content` effects on selection
- [ ] `contain: layout` effects
- [ ] `contain: paint` - selection at boundaries
- [ ] `content-visibility: auto` lazy rendering

## CSS Scroll Snap
- [ ] Scroll snap during selection drag
- [ ] Snap points affecting selection scrolling
- [ ] `scroll-snap-stop: always` behavior

## Typography Features
- [ ] `font-variant: small-caps` selection
- [ ] `text-transform: uppercase` selection (copies original or transformed?)
- [ ] `font-variant-numeric` effects
- [ ] OpenType features selection

## Quotation Marks
- [ ] Smart quotes ("curly") selection
- [ ] Straight quotes selection  
- [ ] `quotes` CSS property effects
- [ ] Quotation marks as word boundaries

## Browser-Specific Behaviors
- [ ] Chrome/Chromium-specific selection quirks
- [ ] Firefox-specific selection behavior
- [ ] Safari/WebKit-specific behavior
- [ ] Edge-specific behavior
- [ ] Browser engine differences documentation

## Reader Mode / Simplified View
- [ ] Selection in browser reader mode
- [ ] Simplified DOM selection
- [ ] Reader mode text extraction

## PDF/Embedded Documents
- [ ] Selection in embedded PDF viewers
- [ ] PDF.js selection integration
- [ ] Selection in `<object>` PDF embeds

## Collaboration/Real-time
- [ ] Multi-user selection visibility (Google Docs style)
- [ ] Collaborative cursor display
- [ ] Real-time selection synchronization
- [ ] Conflict resolution for overlapping selections
- [ ] Selection presence indicators

## Voice Control
- [ ] Voice-commanded selection ("select paragraph")
- [ ] Dictation mode selection
- [ ] Voice accessibility integration
- [ ] Speech-to-text selection boundaries

## Assistive Technology
- [ ] Screen reader selection announcements
- [ ] Braille display selection indication
- [ ] Switch access selection
- [ ] Eye-tracking selection (experimental)
- [ ] Head tracking selection

## Selection Analytics
- [ ] Track what users select (opt-in)
- [ ] Selection heatmaps
- [ ] Popular selection patterns

## Selection History
- [ ] Recent selections list
- [ ] Selection bookmarks/favorites
- [ ] Restore previous selection

## Offline/Service Worker
- [ ] Selection on cached content
- [ ] Selection during offline mode
- [ ] Service worker interception effects

## Memory Management
- [ ] Selection object memory footprint
- [ ] Garbage collection of selection ranges
- [ ] Memory leaks from orphaned selections
- [ ] Large selection performance

## Error Handling
- [ ] Selection when DOM operations throw
- [ ] Graceful degradation on errors
- [ ] Selection recovery after crashes
- [ ] Invalid range handling

## DevTools Integration
- [ ] Inspect selection in DevTools
- [ ] Selection debugging APIs
- [ ] Console access to selection state
- [ ] Selection event logging

## Screen Capture
- [ ] Selection visibility in screenshots
- [ ] Selection during screen recording
- [ ] Selection in picture-in-picture

## CSS Anchor Positioning (New)
- [ ] Selection on anchor-positioned elements
- [ ] Selection across anchor boundaries
- [ ] Popover selection behavior

## CSS View Transitions
- [ ] Selection during view transitions
- [ ] Selection state preservation across transitions
- [ ] Animation of selection changes

## Scroll-Driven Animations
- [ ] Selection during scroll-driven animations
- [ ] Selection position with animated elements

## Container Queries
- [ ] Selection recalculation on container resize
- [ ] Container query breakpoint selection changes

## Subgrid
- [ ] Selection in CSS subgrid items
- [ ] Subgrid boundary selection

## has() Selector Effects
- [ ] Selection triggering `:has()` styles
- [ ] Style changes during selection via `:has(:selected)`

## Layers (@layer)
- [ ] Selection styles in cascade layers
- [ ] Layer priority for ::selection

## Scope (@scope)
- [ ] Scoped selection styles
- [ ] Selection crossing scope boundaries

## HTML Inline Elements
- [ ] Selection with `<br>` elements (line breaks)
- [ ] Selection across `<wbr>` (word break opportunity)
- [ ] Selection in `<ins>` (inserted text)
- [ ] Selection in `<del>` (deleted text)
- [ ] Selection in `<abbr>` and `<dfn>` elements
- [ ] Selection with `<sub>` and `<sup>` (subscript/superscript)
- [ ] Selection in `<bdo>` (bidirectional override)
- [ ] Selection in `<bdi>` (bidirectional isolate)
- [ ] Selection in `<small>` elements
- [ ] Selection in `<s>` (strikethrough)
- [ ] Selection in `<u>` (underline)
- [ ] Selection in `<q>` (inline quotation)

## Special HTML Attributes
- [ ] `inert` attribute prevents selection entirely
- [ ] `hidden` attribute - element not selectable
- [ ] `draggable` attribute interaction with selection
- [ ] `spellcheck` attribute visual effects
- [ ] `translate` attribute (translation tools)
- [ ] `lang` attribute effects on word boundaries
- [ ] `dir` attribute (rtl/ltr/auto)

## CSS unicode-bidi
- [ ] `unicode-bidi: normal`
- [ ] `unicode-bidi: embed`
- [ ] `unicode-bidi: isolate`
- [ ] `unicode-bidi: bidi-override`
- [ ] `unicode-bidi: isolate-override`
- [ ] `unicode-bidi: plaintext`

## CSS Initial Letter
- [ ] `initial-letter` drop cap selection
- [ ] Selection boundary at initial letter
- [ ] Copy behavior with initial letter

## Form Element Selection Details
- [ ] `::placeholder` pseudo-element (not selectable)
- [ ] Selection in `<meter>` elements
- [ ] Selection in `<progress>` elements
- [ ] Selection in `<output>` elements
- [ ] Selection in `<datalist>` options
- [ ] Selection in `<optgroup>` labels

## Template and Slots
- [ ] Selection in `<template>` content (not rendered)
- [ ] Selection across `<slot>` elements
- [ ] Selection with slot fallback content
- [ ] Selection in DocumentFragment

## Observer Callbacks
- [ ] Selection during IntersectionObserver callbacks
- [ ] Selection during ResizeObserver callbacks
- [ ] Selection during MutationObserver callbacks
- [ ] Observer-triggered layout affecting selection

## Performance APIs
- [ ] Selection affecting Largest Contentful Paint (LCP)
- [ ] Selection affecting Cumulative Layout Shift (CLS)
- [ ] Selection affecting First Input Delay (FID)
- [ ] Selection timing in Performance Timeline
- [ ] `requestIdleCallback` selection operations

## Navigation Events
- [ ] Selection during `beforeunload` event
- [ ] Selection state on `pagehide`
- [ ] Selection restoration on `pageshow` (bfcache)
- [ ] Selection during same-document navigation
- [ ] Selection across fragment navigations (#anchor)

## SVG-Specific
- [ ] Selection in SVG `<text>` elements
- [ ] Selection in SVG `<tspan>` elements
- [ ] Selection in SVG `<textPath>`
- [ ] Selection in `<foreignObject>` content
- [ ] SVG transform effects on text selection

## Media Queries Selection Effects
- [ ] `prefers-color-scheme` (dark/light mode selection colors)
- [ ] `forced-colors` mode selection
- [ ] `prefers-contrast` selection visibility
- [ ] `prefers-reduced-motion` selection animations
- [ ] `prefers-reduced-transparency` effects

## Quadruple+ Click
- [ ] Quadruple-click behavior (some apps select all)
- [ ] Click count beyond 4 handling
- [ ] Customizable click count actions

## JavaScript Disabled
- [ ] Selection with JavaScript disabled
- [ ] Selection in `<noscript>` content
- [ ] Progressive enhancement of selection

## Range API Advanced
- [ ] `Range.extractContents()` behavior
- [ ] `Range.cloneContents()` behavior
- [ ] `Range.deleteContents()` effects
- [ ] `Range.surroundContents()` with selection
- [ ] `Range.compareBoundaryPoints()`

## Resource Loading
- [ ] Selection during image loading
- [ ] Selection during stylesheet loading
- [ ] Selection during script execution
- [ ] Selection with slow network
- [ ] Selection under CPU throttling

## Mobile-Specific
- [ ] Selection feedback haptics
- [ ] Selection magnifier loupe
- [ ] Selection callout menu (copy/paste/share)
- [ ] Smart text selection (addresses, phone numbers)
- [ ] Share sheet integration with selection

## Notification/Feedback
- [ ] Visual feedback for copy success
- [ ] Selection toast notifications
- [ ] Sound feedback for selection (optional)

## CSS text-combine-upright
- [ ] Tate-chū-yoko (horizontal in vertical) selection
- [ ] Combined character selection

## Data Attributes
- [ ] Selection with `data-*` attributes (no effect)
- [ ] Custom selection behavior via data attributes
- [ ] Dataset in selection events

## Memory and Lifecycle
- [ ] Selection object lifecycle
- [ ] Selection during page freeze (background tab)
- [ ] Selection memory pressure handling
- [ ] Large selection memory optimization

## Web Vitals
- [ ] Selection impact on Core Web Vitals
- [ ] Selection performance budgeting
- [ ] Selection render timing

## Pointer Capture
- [ ] `setPointerCapture()` during selection drag
- [ ] `releasePointerCapture()` on selection end
- [ ] Pointer capture lost during selection
- [ ] Cross-element pointer tracking

## Composition Events (IME Advanced)
- [ ] `compositionstart` during selection
- [ ] `compositionupdate` with active selection
- [ ] `compositionend` replacing selection
- [ ] Composition preview replacing selection

## Input Events
- [ ] `beforeinput` event with selection
- [ ] `input` event after selection-based edit
- [ ] `inputType` values for selection operations
- [ ] `getTargetRanges()` for selection

## execCommand API (Legacy)
- [ ] `document.execCommand('copy')`
- [ ] `document.execCommand('cut')`
- [ ] `document.execCommand('selectAll')`
- [ ] execCommand deprecation handling

## Selection Geometry
- [ ] `getClientRects()` for selection ranges
- [ ] `getBoundingClientRect()` for selection
- [ ] Selection coordinates: viewport vs document
- [ ] Selection rect during scrolling

## Character Encoding
- [ ] UTF-8 selection offset handling
- [ ] UTF-16 surrogate pair selection
- [ ] Multi-byte character boundaries
- [ ] BOM (Byte Order Mark) handling
- [ ] Selection in mixed encodings

## Document Modes
- [ ] Quirks mode selection behavior
- [ ] Standards mode selection behavior
- [ ] Almost-standards mode
- [ ] Selection in XHTML documents
- [ ] Selection in XML documents

## Dynamic Document Creation
- [ ] Selection after `document.write()`
- [ ] Selection in dynamically created iframes
- [ ] Selection in `about:blank` frames
- [ ] Selection in `data:` URL documents
- [ ] Selection in `blob:` URL documents
- [ ] Selection after `document.open()`/`close()`

## Security Contexts
- [ ] Selection with Content Security Policy (CSP)
- [ ] Selection in Trusted Types context
- [ ] Selection sanitization requirements
- [ ] Selection in sandboxed contexts
- [ ] Cross-origin isolation effects

## Canvas/WebGL Text
- [ ] Selection in Canvas 2D `fillText()`/`strokeText()`
- [ ] Selection in WebGL rendered text
- [ ] Selection in OffscreenCanvas text
- [ ] Custom text selection implementation for canvas

## TreeWalker/NodeIterator
- [ ] Selection with TreeWalker traversal
- [ ] Selection with NodeIterator
- [ ] Selection across filtered nodes
- [ ] Selection when filter rejects nodes

## Malformed Content
- [ ] Selection in malformed HTML
- [ ] Selection with unclosed tags
- [ ] Selection with invalid nesting
- [ ] Selection in recovered parse errors

## DOM Normalization
- [ ] Selection after `Node.normalize()`
- [ ] Selection across adjacent text nodes
- [ ] Selection with empty text nodes
- [ ] Text node splitting effects on selection

## Stylus/Pen Advanced
- [ ] Stylus pressure affecting selection
- [ ] Stylus tilt during selection
- [ ] Stylus barrel button for selection mode
- [ ] Palm rejection during selection

## Touch Advanced
- [ ] Selection gesture disambiguation (tap vs hold)
- [ ] Selection confidence/precision on touch
- [ ] Multi-touch selection interference
- [ ] Gesture conflict resolution

## WCAG/Accessibility Compliance
- [ ] Selection color contrast (WCAG AA/AAA)
- [ ] Minimum selection touch target size (44x44px)
- [ ] Selection visible in all color modes
- [ ] Selection timing for motor impairments

## State Machine
- [ ] Selection state transitions
- [ ] Selection state validation
- [ ] Selection invariant checking
- [ ] Invalid state recovery

## Print Media
- [ ] Selection in `@media print`
- [ ] Selection in `@page` rules
- [ ] Selection in print preview
- [ ] Selection in paged media

## Resize/Reflow
- [ ] Selection during window resize
- [ ] Selection during font resize
- [ ] Selection during container resize
- [ ] Selection anchor adjustment on reflow

## Event Timing
- [ ] Selection event ordering guarantees
- [ ] Microtask timing with selection
- [ ] Selection in Promise callbacks
- [ ] Selection in queueMicrotask

## Deprecated/Legacy Features
- [ ] Selection in `<marquee>` (moving text)
- [ ] Selection in `<blink>`
- [ ] Selection in `<font>` elements
- [ ] Selection in `<center>` elements

## Experimental Web Features
- [ ] Selection in `<portal>` elements (future)
- [ ] Selection in Fenced Frames (future)
- [ ] Selection with Speculation Rules
- [ ] Selection in Shared Element Transitions

## Clipboard Edge Cases
- [ ] Clipboard write failure handling
- [ ] Clipboard permission denied
- [ ] Empty clipboard operations
- [ ] Clipboard size limits

## Selection Coalescing
- [ ] Merge adjacent selection ranges
- [ ] Split selection ranges
- [ ] Selection range optimization
- [ ] Selection deduplication

## Internationalization Advanced
- [ ] Selection in Indic scripts (complex shaping)
- [ ] Selection in Arabic (contextual forms)
- [ ] Selection in Hebrew
- [ ] Selection in Devanagari
- [ ] Selection with combining marks (multiple)
- [ ] Selection in Hangul (Korean syllable blocks)

## Scrollbar Interaction
- [ ] Selection near scrollbar
- [ ] Selection drag over scrollbar
- [ ] Scrollbar clicks during selection
- [ ] Custom scrollbar interaction

## Debug/Development
- [ ] Selection breakpoints in debugger
- [ ] Selection performance profiling
- [ ] Selection memory profiling
- [ ] Selection event tracing

## Web APIs Integration
- [ ] Selection with Web Locks API
- [ ] Selection with AbortController/AbortSignal
- [ ] Selection with Background Sync
- [ ] Selection with Background Fetch
- [ ] Selection with Push API
- [ ] Selection with Notifications API text
- [ ] Selection with Badging API
- [ ] Selection with Screen Wake Lock active
- [ ] Selection with Idle Detection

## WebXR/Immersive
- [ ] Selection in WebXR text overlays
- [ ] Selection in VR browser interfaces
- [ ] Selection in AR content
- [ ] Hand tracking selection gestures
- [ ] Gaze-based selection (eye tracking)
- [ ] Controller-based text selection

## PWA/App Context
- [ ] Selection in PWA standalone mode
- [ ] Selection in PWA minimal-ui mode
- [ ] Selection in TWA (Trusted Web Activity)
- [ ] Selection with Display Override
- [ ] Selection in Window Controls Overlay

## Embedded Contexts
- [ ] Selection in Electron webviews
- [ ] Selection in WebView2 (Windows)
- [ ] Selection in CEF (Chromium Embedded Framework)
- [ ] Selection in Tauri webviews
- [ ] Selection in React Native WebView
- [ ] Selection in Capacitor
- [ ] Selection in Cordova/PhoneGap

## Mini-App Platforms
- [ ] Selection in WeChat Mini Programs
- [ ] Selection in Alipay Mini Programs
- [ ] Selection in ByteDance Mini Apps
- [ ] Selection in Quick Apps (Huawei)

## Remote/Streaming
- [ ] Selection in remote desktop scenarios
- [ ] Selection over high-latency networks
- [ ] Selection in screen sharing
- [ ] Selection in cloud gaming UI
- [ ] Selection synchronization across devices

## File System APIs
- [ ] Selection with File System Access API
- [ ] Selection in Origin Private File System
- [ ] Selection with drag-drop file names
- [ ] Selection in file picker dialogs

## Hardware/Device APIs
- [ ] Selection with WebHID device labels
- [ ] Selection with WebUSB device names
- [ ] Selection with WebBluetooth device names
- [ ] Selection with Web Serial labels
- [ ] Selection with WebMIDI device names
- [ ] Selection with Gamepad button text

## Media APIs Text
- [ ] Selection in Media Session metadata
- [ ] Selection in WebVTT captions/subtitles
- [ ] Selection in chapter titles
- [ ] Selection in audio track labels
- [ ] Selection in video track labels

## Streaming/Encoding
- [ ] Selection with Compression Streams
- [ ] Selection with Encoding API (TextEncoder/Decoder)
- [ ] Selection with Streams API
- [ ] Selection in TransformStream

## Concurrency/Threading
- [ ] Selection state with SharedArrayBuffer
- [ ] Selection with Atomics
- [ ] Selection message passing to Workers
- [ ] Selection in Worklets (Paint, Audio, Animation)

## Module/Import Context
- [ ] Selection in Import Maps context
- [ ] Selection in ES Module scripts
- [ ] Selection in dynamic imports
- [ ] Selection in Module Workers

## Weak References
- [ ] Selection object with WeakRef
- [ ] Selection with FinalizationRegistry
- [ ] Selection garbage collection timing
- [ ] Weak selection caching

## Feature Detection
- [ ] Selection with CSS `@supports`
- [ ] Selection feature flags
- [ ] Selection A/B testing
- [ ] Selection progressive enhancement

## Input Device Advanced
- [ ] Mouse acceleration effects on selection
- [ ] Trackpad gesture selection
- [ ] Selection with external keyboards
- [ ] Selection with on-screen keyboards
- [ ] Selection with hardware keyboard layouts

## Picture-in-Picture Advanced
- [ ] Selection in Document Picture-in-Picture
- [ ] Selection sync between PiP and main
- [ ] Selection in video overlay controls

## Share/Export
- [ ] Selection with Navigator.share()
- [ ] Selection with Web Share Target
- [ ] Selection to native share sheet
- [ ] Selection export formats

## Protocol/Handler
- [ ] Selection with Protocol Handlers
- [ ] Selection with Launch Handler
- [ ] Selection with File Handling API
- [ ] Selection in custom URL schemes

## Telemetry/Analytics
- [ ] Selection metrics collection
- [ ] Selection telemetry export
- [ ] Selection heatmap generation
- [ ] Selection pattern analysis

## Testing Infrastructure
- [ ] Selection assertions/matchers
- [ ] Selection fixtures
- [ ] Selection mocking
- [ ] Selection snapshot testing
- [ ] Selection visual regression tests

## Documentation/Specification
- [ ] Selection behavior documentation
- [ ] Selection edge case matrix
- [ ] Selection decision tree
- [ ] Selection state diagram
- [ ] Selection API reference

## Keyboard Layouts
- [ ] Selection with QWERTY layout
- [ ] Selection with AZERTY layout
- [ ] Selection with QWERTZ layout
- [ ] Selection with Dvorak layout
- [ ] Selection with Colemak layout
- [ ] Selection with non-Latin keyboard layouts

## Text Rendering Engine
- [ ] Selection with HarfBuzz shaping
- [ ] Selection with FreeType rendering
- [ ] Selection with DirectWrite (Windows)
- [ ] Selection with CoreText (macOS)
- [ ] Selection with Skia text

## GPU Text Rendering
- [ ] Selection with WebGPU text
- [ ] Selection in GPU-accelerated canvas
- [ ] Selection with SDF (Signed Distance Field) text
- [ ] Selection in MSDF text rendering

---

## Currently Implemented
1. ✅ Single click selection with nearest-text fallback
2. ✅ Drag selection (character-wise)
3. ✅ Cross-element selection
4. ✅ Double-click word selection (basic)
5. ✅ Triple-click line selection
6. ✅ Click count tracking (with persistence after triple)
7. ✅ Ctrl+C copy
8. ✅ Ctrl+A select all (single element)
9. ✅ Selection highlight rendering
10. ✅ Flexbox text selection fix (horizontal bounds)

## Priority Next Steps
1. Double-click + drag word-wise selection
2. Triple-click selects paragraph (not just visual line)
3. Shift+Click extend selection
4. Apostrophe handling in words
5. Ctrl+A select entire document
