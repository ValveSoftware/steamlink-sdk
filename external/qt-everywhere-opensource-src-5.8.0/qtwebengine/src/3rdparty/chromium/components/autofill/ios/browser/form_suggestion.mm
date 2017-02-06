// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion.h"

@interface FormSuggestion ()
// Local initializer for a FormSuggestion.
- (id)initWithValue:(NSString*)value
    displayDescription:(NSString*)displayDescription
                  icon:(NSString*)icon
            identifier:(NSInteger)identifier;
@end

@implementation FormSuggestion {
  NSString* _value;
  NSString* _displayDescription;
  NSString* _icon;
  NSInteger _identifier;
  base::mac::ObjCPropertyReleaser _propertyReleaser_FormSuggestion;
}

@synthesize value = _value;
@synthesize displayDescription = _displayDescription;
@synthesize icon = _icon;
@synthesize identifier = _identifier;

- (id)initWithValue:(NSString*)value
    displayDescription:(NSString*)displayDescription
                  icon:(NSString*)icon
            identifier:(NSInteger)identifier {
  self = [super init];
  if (self) {
    _propertyReleaser_FormSuggestion.Init(self, [FormSuggestion class]);
    _value = [value copy];
    _displayDescription = [displayDescription copy];
    _icon = [icon copy];
    _identifier = identifier;
  }
  return self;
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSInteger)identifier {
  return [[[FormSuggestion alloc] initWithValue:value
                             displayDescription:displayDescription
                                           icon:icon
                                     identifier:identifier] autorelease];
}

@end
