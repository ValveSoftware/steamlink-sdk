// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations for items in later SDKs than the
// default one with which Chromium is built (currently 10.6).
// If you call any function from this header, be sure to check at runtime for
// respondsToSelector: before calling these functions (else your code will crash
// on older OS X versions that chrome still supports).

#ifndef BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
#define BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

#import <AppKit/AppKit.h>
#import <CoreWLAN/CoreWLAN.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/base_export.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSEventPhaseNone        = 0, // event not associated with a phase.
  NSEventPhaseBegan       = 0x1 << 0,
  NSEventPhaseStationary  = 0x1 << 1,
  NSEventPhaseChanged     = 0x1 << 2,
  NSEventPhaseEnded       = 0x1 << 3,
  NSEventPhaseCancelled   = 0x1 << 4
};
typedef NSUInteger NSEventPhase;

enum {
  NSFullScreenWindowMask = 1 << 14,
};

enum {
  NSApplicationPresentationFullScreen = 1 << 10,
};

enum {
  NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
  NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8,
};

enum {
  NSEventSwipeTrackingLockDirection = 0x1 << 0,
  NSEventSwipeTrackingClampGestureAmount = 0x1 << 1,
};
typedef NSUInteger NSEventSwipeTrackingOptions;

enum {
  NSWindowAnimationBehaviorDefault = 0,
  NSWindowAnimationBehaviorNone = 2,
  NSWindowAnimationBehaviorDocumentWindow = 3,
  NSWindowAnimationBehaviorUtilityWindow = 4,
  NSWindowAnimationBehaviorAlertPanel = 5
};
typedef NSInteger NSWindowAnimationBehavior;

enum {
  NSWindowDocumentVersionsButton = 6,
  NSWindowFullScreenButton,
};
typedef NSUInteger NSWindowButton;

@interface NSEvent (LionSDK)
+ (BOOL)isSwipeTrackingFromScrollEventsEnabled;

- (NSEventPhase)momentumPhase;
- (NSEventPhase)phase;
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options
          dampenAmountThresholdMin:(CGFloat)minDampenThreshold
                               max:(CGFloat)maxDampenThreshold
                      usingHandler:(void (^)(CGFloat gestureAmount,
                                             NSEventPhase phase,
                                             BOOL isComplete,
                                             BOOL *stop))trackingHandler;

- (BOOL)isDirectionInvertedFromDevice;

@end

@interface NSApplication (LionSDK)
- (void)disableRelaunchOnLogin;
@end

@interface CALayer (LionSDK)
- (CGFloat)contentsScale;
- (void)setContentsScale:(CGFloat)contentsScale;
@end

@interface NSScreen (LionSDK)
- (CGFloat)backingScaleFactor;
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

@interface NSWindow (LionSDK)
- (CGFloat)backingScaleFactor;
- (NSWindowAnimationBehavior)animationBehavior;
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
- (void)toggleFullScreen:(id)sender;
- (void)setRestorable:(BOOL)flag;
@end

@interface NSCursor (LionSDKDeclarations)
+ (NSCursor*)IBeamCursorForVerticalLayout;
@end

@interface NSAnimationContext (LionSDK)
+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes
        completionHandler:(void (^)(void))completionHandler;
@property(copy) void(^completionHandler)(void);
@end

@interface NSView (LionSDK)
- (NSSize)convertSizeFromBacking:(NSSize)size;
- (void)setWantsBestResolutionOpenGLSurface:(BOOL)flag;
@end

@interface NSObject (ICCameraDeviceDelegateLionSDK)
- (void)deviceDidBecomeReadyWithCompleteContentCatalog:(ICDevice*)device;
- (void)didDownloadFile:(ICCameraFile*)file
                  error:(NSError*)error
                options:(NSDictionary*)options
            contextInfo:(void*)contextInfo;
@end

@interface NSScroller (LionSDK)
+ (NSInteger)preferredScrollerStyle;
@end

@interface CWInterface (LionSDK)
- (BOOL)associateToNetwork:(CWNetwork*)network
                  password:(NSString*)password
                     error:(NSError**)error;
- (NSSet*)scanForNetworksWithName:(NSString*)networkName
                            error:(NSError**)error;
@end

enum CWChannelBand {
  kCWChannelBandUnknown = 0,
  kCWChannelBand2GHz = 1,
  kCWChannelBand5GHz = 2,
};

@interface CWChannel : NSObject
@property(readonly) CWChannelBand channelBand;
@end

@interface CWNetwork (LionSDK)
@property(readonly) CWChannel* wlanChannel;
@end

@interface IOBluetoothHostController (LionSDK)
- (NSString*)nameAsString;
- (BluetoothHCIPowerState)powerState;
@end

@protocol IOBluetoothDeviceInquiryDelegate
- (void)deviceInquiryStarted:(IOBluetoothDeviceInquiry*)sender;
- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender
                          device:(IOBluetoothDevice*)device;
- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender
                        error:(IOReturn)error
                      aborted:(BOOL)aborted;
@end

@interface IOBluetoothL2CAPChannel (LionSDK)
@property(readonly) BluetoothL2CAPMTU outgoingMTU;
@end

@interface IOBluetoothDevice (LionSDK)
- (NSString*)addressString;
- (unsigned int)classOfDevice;
- (BluetoothConnectionHandle)connectionHandle;
- (BluetoothHCIRSSIValue)rawRSSI;
- (NSArray*)services;
- (IOReturn)performSDPQuery:(id)target uuids:(NSArray*)uuids;
@end

BASE_EXPORT extern "C" NSString* const NSWindowWillEnterFullScreenNotification;

#endif  // MAC_OS_X_VERSION_10_7

#if !defined(MAC_OS_X_VERSION_10_8) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8

enum {
  NSEventPhaseMayBegin    = 0x1 << 5
};

#endif  // MAC_OS_X_VERSION_10_8


#if !defined(MAC_OS_X_VERSION_10_9) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9

// NSProgress is public API in 10.9, but a version of it exists and is usable
// in 10.8.

@interface NSProgress : NSObject

- (instancetype)initWithParent:(NSProgress*)parentProgressOrNil
                      userInfo:(NSDictionary*)userInfoOrNil;
@property (copy) NSString* kind;

@property int64_t totalUnitCount;
@property int64_t completedUnitCount;

@property (getter=isCancellable) BOOL cancellable;
@property (getter=isPausable) BOOL pausable;
@property (readonly, getter=isCancelled) BOOL cancelled;
@property (readonly, getter=isPaused) BOOL paused;
@property (copy) void (^cancellationHandler)(void);
@property (copy) void (^pausingHandler)(void);
- (void)cancel;
- (void)pause;

- (void)setUserInfoObject:(id)objectOrNil forKey:(NSString*)key;
- (NSDictionary*)userInfo;

@property (readonly, getter=isIndeterminate) BOOL indeterminate;
@property (readonly) double fractionCompleted;

- (void)publish;
- (void)unpublish;

@end

@interface NSView (MavericksSDK)
- (void)setCanDrawSubviewsIntoLayer:(BOOL)flag;
@end

enum {
  NSWindowOcclusionStateVisible = 1UL << 1,
};
typedef NSUInteger NSWindowOcclusionState;

@interface NSWindow (MavericksSDK)
- (NSWindowOcclusionState)occlusionState;
@end

#endif  // MAC_OS_X_VERSION_10_9


#if !defined(MAC_OS_X_VERSION_10_10) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_10

enum {
  NSWindowTitleVisible  = 0,
  NSWindowTitleHidden = 1,
  NSWindowTitleHiddenWhenActive = 2,
};
typedef NSInteger NSWindowTitleVisibility;

@interface NSWindow (YosemiteSDK)

@property NSWindowTitleVisibility titleVisibility;

@end

#endif  // MAC_OS_X_VERSION_10_10

#endif  // BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
