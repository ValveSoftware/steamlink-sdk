// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_H_

#include <string>

#include "content/common/content_export.h"

namespace blink {
class WebMediaConstraints;
class WebString;
}

namespace content {

// Method to get boolean value of constraint with |name| from constraints.
// Returns true if the constraint is specified in either mandatory or optional
// constraints.
bool CONTENT_EXPORT GetConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    bool* value);

// Method to get int value of constraint with |name| from constraints.
// Returns true if the constraint is specified in either mandatory or Optional
// constraints.
bool CONTENT_EXPORT GetConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    int* value);

// Method to get std::string value of constraint with |name| from constraints.
// Returns true if the constraint is specified in either mandatory or Optional
// constraints.
bool CONTENT_EXPORT GetConstraintValueAsString(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    std::string* value);

// Method to get boolean value of constraint with |name| from the
// mandatory constraints.
bool CONTENT_EXPORT GetMandatoryConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    bool* value);

// Method to get int value of constraint with |name| from the
// mandatory constraints.
bool CONTENT_EXPORT GetMandatoryConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    int* value);

// Method to get double value of constraint with |name| from the
// mandatory constraints.
bool CONTENT_EXPORT GetMandatoryConstraintValueAsDouble(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    double* value);

// Method to get bool value of constraint with |name| from the
// optional constraints.
bool CONTENT_EXPORT GetOptionalConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    bool* value);

// Method to get int value of constraint with |name| from the
// optional constraints.
bool CONTENT_EXPORT GetOptionalConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    int* value);

// Method to get double value of constraint with |name| from the
// optional constraints.
bool CONTENT_EXPORT GetOptionalConstraintValueAsDouble(
    const blink::WebMediaConstraints& constraints,
    const std::string& name,
    double* value);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_H_
