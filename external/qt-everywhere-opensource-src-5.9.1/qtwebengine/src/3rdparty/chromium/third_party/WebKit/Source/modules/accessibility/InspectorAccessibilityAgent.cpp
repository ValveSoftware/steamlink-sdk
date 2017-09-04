// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorAccessibilityAgent.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/InspectorTypeBuilderHelper.h"
#include <memory>

namespace blink {

using protocol::Accessibility::AXGlobalStates;
using protocol::Accessibility::AXLiveRegionAttributes;
using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;
using protocol::Accessibility::AXProperty;
using protocol::Accessibility::AXValueSource;
using protocol::Accessibility::AXValueType;
using protocol::Accessibility::AXRelatedNode;
using protocol::Accessibility::AXRelationshipAttributes;
using protocol::Accessibility::AXValue;
using protocol::Accessibility::AXWidgetAttributes;
using protocol::Accessibility::AXWidgetStates;

using namespace HTMLNames;

namespace {

static const AXID kIDForInspectedNodeWithNoAXNode = 0;

void fillLiveRegionProperties(AXObject& axObject,
                              protocol::Array<AXProperty>& properties) {
  if (!axObject.liveRegionRoot())
    return;

  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Live,
                     createValue(axObject.containerLiveRegionStatus(),
                                 AXValueTypeEnum::Token)));
  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Atomic,
                     createBooleanValue(axObject.containerLiveRegionAtomic())));
  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Relevant,
                     createValue(axObject.containerLiveRegionRelevant(),
                                 AXValueTypeEnum::TokenList)));
  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Busy,
                     createBooleanValue(axObject.containerLiveRegionBusy())));

  if (!axObject.isLiveRegion()) {
    properties.addItem(createProperty(
        AXLiveRegionAttributesEnum::Root,
        createRelatedNodeListValue(*(axObject.liveRegionRoot()))));
  }
}

void fillGlobalStates(AXObject& axObject,
                      protocol::Array<AXProperty>& properties) {
  if (!axObject.isEnabled())
    properties.addItem(
        createProperty(AXGlobalStatesEnum::Disabled, createBooleanValue(true)));

  if (const AXObject* hiddenRoot = axObject.ariaHiddenRoot()) {
    properties.addItem(
        createProperty(AXGlobalStatesEnum::Hidden, createBooleanValue(true)));
    properties.addItem(createProperty(AXGlobalStatesEnum::HiddenRoot,
                                      createRelatedNodeListValue(*hiddenRoot)));
  }

  InvalidState invalidState = axObject.getInvalidState();
  switch (invalidState) {
    case InvalidStateUndefined:
      break;
    case InvalidStateFalse:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("false", AXValueTypeEnum::Token)));
      break;
    case InvalidStateTrue:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("true", AXValueTypeEnum::Token)));
      break;
    case InvalidStateSpelling:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("spelling", AXValueTypeEnum::Token)));
      break;
    case InvalidStateGrammar:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("grammar", AXValueTypeEnum::Token)));
      break;
    default:
      // TODO(aboxhall): expose invalid: <nothing> and source: aria-invalid as
      // invalid value
      properties.addItem(createProperty(
          AXGlobalStatesEnum::Invalid,
          createValue(axObject.ariaInvalidValue(), AXValueTypeEnum::String)));
      break;
  }
}

bool roleAllowsMultiselectable(AccessibilityRole role) {
  return role == GridRole || role == ListBoxRole || role == TabListRole ||
         role == TreeGridRole || role == TreeRole;
}

bool roleAllowsOrientation(AccessibilityRole role) {
  return role == ScrollBarRole || role == SplitterRole || role == SliderRole;
}

bool roleAllowsReadonly(AccessibilityRole role) {
  return role == GridRole || role == CellRole || role == TextFieldRole ||
         role == ColumnHeaderRole || role == RowHeaderRole ||
         role == TreeGridRole;
}

bool roleAllowsRequired(AccessibilityRole role) {
  return role == ComboBoxRole || role == CellRole || role == ListBoxRole ||
         role == RadioGroupRole || role == SpinButtonRole ||
         role == TextFieldRole || role == TreeRole ||
         role == ColumnHeaderRole || role == RowHeaderRole ||
         role == TreeGridRole;
}

bool roleAllowsSort(AccessibilityRole role) {
  return role == ColumnHeaderRole || role == RowHeaderRole;
}

bool roleAllowsChecked(AccessibilityRole role) {
  return role == MenuItemCheckBoxRole || role == MenuItemRadioRole ||
         role == RadioButtonRole || role == CheckBoxRole ||
         role == TreeItemRole || role == ListBoxOptionRole ||
         role == SwitchRole;
}

bool roleAllowsSelected(AccessibilityRole role) {
  return role == CellRole || role == ListBoxOptionRole || role == RowRole ||
         role == TabRole || role == ColumnHeaderRole ||
         role == MenuItemRadioRole || role == RadioButtonRole ||
         role == RowHeaderRole || role == TreeItemRole;
}

void fillWidgetProperties(AXObject& axObject,
                          protocol::Array<AXProperty>& properties) {
  AccessibilityRole role = axObject.roleValue();
  String autocomplete = axObject.ariaAutoComplete();
  if (!autocomplete.isEmpty())
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Autocomplete,
                       createValue(autocomplete, AXValueTypeEnum::Token)));

  if (axObject.hasAttribute(HTMLNames::aria_haspopupAttr)) {
    bool hasPopup = axObject.ariaHasPopup();
    properties.addItem(createProperty(AXWidgetAttributesEnum::Haspopup,
                                      createBooleanValue(hasPopup)));
  }

  int headingLevel = axObject.headingLevel();
  if (headingLevel > 0) {
    properties.addItem(createProperty(AXWidgetAttributesEnum::Level,
                                      createValue(headingLevel)));
  }
  int hierarchicalLevel = axObject.hierarchicalLevel();
  if (hierarchicalLevel > 0 ||
      axObject.hasAttribute(HTMLNames::aria_levelAttr)) {
    properties.addItem(createProperty(AXWidgetAttributesEnum::Level,
                                      createValue(hierarchicalLevel)));
  }

  if (roleAllowsMultiselectable(role)) {
    bool multiselectable = axObject.isMultiSelectable();
    properties.addItem(createProperty(AXWidgetAttributesEnum::Multiselectable,
                                      createBooleanValue(multiselectable)));
  }

  if (roleAllowsOrientation(role)) {
    AccessibilityOrientation orientation = axObject.orientation();
    switch (orientation) {
      case AccessibilityOrientationVertical:
        properties.addItem(
            createProperty(AXWidgetAttributesEnum::Orientation,
                           createValue("vertical", AXValueTypeEnum::Token)));
        break;
      case AccessibilityOrientationHorizontal:
        properties.addItem(
            createProperty(AXWidgetAttributesEnum::Orientation,
                           createValue("horizontal", AXValueTypeEnum::Token)));
        break;
      case AccessibilityOrientationUndefined:
        break;
    }
  }

  if (role == TextFieldRole) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Multiline,
                       createBooleanValue(axObject.isMultiline())));
  }

  if (roleAllowsReadonly(role)) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Readonly,
                       createBooleanValue(axObject.isReadOnly())));
  }

  if (roleAllowsRequired(role)) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Required,
                       createBooleanValue(axObject.isRequired())));
  }

  if (roleAllowsSort(role)) {
    // TODO(aboxhall): sort
  }

  if (axObject.isRange()) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Valuemin,
                       createValue(axObject.minValueForRange())));
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Valuemax,
                       createValue(axObject.maxValueForRange())));
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Valuetext,
                       createValue(axObject.valueDescription())));
  }
}

void fillWidgetStates(AXObject& axObject,
                      protocol::Array<AXProperty>& properties) {
  AccessibilityRole role = axObject.roleValue();
  if (roleAllowsChecked(role)) {
    AccessibilityButtonState checked = axObject.checkboxOrRadioValue();
    switch (checked) {
      case ButtonStateOff:
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Checked,
                           createValue("false", AXValueTypeEnum::Tristate)));
        break;
      case ButtonStateOn:
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Checked,
                           createValue("true", AXValueTypeEnum::Tristate)));
        break;
      case ButtonStateMixed:
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Checked,
                           createValue("mixed", AXValueTypeEnum::Tristate)));
        break;
    }
  }

  AccessibilityExpanded expanded = axObject.isExpanded();
  switch (expanded) {
    case ExpandedUndefined:
      break;
    case ExpandedCollapsed:
      properties.addItem(createProperty(
          AXWidgetStatesEnum::Expanded,
          createBooleanValue(false, AXValueTypeEnum::BooleanOrUndefined)));
      break;
    case ExpandedExpanded:
      properties.addItem(createProperty(
          AXWidgetStatesEnum::Expanded,
          createBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
      break;
  }

  if (role == ToggleButtonRole) {
    if (!axObject.isPressed()) {
      properties.addItem(
          createProperty(AXWidgetStatesEnum::Pressed,
                         createValue("false", AXValueTypeEnum::Tristate)));
    } else {
      const AtomicString& pressedAttr =
          axObject.getAttribute(HTMLNames::aria_pressedAttr);
      if (equalIgnoringCase(pressedAttr, "mixed"))
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Pressed,
                           createValue("mixed", AXValueTypeEnum::Tristate)));
      else
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Pressed,
                           createValue("true", AXValueTypeEnum::Tristate)));
    }
  }

  if (roleAllowsSelected(role)) {
    properties.addItem(
        createProperty(AXWidgetStatesEnum::Selected,
                       createBooleanValue(axObject.isSelected())));
  }
}

std::unique_ptr<AXProperty> createRelatedNodeListProperty(
    const String& key,
    AXRelatedObjectVector& nodes) {
  std::unique_ptr<AXValue> nodeListValue =
      createRelatedNodeListValue(nodes, AXValueTypeEnum::NodeList);
  return createProperty(key, std::move(nodeListValue));
}

std::unique_ptr<AXProperty> createRelatedNodeListProperty(
    const String& key,
    AXObject::AXObjectVector& nodes,
    const QualifiedName& attr,
    AXObject& axObject) {
  std::unique_ptr<AXValue> nodeListValue = createRelatedNodeListValue(nodes);
  const AtomicString& attrValue = axObject.getAttribute(attr);
  nodeListValue->setValue(protocol::StringValue::create(attrValue));
  return createProperty(key, std::move(nodeListValue));
}

void fillRelationships(AXObject& axObject,
                       protocol::Array<AXProperty>& properties) {
  if (AXObject* activeDescendant = axObject.activeDescendant()) {
    properties.addItem(
        createProperty(AXRelationshipAttributesEnum::Activedescendant,
                       createRelatedNodeListValue(*activeDescendant)));
  }

  AXObject::AXObjectVector results;
  axObject.ariaFlowToElements(results);
  if (!results.isEmpty())
    properties.addItem(
        createRelatedNodeListProperty(AXRelationshipAttributesEnum::Flowto,
                                      results, aria_flowtoAttr, axObject));
  results.clear();

  axObject.ariaControlsElements(results);
  if (!results.isEmpty())
    properties.addItem(
        createRelatedNodeListProperty(AXRelationshipAttributesEnum::Controls,
                                      results, aria_controlsAttr, axObject));
  results.clear();

  axObject.ariaDescribedbyElements(results);
  if (!results.isEmpty())
    properties.addItem(
        createRelatedNodeListProperty(AXRelationshipAttributesEnum::Describedby,
                                      results, aria_describedbyAttr, axObject));
  results.clear();

  axObject.ariaOwnsElements(results);
  if (!results.isEmpty())
    properties.addItem(createRelatedNodeListProperty(
        AXRelationshipAttributesEnum::Owns, results, aria_ownsAttr, axObject));
  results.clear();
}

std::unique_ptr<AXValue> createRoleNameValue(AccessibilityRole role) {
  AtomicString roleName = AXObject::roleName(role);
  std::unique_ptr<AXValue> roleNameValue;
  if (!roleName.isNull()) {
    roleNameValue = createValue(roleName, AXValueTypeEnum::Role);
  } else {
    roleNameValue = createValue(AXObject::internalRoleName(role),
                                AXValueTypeEnum::InternalRole);
  }
  return roleNameValue;
}

bool isAncestorOf(AXObject* possibleAncestor, AXObject* descendant) {
  if (!possibleAncestor || !descendant)
    return false;

  AXObject* ancestor = descendant;
  while (ancestor) {
    if (ancestor == possibleAncestor)
      return true;
    ancestor = ancestor->parentObject();
  }
  return false;
}

}  // namespace

InspectorAccessibilityAgent::InspectorAccessibilityAgent(
    Page* page,
    InspectorDOMAgent* domAgent)
    : m_page(page), m_domAgent(domAgent) {}

Response InspectorAccessibilityAgent::getPartialAXTree(
    int domNodeId,
    Maybe<bool> fetchRelatives,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  if (!m_domAgent->enabled())
    return Response::Error("DOM agent must be enabled");
  Node* domNode = nullptr;
  Response response = m_domAgent->assertNode(domNodeId, domNode);
  if (!response.isSuccess())
    return response;

  Document& document = domNode->document();
  document.updateStyleAndLayoutIgnorePendingStylesheets();
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document.lifecycle());
  LocalFrame* localFrame = document.frame();
  if (!localFrame)
    return Response::Error("Frame is detached.");
  std::unique_ptr<ScopedAXObjectCache> scopedCache =
      ScopedAXObjectCache::create(document);
  AXObjectCacheImpl* cache = toAXObjectCacheImpl(scopedCache->get());

  AXObject* inspectedAXObject = cache->getOrCreate(domNode);
  *nodes = protocol::Array<protocol::Accessibility::AXNode>::create();
  if (!inspectedAXObject || inspectedAXObject->accessibilityIsIgnored()) {
    (*nodes)->addItem(buildObjectForIgnoredNode(domNode, inspectedAXObject,
                                                fetchRelatives.fromMaybe(true),
                                                *nodes, *cache));
  } else {
    (*nodes)->addItem(
        buildProtocolAXObject(*inspectedAXObject, inspectedAXObject,
                              fetchRelatives.fromMaybe(true), *nodes, *cache));
  }

  if (!inspectedAXObject || !inspectedAXObject->isAXLayoutObject())
    return Response::OK();

  AXObject* parent = inspectedAXObject->parentObjectUnignored();
  if (!parent)
    return Response::OK();

  if (fetchRelatives.fromMaybe(true))
    addAncestors(*parent, inspectedAXObject, *nodes, *cache);

  return Response::OK();
}

void InspectorAccessibilityAgent::addAncestors(
    AXObject& firstAncestor,
    AXObject* inspectedAXObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* ancestor = &firstAncestor;
  while (ancestor) {
    nodes->addItem(buildProtocolAXObject(*ancestor, inspectedAXObject, true,
                                         nodes, cache));
    ancestor = ancestor->parentObjectUnignored();
  }
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::buildObjectForIgnoredNode(
    Node* domNode,
    AXObject* axObject,
    bool fetchRelatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject::IgnoredReasons ignoredReasons;
  AXID axID = kIDForInspectedNodeWithNoAXNode;
  if (axObject && axObject->isAXLayoutObject())
    axID = axObject->axObjectID();
  std::unique_ptr<AXNode> ignoredNodeObject =
      AXNode::create().setNodeId(String::number(axID)).setIgnored(true).build();
  AccessibilityRole role = AccessibilityRole::IgnoredRole;
  ignoredNodeObject->setRole(createRoleNameValue(role));

  if (axObject && axObject->isAXLayoutObject()) {
    axObject->computeAccessibilityIsIgnored(&ignoredReasons);

    if (fetchRelatives) {
      populateRelatives(*axObject, axObject, *(ignoredNodeObject.get()), nodes,
                        cache);
    }
  } else if (domNode && !domNode->layoutObject()) {
    if (fetchRelatives) {
      populateDOMNodeRelatives(*domNode, *(ignoredNodeObject.get()), nodes,
                               cache);
    }
    ignoredReasons.append(IgnoredReason(AXNotRendered));
  }

  if (domNode)
    ignoredNodeObject->setBackendDOMNodeId(DOMNodeIds::idForNode(domNode));

  std::unique_ptr<protocol::Array<AXProperty>> ignoredReasonProperties =
      protocol::Array<AXProperty>::create();
  for (size_t i = 0; i < ignoredReasons.size(); i++)
    ignoredReasonProperties->addItem(createProperty(ignoredReasons[i]));
  ignoredNodeObject->setIgnoredReasons(std::move(ignoredReasonProperties));

  return ignoredNodeObject;
}

void InspectorAccessibilityAgent::populateDOMNodeRelatives(
    Node& inspectedDOMNode,
    AXNode& nodeObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  // Populate children.
  std::unique_ptr<protocol::Array<AXNodeId>> childIds =
      protocol::Array<AXNodeId>::create();
  findDOMNodeChildren(childIds, inspectedDOMNode, inspectedDOMNode, nodes,
                      cache);
  nodeObject.setChildIds(std::move(childIds));

  // Walk up parents until an AXObject can be found.
  Node* parentNode = FlatTreeTraversal::parent(inspectedDOMNode);
  AXObject* parentAXObject = cache.getOrCreate(parentNode);
  while (parentNode && !parentAXObject) {
    parentNode = FlatTreeTraversal::parent(inspectedDOMNode);
    parentAXObject = cache.getOrCreate(parentNode);
  };

  if (!parentAXObject)
    return;

  if (parentAXObject->accessibilityIsIgnored())
    parentAXObject = parentAXObject->parentObjectUnignored();
  if (!parentAXObject)
    return;

  // Populate parent, ancestors and siblings.
  std::unique_ptr<AXNode> parentNodeObject =
      buildProtocolAXObject(*parentAXObject, nullptr, true, nodes, cache);
  std::unique_ptr<protocol::Array<AXNodeId>> siblingIds =
      protocol::Array<AXNodeId>::create();
  findDOMNodeChildren(siblingIds, *parentNode, inspectedDOMNode, nodes, cache);
  parentNodeObject->setChildIds(std::move(siblingIds));
  nodes->addItem(std::move(parentNodeObject));

  AXObject* grandparentAXObject = parentAXObject->parentObjectUnignored();
  if (grandparentAXObject)
    addAncestors(*grandparentAXObject, nullptr, nodes, cache);
}

void InspectorAccessibilityAgent::findDOMNodeChildren(
    std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
    Node& parentNode,
    Node& inspectedDOMNode,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  NodeList* childNodes = parentNode.childNodes();
  for (size_t i = 0; i < childNodes->length(); ++i) {
    Node* childNode = childNodes->item(i);
    if (childNode == &inspectedDOMNode) {
      childIds->addItem(String::number(kIDForInspectedNodeWithNoAXNode));
      continue;
    }

    AXObject* childAXObject = cache.getOrCreate(childNode);
    if (childAXObject) {
      if (childAXObject->accessibilityIsIgnored()) {
        // search for un-ignored descendants
        findDOMNodeChildren(childIds, *childNode, inspectedDOMNode, nodes,
                            cache);
      } else {
        addChild(childIds, *childAXObject, nullptr, nodes, cache);
      }

      continue;
    }

    if (!childAXObject ||
        childNode->isShadowIncludingInclusiveAncestorOf(&inspectedDOMNode)) {
      // If the inspected node may be a descendant of this node, keep walking
      // recursively until we find its actual siblings.
      findDOMNodeChildren(childIds, *childNode, inspectedDOMNode, nodes, cache);
      continue;
    }

    // Otherwise, just add the un-ignored children.
    const AXObject::AXObjectVector& indirectChildren =
        childAXObject->children();
    for (unsigned i = 0; i < indirectChildren.size(); ++i)
      addChild(childIds, *(indirectChildren[i]), nullptr, nodes, cache);
  }
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::buildProtocolAXObject(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    bool fetchRelatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AccessibilityRole role = axObject.roleValue();
  std::unique_ptr<AXNode> nodeObject =
      AXNode::create()
          .setNodeId(String::number(axObject.axObjectID()))
          .setIgnored(false)
          .build();
  nodeObject->setRole(createRoleNameValue(role));

  std::unique_ptr<protocol::Array<AXProperty>> properties =
      protocol::Array<AXProperty>::create();
  fillLiveRegionProperties(axObject, *(properties.get()));
  fillGlobalStates(axObject, *(properties.get()));
  fillWidgetProperties(axObject, *(properties.get()));
  fillWidgetStates(axObject, *(properties.get()));
  fillRelationships(axObject, *(properties.get()));

  AXObject::NameSources nameSources;
  String computedName = axObject.name(&nameSources);
  if (!nameSources.isEmpty()) {
    std::unique_ptr<AXValue> name =
        createValue(computedName, AXValueTypeEnum::ComputedString);
    if (!nameSources.isEmpty()) {
      std::unique_ptr<protocol::Array<AXValueSource>> nameSourceProperties =
          protocol::Array<AXValueSource>::create();
      for (size_t i = 0; i < nameSources.size(); ++i) {
        NameSource& nameSource = nameSources[i];
        nameSourceProperties->addItem(createValueSource(nameSource));
        if (nameSource.text.isNull() || nameSource.superseded)
          continue;
        if (!nameSource.relatedObjects.isEmpty()) {
          properties->addItem(createRelatedNodeListProperty(
              AXRelationshipAttributesEnum::Labelledby,
              nameSource.relatedObjects));
        }
      }
      name->setSources(std::move(nameSourceProperties));
    }
    nodeObject->setProperties(std::move(properties));
    nodeObject->setName(std::move(name));
  } else {
    nodeObject->setProperties(std::move(properties));
  }

  fillCoreProperties(axObject, inspectedAXObject, fetchRelatives,
                     *(nodeObject.get()), nodes, cache);
  return nodeObject;
}

void InspectorAccessibilityAgent::fillCoreProperties(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    bool fetchRelatives,
    AXNode& nodeObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXNameFrom nameFrom;
  AXObject::AXObjectVector nameObjects;
  axObject.name(nameFrom, &nameObjects);

  AXDescriptionFrom descriptionFrom;
  AXObject::AXObjectVector descriptionObjects;
  String description =
      axObject.description(nameFrom, descriptionFrom, &descriptionObjects);
  if (!description.isEmpty()) {
    nodeObject.setDescription(
        createValue(description, AXValueTypeEnum::ComputedString));
  }
  // Value.
  if (axObject.supportsRangeValue()) {
    nodeObject.setValue(createValue(axObject.valueForRange()));
  } else {
    String stringValue = axObject.stringValue();
    if (!stringValue.isEmpty())
      nodeObject.setValue(createValue(stringValue));
  }

  if (fetchRelatives)
    populateRelatives(axObject, inspectedAXObject, nodeObject, nodes, cache);

  Node* node = axObject.getNode();
  if (node)
    nodeObject.setBackendDOMNodeId(DOMNodeIds::idForNode(node));
}

void InspectorAccessibilityAgent::populateRelatives(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    AXNode& nodeObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* parentObject = axObject.parentObject();
  if (parentObject && parentObject != inspectedAXObject) {
    // Use unignored parent unless parent is inspected ignored object.
    parentObject = axObject.parentObjectUnignored();
  }

  std::unique_ptr<protocol::Array<AXNodeId>> childIds =
      protocol::Array<AXNodeId>::create();

  if (inspectedAXObject &&
      &axObject == inspectedAXObject->parentObjectUnignored() &&
      inspectedAXObject->accessibilityIsIgnored()) {
    // This is the parent of an ignored object, so search for its siblings.
    addSiblingsOfIgnored(childIds, axObject, inspectedAXObject, nodes, cache);
  } else {
    addChildren(axObject, inspectedAXObject, childIds, nodes, cache);
  }
  nodeObject.setChildIds(std::move(childIds));
}

void InspectorAccessibilityAgent::addSiblingsOfIgnored(
    std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
    AXObject& parentAXObject,
    AXObject* inspectedAXObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  for (AXObject* childAXObject = parentAXObject.rawFirstChild(); childAXObject;
       childAXObject = childAXObject->rawNextSibling()) {
    if (!childAXObject->accessibilityIsIgnored() ||
        childAXObject == inspectedAXObject) {
      addChild(childIds, *childAXObject, inspectedAXObject, nodes, cache);
      continue;
    }

    if (isAncestorOf(childAXObject, inspectedAXObject)) {
      addSiblingsOfIgnored(childIds, *childAXObject, inspectedAXObject, nodes,
                           cache);
      continue;
    }

    const AXObject::AXObjectVector& indirectChildren =
        childAXObject->children();
    for (unsigned i = 0; i < indirectChildren.size(); ++i) {
      addChild(childIds, *(indirectChildren[i]), inspectedAXObject, nodes,
               cache);
    }
  }
}

void InspectorAccessibilityAgent::addChild(
    std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
    AXObject& childAXObject,
    AXObject* inspectedAXObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  childIds->addItem(String::number(childAXObject.axObjectID()));
  if (&childAXObject == inspectedAXObject)
    return;
  nodes->addItem(buildProtocolAXObject(childAXObject, inspectedAXObject, true,
                                       nodes, cache));
}

void InspectorAccessibilityAgent::addChildren(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  const AXObject::AXObjectVector& children = axObject.children();
  for (unsigned i = 0; i < children.size(); i++) {
    AXObject& childAXObject = *children[i].get();
    childIds->addItem(String::number(childAXObject.axObjectID()));
    if (&childAXObject == inspectedAXObject)
      continue;
    if (&axObject != inspectedAXObject) {
      if (!inspectedAXObject)
        continue;
      if (&axObject != inspectedAXObject->parentObjectUnignored())
        continue;
    }

    // Only add children/siblings of inspected node to returned nodes.
    std::unique_ptr<AXNode> childNode = buildProtocolAXObject(
        childAXObject, inspectedAXObject, true, nodes, cache);
    nodes->addItem(std::move(childNode));
  }
}

DEFINE_TRACE(InspectorAccessibilityAgent) {
  visitor->trace(m_page);
  visitor->trace(m_domAgent);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
