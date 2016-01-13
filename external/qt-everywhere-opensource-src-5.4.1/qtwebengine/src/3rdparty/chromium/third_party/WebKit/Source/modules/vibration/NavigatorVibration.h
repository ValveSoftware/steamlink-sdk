/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef NavigatorVibration_h
#define NavigatorVibration_h

#include "core/page/Page.h"
#include "core/page/PageLifecycleObserver.h"
#include "platform/Timer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class LocalFrame;
class Navigator;

class NavigatorVibration FINAL
    : public NoBaseWillBeGarbageCollectedFinalized<NavigatorVibration>
    , public WillBeHeapSupplement<Page>
    , public PageLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorVibration);
public:
    typedef Vector<unsigned> VibrationPattern;

    virtual ~NavigatorVibration();

    bool vibrate(const VibrationPattern&);
    void cancelVibration();
    void timerStartFired(Timer<NavigatorVibration>*);
    void timerStopFired(Timer<NavigatorVibration>*);

    // Inherited from PageLifecycleObserver
    virtual void pageVisibilityChanged() OVERRIDE;
    virtual void didCommitLoad(LocalFrame*) OVERRIDE;

    static bool vibrate(Navigator&, unsigned time);
    static bool vibrate(Navigator&, const VibrationPattern&);
    static NavigatorVibration& from(Page&);

    bool isVibrating() const { return m_isVibrating; }

    VibrationPattern pattern() const { return m_pattern; }

    virtual void trace(Visitor* visitor) OVERRIDE { WillBeHeapSupplement<Page>::trace(visitor); }

private:
    explicit NavigatorVibration(Page&);
    static const char* supplementName();

    Timer<NavigatorVibration> m_timerStart;
    Timer<NavigatorVibration> m_timerStop;
    bool m_isVibrating;
    VibrationPattern m_pattern;
};

} // namespace WebCore

#endif // NavigatorVibration_h
