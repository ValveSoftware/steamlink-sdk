// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorTypeBuilderHelper.h"

#include "core/dom/DOMNodeIds.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;
using namespace protocol::Accessibility;

std::unique_ptr<AXProperty> createProperty(const String& name, std::unique_ptr<AXValue> value)
{
    return AXProperty::create().setName(name).setValue(std::move(value)).build();
}

String ignoredReasonName(AXIgnoredReason reason)
{
    switch (reason) {
    case AXActiveModalDialog:
        return "activeModalDialog";
    case AXAncestorDisallowsChild:
        return "ancestorDisallowsChild";
    case AXAncestorIsLeafNode:
        return "ancestorIsLeafNode";
    case AXAriaHidden:
        return "ariaHidden";
    case AXAriaHiddenRoot:
        return "ariaHiddenRoot";
    case AXEmptyAlt:
        return "emptyAlt";
    case AXEmptyText:
        return "emptyText";
    case AXInert:
        return "inert";
    case AXInheritsPresentation:
        return "inheritsPresentation";
    case AXLabelContainer:
        return "labelContainer";
    case AXLabelFor:
        return "labelFor";
    case AXNotRendered:
        return "notRendered";
    case AXNotVisible:
        return "notVisible";
    case AXPresentationalRole:
        return "presentationalRole";
    case AXProbablyPresentational:
        return "probablyPresentational";
    case AXStaticTextUsedAsNameFor:
        return "staticTextUsedAsNameFor";
    case AXUninteresting:
        return "uninteresting";
    }
    ASSERT_NOT_REACHED();
    return "";
}

std::unique_ptr<AXProperty> createProperty(IgnoredReason reason)
{
    if (reason.relatedObject)
        return createProperty(ignoredReasonName(reason.reason), createRelatedNodeListValue(reason.relatedObject, nullptr, AXValueTypeEnum::Idref));
    return createProperty(ignoredReasonName(reason.reason), createBooleanValue(true));
}

std::unique_ptr<AXValue> createValue(const String& value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

std::unique_ptr<AXValue> createValue(int value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

std::unique_ptr<AXValue> createValue(float value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

std::unique_ptr<AXValue> createBooleanValue(bool value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

std::unique_ptr<AXRelatedNode> relatedNodeForAXObject(const AXObject* axObject, String* name = nullptr)
{
    Node* node = axObject->getNode();
    if (!node)
        return nullptr;
    int backendNodeId = DOMNodeIds::idForNode(node);
    if (!backendNodeId)
        return nullptr;
    std::unique_ptr<AXRelatedNode> relatedNode = AXRelatedNode::create().setBackendNodeId(backendNodeId).build();
    if (!node->isElementNode())
        return relatedNode;

    Element* element = toElement(node);
    String idref = element->getIdAttribute();
    if (!idref.isEmpty())
        relatedNode->setIdref(idref);

    if (name)
        relatedNode->setText(*name);
    return relatedNode;
}

std::unique_ptr<AXValue> createRelatedNodeListValue(const AXObject* axObject, String* name, const String& valueType)
{
    std::unique_ptr<protocol::Array<AXRelatedNode>> relatedNodes = protocol::Array<AXRelatedNode>::create();
    relatedNodes->addItem(relatedNodeForAXObject(axObject, name));
    return AXValue::create().setType(valueType).setRelatedNodes(std::move(relatedNodes)).build();
}

std::unique_ptr<AXValue> createRelatedNodeListValue(AXRelatedObjectVector& relatedObjects, const String& valueType)
{
    std::unique_ptr<protocol::Array<AXRelatedNode>> frontendRelatedNodes = protocol::Array<AXRelatedNode>::create();
    for (unsigned i = 0; i < relatedObjects.size(); i++) {
        std::unique_ptr<AXRelatedNode> frontendRelatedNode = relatedNodeForAXObject(relatedObjects[i]->object, &(relatedObjects[i]->text));
        if (frontendRelatedNode)
            frontendRelatedNodes->addItem(std::move(frontendRelatedNode));
    }
    return AXValue::create().setType(valueType).setRelatedNodes(std::move(frontendRelatedNodes)).build();
}

std::unique_ptr<AXValue> createRelatedNodeListValue(AXObject::AXObjectVector& axObjects, const String& valueType)
{
    std::unique_ptr<protocol::Array<AXRelatedNode>> relatedNodes = protocol::Array<AXRelatedNode>::create();
    for (unsigned i = 0; i < axObjects.size(); i++) {
        std::unique_ptr<AXRelatedNode> relatedNode = relatedNodeForAXObject(axObjects[i].get());
        if (relatedNode)
            relatedNodes->addItem(std::move(relatedNode));
    }
    return AXValue::create().setType(valueType).setRelatedNodes(std::move(relatedNodes)).build();
}

String valueSourceType(AXNameFrom nameFrom)
{
    switch (nameFrom) {
    case AXNameFromAttribute:
    case AXNameFromTitle:
    case AXNameFromValue:
        return AXValueSourceTypeEnum::Attribute;
    case AXNameFromContents:
        return AXValueSourceTypeEnum::Contents;
    case AXNameFromPlaceholder:
        return AXValueSourceTypeEnum::Placeholder;
    case AXNameFromCaption:
    case AXNameFromRelatedElement:
        return AXValueSourceTypeEnum::RelatedElement;
    default:
        return AXValueSourceTypeEnum::Implicit; // TODO(aboxhall): what to do here?
    }
}

String nativeSourceType(AXTextFromNativeHTML nativeSource)
{
    switch (nativeSource) {
    case AXTextFromNativeHTMLFigcaption:
        return AXValueNativeSourceTypeEnum::Figcaption;
    case AXTextFromNativeHTMLLabel:
        return AXValueNativeSourceTypeEnum::Label;
    case AXTextFromNativeHTMLLabelFor:
        return AXValueNativeSourceTypeEnum::Labelfor;
    case AXTextFromNativeHTMLLabelWrapped:
        return AXValueNativeSourceTypeEnum::Labelwrapped;
    case AXTextFromNativeHTMLTableCaption:
        return AXValueNativeSourceTypeEnum::Tablecaption;
    case AXTextFromNativeHTMLLegend:
        return AXValueNativeSourceTypeEnum::Legend;
    case AXTextFromNativeHTMLTitleElement:
        return AXValueNativeSourceTypeEnum::Title;
    default:
        return AXValueNativeSourceTypeEnum::Other;
    }
}

std::unique_ptr<AXValueSource> createValueSource(NameSource& nameSource)
{
    String type = valueSourceType(nameSource.type);
    std::unique_ptr<AXValueSource> valueSource = AXValueSource::create().setType(type).build();
    if (!nameSource.relatedObjects.isEmpty()) {
        if (nameSource.attribute == aria_labelledbyAttr || nameSource.attribute == aria_labeledbyAttr) {
            std::unique_ptr<AXValue> attributeValue = createRelatedNodeListValue(nameSource.relatedObjects, AXValueTypeEnum::IdrefList);
            if (!nameSource.attributeValue.isNull())
                attributeValue->setValue(protocol::StringValue::create(nameSource.attributeValue.getString()));
            valueSource->setAttributeValue(std::move(attributeValue));
        } else if (nameSource.attribute == QualifiedName::null()) {
            valueSource->setNativeSourceValue(createRelatedNodeListValue(nameSource.relatedObjects, AXValueTypeEnum::NodeList));
        }
    } else if (!nameSource.attributeValue.isNull()) {
        valueSource->setAttributeValue(createValue(nameSource.attributeValue));
    }
    if (!nameSource.text.isNull())
        valueSource->setValue(createValue(nameSource.text, AXValueTypeEnum::ComputedString));
    if (nameSource.attribute != QualifiedName::null())
        valueSource->setAttribute(nameSource.attribute.localName().getString());
    if (nameSource.superseded)
        valueSource->setSuperseded(true);
    if (nameSource.invalid)
        valueSource->setInvalid(true);
    if (nameSource.nativeSource != AXTextFromNativeHTMLUninitialized)
        valueSource->setNativeSource(nativeSourceType(nameSource.nativeSource));
    return valueSource;
}

} // namespace blink
