/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXObjectCacheImpl.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/layout/LayoutListBox.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutProgress.h"
#include "core/layout/LayoutSlider.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/AbstractInlineTextBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXARIAGrid.h"
#include "modules/accessibility/AXARIAGridCell.h"
#include "modules/accessibility/AXARIAGridRow.h"
#include "modules/accessibility/AXImageMapLink.h"
#include "modules/accessibility/AXInlineTextBox.h"
#include "modules/accessibility/AXLayoutObject.h"
#include "modules/accessibility/AXList.h"
#include "modules/accessibility/AXListBox.h"
#include "modules/accessibility/AXListBoxOption.h"
#include "modules/accessibility/AXMediaControls.h"
#include "modules/accessibility/AXMenuList.h"
#include "modules/accessibility/AXMenuListOption.h"
#include "modules/accessibility/AXMenuListPopup.h"
#include "modules/accessibility/AXProgressIndicator.h"
#include "modules/accessibility/AXRadioInput.h"
#include "modules/accessibility/AXSVGRoot.h"
#include "modules/accessibility/AXSlider.h"
#include "modules/accessibility/AXSpinButton.h"
#include "modules/accessibility/AXTable.h"
#include "modules/accessibility/AXTableCell.h"
#include "modules/accessibility/AXTableColumn.h"
#include "modules/accessibility/AXTableHeaderContainer.h"
#include "modules/accessibility/AXTableRow.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"

namespace blink {

using namespace HTMLNames;

// static
AXObjectCache* AXObjectCacheImpl::create(Document& document)
{
    return new AXObjectCacheImpl(document);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document)
    : m_document(document)
    , m_modificationCount(0)
#if ENABLE(ASSERT)
    , m_hasBeenDisposed(false)
#endif
    , m_notificationPostTimer(this, &AXObjectCacheImpl::notificationPostTimerFired)
{
}

AXObjectCacheImpl::~AXObjectCacheImpl()
{
    ASSERT(m_hasBeenDisposed);
}

void AXObjectCacheImpl::dispose()
{
    m_notificationPostTimer.stop();

    for (auto& entry : m_objects) {
        AXObject* obj = entry.value;
        obj->detach();
        removeAXID(obj);
    }

#if ENABLE(ASSERT)
    m_hasBeenDisposed = true;
#endif
}

AXObject* AXObjectCacheImpl::root()
{
    return getOrCreate(m_document);
}

AXObject* AXObjectCacheImpl::focusedImageMapUIElement(HTMLAreaElement* areaElement)
{
    // Find the corresponding accessibility object for the HTMLAreaElement. This should be
    // in the list of children for its corresponding image.
    if (!areaElement)
        return 0;

    HTMLImageElement* imageElement = areaElement->imageElement();
    if (!imageElement)
        return 0;

    AXObject* axLayoutImage = getOrCreate(imageElement);
    if (!axLayoutImage)
        return 0;

    const AXObject::AXObjectVector& imageChildren = axLayoutImage->children();
    unsigned count = imageChildren.size();
    for (unsigned k = 0; k < count; ++k) {
        AXObject* child = imageChildren[k];
        if (!child->isImageMapLink())
            continue;

        if (toAXImageMapLink(child)->areaElement() == areaElement)
            return child;
    }

    return 0;
}

AXObject* AXObjectCacheImpl::focusedObject()
{
    if (!accessibilityEnabled())
        return nullptr;

    Node* focusedNode = m_document->focusedElement();
    if (!focusedNode)
        focusedNode = m_document;

    // If it's an image map, get the focused link within the image map.
    if (isHTMLAreaElement(focusedNode))
        return focusedImageMapUIElement(toHTMLAreaElement(focusedNode));

    // See if there's a page popup, for example a calendar picker.
    Element* adjustedFocusedElement = m_document->adjustedFocusedElement();
    if (isHTMLInputElement(adjustedFocusedElement)) {
        if (AXObject* axPopup = toHTMLInputElement(adjustedFocusedElement)->popupRootAXObject()) {
            if (Element* focusedElementInPopup = axPopup->getDocument()->focusedElement())
                focusedNode = focusedElementInPopup;
        }
    }

    AXObject* obj = getOrCreate(focusedNode);
    if (!obj)
        return nullptr;

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();

    return obj;
}

AXObject* AXObjectCacheImpl::get(LayoutObject* layoutObject)
{
    if (!layoutObject)
        return 0;

    AXID axID = m_layoutObjectMapping.get(layoutObject);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

// Returns true if |node| is an <option> element and its parent <select>
// is a menu list (not a list box).
static bool isMenuListOption(Node* node)
{
    if (!isHTMLOptionElement(node))
        return false;
    HTMLSelectElement* select = toHTMLOptionElement(node)->ownerSelectElement();
    if (!select)
        return false;
    LayoutObject* layoutObject = select->layoutObject();
    return layoutObject && layoutObject->isMenuList();
}

AXObject* AXObjectCacheImpl::get(Node* node)
{
    if (!node)
        return 0;

    // Menu list option and HTML area elements are indexed by DOM node, never by layout object.
    LayoutObject* layoutObject = node->layoutObject();
    if (isMenuListOption(node) || isHTMLAreaElement(node))
        layoutObject = nullptr;

    AXID layoutID = layoutObject ? m_layoutObjectMapping.get(layoutObject) : 0;
    ASSERT(!HashTraits<AXID>::isDeletedValue(layoutID));

    AXID nodeID = m_nodeObjectMapping.get(node);
    ASSERT(!HashTraits<AXID>::isDeletedValue(nodeID));

    if (layoutObject && nodeID && !layoutID) {
        // This can happen if an AXNodeObject is created for a node that's not
        // laid out, but later something changes and it gets a layoutObject (like if it's
        // reparented).
        remove(nodeID);
        return 0;
    }

    if (layoutID)
        return m_objects.get(layoutID);

    if (!nodeID)
        return 0;

    return m_objects.get(nodeID);
}

AXObject* AXObjectCacheImpl::get(AbstractInlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return 0;

    AXID axID = m_inlineTextBoxObjectMapping.get(inlineTextBox);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

// FIXME: This probably belongs on Node.
// FIXME: This should take a const char*, but one caller passes nullAtom.
bool nodeHasRole(Node* node, const String& role)
{
    if (!node || !node->isElementNode())
        return false;

    return equalIgnoringCase(toElement(node)->getAttribute(roleAttr), role);
}

AXObject* AXObjectCacheImpl::createFromRenderer(LayoutObject* layoutObject)
{
    // FIXME: How could layoutObject->node() ever not be an Element?
    Node* node = layoutObject->node();

    // If the node is aria role="list" or the aria role is empty and its a
    // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
    if (nodeHasRole(node, "list") || nodeHasRole(node, "directory")
        || (nodeHasRole(node, nullAtom) && (isHTMLUListElement(node) || isHTMLOListElement(node) || isHTMLDListElement(node))))
        return AXList::create(layoutObject, *this);

    // aria tables
    if (nodeHasRole(node, "grid") || nodeHasRole(node, "treegrid"))
        return AXARIAGrid::create(layoutObject, *this);
    if (nodeHasRole(node, "row"))
        return AXARIAGridRow::create(layoutObject, *this);
    if (nodeHasRole(node, "gridcell") || nodeHasRole(node, "columnheader") || nodeHasRole(node, "rowheader"))
        return AXARIAGridCell::create(layoutObject, *this);

    // media controls
    if (node && node->isMediaControlElement())
        return AccessibilityMediaControl::create(layoutObject, *this);

    if (isHTMLOptionElement(node))
        return AXListBoxOption::create(layoutObject, *this);

    if (isHTMLInputElement(node) && toHTMLInputElement(node)->type() == InputTypeNames::radio)
        return AXRadioInput::create(layoutObject, *this);

    if (layoutObject->isSVGRoot())
        return AXSVGRoot::create(layoutObject, *this);

    if (layoutObject->isBoxModelObject()) {
        LayoutBoxModelObject* cssBox = toLayoutBoxModelObject(layoutObject);
        if (cssBox->isListBox())
            return AXListBox::create(toLayoutListBox(cssBox), *this);
        if (cssBox->isMenuList())
            return AXMenuList::create(toLayoutMenuList(cssBox), *this);

        // standard tables
        if (cssBox->isTable())
            return AXTable::create(toLayoutTable(cssBox), *this);
        if (cssBox->isTableRow())
            return AXTableRow::create(toLayoutTableRow(cssBox), *this);
        if (cssBox->isTableCell())
            return AXTableCell::create(toLayoutTableCell(cssBox), *this);

        // progress bar
        if (cssBox->isProgress())
            return AXProgressIndicator::create(toLayoutProgress(cssBox), *this);

        // input type=range
        if (cssBox->isSlider())
            return AXSlider::create(toLayoutSlider(cssBox), *this);
    }

    return AXLayoutObject::create(layoutObject, *this);
}

AXObject* AXObjectCacheImpl::createFromNode(Node* node)
{
    if (isMenuListOption(node))
        return AXMenuListOption::create(toHTMLOptionElement(node), *this);

    if (isHTMLAreaElement(node))
        return AXImageMapLink::create(toHTMLAreaElement(node), *this);

    return AXNodeObject::create(node, *this);
}

AXObject* AXObjectCacheImpl::createFromInlineTextBox(AbstractInlineTextBox* inlineTextBox)
{
    return AXInlineTextBox::create(inlineTextBox, *this);
}

AXObject* AXObjectCacheImpl::getOrCreate(Node* node)
{
    if (!node)
        return 0;

    if (AXObject* obj = get(node))
        return obj;

    // If the node has a layout object, prefer using that as the primary key for the AXObject,
    // with the exception of an HTMLAreaElement, which is created based on its node.
    if (node->layoutObject() && !isHTMLAreaElement(node))
        return getOrCreate(node->layoutObject());

    if (!node->parentElement())
        return 0;

    if (isHTMLHeadElement(node))
        return 0;

    AXObject* newObj = createFromNode(node);

    // Will crash later if we have two objects for the same node.
    ASSERT(!get(node));

    getAXID(newObj);

    m_nodeObjectMapping.set(node, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    if (node->isElementNode())
        updateTreeIfElementIdIsAriaOwned(toElement(node));

    return newObj;
}

AXObject* AXObjectCacheImpl::getOrCreate(LayoutObject* layoutObject)
{
    if (!layoutObject)
        return 0;

    if (AXObject* obj = get(layoutObject))
        return obj;

    AXObject* newObj = createFromRenderer(layoutObject);

    // Will crash later if we have two objects for the same layoutObject.
    ASSERT(!get(layoutObject));

    getAXID(newObj);

    m_layoutObjectMapping.set(layoutObject, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj;
}

AXObject* AXObjectCacheImpl::getOrCreate(AbstractInlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return 0;

    if (AXObject* obj = get(inlineTextBox))
        return obj;

    AXObject* newObj = createFromInlineTextBox(inlineTextBox);

    // Will crash later if we have two objects for the same inlineTextBox.
    ASSERT(!get(inlineTextBox));

    getAXID(newObj);

    m_inlineTextBoxObjectMapping.set(inlineTextBox, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj;
}

AXObject* AXObjectCacheImpl::rootObject()
{
    if (!accessibilityEnabled())
        return 0;

    return getOrCreate(m_document);
}

AXObject* AXObjectCacheImpl::getOrCreate(AccessibilityRole role)
{
    AXObject* obj = nullptr;

    // will be filled in...
    switch (role) {
    case ColumnRole:
        obj = AXTableColumn::create(*this);
        break;
    case TableHeaderContainerRole:
        obj = AXTableHeaderContainer::create(*this);
        break;
    case SliderThumbRole:
        obj = AXSliderThumb::create(*this);
        break;
    case MenuListPopupRole:
        obj = AXMenuListPopup::create(*this);
        break;
    case SpinButtonRole:
        obj = AXSpinButton::create(*this);
        break;
    case SpinButtonPartRole:
        obj = AXSpinButtonPart::create(*this);
        break;
    default:
        obj = nullptr;
    }

    if (obj)
        getAXID(obj);
    else
        return 0;

    m_objects.set(obj->axObjectID(), obj);
    obj->init();
    return obj;
}

void AXObjectCacheImpl::remove(AXID axID)
{
    if (!axID)
        return;

    // first fetch object to operate some cleanup functions on it
    AXObject* obj = m_objects.get(axID);
    if (!obj)
        return;

    obj->detach();
    removeAXID(obj);

    // finally remove the object
    if (!m_objects.take(axID))
        return;

    ASSERT(m_objects.size() >= m_idsInUse.size());
}

void AXObjectCacheImpl::remove(LayoutObject* layoutObject)
{
    if (!layoutObject)
        return;

    AXID axID = m_layoutObjectMapping.get(layoutObject);
    remove(axID);
    m_layoutObjectMapping.remove(layoutObject);
}

void AXObjectCacheImpl::remove(Node* node)
{
    if (!node)
        return;

    // This is all safe even if we didn't have a mapping.
    AXID axID = m_nodeObjectMapping.get(node);
    remove(axID);
    m_nodeObjectMapping.remove(node);

    if (node->layoutObject()) {
        remove(node->layoutObject());
        return;
    }
}

void AXObjectCacheImpl::remove(AbstractInlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return;

    AXID axID = m_inlineTextBoxObjectMapping.get(inlineTextBox);
    remove(axID);
    m_inlineTextBoxObjectMapping.remove(inlineTextBox);
}

AXID AXObjectCacheImpl::platformGenerateAXID() const
{
    static AXID lastUsedID = 0;

    // Generate a new ID.
    AXID objID = lastUsedID;
    do {
        ++objID;
    } while (!objID || HashTraits<AXID>::isDeletedValue(objID) || m_idsInUse.contains(objID));

    lastUsedID = objID;

    return objID;
}

AXID AXObjectCacheImpl::getAXID(AXObject* obj)
{
    // check for already-assigned ID
    AXID objID = obj->axObjectID();
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }

    objID = platformGenerateAXID();

    m_idsInUse.add(objID);
    obj->setAXObjectID(objID);

    return objID;
}

void AXObjectCacheImpl::removeAXID(AXObject* object)
{
    if (!object)
        return;

    AXID objID = object->axObjectID();
    if (!objID)
        return;
    ASSERT(!HashTraits<AXID>::isDeletedValue(objID));
    ASSERT(m_idsInUse.contains(objID));
    object->setAXObjectID(0);
    m_idsInUse.remove(objID);

    if (m_ariaOwnerToChildrenMapping.contains(objID)) {
        Vector<AXID> childAXIDs = m_ariaOwnerToChildrenMapping.get(objID);
        for (size_t i = 0; i < childAXIDs.size(); ++i)
            m_ariaOwnedChildToOwnerMapping.remove(childAXIDs[i]);
        m_ariaOwnerToChildrenMapping.remove(objID);
    }
    m_ariaOwnedChildToOwnerMapping.remove(objID);
    m_ariaOwnedChildToRealParentMapping.remove(objID);
    m_ariaOwnerToIdsMapping.remove(objID);
}

void AXObjectCacheImpl::selectionChanged(Node* node)
{
    // Find the nearest ancestor that already has an accessibility object, since we
    // might be in the middle of a layout.
    while (node) {
        if (AXObject* obj = get(node)) {
            obj->selectionChanged();
            return;
        }
        node = node->parentNode();
    }
}

void AXObjectCacheImpl::textChanged(Node* node)
{
    textChanged(getOrCreate(node));
}

void AXObjectCacheImpl::textChanged(LayoutObject* layoutObject)
{
    textChanged(getOrCreate(layoutObject));
}

void AXObjectCacheImpl::textChanged(AXObject* obj)
{
    if (!obj)
        return;

    bool parentAlreadyExists = obj->parentObjectIfExists();
    obj->textChanged();
    postNotification(obj, AXObjectCacheImpl::AXTextChanged);
    if (parentAlreadyExists)
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCacheImpl::updateCacheAfterNodeIsAttached(Node* node)
{
    // Calling get() will update the AX object if we had an AXNodeObject but now we need
    // an AXLayoutObject, because it was reparented to a location outside of a canvas.
    get(node);
    if (node->isElementNode())
        updateTreeIfElementIdIsAriaOwned(toElement(node));
}

void AXObjectCacheImpl::childrenChanged(Node* node)
{
    childrenChanged(get(node));
}

void AXObjectCacheImpl::childrenChanged(LayoutObject* layoutObject)
{
    childrenChanged(get(layoutObject));
}

void AXObjectCacheImpl::childrenChanged(AXObject* obj)
{
    if (!obj)
        return;

    obj->childrenChanged();
}

void AXObjectCacheImpl::notificationPostTimerFired(Timer<AXObjectCacheImpl>*)
{
    m_notificationPostTimer.stop();

    unsigned i = 0, count = m_notificationsToPost.size();
    for (i = 0; i < count; ++i) {
        AXObject* obj = m_notificationsToPost[i].first;

        if (!obj->axObjectID())
            continue;

        if (obj->isDetached())
            continue;

#if ENABLE(ASSERT)
        // Make sure none of the layout views are in the process of being layed out.
        // Notifications should only be sent after the layoutObject has finished
        if (obj->isAXLayoutObject()) {
            AXLayoutObject* layoutObj = toAXLayoutObject(obj);
            LayoutObject* layoutObject = layoutObj->getLayoutObject();
            if (layoutObject && layoutObject->view())
                ASSERT(!layoutObject->view()->layoutState());
        }
#endif

        AXNotification notification = m_notificationsToPost[i].second;
        postPlatformNotification(obj, notification);

        if (notification == AXChildrenChanged && obj->parentObjectIfExists() && obj->lastKnownIsIgnoredValue() != obj->accessibilityIsIgnored())
            childrenChanged(obj->parentObject());
    }

    m_notificationsToPost.clear();
}

void AXObjectCacheImpl::postNotification(LayoutObject* layoutObject, AXNotification notification)
{
    if (!layoutObject)
        return;

    m_modificationCount++;
    postNotification(get(layoutObject), notification);
}

void AXObjectCacheImpl::postNotification(Node* node, AXNotification notification)
{
    if (!node)
        return;

    m_modificationCount++;
    postNotification(get(node), notification);
}

void AXObjectCacheImpl::postNotification(AXObject* object, AXNotification notification)
{
    m_modificationCount++;
    if (!object)
        return;

    m_notificationsToPost.append(std::make_pair(object, notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0, BLINK_FROM_HERE);
}

bool AXObjectCacheImpl::isAriaOwned(const AXObject* child) const
{
    return m_ariaOwnedChildToOwnerMapping.contains(child->axObjectID());
}

AXObject* AXObjectCacheImpl::getAriaOwnedParent(const AXObject* child) const
{
    return objectFromAXID(m_ariaOwnedChildToOwnerMapping.get(child->axObjectID()));
}

void AXObjectCacheImpl::updateAriaOwns(const AXObject* owner, const Vector<String>& idVector, HeapVector<Member<AXObject>>& ownedChildren)
{
    //
    // Update the map from the AXID of this element to the ids of the owned children,
    // and the reverse map from ids to possible AXID owners.
    //

    HashSet<String> currentIds = m_ariaOwnerToIdsMapping.get(owner->axObjectID());
    HashSet<String> newIds;
    bool idsChanged = false;
    for (const String& id : idVector) {
        newIds.add(id);
        if (!currentIds.contains(id)) {
            idsChanged = true;
            HashSet<AXID>* owners = m_idToAriaOwnersMapping.get(id);
            if (!owners) {
                owners = new HashSet<AXID>();
                m_idToAriaOwnersMapping.set(id, wrapUnique(owners));
            }
            owners->add(owner->axObjectID());
        }
    }
    for (const String& id : currentIds) {
        if (!newIds.contains(id)) {
            idsChanged = true;
            HashSet<AXID>* owners = m_idToAriaOwnersMapping.get(id);
            if (owners) {
                owners->remove(owner->axObjectID());
                if (owners->isEmpty())
                    m_idToAriaOwnersMapping.remove(id);
            }
        }
    }
    if (idsChanged)
        m_ariaOwnerToIdsMapping.set(owner->axObjectID(), newIds);

    //
    // Now figure out the ids that actually correspond to children that exist and
    // that we can legally own (not cyclical, not already owned, etc.) and update
    // the maps and |ownedChildren| based on that.
    //

    // Figure out the children that are owned by this object and are in the tree.
    TreeScope& scope = owner->getNode()->treeScope();
    Vector<AXID> newChildAXIDs;
    for (const String& idName : idVector) {
        Element* element = scope.getElementById(AtomicString(idName));
        if (!element)
            continue;

        AXObject* child = getOrCreate(element);
        if (!child)
            continue;

        // If this child is already aria-owned by a different owner, continue.
        // It's an author error if this happens and we don't worry about which of the
        // two owners wins ownership of the child, as long as only one of them does.
        if (isAriaOwned(child) && getAriaOwnedParent(child) != owner)
            continue;

        // You can't own yourself!
        if (child == owner)
            continue;

        // Walk up the parents of the owner object, make sure that this child doesn't appear
        // there, as that would create a cycle.
        bool foundCycle = false;
        for (AXObject* parent = owner->parentObject(); parent && !foundCycle; parent = parent->parentObject()) {
            if (parent == child)
                foundCycle = true;
        }
        if (foundCycle)
            continue;

        newChildAXIDs.append(child->axObjectID());
        ownedChildren.append(child);
    }

    // Compare this to the current list of owned children, and exit early if there are no changes.
    Vector<AXID> currentChildAXIDs = m_ariaOwnerToChildrenMapping.get(owner->axObjectID());
    bool same = true;
    if (currentChildAXIDs.size() != newChildAXIDs.size()) {
        same = false;
    } else {
        for (size_t i = 0; i < currentChildAXIDs.size() && same; ++i) {
            if (currentChildAXIDs[i] != newChildAXIDs[i])
                same = false;
        }
    }
    if (same)
        return;

    // The list of owned children has changed. Even if they were just reordered, to be safe
    // and handle all cases we remove all of the current owned children and add the new list
    // of owned children.
    for (size_t i = 0; i < currentChildAXIDs.size(); ++i) {
        // Find the AXObject for the child that this owner no longer owns.
        AXID removedChildID = currentChildAXIDs[i];
        AXObject* removedChild = objectFromAXID(removedChildID);

        // It's possible that this child has already been owned by some other owner,
        // in which case we don't need to do anything.
        if (removedChild && getAriaOwnedParent(removedChild) != owner)
            continue;

        // Remove it from the child -> owner mapping so it's not owned by this owner anymore.
        m_ariaOwnedChildToOwnerMapping.remove(removedChildID);

        if (removedChild) {
            // If the child still exists, find its "real" parent, and reparent it back to
            // its real parent in the tree by detaching it from its current parent and
            // calling childrenChanged on its real parent.
            removedChild->detachFromParent();
            AXID realParentID = m_ariaOwnedChildToRealParentMapping.get(removedChildID);
            AXObject* realParent = objectFromAXID(realParentID);
            childrenChanged(realParent);
        }

        // Remove the child -> original parent mapping too since this object has now been
        // reparented back to its original parent.
        m_ariaOwnedChildToRealParentMapping.remove(removedChildID);
    }

    for (size_t i = 0; i < newChildAXIDs.size(); ++i) {
        // Find the AXObject for the child that will now be a child of this owner.
        AXID addedChildID = newChildAXIDs[i];
        AXObject* addedChild = objectFromAXID(addedChildID);

        // Add this child to the mapping from child to owner.
        m_ariaOwnedChildToOwnerMapping.set(addedChildID, owner->axObjectID());

        // Add its parent object to a mapping from child to real parent. If later this owner
        // doesn't own this child anymore, we need to return it to its original parent.
        AXObject* originalParent = addedChild->parentObject();
        m_ariaOwnedChildToRealParentMapping.set(addedChildID, originalParent->axObjectID());

        // Now detach the object from its original parent and call childrenChanged on the
        // original parent so that it can recompute its list of children.
        addedChild->detachFromParent();
        childrenChanged(originalParent);
    }

    // Finally, update the mapping from the owner to the list of child IDs.
    m_ariaOwnerToChildrenMapping.set(owner->axObjectID(), newChildAXIDs);
}

void AXObjectCacheImpl::updateTreeIfElementIdIsAriaOwned(Element* element)
{
    if (!element->hasID())
        return;

    String id = element->getIdAttribute();
    HashSet<AXID>* owners = m_idToAriaOwnersMapping.get(id);
    if (!owners)
        return;

    AXObject* axElement = getOrCreate(element);
    if (!axElement)
        return;

    // If it's already owned, call childrenChanged on the owner to make sure it's
    // still an owner.
    if (isAriaOwned(axElement)) {
        AXObject* ownedParent = getAriaOwnedParent(axElement);
        ASSERT(ownedParent);
        childrenChanged(ownedParent);
        return;
    }

    // If it's not already owned, check the possible owners based on our mapping from
    // ids to elements that have that id listed in their aria-owns attribute.
    for (const auto& axID : *owners) {
        AXObject* owner = objectFromAXID(axID);
        if (owner)
            childrenChanged(owner);
    }
}

void AXObjectCacheImpl::checkedStateChanged(Node* node)
{
    postNotification(node, AXObjectCacheImpl::AXCheckedStateChanged);
}

void AXObjectCacheImpl::listboxOptionStateChanged(HTMLOptionElement* option)
{
    postNotification(option, AXCheckedStateChanged);
}

void AXObjectCacheImpl::listboxSelectedChildrenChanged(HTMLSelectElement* select)
{
    postNotification(select, AXSelectedChildrenChanged);
}

void AXObjectCacheImpl::listboxActiveIndexChanged(HTMLSelectElement* select)
{
    AXObject* obj = get(select);
    if (!obj || !obj->isAXListBox())
        return;

    toAXListBox(obj)->activeIndexChanged();
}

void AXObjectCacheImpl::radiobuttonRemovedFromGroup(HTMLInputElement* groupMember)
{
    AXObject* obj = get(groupMember);
    if (!obj || !obj->isAXRadioInput())
        return;

    // The 'posInSet' and 'setSize' attributes should be updated from the first node,
    // as the removed node is already detached from tree.
    HTMLInputElement* firstRadio = toAXRadioInput(obj)->findFirstRadioButtonInGroup(groupMember);
    AXObject* firstObj = get(firstRadio);
    if (!firstObj || !firstObj->isAXRadioInput())
        return;

    toAXRadioInput(firstObj)->updatePosAndSetSize(1);
    postNotification(firstObj, AXAriaAttributeChanged);
    toAXRadioInput(firstObj)->requestUpdateToNextNode(true);
}

void AXObjectCacheImpl::handleLayoutComplete(LayoutObject* layoutObject)
{
    if (!layoutObject)
        return;

    m_modificationCount++;

    // Create the AXObject if it didn't yet exist - that's always safe at the end of a layout, and it
    // allows an AX notification to be sent when a page has its first layout, rather than when the
    // document first loads.
    if (AXObject* obj = getOrCreate(layoutObject))
        postNotification(obj, AXLayoutComplete);
}

void AXObjectCacheImpl::handleClicked(Node* node)
{
    if (AXObject* obj = getOrCreate(node))
        postNotification(obj, AXClicked);
}

void AXObjectCacheImpl::handleAriaExpandedChange(Node* node)
{
    if (AXObject* obj = getOrCreate(node))
        obj->handleAriaExpandedChanged();
}

void AXObjectCacheImpl::handleAriaSelectedChanged(Node* node)
{
    AXObject* obj = get(node);
    if (!obj)
        return;

    postNotification(obj, AXCheckedStateChanged);

    AXObject* listbox = obj->parentObjectUnignored();
    if (listbox && listbox->roleValue() == ListBoxRole)
        postNotification(listbox, AXSelectedChildrenChanged);
}

void AXObjectCacheImpl::handleActiveDescendantChanged(Node* node)
{
    if (AXObject* obj = getOrCreate(node))
        obj->handleActiveDescendantChanged();
}

void AXObjectCacheImpl::handleAriaRoleChanged(Node* node)
{
    if (AXObject* obj = getOrCreate(node)) {
        obj->updateAccessibilityRole();
        m_modificationCount++;
        obj->notifyIfIgnoredValueChanged();
    }
}

void AXObjectCacheImpl::handleAttributeChanged(const QualifiedName& attrName, Element* element)
{
    if (attrName == roleAttr)
        handleAriaRoleChanged(element);
    else if (attrName == altAttr || attrName == titleAttr)
        textChanged(element);
    else if (attrName == forAttr && isHTMLLabelElement(*element))
        labelChanged(element);
    else if (attrName == idAttr)
        updateTreeIfElementIdIsAriaOwned(element);

    if (!attrName.localName().startsWith("aria-"))
        return;

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChanged(element);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXObjectCacheImpl::AXValueChanged);
    else if (attrName == aria_labelAttr || attrName == aria_labeledbyAttr || attrName == aria_labelledbyAttr)
        textChanged(element);
    else if (attrName == aria_checkedAttr)
        checkedStateChanged(element);
    else if (attrName == aria_selectedAttr)
        handleAriaSelectedChanged(element);
    else if (attrName == aria_expandedAttr)
        handleAriaExpandedChange(element);
    else if (attrName == aria_hiddenAttr)
        childrenChanged(element->parentNode());
    else if (attrName == aria_invalidAttr)
        postNotification(element, AXObjectCacheImpl::AXInvalidStatusChanged);
    else if (attrName == aria_ownsAttr)
        childrenChanged(element);
    else
        postNotification(element, AXObjectCacheImpl::AXAriaAttributeChanged);
}

void AXObjectCacheImpl::labelChanged(Element* element)
{
    textChanged(toHTMLLabelElement(element)->control());
}

void AXObjectCacheImpl::inlineTextBoxesUpdated(LineLayoutItem lineLayoutItem)
{
    if (!inlineTextBoxAccessibilityEnabled())
        return;

    LayoutObject* layoutObject = LineLayoutAPIShim::layoutObjectFrom(lineLayoutItem);

    // Only update if the accessibility object already exists and it's
    // not already marked as dirty.
    if (AXObject* obj = get(layoutObject)) {
        if (!obj->needsToUpdateChildren()) {
            obj->setNeedsToUpdateChildren();
            postNotification(layoutObject, AXChildrenChanged);
        }
    }
}

Settings* AXObjectCacheImpl::settings()
{
    return m_document->settings();
}

bool AXObjectCacheImpl::accessibilityEnabled()
{
    Settings* settings = this->settings();
    if (!settings)
        return false;
    return settings->accessibilityEnabled();
}

bool AXObjectCacheImpl::inlineTextBoxAccessibilityEnabled()
{
    Settings* settings = this->settings();
    if (!settings)
        return false;
    return settings->inlineTextBoxAccessibilityEnabled();
}

const Element* AXObjectCacheImpl::rootAXEditableElement(const Node* node)
{
    const Element* result = node->rootEditableElement();
    const Element* element = node->isElementNode() ? toElement(node) : node->parentElement();

    for (; element; element = element->parentElement()) {
        if (nodeIsTextControl(element))
            result = element;
    }

    return result;
}

AXObject* AXObjectCacheImpl::firstAccessibleObjectFromNode(const Node* node)
{
    if (!node)
        return 0;

    AXObject* accessibleObject = getOrCreate(node->layoutObject());
    while (accessibleObject && accessibleObject->accessibilityIsIgnored()) {
        node = NodeTraversal::next(*node);

        while (node && !node->layoutObject())
            node = NodeTraversal::nextSkippingChildren(*node);

        if (!node)
            return 0;

        accessibleObject = getOrCreate(node->layoutObject());
    }

    return accessibleObject;
}

bool AXObjectCacheImpl::nodeIsTextControl(const Node* node)
{
    if (!node)
        return false;

    const AXObject* axObject = getOrCreate(const_cast<Node*>(node));
    return axObject && axObject->isTextControl();
}

bool isNodeAriaVisible(Node* node)
{
    if (!node)
        return false;

    if (!node->isElementNode())
        return false;

    return equalIgnoringCase(toElement(node)->getAttribute(aria_hiddenAttr), "false");
}

void AXObjectCacheImpl::postPlatformNotification(AXObject* obj, AXNotification notification)
{
    if (!obj || !obj->getDocument() || !obj->documentFrameView() || !obj->documentFrameView()->frame().page())
        return;

    ChromeClient& client = obj->getDocument()->axObjectCacheOwner().page()->chromeClient();
    client.postAccessibilityNotification(obj, notification);
}

void AXObjectCacheImpl::handleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode)
{
    if (!newFocusedNode)
        return;

    Page* page = newFocusedNode->document().page();
    if (!page)
        return;

    AXObject* focusedObject = this->focusedObject();
    if (!focusedObject)
        return;

    AXObject* oldFocusedObject = get(oldFocusedNode);

    postPlatformNotification(oldFocusedObject, AXBlur);
    postPlatformNotification(focusedObject, AXFocusedUIElementChanged);
}

void AXObjectCacheImpl::handleInitialFocus()
{
    postNotification(m_document, AXObjectCache::AXFocusedUIElementChanged);
}

void AXObjectCacheImpl::handleEditableTextContentChanged(Node* node)
{
    AXObject* obj = get(node);
    while (obj && !obj->isNativeTextControl() && !obj->isNonNativeTextControl())
        obj = obj->parentObject();
    postNotification(obj, AXObjectCache::AXValueChanged);
}

void AXObjectCacheImpl::handleTextFormControlChanged(Node* node)
{
    handleEditableTextContentChanged(node);
}

void AXObjectCacheImpl::handleValueChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXValueChanged);
}

void AXObjectCacheImpl::handleUpdateActiveMenuOption(LayoutMenuList* menuList, int optionIndex)
{
    AXObject* obj = get(menuList);
    if (!obj || !obj->isMenuList())
        return;

    toAXMenuList(obj)->didUpdateActiveOption(optionIndex);
}

void AXObjectCacheImpl::didShowMenuListPopup(LayoutMenuList* menuList)
{
    AXObject* obj = get(menuList);
    if (!obj || !obj->isMenuList())
        return;

    toAXMenuList(obj)->didShowPopup();
}

void AXObjectCacheImpl::didHideMenuListPopup(LayoutMenuList* menuList)
{
    AXObject* obj = get(menuList);
    if (!obj || !obj->isMenuList())
        return;

    toAXMenuList(obj)->didHidePopup();
}

void AXObjectCacheImpl::handleLoadComplete(Document* document)
{
    postNotification(getOrCreate(document), AXObjectCache::AXLoadComplete);
}

void AXObjectCacheImpl::handleLayoutComplete(Document* document)
{
    postNotification(getOrCreate(document), AXObjectCache::AXLayoutComplete);
}

void AXObjectCacheImpl::handleScrolledToAnchor(const Node* anchorNode)
{
    // The anchor node may not be accessible. Post the notification for the
    // first accessible object.
    postPlatformNotification(firstAccessibleObjectFromNode(anchorNode), AXScrolledToAnchor);
}

void AXObjectCacheImpl::handleScrollPositionChanged(FrameView* frameView)
{
    AXObject* targetAXObject = getOrCreate(frameView->frame().document());
    postPlatformNotification(targetAXObject, AXScrollPositionChanged);
}

void AXObjectCacheImpl::handleScrollPositionChanged(LayoutObject* layoutObject)
{
    postPlatformNotification(getOrCreate(layoutObject), AXScrollPositionChanged);
}

const AtomicString& AXObjectCacheImpl::computedRoleForNode(Node* node)
{
    AXObject* obj = getOrCreate(node);
    if (!obj)
        return AXObject::roleName(UnknownRole);
    return AXObject::roleName(obj->roleValue());
}

String AXObjectCacheImpl::computedNameForNode(Node* node)
{
    AXObject* obj = getOrCreate(node);
    if (!obj)
        return "";

    return obj->computedName();
}

void AXObjectCacheImpl::onTouchAccessibilityHover(const IntPoint& location)
{
    AXObject* hit = root()->accessibilityHitTest(location);
    if (hit) {
        // Ignore events on a frame or plug-in, because the touch events
        // will be re-targeted there and we don't want to fire duplicate
        // accessibility events.
        if (hit->getLayoutObject() && hit->getLayoutObject()->isLayoutPart())
            return;

        postPlatformNotification(hit, AXHover);
    }
}

void AXObjectCacheImpl::setCanvasObjectBounds(Element* element, const LayoutRect& rect)
{
    AXObject* obj = getOrCreate(element);
    if (!obj)
        return;

    obj->setElementRect(rect);
}

DEFINE_TRACE(AXObjectCacheImpl)
{
    visitor->trace(m_document);
    visitor->trace(m_nodeObjectMapping);

    visitor->trace(m_objects);
    visitor->trace(m_notificationsToPost);

    AXObjectCache::trace(visitor);
}

} // namespace blink
