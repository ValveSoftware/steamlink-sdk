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

#ifndef VibrationController_h
#define VibrationController_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/PageLifecycleObserver.h"
#include "device/vibration/vibration_manager.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "platform/Timer.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class Document;
class ExecutionContext;
class UnsignedLongOrUnsignedLongSequence;

class MODULES_EXPORT VibrationController final
    : public GarbageCollectedFinalized<VibrationController>
    , public ContextLifecycleObserver
    , public PageLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(VibrationController);
    WTF_MAKE_NONCOPYABLE(VibrationController);
public:
    using VibrationPattern = Vector<unsigned>;

    explicit VibrationController(Document&);
    virtual ~VibrationController();

    static VibrationPattern sanitizeVibrationPattern(const UnsignedLongOrUnsignedLongSequence&);

    bool vibrate(const VibrationPattern&);
    void doVibrate(Timer<VibrationController>*);
    void didVibrate();

    // Cancels the ongoing vibration if there is one.
    void cancel();
    void didCancel();

    // Whether a pattern is being processed. If this is true, the vibration
    // hardware may currently be active, but during a pause it may be inactive.
    bool isRunning() const { return m_isRunning; }

    VibrationPattern pattern() const { return m_pattern; }

    DECLARE_VIRTUAL_TRACE();

private:
    // Inherited from ContextLifecycleObserver AND PageLifecycleObserver.
    void contextDestroyed() override;

    // Inherited from PageLifecycleObserver.
    void pageVisibilityChanged() override;

    // The VibrationManager mojo service. This is reset in |contextDestroyed|
    // and must not be called or recreated after it is reset.
    device::blink::VibrationManagerPtr m_service;

    // Timer for calling |doVibrate| after a delay. It is safe to call
    // |startOneshot| when the timer is already running: it may affect the time
    // at which it fires, but |doVibrate| will still be called only once.
    Timer<VibrationController> m_timerDoVibrate;

    // Whether a pattern is being processed. The vibration hardware may
    // currently be active, or during a pause it may be inactive.
    bool m_isRunning;

    // Whether an async mojo call to cancel is pending.
    bool m_isCallingCancel;

    // Whether an async mojo call to vibrate is pending.
    bool m_isCallingVibrate;

    VibrationPattern m_pattern;
};

} // namespace blink

#endif // VibrationController_h
