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

#include "platform/UserGestureIndicator.h"

#include "platform/Histogram.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"

namespace blink {

// User gestures timeout in 1 second.
const double userGestureTimeout = 1.0;

// For out of process tokens we allow a 10 second delay.
const double userGestureOutOfProcessTimeout = 10.0;

UserGestureToken::UserGestureToken(Status status)
    : m_consumableGestures(0),
      m_timestamp(WTF::currentTime()),
      m_timeoutPolicy(Default),
      m_usageCallback(nullptr) {
  if (status == NewGesture || !UserGestureIndicator::currentToken())
    m_consumableGestures++;
}

bool UserGestureToken::hasGestures() const {
  return m_consumableGestures && !hasTimedOut();
}

void UserGestureToken::transferGestureTo(UserGestureToken* other) {
  if (!hasGestures())
    return;
  m_consumableGestures--;
  other->m_consumableGestures++;
}

bool UserGestureToken::consumeGesture() {
  if (!m_consumableGestures)
    return false;
  m_consumableGestures--;
  return true;
}

void UserGestureToken::setTimeoutPolicy(TimeoutPolicy policy) {
  if (!hasTimedOut() && hasGestures() && policy > m_timeoutPolicy)
    m_timeoutPolicy = policy;
}

void UserGestureToken::resetTimestamp() {
  m_timestamp = WTF::currentTime();
}

bool UserGestureToken::hasTimedOut() const {
  if (m_timeoutPolicy == HasPaused)
    return false;
  double timeout = m_timeoutPolicy == OutOfProcess
                       ? userGestureOutOfProcessTimeout
                       : userGestureTimeout;
  return WTF::currentTime() - m_timestamp > timeout;
}

void UserGestureToken::setUserGestureUtilizedCallback(
    UserGestureUtilizedCallback* callback) {
  CHECK(this == UserGestureIndicator::currentToken());
  m_usageCallback = callback;
}

void UserGestureToken::userGestureUtilized() {
  if (m_usageCallback) {
    m_usageCallback->userGestureUtilized();
    m_usageCallback = nullptr;
  }
}

// This enum is used in a histogram, so its values shouldn't change.
enum GestureMergeState {
  OldTokenHasGesture = 1 << 0,
  NewTokenHasGesture = 1 << 1,
  GestureMergeStateEnd = 1 << 2,
};

// Remove this when user gesture propagation is standardized. See
// https://crbug.com/404161.
static void RecordUserGestureMerge(const UserGestureToken& oldToken,
                                   const UserGestureToken& newToken) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, gestureMergeHistogram,
                      ("Blink.Gesture.Merged", GestureMergeStateEnd));
  int sample = 0;
  if (oldToken.hasGestures())
    sample |= OldTokenHasGesture;
  if (newToken.hasGestures())
    sample |= NewTokenHasGesture;
  gestureMergeHistogram.count(sample);
}

UserGestureToken* UserGestureIndicator::s_rootToken = nullptr;

UserGestureIndicator::UserGestureIndicator(PassRefPtr<UserGestureToken> token) {
  // Silently ignore UserGestureIndicators on non-main threads and tokens that
  // are already active.
  if (!isMainThread() || !token || token == s_rootToken)
    return;

  m_token = token;
  if (!s_rootToken) {
    s_rootToken = m_token.get();
  } else {
    RecordUserGestureMerge(*s_rootToken, *m_token);
    m_token->transferGestureTo(s_rootToken);
  }
  m_token->resetTimestamp();
}

UserGestureIndicator::~UserGestureIndicator() {
  if (isMainThread() && m_token && m_token == s_rootToken) {
    s_rootToken->setUserGestureUtilizedCallback(nullptr);
    s_rootToken = nullptr;
  }
}

// static
bool UserGestureIndicator::utilizeUserGesture() {
  if (UserGestureIndicator::processingUserGesture()) {
    s_rootToken->userGestureUtilized();
    return true;
  }
  return false;
}

bool UserGestureIndicator::processingUserGesture() {
  if (auto* token = currentToken()) {
    ASSERT(isMainThread());
    return token->hasGestures();
  }

  return false;
}

// static
bool UserGestureIndicator::consumeUserGesture() {
  if (auto* token = currentToken()) {
    ASSERT(isMainThread());
    if (token->consumeGesture()) {
      token->userGestureUtilized();
      return true;
    }
  }
  return false;
}

// static
UserGestureToken* UserGestureIndicator::currentToken() {
  if (!isMainThread() || !s_rootToken)
    return nullptr;
  return s_rootToken;
}

}  // namespace blink
