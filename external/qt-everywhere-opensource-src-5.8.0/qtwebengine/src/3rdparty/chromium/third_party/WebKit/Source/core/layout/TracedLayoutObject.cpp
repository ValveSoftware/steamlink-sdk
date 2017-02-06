// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/TracedLayoutObject.h"

#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include <inttypes.h>
#include <memory>

namespace blink {

namespace {

void dumpToTracedValue(const LayoutObject& object, bool traceGeometry, TracedValue* tracedValue)
{
    tracedValue->setString("address", String::format("%" PRIxPTR, reinterpret_cast<uintptr_t>(&object)));
    tracedValue->setString("name", object.name());
    if (Node* node = object.node()) {
        tracedValue->setString("tag", node->nodeName());
        if (node->isElementNode()) {
            Element& element = toElement(*node);
            if (element.hasID())
                tracedValue->setString("htmlId", element.getIdAttribute());
            if (element.hasClass()) {
                tracedValue->beginArray("classNames");
                for (size_t i = 0; i < element.classNames().size(); ++i)
                    tracedValue->pushString(element.classNames()[i]);
                tracedValue->endArray();
            }
        }
    }

    // FIXME: When the fixmes in LayoutTreeAsText::writeLayoutObject() are
    // fixed, deduplicate it with this.
    if (traceGeometry) {
        tracedValue->setDouble("absX", object.absoluteBoundingBoxRect().x());
        tracedValue->setDouble("absY", object.absoluteBoundingBoxRect().y());
        LayoutRect rect;
        if (object.isText())
            rect = LayoutRect(toLayoutText(object).linesBoundingBox());
        else if (object.isLayoutInline())
            rect = LayoutRect(toLayoutInline(object).linesBoundingBox());
        else if (object.isBox())
            rect = toLayoutBox(&object)->frameRect();
        tracedValue->setDouble("relX", rect.x());
        tracedValue->setDouble("relY", rect.y());
        tracedValue->setDouble("width", rect.width());
        tracedValue->setDouble("height", rect.height());
    } else {
        tracedValue->setDouble("absX", 0);
        tracedValue->setDouble("absY", 0);
        tracedValue->setDouble("relX", 0);
        tracedValue->setDouble("relY", 0);
        tracedValue->setDouble("width", 0);
        tracedValue->setDouble("height", 0);
    }

    if (object.isOutOfFlowPositioned())
        tracedValue->setBoolean("positioned", object.isOutOfFlowPositioned());
    if (object.selfNeedsLayout())
        tracedValue->setBoolean("selfNeeds", object.selfNeedsLayout());
    if (object.needsPositionedMovementLayout())
        tracedValue->setBoolean("positionedMovement", object.needsPositionedMovementLayout());
    if (object.normalChildNeedsLayout())
        tracedValue->setBoolean("childNeeds", object.normalChildNeedsLayout());
    if (object.posChildNeedsLayout())
        tracedValue->setBoolean("posChildNeeds", object.posChildNeedsLayout());
    if (object.isTableCell()) {
        const LayoutTableCell& c = toLayoutTableCell(object);
        tracedValue->setDouble("row", c.rowIndex());
        tracedValue->setDouble("col", c.absoluteColumnIndex());
        if (c.rowSpan() != 1)
            tracedValue->setDouble("rowSpan", c.rowSpan());
        if (c.colSpan() != 1)
            tracedValue->setDouble("colSpan", c.colSpan());
    }
    if (object.isAnonymous())
        tracedValue->setBoolean("anonymous", object.isAnonymous());
    if (object.isRelPositioned())
        tracedValue->setBoolean("relativePositioned", object.isRelPositioned());
    if (object.isStickyPositioned())
        tracedValue->setBoolean("stickyPositioned", object.isStickyPositioned());
    if (object.isFloating())
        tracedValue->setBoolean("float", object.isFloating());

    if (object.slowFirstChild()) {
        tracedValue->beginArray("children");
        for (LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling()) {
            tracedValue->beginDictionary();
            dumpToTracedValue(*child, traceGeometry, tracedValue);
            tracedValue->endDictionary();
        }
        tracedValue->endArray();
    }
}

} // namespace

std::unique_ptr<TracedValue> TracedLayoutObject::create(const LayoutView& view, bool traceGeometry)
{
    std::unique_ptr<TracedValue> tracedValue = TracedValue::create();
    dumpToTracedValue(view, traceGeometry, tracedValue.get());
    return tracedValue;
}

} // namespace blink
