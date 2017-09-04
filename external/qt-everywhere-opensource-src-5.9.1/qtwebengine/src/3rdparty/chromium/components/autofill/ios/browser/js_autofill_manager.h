// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_

#include "base/ios/block_types.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

@class CRWJSInjectionReceiver;

// Loads the JavaScript file, autofill_controller.js, which contains form
// parsing and autofill functions.
@interface JsAutofillManager : CRWJSInjectionManager

// Extracts forms from a web page. Only forms with at least |requiredFields|
// fields are extracted.
// |completionHandler| is called with the JSON string of forms of a web page.
// |completionHandler| cannot be nil.
- (void)fetchFormsWithMinimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                               completionHandler:
                                   (void (^)(NSString*))completionHandler;

// Fills the data in JSON string |dataString| into the active form field, then
// executes the |completionHandler|.
- (void)fillActiveFormField:(NSString*)dataString
          completionHandler:(ProceduralBlock)completionHandler;

// Fills a number of fields in the same named form for full-form Autofill.
// Applies Autofill CSS (i.e. yellow background) to filled elements.
// Only empty fields will be filled, except that field named
// |forceFillFieldName| will always be filled even if non-empty.
// |forceFillFieldName| may be null.
// |completionHandler| is called after the forms are filled. |completionHandler|
// cannot be nil.
- (void)fillForm:(NSString*)dataString
    forceFillFieldName:(NSString*)forceFillFieldName
     completionHandler:(ProceduralBlock)completionHandler;

// Clear autofilled fields of the specified form. Fields that are not currently
// autofilled are not modified. Field contents are cleared, and Autofill flag
// and styling are removed. 'change' events are sent for fields whose contents
// changed.
// |completionHandler| is called after the forms are filled. |completionHandler|
// cannot be nil.
- (void)clearAutofilledFieldsForFormNamed:(NSString*)formName
                        completionHandler:(ProceduralBlock)completionHandler;

// Marks up the form with autofill field prediction data (diagnostic tool).
- (void)fillPredictionData:(NSString*)dataString;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_
