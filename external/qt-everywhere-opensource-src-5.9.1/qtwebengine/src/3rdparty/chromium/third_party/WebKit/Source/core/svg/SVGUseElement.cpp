/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/svg/SVGUseElement.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/layout/svg/LayoutSVGTransformableContainer.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGGElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGSymbolElement.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "wtf/Vector.h"

namespace blink {

inline SVGUseElement::SVGUseElement(Document& document)
    : SVGGraphicsElement(SVGNames::useTag, document),
      SVGURIReference(this),
      m_x(SVGAnimatedLength::create(this,
                                    SVGNames::xAttr,
                                    SVGLength::create(SVGLengthMode::Width),
                                    CSSPropertyX)),
      m_y(SVGAnimatedLength::create(this,
                                    SVGNames::yAttr,
                                    SVGLength::create(SVGLengthMode::Height),
                                    CSSPropertyY)),
      m_width(
          SVGAnimatedLength::create(this,
                                    SVGNames::widthAttr,
                                    SVGLength::create(SVGLengthMode::Width))),
      m_height(
          SVGAnimatedLength::create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::create(SVGLengthMode::Height))),
      m_elementIdentifierIsLocal(true),
      m_haveFiredLoadEvent(false),
      m_needsShadowTreeRecreation(false) {
  ASSERT(hasCustomStyleCallbacks());
  ThreadState::current()->registerPreFinalizer(this);

  addToPropertyMap(m_x);
  addToPropertyMap(m_y);
  addToPropertyMap(m_width);
  addToPropertyMap(m_height);
}

SVGUseElement* SVGUseElement::create(Document& document) {
  // Always build a user agent #shadow-root for SVGUseElement.
  SVGUseElement* use = new SVGUseElement(document);
  use->ensureUserAgentShadowRoot();
  return use;
}

SVGUseElement::~SVGUseElement() {}

void SVGUseElement::dispose() {
  setDocumentResource(nullptr);
}

DEFINE_TRACE(SVGUseElement) {
  visitor->trace(m_x);
  visitor->trace(m_y);
  visitor->trace(m_width);
  visitor->trace(m_height);
  visitor->trace(m_targetElementInstance);
  visitor->trace(m_resource);
  SVGGraphicsElement::trace(visitor);
  SVGURIReference::trace(visitor);
  DocumentResourceClient::trace(visitor);
}

#if ENABLE(ASSERT)
static inline bool isWellFormedDocument(Document* document) {
  if (document->isXMLDocument())
    return static_cast<XMLDocumentParser*>(document->parser())->wellFormed();
  return true;
}
#endif

Node::InsertionNotificationRequest SVGUseElement::insertedInto(
    ContainerNode* rootParent) {
  // This functions exists to assure assumptions made in the code regarding
  // SVGElementInstance creation/destruction are satisfied.
  SVGGraphicsElement::insertedInto(rootParent);
  if (!rootParent->isConnected())
    return InsertionDone;
  ASSERT(!m_targetElementInstance || !isWellFormedDocument(&document()));
  ASSERT(!hasPendingResources() || !isWellFormedDocument(&document()));
  invalidateShadowTree();
  return InsertionDone;
}

void SVGUseElement::removedFrom(ContainerNode* rootParent) {
  SVGGraphicsElement::removedFrom(rootParent);
  if (rootParent->isConnected()) {
    clearShadowTree();
    cancelShadowTreeRecreation();
  }
}

static void transferUseWidthAndHeightIfNeeded(
    const SVGUseElement& use,
    SVGElement& shadowElement,
    const SVGElement& originalElement) {
  DEFINE_STATIC_LOCAL(const AtomicString, hundredPercentString, ("100%"));
  // Use |originalElement| for checking the element type, because we will
  // have replaced a <symbol> with an <svg> in the instance tree.
  if (isSVGSymbolElement(originalElement)) {
    // Spec (<use> on <symbol>): This generated 'svg' will always have
    // explicit values for attributes width and height.  If attributes
    // width and/or height are provided on the 'use' element, then these
    // attributes will be transferred to the generated 'svg'. If attributes
    // width and/or height are not specified, the generated 'svg' element
    // will use values of 100% for these attributes.
    shadowElement.setAttribute(
        SVGNames::widthAttr,
        use.width()->isSpecified()
            ? AtomicString(use.width()->currentValue()->valueAsString())
            : hundredPercentString);
    shadowElement.setAttribute(
        SVGNames::heightAttr,
        use.height()->isSpecified()
            ? AtomicString(use.height()->currentValue()->valueAsString())
            : hundredPercentString);
  } else if (isSVGSVGElement(originalElement)) {
    // Spec (<use> on <svg>): If attributes width and/or height are
    // provided on the 'use' element, then these values will override the
    // corresponding attributes on the 'svg' in the generated tree.
    shadowElement.setAttribute(
        SVGNames::widthAttr,
        use.width()->isSpecified()
            ? AtomicString(use.width()->currentValue()->valueAsString())
            : originalElement.getAttribute(SVGNames::widthAttr));
    shadowElement.setAttribute(
        SVGNames::heightAttr,
        use.height()->isSpecified()
            ? AtomicString(use.height()->currentValue()->valueAsString())
            : originalElement.getAttribute(SVGNames::heightAttr));
  }
}

void SVGUseElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  SVGAnimatedPropertyBase* property = propertyFromAttribute(name);
  if (property == m_x)
    addPropertyToPresentationAttributeStyle(
        style, CSSPropertyX, m_x->currentValue()->asCSSPrimitiveValue());
  else if (property == m_y)
    addPropertyToPresentationAttributeStyle(
        style, CSSPropertyY, m_y->currentValue()->asCSSPrimitiveValue());
  else
    SVGGraphicsElement::collectStyleForPresentationAttribute(name, value,
                                                             style);
}

bool SVGUseElement::isStructurallyExternal() const {
  return !m_elementIdentifierIsLocal;
}

void SVGUseElement::updateTargetReference() {
  SVGURLReferenceResolver resolver(hrefString(), document());
  m_elementIdentifier = resolver.fragmentIdentifier();
  m_elementIdentifierIsLocal = resolver.isLocal();
  if (m_elementIdentifierIsLocal) {
    setDocumentResource(nullptr);
    return;
  }
  KURL resolvedUrl = resolver.absoluteUrl();
  if (m_elementIdentifier.isEmpty() ||
      (m_resource &&
       equalIgnoringFragmentIdentifier(resolvedUrl, m_resource->url())))
    return;
  FetchRequest request(ResourceRequest(resolvedUrl), localName());
  setDocumentResource(
      DocumentResource::fetchSVGDocument(request, document().fetcher()));
}

void SVGUseElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
      attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr) {
    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr) {
      invalidateSVGPresentationAttributeStyle();
      setNeedsStyleRecalc(LocalStyleChange,
                          StyleChangeReasonForTracing::fromAttribute(attrName));
    }

    updateRelativeLengthsInformation();
    if (m_targetElementInstance) {
      ASSERT(m_targetElementInstance->correspondingElement());
      transferUseWidthAndHeightIfNeeded(
          *this, *m_targetElementInstance,
          *m_targetElementInstance->correspondingElement());
    }

    LayoutObject* object = this->layoutObject();
    if (object)
      markForLayoutAndParentResourceInvalidation(object);
    return;
  }

  if (SVGURIReference::isKnownAttribute(attrName)) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    updateTargetReference();
    invalidateShadowTree();
    return;
  }

  SVGGraphicsElement::svgAttributeChanged(attrName);
}

static bool isDisallowedElement(const Element& element) {
  // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is
  // potentially a template object that can be re-used (i.e., "instanced") in
  // the SVG document via a 'use' element." "Graphics Element" is defined as
  // 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline',
  // 'rect', 'text' Excluded are anything that is used by reference or that only
  // make sense to appear once in a document.
  if (!element.isSVGElement())
    return true;

  DEFINE_STATIC_LOCAL(
      HashSet<QualifiedName>, allowedElementTags,
      ({
          SVGNames::aTag,       SVGNames::circleTag,   SVGNames::descTag,
          SVGNames::ellipseTag, SVGNames::gTag,        SVGNames::imageTag,
          SVGNames::lineTag,    SVGNames::metadataTag, SVGNames::pathTag,
          SVGNames::polygonTag, SVGNames::polylineTag, SVGNames::rectTag,
          SVGNames::svgTag,     SVGNames::switchTag,   SVGNames::symbolTag,
          SVGNames::textTag,    SVGNames::textPathTag, SVGNames::titleTag,
          SVGNames::tspanTag,   SVGNames::useTag,
      }));
  return !allowedElementTags.contains<SVGAttributeHashTranslator>(
      element.tagQName());
}

void SVGUseElement::scheduleShadowTreeRecreation() {
  if (inUseShadowTree())
    return;
  m_needsShadowTreeRecreation = true;
  document().scheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::cancelShadowTreeRecreation() {
  m_needsShadowTreeRecreation = false;
  document().unscheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::clearInstanceRoot() {
  if (m_targetElementInstance)
    m_targetElementInstance = nullptr;
}

void SVGUseElement::clearShadowTree() {
  clearInstanceRoot();

  // FIXME: We should try to optimize this, to at least allow partial reclones.
  if (ShadowRoot* shadowTreeRootElement = userAgentShadowRoot())
    shadowTreeRootElement->removeChildren(OmitSubtreeModifiedEvent);

  removeAllOutgoingReferences();
}

Element* SVGUseElement::resolveTargetElement() {
  if (m_elementIdentifier.isEmpty())
    return nullptr;
  const TreeScope* lookupScope = nullptr;
  if (m_elementIdentifierIsLocal)
    lookupScope = &treeScope();
  else if (resourceIsValid())
    lookupScope = m_resource->document();
  else
    return nullptr;
  Element* target = lookupScope->getElementById(m_elementIdentifier);
  // TODO(fs): Why would the Element not be "connected" at this point?
  if (target && target->isConnected())
    return target;
  // Don't record any pending references for external resources.
  if (!m_resource) {
    document().accessSVGExtensions().addPendingResource(m_elementIdentifier,
                                                        this);
    DCHECK(hasPendingResources());
  }
  return nullptr;
}

void SVGUseElement::buildPendingResource() {
  if (inUseShadowTree())
    return;
  clearShadowTree();
  cancelShadowTreeRecreation();
  if (!isConnected())
    return;
  Element* target = resolveTargetElement();
  if (target && target->isSVGElement()) {
    buildShadowAndInstanceTree(toSVGElement(*target));
    invalidateDependentShadowTrees();
  }

  ASSERT(!m_needsShadowTreeRecreation);
}

static void associateCorrespondingElements(SVGElement& targetRoot,
                                           SVGElement& instanceRoot) {
  auto targetRange = Traversal<SVGElement>::inclusiveDescendantsOf(targetRoot);
  auto targetIterator = targetRange.begin();
  for (SVGElement& instance :
       Traversal<SVGElement>::inclusiveDescendantsOf(instanceRoot)) {
    ASSERT(!instance.correspondingElement());
    instance.setCorrespondingElement(&*targetIterator);
    ++targetIterator;
  }
  ASSERT(!(targetIterator != targetRange.end()));
}

// We don't walk the target tree element-by-element, and clone each element,
// but instead use cloneNode(deep=true). This is an optimization for the common
// case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
// Though if there are disallowed elements in the subtree, we have to remove
// them.  For instance: <use> on <g> containing <foreignObject> (indirect
// case).
static inline void removeDisallowedElementsFromSubtree(SVGElement& subtree) {
  ASSERT(!subtree.isConnected());
  Element* element = ElementTraversal::firstWithin(subtree);
  while (element) {
    if (isDisallowedElement(*element)) {
      Element* next =
          ElementTraversal::nextSkippingChildren(*element, &subtree);
      // The subtree is not in document so this won't generate events that could
      // mutate the tree.
      element->parentNode()->removeChild(element);
      element = next;
    } else {
      element = ElementTraversal::next(*element, &subtree);
    }
  }
}

static void moveChildrenToReplacementElement(ContainerNode& sourceRoot,
                                             ContainerNode& destinationRoot) {
  for (Node* child = sourceRoot.firstChild(); child;) {
    Node* nextChild = child->nextSibling();
    destinationRoot.appendChild(child);
    child = nextChild;
  }
}

Element* SVGUseElement::createInstanceTree(SVGElement& targetRoot) const {
  Element* instanceRoot = targetRoot.cloneElementWithChildren();
  ASSERT(instanceRoot->isSVGElement());
  if (isSVGSymbolElement(targetRoot)) {
    // Spec: The referenced 'symbol' and its contents are deep-cloned into
    // the generated tree, with the exception that the 'symbol' is replaced
    // by an 'svg'. This generated 'svg' will always have explicit values
    // for attributes width and height. If attributes width and/or height
    // are provided on the 'use' element, then these attributes will be
    // transferred to the generated 'svg'. If attributes width and/or
    // height are not specified, the generated 'svg' element will use
    // values of 100% for these attributes.
    SVGSVGElement* svgElement = SVGSVGElement::create(targetRoot.document());
    // Transfer all data (attributes, etc.) from the <symbol> to the new
    // <svg> element.
    svgElement->cloneDataFromElement(*instanceRoot);
    // Move already cloned elements to the new <svg> element.
    moveChildrenToReplacementElement(*instanceRoot, *svgElement);
    instanceRoot = svgElement;
  }
  transferUseWidthAndHeightIfNeeded(*this, toSVGElement(*instanceRoot),
                                    targetRoot);
  associateCorrespondingElements(targetRoot, toSVGElement(*instanceRoot));
  removeDisallowedElementsFromSubtree(toSVGElement(*instanceRoot));
  return instanceRoot;
}

void SVGUseElement::buildShadowAndInstanceTree(SVGElement& target) {
  ASSERT(!m_targetElementInstance);
  ASSERT(!m_needsShadowTreeRecreation);

  // <use> creates a "user agent" shadow root. Do not build the shadow/instance
  // tree for <use> elements living in a user agent shadow tree because they
  // will get expanded in a second pass -- see expandUseElementsInShadowTree().
  if (inUseShadowTree())
    return;

  // Do not allow self-referencing.
  if (&target == this || isDisallowedElement(target))
    return;

  // Set up root SVG element in shadow tree.
  // Clone the target subtree into the shadow tree, not handling <use> and
  // <symbol> yet.
  Element* instanceRoot = createInstanceTree(target);
  m_targetElementInstance = toSVGElement(instanceRoot);
  ShadowRoot* shadowTreeRootElement = userAgentShadowRoot();
  shadowTreeRootElement->appendChild(instanceRoot);

  addReferencesToFirstDegreeNestedUseElements(target);

  if (instanceTreeIsLoading())
    return;

  // Assure shadow tree building was successful.
  ASSERT(m_targetElementInstance);
  ASSERT(m_targetElementInstance->correspondingUseElement() == this);
  ASSERT(m_targetElementInstance->correspondingElement() == &target);

  // Expand all <use> elements in the shadow tree.
  // Expand means: replace the actual <use> element by what it references.
  if (!expandUseElementsInShadowTree()) {
    clearShadowTree();
    return;
  }

  // If the instance root was a <use>, it could have been replaced now, so
  // reset |m_targetElementInstance|.
  m_targetElementInstance =
      toSVGElementOrDie(shadowTreeRootElement->firstChild());
  ASSERT(m_targetElementInstance->parentNode() == shadowTreeRootElement);

  // Update relative length information.
  updateRelativeLengthsInformation();
}

LayoutObject* SVGUseElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutSVGTransformableContainer(this);
}

static bool isDirectReference(const SVGElement& element) {
  return isSVGPathElement(element) || isSVGRectElement(element) ||
         isSVGCircleElement(element) || isSVGEllipseElement(element) ||
         isSVGPolygonElement(element) || isSVGPolylineElement(element) ||
         isSVGTextElement(element);
}

void SVGUseElement::toClipPath(Path& path) const {
  ASSERT(path.isEmpty());

  const SVGGraphicsElement* element = visibleTargetGraphicsElementForClipping();

  if (!element)
    return;

  if (element->isSVGGeometryElement()) {
    toSVGGeometryElement(*element).toClipPath(path);
    // FIXME: Avoid manual resolution of x/y here. Its potentially harmful.
    SVGLengthContext lengthContext(this);
    path.translate(FloatSize(m_x->currentValue()->value(lengthContext),
                             m_y->currentValue()->value(lengthContext)));
    path.transform(calculateAnimatedLocalTransform());
  }
}

SVGGraphicsElement* SVGUseElement::visibleTargetGraphicsElementForClipping()
    const {
  Node* n = userAgentShadowRoot()->firstChild();
  if (!n || !n->isSVGElement())
    return nullptr;

  SVGElement& element = toSVGElement(*n);

  if (!element.isSVGGraphicsElement())
    return nullptr;

  if (!element.layoutObject())
    return nullptr;

  const ComputedStyle* style = element.layoutObject()->style();
  if (!style || style->visibility() != EVisibility::Visible)
    return nullptr;

  // Spec: "If a <use> element is a child of a clipPath element, it must
  // directly reference <path>, <text> or basic shapes elements. Indirect
  // references are an error and the clipPath element must be ignored."
  // http://dev.w3.org/fxtf/css-masking-1/#the-clip-path
  if (!isDirectReference(element)) {
    // Spec: Indirect references are an error (14.3.5)
    return nullptr;
  }

  return &toSVGGraphicsElement(element);
}

void SVGUseElement::addReferencesToFirstDegreeNestedUseElements(
    SVGElement& target) {
  // Don't track references to external documents.
  if (isStructurallyExternal())
    return;
  // We only need to track first degree <use> dependencies. Indirect
  // references are handled as the invalidation bubbles up the dependency
  // chain.
  SVGUseElement* useElement =
      isSVGUseElement(target) ? toSVGUseElement(&target)
                              : Traversal<SVGUseElement>::firstWithin(target);
  for (; useElement;
       useElement =
           Traversal<SVGUseElement>::nextSkippingChildren(*useElement, &target))
    addReferenceTo(useElement);
}

bool SVGUseElement::hasCycleUseReferencing(const SVGUseElement& use,
                                           const ContainerNode& targetInstance,
                                           SVGElement*& newTarget) const {
  Element* targetElement =
      targetElementFromIRIString(use.hrefString(), use.treeScope());
  newTarget = 0;
  if (targetElement && targetElement->isSVGElement())
    newTarget = toSVGElement(targetElement);

  if (!newTarget)
    return false;

  // Shortcut for self-references
  if (newTarget == this)
    return true;

  AtomicString targetId = newTarget->getIdAttribute();
  ContainerNode* instance = targetInstance.parentNode();
  while (instance && instance->isSVGElement()) {
    SVGElement* element = toSVGElement(instance);
    if (element->hasID() && element->getIdAttribute() == targetId &&
        element->document() == newTarget->document())
      return true;

    instance = instance->parentNode();
  }
  return false;
}

// Spec: In the generated content, the 'use' will be replaced by 'g', where all
// attributes from the 'use' element except for x, y, width, height and
// xlink:href are transferred to the generated 'g' element.
static void removeAttributesFromReplacementElement(
    SVGElement& replacementElement) {
  replacementElement.removeAttribute(SVGNames::xAttr);
  replacementElement.removeAttribute(SVGNames::yAttr);
  replacementElement.removeAttribute(SVGNames::widthAttr);
  replacementElement.removeAttribute(SVGNames::heightAttr);
  replacementElement.removeAttribute(SVGNames::hrefAttr);
  replacementElement.removeAttribute(XLinkNames::hrefAttr);
}

bool SVGUseElement::expandUseElementsInShadowTree() {
  // Why expand the <use> elements in the shadow tree here, and not just
  // do this directly in buildShadowTree, if we encounter a <use> element?
  //
  // Short answer: Because we may miss to expand some elements. For example, if
  // a <symbol> contains <use> tags, we'd miss them. So once we're done with
  // setting up the actual shadow tree (after the special case modification for
  // svg/symbol) we have to walk it completely and expand all <use> elements.
  ShadowRoot* shadowRoot = userAgentShadowRoot();
  for (SVGUseElement* use = Traversal<SVGUseElement>::firstWithin(*shadowRoot);
       use;) {
    ASSERT(!use->resourceIsStillLoading());

    SVGUseElement& originalUse = toSVGUseElement(*use->correspondingElement());
    SVGElement* target = nullptr;
    if (hasCycleUseReferencing(originalUse, *use, target))
      return false;

    if (target && isDisallowedElement(*target))
      return false;
    // Don't ASSERT(target) here, it may be "pending", too.
    // Setup sub-shadow tree root node
    SVGGElement* cloneParent = SVGGElement::create(originalUse.document());
    // Transfer all data (attributes, etc.) from <use> to the new <g> element.
    cloneParent->cloneDataFromElement(*use);
    cloneParent->setCorrespondingElement(&originalUse);

    removeAttributesFromReplacementElement(*cloneParent);

    // Move already cloned elements to the new <g> element.
    moveChildrenToReplacementElement(*use, *cloneParent);

    if (target)
      cloneParent->appendChild(use->createInstanceTree(*target));

    SVGElement* replacingElement(cloneParent);

    // Replace <use> with referenced content.
    use->parentNode()->replaceChild(cloneParent, use);

    use = Traversal<SVGUseElement>::next(*replacingElement, shadowRoot);
  }
  return true;
}

void SVGUseElement::invalidateShadowTree() {
  if (!inActiveDocument() || m_needsShadowTreeRecreation)
    return;
  clearInstanceRoot();
  scheduleShadowTreeRecreation();
  invalidateDependentShadowTrees();
}

void SVGUseElement::invalidateDependentShadowTrees() {
  // Recursively invalidate dependent <use> shadow trees
  const HeapHashSet<WeakMember<SVGElement>>& rawInstances =
      instancesForElement();
  HeapVector<Member<SVGElement>> instances;
  instances.appendRange(rawInstances.begin(), rawInstances.end());
  for (auto& instance : instances) {
    if (SVGUseElement* element = instance->correspondingUseElement()) {
      ASSERT(element->isConnected());
      element->invalidateShadowTree();
    }
  }
}

bool SVGUseElement::selfHasRelativeLengths() const {
  if (m_x->currentValue()->isRelative() || m_y->currentValue()->isRelative() ||
      m_width->currentValue()->isRelative() ||
      m_height->currentValue()->isRelative())
    return true;

  if (!m_targetElementInstance)
    return false;

  return m_targetElementInstance->hasRelativeLengths();
}

FloatRect SVGUseElement::getBBox() {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!layoutObject())
    return FloatRect();

  LayoutSVGTransformableContainer& transformableContainer =
      toLayoutSVGTransformableContainer(*layoutObject());
  // Don't apply the additional translation if the oBB is invalid.
  if (!transformableContainer.isObjectBoundingBoxValid())
    return FloatRect();

  // TODO(fs): Preferably this would just use objectBoundingBox() (and hence
  // don't need to override SVGGraphicsElement::getBBox at all) and be
  // correct without additional work. That will not work out ATM without
  // additional quirks. The problem stems from including the additional
  // translation directly on the LayoutObject corresponding to the
  // SVGUseElement.
  FloatRect bbox = transformableContainer.objectBoundingBox();
  bbox.move(transformableContainer.additionalTranslation());
  return bbox;
}

void SVGUseElement::dispatchPendingEvent() {
  ASSERT(isStructurallyExternal() && m_haveFiredLoadEvent);
  dispatchEvent(Event::create(EventTypeNames::load));
}

void SVGUseElement::notifyFinished(Resource* resource) {
  ASSERT(m_resource == resource);
  if (!isConnected())
    return;

  invalidateShadowTree();
  if (!resourceIsValid()) {
    dispatchEvent(Event::create(EventTypeNames::error));
  } else if (!resource->wasCanceled()) {
    if (m_haveFiredLoadEvent)
      return;
    if (!isStructurallyExternal())
      return;
    ASSERT(!m_haveFiredLoadEvent);
    m_haveFiredLoadEvent = true;
    TaskRunnerHelper::get(TaskType::DOMManipulation, &document())
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&SVGUseElement::dispatchPendingEvent,
                             wrapPersistent(this)));
  }
}

bool SVGUseElement::resourceIsStillLoading() const {
  return m_resource && m_resource->isLoading();
}

bool SVGUseElement::resourceIsValid() const {
  return m_resource && m_resource->isLoaded() && !m_resource->errorOccurred() &&
         m_resource->document();
}

bool SVGUseElement::instanceTreeIsLoading() const {
  for (const SVGUseElement& useElement :
       Traversal<SVGUseElement>::descendantsOf(*userAgentShadowRoot())) {
    if (useElement.resourceIsStillLoading())
      return true;
  }
  return false;
}

void SVGUseElement::setDocumentResource(DocumentResource* resource) {
  if (m_resource == resource)
    return;

  if (m_resource)
    m_resource->removeClient(this);

  m_resource = resource;
  if (m_resource)
    m_resource->addClient(this);
}

}  // namespace blink
