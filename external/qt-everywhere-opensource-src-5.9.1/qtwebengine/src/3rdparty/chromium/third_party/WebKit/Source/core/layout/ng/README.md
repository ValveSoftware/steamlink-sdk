# LayoutNG #

This directory contains the implementation of Blink's new layout engine
"LayoutNG".

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/layout/ng/README.md).

The original design document can be seen [here](https://docs.google.com/document/d/1uxbDh4uONFQOiGuiumlJBLGgO4KDWB8ZEkp7Rd47fw4/edit).

## High level overview ##

CSS has many different types of layout modes, controlled by the `display`
property. (In addition to this specific HTML elements have custom layout modes
as well). For each different type of layout, we have a
[NGLayoutAlgorithm](ng_layout_algorithm.h).

The input to an [NGLayoutAlgorithm](ng_layout_algorithm.h) is the same tuple
for every kind of layout:

 - The [NGBox](ng_box.h) which we are currently performing layout for. The
   following information is accessed:

   - The [ComputedStyle](../../style/ComputedStyle.h) for the node which we are
     currently performing laying for.

   - The list of children [NGBox](ng_box.h)es to perform layout upon, and their
     respective style objects.

 - The [NGConstraintSpace](ng_constraint_space.h) which represents the "space"
   in which the current layout should produce a
   [NGPhysicalFragment](ng_physical_fragment.h).

 - TODO(layout-dev): BreakTokens should go here once implemented.

The current layout should not access any information outside this set, this
will break invariants in the system. (As a concrete example we intend to cache
[NGPhysicalFragment](ng_physical_fragment.h)s based on this set, accessing
additional information outside this set will break caching behaviour).

### Box Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Fragment Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Constraint Spaces ###

TODO(layout-dev): Document with lots of pretty pictures.

## Block Flow Algorithm ##

This section contains details specific to the
[NGBlockLayoutAlgorithm](ng_block_layout_algorithm.h).

### Collapsing Margins ###

TODO(layout-dev): Document with lots of pretty pictures.

### Float Positioning ###

TODO(layout-dev): Document with lots of pretty pictures.
