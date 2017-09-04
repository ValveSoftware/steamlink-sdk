# Whitespace LayoutObjects

Text nodes which only contain whitespace sometimes have a layout object and
sometimes they don't. This document tries to explain why and how these layout
objects are created.

## Why

For layout purposes, whitespace nodes are sometimes significant, but not
always. In Blink, we try to create as few of them as possible to save memory,
and save CPU by having fewer layout objects to traverse.

### Inline flow

Whitespace typically matters in an inline flow context. Example:

    <span>A</span> </span>B</span>

If we didn't create a LayoutObject for the whitespace node between the two
spans, we would have rendered the markup above as "AB" as the span layout
objects would have been siblings in the layout tree.

### Block flow

Whitespace typically doesn't matter in a block flow context. Example:

    <div>A</div> <div>B</div>

In the example above, the whitespace node between the divs would not contribute
to layout/rendering. Hence, we can skip creating a LayoutText for it.

### Out-of-flow

Out-of-flow elements like absolutely positioned elements do not affect inline
or block in-flow layout. That means we can skip such elements when considering
the need whitespace layout objects.

Example:

    <div><span style="position:absolute">A</span> </span>B</span></div>

In the example above, we don't need to create a whitespace layout object since
it will be the first in-flow child of the block, and will not contribute to the
layout/rendering.

Example:

    <span>A</span> <span style="position:absolute">Z</span> <span>B</span>

In the example above, we need to create a whitespace layout object to separate
the A and B in the rendering. However, we only need to create a layout object
for one of the whitespace nodes as whitespace collapse.

### Preformatted text and editing

Some values of the CSS white-space property will cause whitespace to not
collapse and affect layout and rendering also in block layout. In those cases
we always create layout objects for whitespace nodes.

Whitespace nodes are also significant in editing mode.

## How

### Initial layout tree attachment

When attaching a layout tree, we walk the flat-tree in a depth-first order
walking the siblings from left to right. As we reach a text node, we check if
we need to create a layout object in textLayoutObjectIsNeeded(). In particular,
if we found that it only contains whitespace, we start traversing the
previously added layout object siblings, skipping out-of-flow elements, to
decide if we need to create a whitespace layout object.

Important methods:

    Text::attachLayoutTree()
    Text::textLayoutObjectIsNeeded()

### Layout object re-attachment

During style recalculation, elements whose computed value for display changes
will have its layout sub-tree re-attached. Attachment of the descendant layout
objects happens the same way as for initial layout tree attachment, but the
interesting part for whitespace layout objects is how they are affected by
re-attachment of sibling elements. Sibling nodes may or may not be re-attached
during the same style recalc traversal depending on whether they change their
computed display values or not.

#### Style recalc traversal

An important prerequisite for how whitespace layout objects are re-attached
is the traversal order we use for style recalc. The current traversal order
makes it hard or costly to implement whitespace re-attachment without bugs in
the presence of shadow trees, but let's describe what we do here.

Style recalc happens in the shadow-including tree order with the exception that
siblings are traversed from right to left. The ::before and ::after pseudo
elements are recalculated in left-to-right (!?!) order, before and after
shadow-including descendants respectively.

Inheritance happens down the flat-tree. Since we are not traversing in
flat-tree order, we implement this propagation from slot/content elements down
to assigned/distributed nodes by marking these nodes with LocalStyleChange when
we need to do inheritance propagation (done in HTMLSlotElement::willRecalcStyle
for instance). This works since light-tree children are traversed after the
shadow tree(s).

#### Re-attaching whitespace layout objects

When the computed display value changes, the requirement for whitespace
siblings may change.

Example:

    <span style="position:absolute">A</span> <span>B</span>

Initially, we don't need a layout object for the whitespace above. If we change
the position of the first span to static, we need a layout object for the
whitespace. During style recalc we keep track of the last text node sibling we
traversed. The text node is reset when we traverse an element with a layout
box. Remember that we traverse from right to left. That means we have stored
the whitespace node above when we re-attach the left-most span. After
re-attachment we re-attach the stored text node to see if the need for a
layout object changed. If the text node re-attach changed the need for a layout
object we continue to re-attach following layout object siblings until we
reach an element with a layout object, or the re-attach was a no-op.

The need for a whitespace layout object is dictated by the layout tree
structure which is based on the flat-tree. That means the tracked text node
solution we use does not work properly when (re-)attaching slotted and
distributed nodes.

Example:

    <div id="host">
        <:shadow-root>
            <span style="position:absolute">A</span><slot></slot>
        </:shadow-root>
        <span>B</span>
    </div>

Initially, the whitespace before the B span above does not get a layout object.
If we change the absolute positioned span in the shadow tree to static, we need
to have a layout object for that whitespace node. However, since we traverse
the light-tree children of #host after the shadow tree, we do not see the text
node before re-attaching the absolute positioned span.

Likewise we currently have issues with ::before and ::after elements because we
do not keep track of text nodes and pass them to ::before/::after element
re-attachments. See the "Known issues" below.

Important methods:

    Element::recalcStyle()
    ContainerNode::recalcDescendantStyles()
    Node::reattachWhitespaceSiblingsIfNeeded()
    Text::textLayoutObjectIsNeeded()
    Text::reattachLayoutTreeIfNeeded()
    Text::recalcTextStyle()

Known issues:

    https://crbug.com/648931
    https://crbug.com/648951
    https://crbug.com/650168
