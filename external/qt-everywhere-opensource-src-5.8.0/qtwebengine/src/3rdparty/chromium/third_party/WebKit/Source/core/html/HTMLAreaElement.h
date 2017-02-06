/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2009, 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLAreaElement_h
#define HTMLAreaElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLAnchorElement.h"
#include "platform/geometry/LayoutRect.h"
#include <memory>

namespace blink {

class HitTestResult;
class HTMLImageElement;
class Path;

class CORE_EXPORT HTMLAreaElement final : public HTMLAnchorElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    DECLARE_NODE_FACTORY(HTMLAreaElement);

    bool isDefault() const { return m_shape == Default; }

    // |containerObject| in the following functions is an object (normally a LayoutImage)
    // which references the containing image map of this area. There might be multiple
    // objects referencing the same map. For these functions, the effective geometry of
    // this map will be calculated based on the specified container object, e.g. the
    // rectangle of the default shape will be the border box rect of the container object,
    // and effective zoom factor of the container object will be applied on non-default shape.
    bool pointInArea(const LayoutPoint&, const LayoutObject* containerObject) const;
    LayoutRect computeAbsoluteRect(const LayoutObject* containerObject) const;
    Path getPath(const LayoutObject* containerObject) const;

    // The parent map's image.
    HTMLImageElement* imageElement() const;

private:
    explicit HTMLAreaElement(Document&);
    ~HTMLAreaElement();

    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    bool isKeyboardFocusable() const override;
    bool isMouseFocusable() const override;
    bool layoutObjectIsFocusable() const override;
    void updateFocusAppearance(SelectionBehaviorOnFocus) override;
    void setFocus(bool) override;

    enum Shape { Default, Poly, Rect, Circle };
    void invalidateCachedPath();

    mutable std::unique_ptr<Path> m_path;
    Vector<double> m_coords;
    Shape m_shape;
};

} // namespace blink

#endif // HTMLAreaElement_h
