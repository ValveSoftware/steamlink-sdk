// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTypeBuilderHelper_h
#define InspectorTypeBuilderHelper_h

#include "core/inspector/protocol/Accessibility.h"
#include "modules/ModulesExport.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace protocol::Accessibility;

std::unique_ptr<AXProperty> createProperty(const String& name,
                                           std::unique_ptr<AXValue>);
std::unique_ptr<AXProperty> createProperty(IgnoredReason);

std::unique_ptr<AXValue> createValue(
    const String& value,
    const String& type = AXValueTypeEnum::String);
std::unique_ptr<AXValue> createValue(
    int value,
    const String& type = AXValueTypeEnum::Integer);
std::unique_ptr<AXValue> createValue(
    float value,
    const String& valueType = AXValueTypeEnum::Number);
std::unique_ptr<AXValue> createBooleanValue(
    bool value,
    const String& valueType = AXValueTypeEnum::Boolean);
std::unique_ptr<AXValue> createRelatedNodeListValue(
    const AXObject&,
    String* name = nullptr,
    const String& valueType = AXValueTypeEnum::Idref);
std::unique_ptr<AXValue> createRelatedNodeListValue(AXRelatedObjectVector&,
                                                    const String& valueType);
std::unique_ptr<AXValue> createRelatedNodeListValue(
    AXObject::AXObjectVector& axObjects,
    const String& valueType = AXValueTypeEnum::IdrefList);

std::unique_ptr<AXValueSource> createValueSource(NameSource&);

}  // namespace blink

#endif  // InspectorAccessibilityAgent_h
