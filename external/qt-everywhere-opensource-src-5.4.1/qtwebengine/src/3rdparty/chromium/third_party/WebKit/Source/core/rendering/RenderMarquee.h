/*
 * Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef RenderMarquee_h
#define RenderMarquee_h

#include "core/html/HTMLMarqueeElement.h"
#include "core/rendering/RenderBlockFlow.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/Length.h"
#include "platform/Timer.h"

namespace WebCore {

// This class handles the auto-scrolling for <marquee>
class RenderMarquee FINAL : public RenderBlockFlow {
public:
    explicit RenderMarquee(HTMLMarqueeElement*);
    virtual ~RenderMarquee();

    int speed() const { return m_speed; }
    int marqueeSpeed() const;

    EMarqueeDirection reverseDirection() const { return static_cast<EMarqueeDirection>(-direction()); }
    EMarqueeDirection direction() const;

    bool isHorizontal() const;

    int computePosition(EMarqueeDirection, bool stopAtClientEdge);

    void setEnd(int end) { m_end = end; }

    void start();
    void suspend();
    void stop();

    // FIXME: This function should be private and called at layout time.
    // However <marquee> tests are very timing dependent so we need to keep the existing timing.
    void updateMarqueePosition();

    void timerFired();

private:
    virtual const char* renderName() const OVERRIDE;

    virtual bool isMarquee() const OVERRIDE { return true; }

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    virtual void layoutBlock(bool relayoutChildren) OVERRIDE;

    int m_currentLoop;
    int m_totalLoops;
    Timer<HTMLMarqueeElement> m_timer;
    int m_start;
    int m_end;
    int m_speed;
    Length m_height;
    bool m_reset: 1;
    bool m_suspended : 1;
    bool m_stopped : 1;
    EMarqueeDirection m_direction : 4;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderMarquee, isMarquee());

} // namespace WebCore

#endif // RenderMarquee_h
