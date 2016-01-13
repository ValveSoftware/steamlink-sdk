/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef RenderLayerBlendInfo_h
#define RenderLayerBlendInfo_h

#include "public/platform/WebBlendMode.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class RenderLayerModelObject;

class RenderLayerBlendInfo {
    WTF_MAKE_NONCOPYABLE(RenderLayerBlendInfo);
public:
    explicit RenderLayerBlendInfo(RenderLayerModelObject&);

    bool hasBlendMode() const;
    void updateBlendMode();

    bool childLayerHasBlendMode() const
    {
        ASSERT(!m_childLayerHasBlendModeStatusDirty);
        return m_childLayerHasBlendMode;
    }
    bool childLayerHasBlendModeWhileDirty() const { return m_childLayerHasBlendMode; }

    void setChildLayerHasBlendMode(bool b) { m_childLayerHasBlendMode = b; }

    bool childLayerHasBlendModeStatusDirty() const { return m_childLayerHasBlendModeStatusDirty; }
    void setChildLayerHasBlendModeStatusDirty(bool b) { m_childLayerHasBlendModeStatusDirty = b; }

    void setAncestorChainBlendedDescendant();
    void dirtyAncestorChainBlendedDescendantStatus();

    blink::WebBlendMode blendMode() const { return m_blendMode; }

private:
    RenderLayerModelObject& m_renderer;
    blink::WebBlendMode m_blendMode;

    unsigned m_childLayerHasBlendMode : 1;
    unsigned m_childLayerHasBlendModeStatusDirty : 1;
};

} // namespace WebCore

#endif // RenderLayerBlendInfo_h
