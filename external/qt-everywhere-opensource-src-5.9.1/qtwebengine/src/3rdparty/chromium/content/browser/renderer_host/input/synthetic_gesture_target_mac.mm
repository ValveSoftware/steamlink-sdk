// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_mac.h"

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

// Unlike some event APIs, Apple does not provide a way to programmatically
// build a zoom event. To work around this, we leverage ObjectiveC's flexible
// typing and build up an object with the right interface to provide a zoom
// event.
@interface SyntheticPinchEvent : NSObject

// Populated based on desired zoom level.
@property CGFloat magnification;
@property NSPoint locationInWindow;
@property NSEventType type;

// Filled with default values.
@property(readonly) CGFloat deltaX;
@property(readonly) CGFloat deltaY;
@property(readonly) NSEventModifierFlags modifierFlags;
@property(readonly) NSTimeInterval timestamp;

@end

@implementation SyntheticPinchEvent

@synthesize magnification = magnification_;
@synthesize locationInWindow = locationInWindow_;
@synthesize type = type_;
@synthesize deltaX = deltaX_;
@synthesize deltaY = deltaY_;
@synthesize modifierFlags = modifierFlags_;
@synthesize timestamp = timestamp_;

- (id)initWithMagnification:(float)magnification
           locationInWindow:(NSPoint)location {
  self = [super init];
  if (self) {
    type_ = NSEventTypeMagnify;
    magnification_ = magnification;
    locationInWindow_ = location;

    deltaX_ = 0;
    deltaY_ = 0;
    modifierFlags_ = 0;

    // Default timestamp to current time.
    timestamp_ = [[NSDate date] timeIntervalSince1970];
  }

  return self;
}

+ (id)eventWithMagnification:(float)magnification
            locationInWindow:(NSPoint)location {
  SyntheticPinchEvent* event =
      [[SyntheticPinchEvent alloc] initWithMagnification:magnification
                                        locationInWindow:location];
  return [event autorelease];
}

@end

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {

SyntheticGestureTargetMac::SyntheticGestureTargetMac(
    RenderWidgetHostImpl* host,
    RenderWidgetHostViewCocoa* cocoa_view)
    : SyntheticGestureTargetBase(host), cocoa_view_(cocoa_view) {}

void SyntheticGestureTargetMac::DispatchInputEventToPlatform(
    const WebInputEvent& event) {
  if (WebInputEvent::isGestureEventType(event.type)) {
    // Create an autorelease pool so that we clean up any synthetic events we
    // generate.
    base::mac::ScopedNSAutoreleasePool pool;

    const WebGestureEvent* gesture_event =
        static_cast<const WebGestureEvent*>(&event);

    switch (event.type) {
      case WebInputEvent::GesturePinchBegin: {
        id event = [SyntheticPinchEvent
            eventWithMagnification:0.0f
                  locationInWindow:NSMakePoint(gesture_event->x,
                                               gesture_event->y)];
        [cocoa_view_ beginGestureWithEvent:event];
        return;
      }
      case WebInputEvent::GesturePinchEnd: {
        id event = [SyntheticPinchEvent
            eventWithMagnification:0.0f
                  locationInWindow:NSMakePoint(gesture_event->x,
                                               gesture_event->y)];
        [cocoa_view_ endGestureWithEvent:event];
        return;
      }
      case WebInputEvent::GesturePinchUpdate: {
        id event = [SyntheticPinchEvent
            eventWithMagnification:gesture_event->data.pinchUpdate.scale - 1.0f
                  locationInWindow:NSMakePoint(gesture_event->x,
                                               gesture_event->y)];
        [cocoa_view_ magnifyWithEvent:event];
        return;
      }
      default:
        break;
    }
  }

  // This event wasn't handled yet, forward to the base class.
  SyntheticGestureTargetBase::DispatchInputEventToPlatform(event);
}

}  // namespace content
