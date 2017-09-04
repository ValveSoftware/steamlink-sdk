// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenWakeLock_h
#define ScreenWakeLock_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/PageVisibilityObserver.h"
#include "device/wake_lock/public/interfaces/wake_lock_service.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;
class Screen;

class MODULES_EXPORT ScreenWakeLock final
    : public GarbageCollectedFinalized<ScreenWakeLock>,
      public Supplement<LocalFrame>,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenWakeLock);
  WTF_MAKE_NONCOPYABLE(ScreenWakeLock);

 public:
  static bool keepAwake(Screen&);
  static void setKeepAwake(Screen&, bool);

  static const char* supplementName();
  static ScreenWakeLock* from(LocalFrame*);

  ~ScreenWakeLock() = default;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit ScreenWakeLock(LocalFrame&);

  // Inherited from PageVisibilityObserver.
  void pageVisibilityChanged() override;
  void contextDestroyed() override;

  bool keepAwake() const;
  void setKeepAwake(bool);

  static ScreenWakeLock* fromScreen(Screen&);
  void notifyService();

  device::mojom::blink::WakeLockServicePtr m_service;
  bool m_keepAwake;
};

}  // namespace blink

#endif  // ScreenWakeLock_h
