// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

// BrowserAccessibilityCocoa is a cocoa wrapper around the BrowserAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to the browser process over IPC.
// This class converts it into a format Cocoa can query.
@interface BrowserAccessibilityCocoa : NSObject {
 @private
  content::BrowserAccessibility* browserAccessibility_;
  base::scoped_nsobject<NSMutableArray> children_;
}

// This creates a cocoa browser accessibility object around
// the cross platform BrowserAccessibility object, which can't be null.
- (id)initWithObject:(content::BrowserAccessibility*)accessibility;

// Clear this object's pointer to the wrapped BrowserAccessibility object
// because the wrapped object has been deleted, but this object may
// persist if the system still has references to it.
- (void)detach;

// Invalidate children for a non-ignored ancestor (including self).
- (void)childrenChanged;

// Convenience method to get the internal, cross-platform role
// from browserAccessibility_.
- (ui::AXRole)internalRole;

// Convenience method to get the BrowserAccessibilityDelegate from
// the manager.
- (content::BrowserAccessibilityDelegate*)delegate;

// Convert the local objet's origin to a global point.
- (NSPoint)pointInScreen:(NSPoint)origin
                    size:(NSSize)size;

// Return the method name for the given attribute. For testing only.
- (NSString*)methodNameForAttribute:(NSString*)attribute;

// Swap the children array with the given scoped_nsobject.
- (void)swapChildren:(base::scoped_nsobject<NSMutableArray>*)other;

// Internally-used method.
@property(nonatomic, readonly) NSPoint origin;

// Children is an array of BrowserAccessibility objects, representing
// the accessibility children of this object.
@property(nonatomic, readonly) NSString* accessKey;
@property(nonatomic, readonly) NSNumber* ariaAtomic;
@property(nonatomic, readonly) NSNumber* ariaBusy;
@property(nonatomic, readonly) NSString* ariaLive;
@property(nonatomic, readonly) NSString* ariaRelevant;
@property(nonatomic, readonly) NSArray* children;
@property(nonatomic, readonly) NSArray* columns;
@property(nonatomic, readonly) NSArray* columnHeaders;
@property(nonatomic, readonly) NSValue* columnIndexRange;
@property(nonatomic, readonly) NSString* description;
@property(nonatomic, readonly) NSNumber* disclosing;
@property(nonatomic, readonly) id disclosedByRow;
@property(nonatomic, readonly) NSNumber* disclosureLevel;
@property(nonatomic, readonly) id disclosedRows;
@property(nonatomic, readonly) NSNumber* enabled;
@property(nonatomic, readonly) NSNumber* focused;
@property(nonatomic, readonly) NSString* help;
// isIgnored returns whether or not the accessibility object
// should be ignored by the accessibility hierarchy.
@property(nonatomic, readonly, getter=isIgnored) BOOL ignored;
// Index of a row, column, or tree item.
@property(nonatomic, readonly) NSNumber* index;
@property(nonatomic, readonly) NSString* invalid;
@property(nonatomic, readonly) NSNumber* loaded;
@property(nonatomic, readonly) NSNumber* loadingProgress;
@property(nonatomic, readonly) NSNumber* maxValue;
@property(nonatomic, readonly) NSNumber* minValue;
@property(nonatomic, readonly) NSNumber* numberOfCharacters;
@property(nonatomic, readonly) NSString* orientation;
@property(nonatomic, readonly) id parent;
@property(nonatomic, readonly) NSValue* position;
@property(nonatomic, readonly) NSNumber* required;
// A string indicating the role of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* role;
@property(nonatomic, readonly) NSString* roleDescription;
@property(nonatomic, readonly) NSArray* rowHeaders;
@property(nonatomic, readonly) NSValue* rowIndexRange;
@property(nonatomic, readonly) NSArray* rows;
// The size of this object.
@property(nonatomic, readonly) NSValue* size;
// A string indicating the subrole of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* subrole;
// The tabs owned by a tablist.
@property(nonatomic, readonly) NSArray* tabs;
@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) id titleUIElement;
@property(nonatomic, readonly) NSURL* url;
@property(nonatomic, readonly) NSString* value;
@property(nonatomic, readonly) NSString* valueDescription;
@property(nonatomic, readonly) NSValue* visibleCharacterRange;
@property(nonatomic, readonly) NSArray* visibleCells;
@property(nonatomic, readonly) NSArray* visibleColumns;
@property(nonatomic, readonly) NSArray* visibleRows;
@property(nonatomic, readonly) NSNumber* visited;
@property(nonatomic, readonly) id window;
@end

#endif // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
