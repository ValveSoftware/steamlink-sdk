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

#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class Navigator;
class VibrationController;

enum NavigatorVibrationType {
    MainFrameNoUserGesture = 0,
    MainFrameWithUserGesture = 1,
    SameOriginSubFrameNoUserGesture = 2,
    SameOriginSubFrameWithUserGesture = 3,
    CrossOriginSubFrameNoUserGesture = 4,
    CrossOriginSubFrameWithUserGesture = 5,
    EnumMax = 6
};

class MODULES_EXPORT NavigatorVibration final
    : public GarbageCollectedFinalized<NavigatorVibration>
    , public Supplement<Navigator>
    , public DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(NavigatorVibration);
    WTF_MAKE_NONCOPYABLE(NavigatorVibration);
public:
    using VibrationPattern = Vector<unsigned>;

    virtual ~NavigatorVibration();

    static NavigatorVibration& from(Navigator&);

    static bool vibrate(Navigator&, unsigned time);
    static bool vibrate(Navigator&, const VibrationPattern&);

    VibrationController* controller();

    DECLARE_VIRTUAL_TRACE();

private:
    static const char* supplementName();

    explicit NavigatorVibration(Navigator&);

    // Inherited from DOMWindowProperty.
    void willDetachGlobalObjectFromFrame() override;

    static void collectHistogramMetrics(const LocalFrame&);

    Member<VibrationController> m_controller;
};

} // namespace blink

#endif // NavigatorVibration_h
