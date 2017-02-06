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
  return base::Singleton<TextInputClientMac>::get();
}

void TextInputClientMac::GetStringAtPoint(
    RenderWidgetHost* rwh,
    gfx::Point point,
    void (^reply_handler)(NSAttributedString*, NSPoint)) {
  DCHECK(replyForPointHandler_.get() == nil);
  replyForPointHandler_.reset(reply_handler, base::scoped_policy::RETAIN);
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  rwhi->Send(new TextInputClientMsg_StringAtPoint(rwhi->GetRoutingID(), point));
}

void TextInputClientMac::GetStringAtPointReply(NSAttributedString* string,
                                               NSPoint point) {
  if (replyForPointHandler_.get()) {
    replyForPointHandler_.get()(string, point);
    replyForPointHandler_.reset();
  }
}

void TextInputClientMac::GetStringFromRange(
    RenderWidgetHost* rwh,
    NSRange range,
    void (^reply_handler)(NSAttributedString*, NSPoint)) {
  DCHECK(replyForRangeHandler_.get() == nil);
  replyForRangeHandler_.reset(reply_handler, base::scoped_policy::RETAIN);
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  rwhi->Send(new TextInputClientMsg_StringForRange(rwhi->GetRoutingID(),
                                                   gfx::Range(range)));
}

void TextInputClientMac::GetStringFromRangeReply(NSAttributedString* string,
                                                 NSPoint point) {
  if (replyForRangeHandler_.get()) {
    replyForRangeHandler_.get()(string, point);
    replyForRangeHandler_.reset();
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

void TextInputClientMac::BeforeRequest() {
  base::TimeTicks start = base::TimeTicks::Now();

  lock_.Acquire();

  base::TimeDelta delta(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_LONG_TIMES("TextInputClient.LockWait",
                           delta * base::Time::kMicrosecondsPerMillisecond);

  character_index_ = NSNotFound;
  first_rect_ = NSZeroRect;
}

void TextInputClientMac::AfterRequest() {
  lock_.Release();
}

}  // namespace content
