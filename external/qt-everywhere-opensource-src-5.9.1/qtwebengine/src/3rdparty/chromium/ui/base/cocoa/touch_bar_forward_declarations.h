// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOUCH_BAR_FORWARD_DECLARATIONS_H_
#define UI_BASE_COCOA_TOUCH_BAR_FORWARD_DECLARATIONS_H_

// Once Chrome no longer supports OSX 10.12.0, this file can be deleted.

#import <Foundation/Foundation.h>

#if !defined(MAC_OS_X_VERSION_10_12_1)

// The TouchBar classes do not exist at all without the 10.12.1 SDK. When
// compiling with older SDKs, pretend they are NSObject and add categories to
// NSObject to expose the methods.
// To alloc one of these classes, use -[NSClassFromString(@"..") alloc].

// Incomplete. Add more as necessary.

typedef NSObject NSCustomTouchBarItem;
typedef NSObject NSGroupTouchBarItem;
typedef NSObject NSTouchBar;
typedef NSObject NSTouchBarItem;
typedef NSString* NSTouchBarItemIdentifier;

@protocol NSTouchBarDelegate<NSObject>
@end

@interface NSObject (FakeNSCustomTouchBarItem)
@property(readwrite, strong) NSView* view;
@end

@interface NSObject (FakeNSGroupTouchBarItem)
+ (NSGroupTouchBarItem*)groupItemWithIdentifier:
                            (NSTouchBarItemIdentifier)identifier
                                          items:(NSArray*)items;
@end

@interface NSObject (FakeNSTouchBar)
@property(copy) NSArray* defaultItemIdentifiers;
@property(copy) NSTouchBarItemIdentifier principalItemIdentifier;
@property(weak) id<NSTouchBarDelegate> delegate;
@end

#elif MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12_1

// When compiling against the 10.12.1 SDK or later, just provide forward
// declarations to suppress the partial availability warnings.

@class NSCustomTouchBarItem;
@class NSGroupTouchBarItem;
@class NSTouchBar;
@protocol NSTouchBarDelegate;
@class NSTouchBarItem;

#endif  // MAC_OS_X_VERSION_10_12_1

#endif  // UI_BASE_COCOA_TOUCH_BAR_FORWARD_DECLARATIONS_H_
