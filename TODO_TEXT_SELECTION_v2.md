# Text Selection - Realistic Implementation Checklist

## Single Click
- [x] Click on text starts selection at character position
- [x] Click on empty space finds nearest text box
- [x] Click position determines anchor position
- [x] Shift+Click extends selection from anchor to click point

## Drag Selection
- [x] Drag selects character by character
- [x] Selection updates in real-time during drag
- [x] Cross-element selection works
- [x] Selection works in flexbox layouts
- [ ] Auto-scroll when dragging near edges

## Double Click
- [x] Double-click selects word under cursor
- [x] Word boundaries: whitespace and punctuation
- [x] Apostrophe within word NOT a boundary ("don't" = one word)
- [ ] Double-click on whitespace selects nothing or whitespace run
- [x] Double-click + drag extends by whole words

## Triple Click
- [x] Triple-click selects line
- [ ] Triple-click should select entire paragraph/block element
- [x] Triple-click + drag extends by lines

## Click Count
- [x] Consecutive clicks within timeout increment count
- [x] Clicks after triple-click maintain selection
- [x] Click count resets if mouse moves too far

## Keyboard
- [x] Ctrl+C copies selection
- [x] Ctrl+A selects all text in document
- [x] Shift+Arrow extends selection character by character
- [x] Ctrl+Shift+Arrow extends selection word by word

## Selection Appearance
- [x] Blue highlight on selected text
- [x] I-beam cursor over text
- [ ] Selection color from ::selection CSS

## Copy Behavior
- [x] Copy preserves newlines from block elements
- [x] Copy plain text (not rich text for now)

## CSS Properties
- [x] `user-select: none` prevents selection
- [x] `user-select: all` selects entire element on click

## Edge Cases
- [ ] Selection in scrollable containers
- [ ] Selection across absolutely positioned elements
- [ ] Empty text nodes handling

---

## Currently Implemented
1. ✅ Single click selection with nearest-text fallback
2. ✅ Drag selection (character-wise)
3. ✅ Cross-element selection with proper ordering
4. ✅ Double-click word selection (with apostrophe handling)
5. ✅ Triple-click line selection
6. ✅ Click count tracking (resets on distance)
7. ✅ Ctrl+C copy (preserves newlines between blocks)
8. ✅ Ctrl+A select entire document
9. ✅ Selection highlight rendering
10. ✅ Flexbox text selection
11. ✅ Shift+Click extends selection
12. ✅ Double-click + drag word-wise selection
13. ✅ Triple-click + drag line-wise selection
14. ✅ I-beam cursor over text
15. ✅ Shift+Arrow character selection
16. ✅ Ctrl+Shift+Arrow word selection
17. ✅ user-select: none/all CSS

## Remaining Items
1. Auto-scroll when dragging near edges
2. Double-click on whitespace behavior
3. Triple-click selects paragraph (not just line)
4. ::selection CSS pseudo-element
5. Selection in scrollable containers
6. Absolutely positioned element selection
7. Empty text node handling
