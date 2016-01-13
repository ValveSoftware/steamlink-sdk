/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef RenderSearchField_h
#define RenderSearchField_h

#include "core/rendering/RenderTextControlSingleLine.h"

namespace WebCore {

class HTMLInputElement;

class RenderSearchField FINAL : public RenderTextControlSingleLine {
public:
    RenderSearchField(HTMLInputElement*);
    virtual ~RenderSearchField();
    void stopSearchEventTimer();

private:
    virtual void centerContainerIfNeeded(RenderBox*) const OVERRIDE;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const OVERRIDE;
    virtual LayoutUnit computeLogicalHeightLimit() const OVERRIDE;

    Element* searchDecorationElement() const;
    Element* cancelButtonElement() const;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSearchField, isTextField());

}

#endif
