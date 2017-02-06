/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutReplica_h
#define LayoutReplica_h

#include "core/layout/LayoutBox.h"

namespace blink {

// LayoutReplica is a synthetic object used to represent reflections.
// https://www.webkit.org/blog/182/css-reflections/
//
// The object is part of the layout tree, however it is not fully inserted into
// it: LayoutReplica::parent() will return the right information but the
// parent's children() don't know about the LayoutReplica.
// Note that its PaintLayer is fully inserted into the PaintLayer tree, which
// requires some special casing in the painting code (e.g. in
// PaintLayerStackingNode).
//
// LayoutReplica's parent() is the object with the -webkit-box-reflect.
// LayoutReplica inherits its style from its parent() but also have an extra
// 'transform' to paint the reflection correctly. This is done by
// PaintLayerReflectionInfo.
//
// The object is created and managed by PaintLayerReflectionInfo. Also all of
// its operations happen through PaintLayerReflectionInfo (e.g. layout, style
// updates, ...).
//
// This class is a big hack. The original intent for it is unclear but has to do
// with implementing the correct painting of reflection. It may be to apply a
// 'transform' and offset during painting.
class LayoutReplica final : public LayoutBox {
public:
    static LayoutReplica* createAnonymous(Document*);
    ~LayoutReplica() override;

    const char* name() const override { return "LayoutReplica"; }

    PaintLayerType layerTypeRequired() const override { return NormalPaintLayer; }

    void layout() override;

    void paint(const PaintInfo&, const LayoutPoint&) const override;

private:
    LayoutReplica();

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectReplica || LayoutBox::isOfType(type); }
    void computePreferredLogicalWidths() override;

};

} // namespace blink

#endif // LayoutReplica_h
