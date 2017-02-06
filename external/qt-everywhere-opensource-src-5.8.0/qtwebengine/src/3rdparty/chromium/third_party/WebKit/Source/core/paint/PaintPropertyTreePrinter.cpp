// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreePrinter.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"

#ifndef NDEBUG

namespace blink {
namespace {

template <typename PaintPropertyTreeNode>
class PropertyTreePrinterTraits;

template <typename PropertyTreeNode>
class PropertyTreePrinter {
public:
    PropertyTreePrinter(const FrameView& frameView)
    {
        collectPropertyNodes(frameView);
    }

    void show()
    {
        showAllPropertyNodes(nullptr);
    }

    void addPropertyNode(const PropertyTreeNode* node, String debugInfo)
    {
        m_nodeToDebugString.set(node, debugInfo);
    }

private:
    using Traits = PropertyTreePrinterTraits<PropertyTreeNode>;

    void collectPropertyNodes(const FrameView& frameView)
    {
        Traits::addFrameViewProperties(frameView, *this);
        if (LayoutView* layoutView = frameView.layoutView())
            collectPropertyNodes(*layoutView);
        for (Frame* child = frameView.frame().tree().firstChild(); child; child = child->tree().nextSibling()) {
            if (!child->isLocalFrame())
                continue;
            if (FrameView* childView = toLocalFrame(child)->view())
                collectPropertyNodes(*childView);
        }
    }

    void collectPropertyNodes(const LayoutObject& object)
    {
        if (const ObjectPaintProperties* paintProperties = object.objectPaintProperties())
            Traits::addObjectPaintProperties(*paintProperties, *this);
        for (LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling())
            collectPropertyNodes(*child);
    }

    void showAllPropertyNodes(const PropertyTreeNode* node, unsigned indent = 0)
    {
        if (node) {
            StringBuilder stringBuilder;
            for (unsigned i = 0; i < indent; i++)
                stringBuilder.append(' ');
            if (m_nodeToDebugString.contains(node))
                stringBuilder.append(m_nodeToDebugString.get(node));
            stringBuilder.append(String::format(" %p", node));
            Traits::printNodeAsString(node, stringBuilder);
            fprintf(stderr, "%s\n", stringBuilder.toString().ascii().data());
        }

        for (const auto* childNode : m_nodeToDebugString.keys()) {
            if (childNode->parent() == node)
                showAllPropertyNodes(childNode, indent + 2);
        }
    }

    HashMap<const PropertyTreeNode*, String> m_nodeToDebugString;
};

template <>
class PropertyTreePrinterTraits<TransformPaintPropertyNode> {
public:
    static void addFrameViewProperties(const FrameView& frameView, PropertyTreePrinter<TransformPaintPropertyNode>& printer)
    {
        if (const TransformPaintPropertyNode* preTranslation = frameView.preTranslation())
            printer.addPropertyNode(preTranslation, "PreTranslation");
        if (const TransformPaintPropertyNode* scrollTranslation = frameView.scrollTranslation())
            printer.addPropertyNode(scrollTranslation, "ScrollTranslation");
    }

    static void addObjectPaintProperties(const ObjectPaintProperties& paintProperties, PropertyTreePrinter<TransformPaintPropertyNode>& printer)
    {
        if (const TransformPaintPropertyNode* paintOffsetTranslation = paintProperties.paintOffsetTranslation())
            printer.addPropertyNode(paintOffsetTranslation, "PaintOffsetTranslation");
        if (const TransformPaintPropertyNode* transform = paintProperties.transform())
            printer.addPropertyNode(transform, "Transform");
        if (const TransformPaintPropertyNode* perspective = paintProperties.perspective())
            printer.addPropertyNode(perspective, "Perspective");
        if (const TransformPaintPropertyNode* svgLocalToBorderBoxTransform = paintProperties.svgLocalToBorderBoxTransform())
            printer.addPropertyNode(svgLocalToBorderBoxTransform, "SvgLocalToBorderBoxTransform");
        if (const TransformPaintPropertyNode* scrollTranslation = paintProperties.scrollTranslation())
            printer.addPropertyNode(scrollTranslation, "ScrollTranslation");
        if (const TransformPaintPropertyNode* scrollbarPaintOffset = paintProperties.scrollbarPaintOffset())
            printer.addPropertyNode(scrollbarPaintOffset, "ScrollbarPaintOffset");
    }

    static void printNodeAsString(const TransformPaintPropertyNode* node, StringBuilder& stringBuilder)
    {
        stringBuilder.append(" transform=");

        TransformationMatrix::DecomposedType decomposition;
        if (!node->matrix().decompose(decomposition)) {
            stringBuilder.append("degenerate");
            return;
        }

        stringBuilder.append(String::format("translation=%f,%f,%f", decomposition.translateX, decomposition.translateY, decomposition.translateZ));
        if (node->matrix().isIdentityOrTranslation())
            return;

        stringBuilder.append(String::format(", scale=%f,%f,%f", decomposition.scaleX, decomposition.scaleY, decomposition.scaleZ));
        stringBuilder.append(String::format(", skew=%f,%f,%f", decomposition.skewXY, decomposition.skewXZ, decomposition.skewYZ));
        stringBuilder.append(String::format(", quaternion=%f,%f,%f,%f", decomposition.quaternionX, decomposition.quaternionY, decomposition.quaternionZ, decomposition.quaternionW));
        stringBuilder.append(String::format(", perspective=%f,%f,%f,%f", decomposition.perspectiveX, decomposition.perspectiveY, decomposition.perspectiveZ, decomposition.perspectiveW));
    }
};

template <>
class PropertyTreePrinterTraits<ClipPaintPropertyNode> {
public:
    static void addFrameViewProperties(const FrameView& frameView, PropertyTreePrinter<ClipPaintPropertyNode>& printer)
    {
        if (const ClipPaintPropertyNode* contentClip = frameView.contentClip())
            printer.addPropertyNode(contentClip, "ContentClip");
    }

    static void addObjectPaintProperties(const ObjectPaintProperties& paintProperties, PropertyTreePrinter<ClipPaintPropertyNode>& printer)
    {
        if (const ClipPaintPropertyNode* cssClip = paintProperties.cssClip())
            printer.addPropertyNode(cssClip, "CssClip");
        if (const ClipPaintPropertyNode* cssClipFixedPosition = paintProperties.cssClipFixedPosition())
            printer.addPropertyNode(cssClipFixedPosition, "CssClipFixedPosition");
        if (const ClipPaintPropertyNode* overflowClip = paintProperties.overflowClip())
            printer.addPropertyNode(overflowClip, "OverflowClip");
    }

    static void printNodeAsString(const ClipPaintPropertyNode* node, StringBuilder& stringBuilder)
    {
        stringBuilder.append(String::format(" localTransformSpace=%p ", node->localTransformSpace()));
        stringBuilder.append(String::format("rect=%f,%f,%f,%f",
            node->clipRect().rect().x(), node->clipRect().rect().y(),
            node->clipRect().rect().width(), node->clipRect().rect().height()));
    }
};

template <>
class PropertyTreePrinterTraits<EffectPaintPropertyNode> {
public:
    static void addFrameViewProperties(const FrameView& frameView, PropertyTreePrinter<EffectPaintPropertyNode>& printer)
    {
        // FrameView does not create any effect nodes.
    }

    static void addObjectPaintProperties(const ObjectPaintProperties& paintProperties, PropertyTreePrinter<EffectPaintPropertyNode>& printer)
    {
        if (const EffectPaintPropertyNode* effect = paintProperties.effect())
            printer.addPropertyNode(effect, "Effect");
    }

    static void printNodeAsString(const EffectPaintPropertyNode* node, StringBuilder& stringBuilder)
    {
        stringBuilder.append(String::format(" opacity=%f", node->opacity()));
    }
};

} // anonymous namespace

void showTransformPropertyTree(const FrameView& rootFrame)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    PropertyTreePrinter<TransformPaintPropertyNode>(rootFrame).show();
}

void showClipPropertyTree(const FrameView& rootFrame)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    PropertyTreePrinter<ClipPaintPropertyNode>(rootFrame).show();
}

void showEffectPropertyTree(const FrameView& rootFrame)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    PropertyTreePrinter<EffectPaintPropertyNode>(rootFrame).show();
}

} // namespace blink

#endif
