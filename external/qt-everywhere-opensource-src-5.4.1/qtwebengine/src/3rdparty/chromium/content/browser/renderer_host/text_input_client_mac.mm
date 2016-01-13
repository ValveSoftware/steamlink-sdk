// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/text_input_client_messages.h"

namespace content {

// The amount of time in milliseconds that the browser process will wait for a
// response from the renderer.
// TODO(rsesek): Using the histogram data, find the best upper-bound for this
// value.
const float kWaitTimeout = 1500;

TextInputClientMac::TextInputClientMac()
    : character_index_(NSNotFound),
      lock_(),
      condition_(&lock_) {
}

TextInputClientMac::~TextInputClientMac() {
}

// static
TextInputClientMac* TextInputClientMac::GetInstance() {
  return Singleton<TextInputClientMac>::get();
}

void TextInputClientMac::GetStringAtPoint(
    RenderWidgetHost* rwh,
    gfx::Point point,
    void (^replyHandler)(NSAttributedString*, NSPoint)) {
  DCHECK(replyHandler_.get() == nil);
  replyHandler_.reset(replyHandler, base::scoped_policy::RETAIN);
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  rwhi->Send(new TextInputClientMsg_StringAtPoint(rwhi->GetRoutingID(), point));
}

void TextInputClientMac::GetStringAtPointReply(NSAttributedString* string,
                                               NSPoint point) {
  if (replyHandler_.get()) {
    replyHandler_.get()(string, point);
    replyHandler_.reset();
  }
}

NSUInteger TextInputClientMac::GetCharacterIndexAtPoint(RenderWidgetHost* rwh,
    gfx::Point point) {
  base::TimeTicks start = base::TimeTicks::Now();

  BeforeRequest();
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  rwhi->Send(new TextInputClientMsg_CharacterIndexForPoint(rwhi->GetRoutingID(),
                                                          point));
  // http://crbug.com/121917
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  condition_.TimedWait(base::TimeDelta::FromMilliseconds(kWaitTimeout));
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.CharacterIndex",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return character_index_;
}

NSRect TextInputClientMac::GetFirstRectForRange(RenderWidgetHost* rwh,
    NSRange range) {
  base::TimeTicks start = base::TimeTicks::Now();

  BeforeRequest();
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  rwhi->Send(
      new TextInputClientMsg_FirstRectForCharacterRange(rwhi->GetRoutingID(),
                                                        gfx::Range(range)));
  // http://crbug.com/121917
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  condition_.TimedWait(base::TimeDelta::FromMilliseconds(kWaitTimeout));
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.FirstRect",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  return first_rect_;
}

NSAttributedString* TextInputClientMac::GetAttributedSubstringFromRange(
    RenderWidgetHost* rwh,
    NSRange range) {
  base::TimeTicks start = base::TimeTicks::Now();

  BeforeRequest();
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  rwhi->Send(new TextInputClientMsg_StringForRange(rwhi->GetRoutingID(),
                                                   gfx::Range(range)));
  // http://crbug.com/121917
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  condition_.TimedWait(base::TimeDelta::FromMilliseconds(kWaitTimeout));
  AfterRequest();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.Substring",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  // Lookup.framework calls this method repeatedly and expects that repeated
  // calls don't deallocate previous results immediately. Returning an
  // autoreleased string is better convention anyway.
  return [[substring_.get() retain] autorelease];
}

void TextInputClientMac::SetCharacterIndexAndSignal(NSUInteger index) {
  lock_.Acquire();
  character_index_ = index;
  lock_.Release();
  condition_.Signal();
}

void TextInputClientMac::SetFirstRectAndSignal(NSRect first_rect) {
  lock_.Acquire();
  first_rect_ = first_rect;
  lock_.Release();
  condition_.Signal();
}

void TextInputClientMac::SetSubstringAndSignal(NSAttributedString* string) {
  lock_.Acquire();
  substring_.reset([string copy]);
  lock_.Release();
  condition_.Signal();
}

void TextInputClientMac::BeforeRequest() {
  base::TimeTicks start = base::TimeTicks::Now();

  lock_.Acquire();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.LockWait",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  character_index_ = NSNotFound;
  first_rect_ = NSZeroRect;
  substring_.reset();
}

void TextInputClientMac::AfterRequest() {
  lock_.Release();
}

}  // namespace content
