# Platform paint code

This directory contains the implementation of display lists and display
list-based painting, except for code which requires knowledge of `core/`
concepts, such as DOM elements and layout objects.

This code is owned by the [paint team][paint-team-site].

Slimming Paint v2 is currently being implemented. Unlike Slimming Paint v1, SPv2
represents its paint artifact not as a flat display list, but as a list of
drawings, and a list of paint chunks, stored together.

This document explains the SPv2 world as it develops, not the SPv1 world it
replaces.

[paint-team-site]: https://www.chromium.org/developers/paint-team

## Paint artifact

The SPv2 [paint artifact](PaintArtifact.h) consists of a list of display items
in paint order (ideally mostly or all drawings), partitioned into *paint chunks*
which define certain *paint properties* which affect how the content should be
drawn or composited.

## Paint properties

Paint properties define characteristics of how a paint chunk should be drawn,
such as the transform it should be drawn with. To enable efficient updates,
a chunk's paint properties are described hierarchically. For instance, each
chunk is associated with a transform node, whose matrix should be multiplied by
its ancestor transform nodes in order to compute the final transformation matrix
to the screen.

*** note
Support for all paint properties has yet to be implemented in SPv2.
***

*** aside
TODO(jbroman): Explain the semantics of transforms, clips, scrolls and effects
as support for them is added to SPv2.
***

### Transforms

Each paint chunk is associated with a [transform node](TransformPaintPropertyNode.h),
which defines the coordinate space in which the content should be painted.

Each transform node has:

* a 4x4 [`TransformationMatrix`](../../transforms/TransformationMatrix.h)
* a 3-dimensional transform origin, which defines the origin relative to which
  the transformation matrix should be applied (e.g. a rotation applied with some
  transform origin will rotate the plane about that point)
* a pointer to the parent node, which defines the coordinate space relative to
  which the above should be interpreted

The parent node pointers link the transform nodes in a hierarchy (the *transform
tree*), which defines how the transform for any painted content can be
determined.

***promo
The painting system may create transform nodes which don't affect the position
of points in the xy-plane, but which have an apparent effect only when
multiplied with other transformation matrices. In particular, a transform node
may be created to establish a perspective matrix for descendant transforms in
order to create the illusion of depth.
***

*** aside
TODO(jbroman): Explain flattening, etc., once it exists in the paint properties.
***

### Clips

Each paint chunk is associated with a [clip node](ClipPaintPropertyNode.h),
which defines the raster region that will be applied on the canvas when
the chunk is rastered.

Each clip node has:

* A float rect with (optionally) rounded corner radius.
* An associated transform node, which the clip rect is based on.

The raster region defined by a node is the rounded rect transformed to the
root space, intersects with the raster region defined by its parent clip node
(if not root).

### Effects

Each paint chunk is associated with an [effect node](EffectPaintPropertyNode.h),
which defines the effect (opacity, transfer mode, filter, mask, etc.) that
should be applied to the content before or as it is composited into the content
below.

Each effect node has:

* a floating-point opacity (from 0 to 1, inclusive)
* a pointer to the parent node, which will be applied to the result of this
  effect before or as it is composited into its parent in the effect tree

The paret node pointers link the effect nodes in a hierarchy (the *effect
tree*), which defines the dependencies between rasterization of different
contents.

One can imagine each effect node as corresponding roughly to a bitmap that is
drawn before being composited into another bitmap, though for implementation
reasons this may not be how it is actually implemented.

*** aside
TODO(jbroman): Explain the connection with the transform and clip trees, once it
exists, as well as effects other than opacity.
***

## Display items

A display item is the smallest unit of a display list in Blink. Each display
item is identified by an ID consisting of:

* an opaque pointer to the *display item client* that produced it
* a type (from the `DisplayItem::Type` enum)

In practice, display item clients are generally subclasses of `LayoutObject`,
but can be other Blink objects which get painted, such as inline boxes and drag
images.

*** note
It is illegal for there to be two drawings with the same ID in a display item
list, except for drawings that are marked uncacheable
(see [DisplayItemCacheSkipper](DisplayItemCacheSkipper.h)).
***

Generally, clients of this code should use stack-allocated recorder classes to
emit display items to a `PaintController` (using `GraphicsContext`).

### Standalone display items

#### [CachedDisplayItem](CachedDisplayItem.h)

See [Display item caching](../../../core/paint/README.md#paint-result-caching).

#### [DrawingDisplayItem](DrawingDisplayItem.h)

Holds an `SkPicture` which contains the Skia commands required to draw some atom
of content.

#### [ForeignLayerDisplayItem](ForeignLayerDisplayItem.h)

Draws an atom of content, but using a `cc::Layer` produced by some agent outside
of the normal Blink paint system (for example, a plugin). Since they always map
to a `cc::Layer`, they are always the only display item in their paint chunk,
and are ineligible for squashing with other layers.

### Paired begin/end display items

*** aside
TODO(jbroman): Describe how these work, once we've worked out what happens to
them in SPv2.
***

## Paint controller

Callers use `GraphicsContext` (via its drawing methods, and its
`paintController()` accessor) and scoped recorder classes, which emit items into
a `PaintController`.

`PaintController` is responsible for producing the paint artifact. It contains
the *current* paint artifact, which is always complete (i.e. it has no
`CachedDisplayItem` objects), and *new* display items and paint chunks, which
are added as content is painted.

When the new display items have been populated, clients call
`commitNewDisplayItems`, which merges the previous artifact with the new data,
producing a new paint artifact, where `CachedDisplayItem` objects have been
replaced with the cached content from the previous artifact.

At this point, the paint artifact is ready to be drawn or composited.

### Paint result caching and invalidation

See [Display item caching](../../../core/paint/README.md#paint-result-caching)
and [Paint invalidation](../../../core/paint/README.md#paint-invalidation) for
more details about how caching and invalidation are handled in blink core
module using `PaintController` API.

We use 'cache generation' which is a unique id of cache status in each
`DisplayItemClient` and `PaintController` to determine if the client is validly
cached by a `PaintController`.

A paint controller sets its cache generation to
`DisplayItemCacheGeneration::next()` at the end of each
`commitNewDisplayItems()`, and updates the cache generation of each client with
cached drawings by calling `DisplayItemClient::setDisplayItemsCached()`.
A display item is treated as validly cached in a paint controller if its cache
generation matches the paint controller's cache generation.

`kInvalidCacheGeneration` is a special cache generation value which matches no
other cache generations. When a `DisplayItemClient` is invalidated, we set its
cache generation to `kInvalidCacheGeneration`. When a `PaintController` is
cleared (e.g. when the corresponding `GraphicsLayer` is fully invalidated), we
also set its cache generation to `kInvalidCacheGeneration`.

For now we use a uint32_t variable to store cache generation. Assuming there is
an animation in 60fps needing main-thread repaint, the cache generation will
overflow after 2^32/86400/60 = 828 days. The time may be shorter if there are
multiple animating `PaintController`s in the same process. When it overflows,
we may treat some object that is not cached as cached if the following
conditions are all met:
*   the object was painted when the cache generation was *n*;
*   the object has been neither painted nor invalidated since cache generation
    *n*;
*   when the cache generation wraps back to exact *n*, the object happens to be
    painted again.
As the paint controller doesn't have cached display items for the object, there
will be corrupted painting or assertion failure. The chance is too low to be
concerned.

SPv1 only: If a display item is painted on multiple paint controllers, because
cache generations are unique, the client's cache generation matches the last
paint controller only. The client will be treated as invalid on other paint
controllers regardless if it's validly cached by these paint controllers.
The situation is very rare (about 0.07% clients were painted on multiple paint
controllers in a [Cluster Telemetry run](https://ct.skia.org/chromium_perf_runs)
(run 803) so the performance penalty is trivial.

## Paint artifact compositor

The [`PaintArtifactCompositor`](PaintArtifactCompositor.h) is responsible for
consuming the `PaintArtifact` produced by the `PaintController`, and converting
it into a form suitable for the compositor to consume.

At present, `PaintArtifactCompositor` creates a cc layer tree, with one layer
for each paint chunk. In the future, it is expected that we will use heuristics
to combine paint chunks into a smaller number of layers.

The owner of the `PaintArtifactCompositor` (e.g. `WebView`) can then attach its
root layer to the overall layer hierarchy to be displayed to the user.

## Geometry routines

The [`GeometryMapper`](GeometryMapper.h) is responsible for efficiently computing
visual and transformed rects of display items in the coordinate space of ancestor
[`PropertyTreeState`](PropertyTreeState.h)s.

The transformed rect of a display item in an ancestor `PropertyTreeState` is that
rect, multiplied by the transforms between the display item's `PropertyTreeState`
and the ancestors, then flattened into 2D.

The visual rect of a display item in an ancestor `PropertyTreeState` is the intersection
of all of the intermediate clips (transformed in to the ancestor state), with
the display item's transformed rect.
