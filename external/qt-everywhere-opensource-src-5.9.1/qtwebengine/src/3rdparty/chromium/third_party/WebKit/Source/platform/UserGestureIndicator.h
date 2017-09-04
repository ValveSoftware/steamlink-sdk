/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UserGestureIndicator_h
#define UserGestureIndicator_h

#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

// Callback to be invoked when the state of a UserGestureIndicator is
// used (only during the scope of a UserGestureIndicator, does
// not flow with the UserGestureToken).  It's the responsibility of the
// caller to ensure the UserGestureUtilizedCallback is kept alive as long
// as the UserGestureIndicator it's used in.
// Note that this doesn't currently track EVERY way in which the
// state of a UserGesture can be read (sometimes it's just propagated
// elsewhere, or otherwise read in a way that's hard to know if it will
// actually be used), but should include the primary use cases.  Therefore
// this is suitable mainly for diagnostics and measurement purposes.
class PLATFORM_EXPORT UserGestureUtilizedCallback {
 public:
  virtual ~UserGestureUtilizedCallback() = default;
  virtual void userGestureUtilized() = 0;
};

// A UserGestureToken represents a user gesture. It can be referenced and saved
// for later (see, e.g., DOMTimer, which propagates user gestures to the timer
// fire in certain situations). Passing it to a UserGestureIndicator will cause
// it to be considered as currently being processed.
class PLATFORM_EXPORT UserGestureToken : public RefCounted<UserGestureToken> {
  WTF_MAKE_NONCOPYABLE(UserGestureToken);

 public:
  enum Status { NewGesture, PossiblyExistingGesture };
  enum TimeoutPolicy { Default, OutOfProcess, HasPaused };

  ~UserGestureToken() {}
  bool hasGestures() const;
  void transferGestureTo(UserGestureToken*);
  bool consumeGesture();
  void setTimeoutPolicy(TimeoutPolicy);
  void resetTimestamp();

  // If this UserGestureToken is wrapped in a UserGestureIndicator, and the
  // UserGestureIndicator is the lowest on the callstack (and therefore this
  // UserGestureToken is UserGestureIndicator::s_rootToken), then the callback
  // provided here will be called when this UserGestureToken is utilized.
  // Calling setUserGestureUtilizedCallback() on a UserGestureToken that is not
  // UserGestureIndicator::s_rootToken would be unsafe and never result in a
  // callback, so it will fail a CHECK() instead.
  void setUserGestureUtilizedCallback(UserGestureUtilizedCallback*);
  void userGestureUtilized();

 protected:
  UserGestureToken(Status);

 private:
  bool hasTimedOut() const;

  size_t m_consumableGestures;
  double m_timestamp;
  TimeoutPolicy m_timeoutPolicy;
  UserGestureUtilizedCallback* m_usageCallback;
};

class PLATFORM_EXPORT UserGestureIndicator final {
  USING_FAST_MALLOC(UserGestureIndicator);
  WTF_MAKE_NONCOPYABLE(UserGestureIndicator);

 public:
  // Returns whether a user gesture is currently in progress.
  // Does not invoke the UserGestureUtilizedCallback.  Consider calling
  // utilizeUserGesture instead if you know for sure that the return value
  // will have an effect.
  static bool processingUserGesture();

  // Indicates that a user gesture (if any) is being used, without preventing it
  // from being used again.  Returns whether a user gesture is currently in
  // progress.  If true, invokes (and then clears) any
  // UserGestureUtilizedCallback.
  static bool utilizeUserGesture();

  // Mark the current user gesture (if any) as having been used, such that
  // it cannot be used again.  This is done only for very security-sensitive
  // operations like creating a new process.
  // Like utilizeUserGesture, may invoke/clear any UserGestureUtilizedCallback.
  static bool consumeUserGesture();

  static UserGestureToken* currentToken();

  explicit UserGestureIndicator(PassRefPtr<UserGestureToken>);
  ~UserGestureIndicator();

 private:
  static UserGestureToken* s_rootToken;

  RefPtr<UserGestureToken> m_token;
};

}  // namespace blink

#endif
