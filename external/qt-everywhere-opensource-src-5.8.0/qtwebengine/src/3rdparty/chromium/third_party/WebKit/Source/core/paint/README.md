# `Source/core/paint`

This directory contains implementation of painters of layout objects. It covers
the following document lifecycle states:

*   PaintInvalidation (`InPaintInvalidation` and `PaintInvalidationClean`)
*   PrePaint (`InPrePaint` and `PrePaintClean`)
*   Paint (`InPaint` and `PaintClean`)

## Glossaries

### Stacked elements and stacking contexts

This chapter is basically a clarification of [CSS 2.1 appendix E. Elaborate description
of Stacking Contexts](http://www.w3.org/TR/CSS21/zindex.html).

Note: we use 'element' instead of 'object' in this chapter to keep consistency with
the spec. We use 'object' in other places in this document.

According to the documentation, we can have the following types of elements that are
treated in different ways during painting:

*   Stacked objects: objects that are z-ordered in stacking contexts, including:

    *   Stacking contexts: elements with non-auto z-indices.

    *   Elements that are not real stacking contexts but are treated as stacking
        contexts but don't manage other stacked elements. Their z-ordering are
        managed by real stacking contexts. They are positioned elements with
        `z-index: auto` (E.2.8 in the documentation).

        They must be managed by the enclosing stacking context as stacked elements
        because `z-index:auto` and `z-index:0` are considered equal for stacking
        context sorting and they may interleave by DOM order.

        The difference of a stacked element of this type from a real stacking context
        is that it doesn't manage z-ordering of stacked descendants. These descendants
        are managed by the parent stacking context of this stacked element.

    "Stacked element" is not defined as a formal term in the documentation, but we found
    it convenient to use this term to refer to any elements participating z-index ordering
    in stacking contexts.

    A stacked element is represented by a `PaintLayerStackingNode` associated with a
    `PaintLayer`. It's painted as self-painting `PaintLayer`s by `PaintLayerPainter`
    by executing all of the steps of the painting algorithm explained in the documentation
    for the element. When painting a stacked element of the second type, we don't
    paint its stacked descendants which are managed by the parent stacking context.

*   Non-stacked pseudo stacking contexts: elements that are not stacked, but paint
    their descendants (excluding any stacked contents) as if they created stacking
    contexts. This includes

    *   inline blocks, inline tables, inline-level replaced elements
        (E.2.7.2.1.4 in the documentation)
    *   non-positioned floating elements (E.2.5 in the documentation)
    *   [flex items](http://www.w3.org/TR/css-flexbox-1/#painting)
    *   [grid items](http://www.w3.org/TR/css-grid-1/#z-order)
    *   custom scrollbar parts

    They are painted by `ObjectPainter::paintAllPhasesAtomically()` which executes
    all of the steps of the painting algorithm explained in the documentation, except
    ignores any descendants which are positioned or have non-auto z-index (which is
    achieved by skipping descendants with self-painting layers).

*   Other normal elements.

### Other glossaries

*   Paint container: the parent of an object for painting, as defined by [CSS2.1 spec
    for painting]((http://www.w3.org/TR/CSS21/zindex.html)). For regular objects,
    this is the parent in the DOM. For stacked objects, it's the containing stacking
    context-inducing object.

*   Paint container chain: the chain of paint ancestors between an element and the
    root of the page.

*   Compositing container: an implementation detail of Blink, which uses
    `PaintLayer`s to represent some layout objects. It is the ancestor along the paint
    ancestor chain which has a PaintLayer. Implemented in
    `PaintLayer::compositingContainer()`. Think of it as skipping intermediate normal
    objects and going directly to the containing stacked object.

*   Compositing container chain: same as paint chain, but for compositing container.

*   Paint invalidation container: the nearest object on the compositing container
    chain which is composited.

## Paint invalidation

Paint invalidation marks anything that need to be painted differently from the original
cached painting.

### Slimming paint v1

Though described in this document, most of the actual paint invalidation code is under
`Source/core/layout`.

Paint invalidation is a document cycle stage after compositing update and before paint.
During the previous stages, objects are marked for needing paint invalidation checking
if needed by style change, layout change, compositing change, etc. In paint invalidation stage,
we traverse the layout tree in pre-order, crossing frame boundaries, for marked subtrees
and objects and send the following information to `GraphicsLayer`s and `PaintController`s:

*   paint invalidation rects: must cover all areas that will generete different pixels.
*   invalidated display item clients: must invalidate all display item clients that will
    generate different display items.

#### `PaintInvalidationState`

`PaintInvalidationState` is an optimization used during the paint invalidation phase. Before
the paint invalidation tree walk, a root `PaintInvalidationState` is created for the root
`LayoutView`. During the tree walk, one `PaintInvalidationState` is created for each visited
object based on the `PaintInvalidationState` passed from the parent object.
It tracks the following information to provide O(1) complexity access to them if possible:

*   Paint invalidation container: Since as indicated by the definitions in [Glossaries](#Other glossaries),
    the paint invalidation container for stacked objects can differ from normal objects, we
    have to track both separately. Here is an example:

        <div style="overflow: scroll">
            <div id=A style="position: absolute"></div>
            <div id=B></div>
        </div>

    If the scroller is composited (for high-DPI screens for example), it is the paint invalidation
    container for div B, but not A.

*   Paint offset and clip rect: if possible, `PaintInvalidationState` accumulates paint offsets
    and overflow clipping rects from the paint invalidation container to provide O(1) complexity to
    map a point or a rect in current object's local space to paint invalidation container's space.
    Because locations of objects are determined by their containing blocks, and the containing block
    for absolute-position objects differs from non-absolute, we track paint offsets and overflow
    clipping rects for absolute-position objects separately.

In cases that accurate accumulation of paint offsets and clipping rects is impossible,
we will fall back to slow-path using `LayoutObject::localToAncestorPoint()` or
`LayoutObject::mapToVisualRectInAncestorSpace()`. This includes the following cases:

*   An object has transform related property, is multi-column or has flipped blocks writing-mode,
    causing we can't simply accumulate paint offset for mapping a local rect to paint invalidation
    container;

*   An object has has filter or reflection, which needs to expand paint invalidation rect
    for descendants, because currently we don't include and reflection extents into visual overflow;

*   For a fixed-position object we calculate its offset using `LayoutObject::localToAncestorPoint()`,
    but map for its descendants in fast-path if no other things prevent us from doing this;

*   Because we track paint offset from the normal paint invalidation container only, if we are going
    to use `m_paintInvalidationContainerForStackedContents` and it's different from the normal paint
    invalidation container, we have to force slow-path because the accumulated paint offset is not
    usable;

*   We also stop to track paint offset and clipping rect for absolute-position objects when
    `m_paintInvalidationContainerForStackedContents` becomes different from `m_paintInvalidationContainer`.

### Paint invalidation of texts

Texts are painted by `InlineTextBoxPainter` using `InlineTextBox` as display item client.
Text backgrounds and masks are painted by `InlineTextFlowPainter` using `InlineFlowBox`
as display item client. We should invalidate these display item clients when their painting
will change.

`LayoutInline`s and `LayoutText`s are marked for full paint invalidation if needed when
new style is set on them. During paint invalidation, we invalidate the `InlineFlowBox`s
directly contained by the `LayoutInline` in `LayoutInline::invalidateDisplayItemClients()` and
`InlineTextBox`s contained by the `LayoutText` in `LayoutText::invalidateDisplayItemClients()`.
We don't need to traverse into the subtree of `InlineFlowBox`s in `LayoutInline::invalidateDisplayItemClients()`
because the descendant `InlineFlowBox`s and `InlineTextBox`s will be handled by their
owning `LayoutInline`s and `LayoutText`s, respectively, when changed style is propagated.

### Specialty of `::first-line`

`::first-line` pseudo style dynamically applies to all `InlineBox`'s in the first line in the
block having `::first-line` style. The actual applied style is computed from the `::first-line`
style and other applicable styles.

If the first line contains any `LayoutInline`, we compute the style from the `::first-line` style
and the style of the `LayoutInline` and apply the computed style to the first line part of the
`LayoutInline`. In blink's style implementation, the combined first line style of `LayoutInline`
is identified with `FIRST_LINE_INHERITED` pseudo ID.

The normal paint invalidation of texts doesn't work for first line because
*   `ComputedStyle::visualInvalidationDiff()` can't detect first line style changes;
*   The normal paint invalidation is based on whole LayoutObject's, not aware of the first line.

We have a special path for first line style change: the style system informs the layout system
when the computed first-line style changes through `LayoutObject::firstLineStyleDidChange()`.
When this happens, we invalidate all `InlineBox`es in the first line.

### Slimming paint v2

TODO(wangxianzhu): add details

## [`PrePaintTreeWalk`](PrePaintTreeWalk.h) (Slimming paint v2 only)

During `InPrePaint` document lifecycle state, this class is called to walk the whole
layout tree, beginning from the root FrameView, across frame boundaries. We do the
following during the tree walk:

*   Building paint property tree: creates paint property tree nodes for special
    things in the layout tree, including but not limit to: overflow clip, transform,
    fixed-pos, animation, mask, filter, etc.

*   Paint invalidation: Not implemented yet. TODO(wangxianzhu): add details after
    it's implemented.

## Paint result caching

`PaintController` holds the previous painting result as a cache of display items.
If some painter would generate results same as those of the previous painting,
we'll skip the painting and reuse the display items from cache.

### Display item caching

We'll create a `CachedDisplayItem` when a painter would create a `DrawingDisplayItem` exactly
the same as the display item created in the previous painting. After painting, `PaintController`
will replace `CachedDisplayItem` with the corresponding display item from the cache.

### Subsequence caching

When possible, we enclose the display items that `PaintLayerPainter::paintContents()` generates
(including display items generated by sublayers) in a pair of `BeginSubsequence/EndSubsequence`
display items.

In a subsequence paint, if the layer would generate exactly the same display items, we'll simply
output a `CachedSubsequence` display item in place of the display items, and skip all paintings
of the layer and its descendants in painting order. After painting, `PaintController` will
replace `CacheSubsequence` with cached display items created in previous paintings.

There are many conditions affecting
*   whether we need to generate subsequence for a PaintLayer;
*   whether we can use cached subsequence for a PaintLayer.
See `shouldCreateSubsequence()` and `shouldRepaintSubsequence()` in `PaintLayerPainter.cpp` for
the conditions.

## Empty paint phase optimization

During painting, we walk the layout tree multiple times for multiple paint phases. Sometimes
a layer contain nothing needing a certain paint phase and we can skip tree walk for such
empty phases. Now we have optimized `PaintPhaseDescendantBlockBackgroundsOnly`,
`PaintPhaseDescendantOutlinesOnly` and `PaintPhaseFloat` for empty paint phases.

During paint invalidation, we set the containing self-painting layer's `needsPaintPhaseXXX`
flag if the object has something needing to be painted in the paint phase.

During painting, we check the flag before painting a paint phase and skip the tree walk if
the flag is not set.

It's hard to clear a `needsPaintPhaseXXX` flag when a layer no longer needs the paint phase,
so we never clear the flags.

When layer structure changes, and we are not invalidate paint of the changed subtree,
we need to manually update the `needsPaintPhaseXXX` flags. For example, if an object changes
style and creates a self-painting-layer, we copy the flags from its containing self-painting
layer to this layer, assuming that this layer needs all paint phases that its container
self-painting layer needs.

We could update the `needsPaintPhaseXXX` flags in a separate tree walk, but that would regress
performance of the first paint. For slimming paint v2, we can update the flags during the
pre-painting tree walk to simplify the logics.
