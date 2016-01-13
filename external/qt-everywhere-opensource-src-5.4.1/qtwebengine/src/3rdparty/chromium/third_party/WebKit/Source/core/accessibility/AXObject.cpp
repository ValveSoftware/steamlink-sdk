/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/accessibility/AXObject.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/dom/NodeTraversal.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "platform/UserGestureIndicator.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

using blink::WebLocalizedString;

namespace WebCore {

using namespace HTMLNames;

typedef HashMap<String, AccessibilityRole, CaseFoldingHash> ARIARoleMap;

struct RoleEntry {
    String ariaRole;
    AccessibilityRole webcoreRole;
};

static ARIARoleMap* createARIARoleMap()
{
    const RoleEntry roles[] = {
        { "alert", AlertRole },
        { "alertdialog", AlertDialogRole },
        { "application", ApplicationRole },
        { "article", ArticleRole },
        { "banner", BannerRole },
        { "button", ButtonRole },
        { "checkbox", CheckBoxRole },
        { "complementary", ComplementaryRole },
        { "contentinfo", ContentInfoRole },
        { "dialog", DialogRole },
        { "directory", DirectoryRole },
        { "grid", GridRole },
        { "gridcell", CellRole },
        { "columnheader", ColumnHeaderRole },
        { "combobox", ComboBoxRole },
        { "definition", DefinitionRole },
        { "document", DocumentRole },
        { "rowheader", RowHeaderRole },
        { "group", GroupRole },
        { "heading", HeadingRole },
        { "img", ImageRole },
        { "link", LinkRole },
        { "list", ListRole },
        { "listitem", ListItemRole },
        { "listbox", ListBoxRole },
        { "log", LogRole },
        // "option" isn't here because it may map to different roles depending on the parent element's role
        { "main", MainRole },
        { "marquee", MarqueeRole },
        { "math", MathRole },
        { "menu", MenuRole },
        { "menubar", MenuBarRole },
        { "menuitem", MenuItemRole },
        { "menuitemcheckbox", MenuItemRole },
        { "menuitemradio", MenuItemRole },
        { "note", NoteRole },
        { "navigation", NavigationRole },
        { "option", ListBoxOptionRole },
        { "presentation", PresentationalRole },
        { "progressbar", ProgressIndicatorRole },
        { "radio", RadioButtonRole },
        { "radiogroup", RadioGroupRole },
        { "region", RegionRole },
        { "row", RowRole },
        { "scrollbar", ScrollBarRole },
        { "search", SearchRole },
        { "separator", SplitterRole },
        { "slider", SliderRole },
        { "spinbutton", SpinButtonRole },
        { "status", StatusRole },
        { "tab", TabRole },
        { "tablist", TabListRole },
        { "tabpanel", TabPanelRole },
        { "text", StaticTextRole },
        { "textbox", TextAreaRole },
        { "timer", TimerRole },
        { "toolbar", ToolbarRole },
        { "tooltip", UserInterfaceTooltipRole },
        { "tree", TreeRole },
        { "treegrid", TreeGridRole },
        { "treeitem", TreeItemRole }
    };
    ARIARoleMap* roleMap = new ARIARoleMap;

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(roles); ++i)
        roleMap->set(roles[i].ariaRole, roles[i].webcoreRole);
    return roleMap;
}

AXObject::AXObject()
    : m_id(0)
    , m_haveChildren(false)
    , m_role(UnknownRole)
    , m_lastKnownIsIgnoredValue(DefaultBehavior)
    , m_detached(false)
{
}

AXObject::~AXObject()
{
    ASSERT(isDetached());
}

void AXObject::detach()
{
    // Clear any children and call detachFromParent on them so that
    // no children are left with dangling pointers to their parent.
    clearChildren();

    m_detached = true;
}

bool AXObject::isDetached() const
{
    return m_detached;
}

AXObjectCache* AXObject::axObjectCache() const
{
    Document* doc = document();
    if (doc)
        return doc->axObjectCache();
    return 0;
}

bool AXObject::isARIATextControl() const
{
    return ariaRoleAttribute() == TextAreaRole || ariaRoleAttribute() == TextFieldRole;
}

bool AXObject::isButton() const
{
    AccessibilityRole role = roleValue();

    return role == ButtonRole || role == PopUpButtonRole || role == ToggleButtonRole;
}

bool AXObject::isLandmarkRelated() const
{
    switch (roleValue()) {
    case ApplicationRole:
    case ArticleRole:
    case BannerRole:
    case ComplementaryRole:
    case ContentInfoRole:
    case FooterRole:
    case MainRole:
    case NavigationRole:
    case RegionRole:
    case SearchRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::isMenuRelated() const
{
    switch (roleValue()) {
    case MenuRole:
    case MenuBarRole:
    case MenuButtonRole:
    case MenuItemRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::isTextControl() const
{
    switch (roleValue()) {
    case TextAreaRole:
    case TextFieldRole:
    case ComboBoxRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::isClickable() const
{
    switch (roleValue()) {
    case ButtonRole:
    case CheckBoxRole:
    case ColorWellRole:
    case ComboBoxRole:
    case EditableTextRole:
    case ImageMapLinkRole:
    case LinkRole:
    case ListBoxOptionRole:
    case MenuButtonRole:
    case PopUpButtonRole:
    case RadioButtonRole:
    case TabRole:
    case TextAreaRole:
    case TextFieldRole:
    case ToggleButtonRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::isExpanded() const
{
    if (equalIgnoringCase(getAttribute(aria_expandedAttr), "true"))
        return true;

    return false;
}

bool AXObject::accessibilityIsIgnored() const
{
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return true;

    AXComputedObjectAttributeCache* attributeCache = cache->computedObjectAttributeCache();
    if (attributeCache) {
        AXObjectInclusion ignored = attributeCache->getIgnored(axObjectID());
        switch (ignored) {
        case IgnoreObject:
            return true;
        case IncludeObject:
            return false;
        case DefaultBehavior:
            break;
        }
    }

    bool result = computeAccessibilityIsIgnored();

    if (attributeCache)
        attributeCache->setIgnored(axObjectID(), result ? IgnoreObject : IncludeObject);

    return result;
}

bool AXObject::accessibilityIsIgnoredByDefault() const
{
    return defaultObjectInclusion() == IgnoreObject;
}

AXObjectInclusion AXObject::accessibilityPlatformIncludesObject() const
{
    if (isMenuListPopup() || isMenuListOption())
        return IncludeObject;

    return DefaultBehavior;
}

AXObjectInclusion AXObject::defaultObjectInclusion() const
{
    if (isInertOrAriaHidden())
        return IgnoreObject;

    if (isPresentationalChildOfAriaRole())
        return IgnoreObject;

    return accessibilityPlatformIncludesObject();
}

bool AXObject::isInertOrAriaHidden() const
{
    bool mightBeInInertSubtree = true;
    for (const AXObject* object = this; object; object = object->parentObject()) {
        if (equalIgnoringCase(object->getAttribute(aria_hiddenAttr), "true"))
            return true;
        if (mightBeInInertSubtree && object->node()) {
            if (object->node()->isInert())
                return true;
            mightBeInInertSubtree = false;
        }
    }

    return false;
}

bool AXObject::lastKnownIsIgnoredValue()
{
    if (m_lastKnownIsIgnoredValue == DefaultBehavior)
        m_lastKnownIsIgnoredValue = accessibilityIsIgnored() ? IgnoreObject : IncludeObject;

    return m_lastKnownIsIgnoredValue == IgnoreObject;
}

void AXObject::setLastKnownIsIgnoredValue(bool isIgnored)
{
    m_lastKnownIsIgnoredValue = isIgnored ? IgnoreObject : IncludeObject;
}

// Lacking concrete evidence of orientation, horizontal means width > height. vertical is height > width;
AccessibilityOrientation AXObject::orientation() const
{
    LayoutRect bounds = elementRect();
    if (bounds.size().width() > bounds.size().height())
        return AccessibilityOrientationHorizontal;
    if (bounds.size().height() > bounds.size().width())
        return AccessibilityOrientationVertical;

    // A tie goes to horizontal.
    return AccessibilityOrientationHorizontal;
}

static String queryString(WebLocalizedString::Name name)
{
    return Locale::defaultLocale().queryString(name);
}

String AXObject::actionVerb() const
{
    // FIXME: Need to add verbs for select elements.

    switch (roleValue()) {
    case ButtonRole:
    case ToggleButtonRole:
        return queryString(WebLocalizedString::AXButtonActionVerb);
    case TextFieldRole:
    case TextAreaRole:
        return queryString(WebLocalizedString::AXTextFieldActionVerb);
    case RadioButtonRole:
        return queryString(WebLocalizedString::AXRadioButtonActionVerb);
    case CheckBoxRole:
        return queryString(isChecked() ? WebLocalizedString::AXCheckedCheckBoxActionVerb : WebLocalizedString::AXUncheckedCheckBoxActionVerb);
    case LinkRole:
        return queryString(WebLocalizedString::AXLinkActionVerb);
    case PopUpButtonRole:
        // FIXME: Implement.
        return String();
    case MenuListPopupRole:
        // FIXME: Implement.
        return String();
    default:
        return emptyString();
    }
}

AccessibilityButtonState AXObject::checkboxOrRadioValue() const
{
    // If this is a real checkbox or radio button, AXRenderObject will handle.
    // If it's an ARIA checkbox or radio, the aria-checked attribute should be used.

    const AtomicString& result = getAttribute(aria_checkedAttr);
    if (equalIgnoringCase(result, "true"))
        return ButtonStateOn;
    if (equalIgnoringCase(result, "mixed"))
        return ButtonStateMixed;

    return ButtonStateOff;
}

bool AXObject::ariaIsMultiline() const
{
    return equalIgnoringCase(getAttribute(aria_multilineAttr), "true");
}

bool AXObject::ariaPressedIsPresent() const
{
    return !getAttribute(aria_pressedAttr).isEmpty();
}

bool AXObject::supportsARIAAttributes() const
{
    return supportsARIALiveRegion()
        || supportsARIADragging()
        || supportsARIADropping()
        || supportsARIAFlowTo()
        || supportsARIAOwns()
        || hasAttribute(aria_labelAttr);
}

bool AXObject::supportsRangeValue() const
{
    return isProgressIndicator()
        || isSlider()
        || isScrollbar()
        || isSpinButton();
}

void AXObject::ariaTreeRows(AccessibilityChildrenVector& result)
{
    AccessibilityChildrenVector axChildren = children();
    unsigned count = axChildren.size();
    for (unsigned k = 0; k < count; ++k) {
        AXObject* obj = axChildren[k].get();

        // Add tree items as the rows.
        if (obj->roleValue() == TreeItemRole)
            result.append(obj);

        // Now see if this item also has rows hiding inside of it.
        obj->ariaTreeRows(result);
    }
}

bool AXObject::supportsARIALiveRegion() const
{
    const AtomicString& liveRegion = ariaLiveRegionStatus();
    return equalIgnoringCase(liveRegion, "polite") || equalIgnoringCase(liveRegion, "assertive");
}

void AXObject::markCachedElementRectDirty() const
{
    for (unsigned i = 0; i < m_children.size(); ++i)
        m_children[i].get()->markCachedElementRectDirty();
}

IntPoint AXObject::clickPoint()
{
    LayoutRect rect = elementRect();
    return roundedIntPoint(LayoutPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2));
}

IntRect AXObject::boundingBoxForQuads(RenderObject* obj, const Vector<FloatQuad>& quads)
{
    ASSERT(obj);
    if (!obj)
        return IntRect();

    size_t count = quads.size();
    if (!count)
        return IntRect();

    IntRect result;
    for (size_t i = 0; i < count; ++i) {
        IntRect r = quads[i].enclosingBoundingBox();
        if (!r.isEmpty()) {
            if (obj->style()->hasAppearance())
                RenderTheme::theme().adjustRepaintRect(obj, r);
            result.unite(r);
        }
    }
    return result;
}

AXObject* AXObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    // Send the hit test back into the sub-frame if necessary.
    if (isAttachment()) {
        Widget* widget = widgetForAttachmentView();
        // Normalize the point for the widget's bounds.
        if (widget && widget->isFrameView())
            return axObjectCache()->getOrCreate(widget)->accessibilityHitTest(IntPoint(point - widget->frameRect().location()));
    }

    // Check if there are any mock elements that need to be handled.
    size_t count = m_children.size();
    for (size_t k = 0; k < count; k++) {
        if (m_children[k]->isMockObject() && m_children[k]->elementRect().contains(point))
            return m_children[k]->elementAccessibilityHitTest(point);
    }

    return const_cast<AXObject*>(this);
}

const AXObject::AccessibilityChildrenVector& AXObject::children()
{
    updateChildrenIfNecessary();

    return m_children;
}

AXObject* AXObject::parentObjectUnignored() const
{
    AXObject* parent;
    for (parent = parentObject(); parent && parent->accessibilityIsIgnored(); parent = parent->parentObject()) {
    }

    return parent;
}

AXObject* AXObject::firstAccessibleObjectFromNode(const Node* node)
{
    if (!node)
        return 0;

    AXObjectCache* cache = node->document().axObjectCache();
    AXObject* accessibleObject = cache->getOrCreate(node->renderer());
    while (accessibleObject && accessibleObject->accessibilityIsIgnored()) {
        node = NodeTraversal::next(*node);

        while (node && !node->renderer())
            node = NodeTraversal::nextSkippingChildren(*node);

        if (!node)
            return 0;

        accessibleObject = cache->getOrCreate(node->renderer());
    }

    return accessibleObject;
}

void AXObject::updateChildrenIfNecessary()
{
    if (!hasChildren())
        addChildren();
}

void AXObject::clearChildren()
{
    // Some objects have weak pointers to their parents and those associations need to be detached.
    size_t length = m_children.size();
    for (size_t i = 0; i < length; i++)
        m_children[i]->detachFromParent();

    m_children.clear();
    m_haveChildren = false;
}

AXObject* AXObject::focusedUIElement() const
{
    Document* doc = document();
    if (!doc)
        return 0;

    Page* page = doc->page();
    if (!page)
        return 0;

    return AXObjectCache::focusedUIElementForPage(page);
}

Document* AXObject::document() const
{
    FrameView* frameView = documentFrameView();
    if (!frameView)
        return 0;

    return frameView->frame().document();
}

FrameView* AXObject::documentFrameView() const
{
    const AXObject* object = this;
    while (object && !object->isAXRenderObject())
        object = object->parentObject();

    if (!object)
        return 0;

    return object->documentFrameView();
}

String AXObject::language() const
{
    const AtomicString& lang = getAttribute(langAttr);
    if (!lang.isEmpty())
        return lang;

    AXObject* parent = parentObject();

    // as a last resort, fall back to the content language specified in the meta tag
    if (!parent) {
        Document* doc = document();
        if (doc)
            return doc->contentLanguage();
        return nullAtom;
    }

    return parent->language();
}

bool AXObject::hasAttribute(const QualifiedName& attribute) const
{
    Node* elementNode = node();
    if (!elementNode)
        return false;

    if (!elementNode->isElementNode())
        return false;

    Element* element = toElement(elementNode);
    return element->fastHasAttribute(attribute);
}

const AtomicString& AXObject::getAttribute(const QualifiedName& attribute) const
{
    Node* elementNode = node();
    if (!elementNode)
        return nullAtom;

    if (!elementNode->isElementNode())
        return nullAtom;

    Element* element = toElement(elementNode);
    return element->fastGetAttribute(attribute);
}

bool AXObject::press() const
{
    Element* actionElem = actionElement();
    if (!actionElem)
        return false;
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    actionElem->accessKeyAction(true);
    return true;
}

void AXObject::scrollToMakeVisible() const
{
    IntRect objectRect = pixelSnappedIntRect(elementRect());
    objectRect.setLocation(IntPoint());
    scrollToMakeVisibleWithSubFocus(objectRect);
}

// This is a 1-dimensional scroll offset helper function that's applied
// separately in the horizontal and vertical directions, because the
// logic is the same. The goal is to compute the best scroll offset
// in order to make an object visible within a viewport.
//
// In case the whole object cannot fit, you can specify a
// subfocus - a smaller region within the object that should
// be prioritized. If the whole object can fit, the subfocus is
// ignored.
//
// Example: the viewport is scrolled to the right just enough
// that the object is in view.
//   Before:
//   +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
//   After:
//          +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
// When constraints cannot be fully satisfied, the min
// (left/top) position takes precedence over the max (right/bottom).
//
// Note that the return value represents the ideal new scroll offset.
// This may be out of range - the calling function should clip this
// to the available range.
static int computeBestScrollOffset(int currentScrollOffset, int subfocusMin, int subfocusMax, int objectMin, int objectMax, int viewportMin, int viewportMax)
{
    int viewportSize = viewportMax - viewportMin;

    // If the focus size is larger than the viewport size, shrink it in the
    // direction of subfocus.
    if (objectMax - objectMin > viewportSize) {
        // Subfocus must be within focus:
        subfocusMin = std::max(subfocusMin, objectMin);
        subfocusMax = std::min(subfocusMax, objectMax);

        // Subfocus must be no larger than the viewport size; favor top/left.
        if (subfocusMax - subfocusMin > viewportSize)
            subfocusMax = subfocusMin + viewportSize;

        if (subfocusMin + viewportSize > objectMax) {
            objectMin = objectMax - viewportSize;
        } else {
            objectMin = subfocusMin;
            objectMax = subfocusMin + viewportSize;
        }
    }

    // Exit now if the focus is already within the viewport.
    if (objectMin - currentScrollOffset >= viewportMin
        && objectMax - currentScrollOffset <= viewportMax)
        return currentScrollOffset;

    // Scroll left if we're too far to the right.
    if (objectMax - currentScrollOffset > viewportMax)
        return objectMax - viewportMax;

    // Scroll right if we're too far to the left.
    if (objectMin - currentScrollOffset < viewportMin)
        return objectMin - viewportMin;

    ASSERT_NOT_REACHED();

    // This shouldn't happen.
    return currentScrollOffset;
}

void AXObject::scrollToMakeVisibleWithSubFocus(const IntRect& subfocus) const
{
    // Search up the parent chain until we find the first one that's scrollable.
    AXObject* scrollParent = parentObject();
    ScrollableArea* scrollableArea;
    for (scrollableArea = 0;
        scrollParent && !(scrollableArea = scrollParent->getScrollableAreaIfScrollable());
        scrollParent = scrollParent->parentObject()) { }
    if (!scrollableArea)
        return;

    IntRect objectRect = pixelSnappedIntRect(elementRect());
    IntPoint scrollPosition = scrollableArea->scrollPosition();
    IntRect scrollVisibleRect = scrollableArea->visibleContentRect();

    int desiredX = computeBestScrollOffset(
        scrollPosition.x(),
        objectRect.x() + subfocus.x(), objectRect.x() + subfocus.maxX(),
        objectRect.x(), objectRect.maxX(),
        0, scrollVisibleRect.width());
    int desiredY = computeBestScrollOffset(
        scrollPosition.y(),
        objectRect.y() + subfocus.y(), objectRect.y() + subfocus.maxY(),
        objectRect.y(), objectRect.maxY(),
        0, scrollVisibleRect.height());

    scrollParent->scrollTo(IntPoint(desiredX, desiredY));

    // Recursively make sure the scroll parent itself is visible.
    if (scrollParent->parentObject())
        scrollParent->scrollToMakeVisible();
}

void AXObject::scrollToGlobalPoint(const IntPoint& globalPoint) const
{
    // Search up the parent chain and create a vector of all scrollable parent objects
    // and ending with this object itself.
    Vector<const AXObject*> objects;
    AXObject* parentObject;
    for (parentObject = this->parentObject(); parentObject; parentObject = parentObject->parentObject()) {
        if (parentObject->getScrollableAreaIfScrollable())
            objects.prepend(parentObject);
    }
    objects.append(this);

    // Start with the outermost scrollable (the main window) and try to scroll the
    // next innermost object to the given point.
    int offsetX = 0, offsetY = 0;
    IntPoint point = globalPoint;
    size_t levels = objects.size() - 1;
    for (size_t i = 0; i < levels; i++) {
        const AXObject* outer = objects[i];
        const AXObject* inner = objects[i + 1];

        ScrollableArea* scrollableArea = outer->getScrollableAreaIfScrollable();

        IntRect innerRect = inner->isAXScrollView() ? pixelSnappedIntRect(inner->parentObject()->elementRect()) : pixelSnappedIntRect(inner->elementRect());
        IntRect objectRect = innerRect;
        IntPoint scrollPosition = scrollableArea->scrollPosition();

        // Convert the object rect into local coordinates.
        objectRect.move(offsetX, offsetY);
        if (!outer->isAXScrollView())
            objectRect.move(scrollPosition.x(), scrollPosition.y());

        int desiredX = computeBestScrollOffset(
            0,
            objectRect.x(), objectRect.maxX(),
            objectRect.x(), objectRect.maxX(),
            point.x(), point.x());
        int desiredY = computeBestScrollOffset(
            0,
            objectRect.y(), objectRect.maxY(),
            objectRect.y(), objectRect.maxY(),
            point.y(), point.y());
        outer->scrollTo(IntPoint(desiredX, desiredY));

        if (outer->isAXScrollView() && !inner->isAXScrollView()) {
            // If outer object we just scrolled is a scroll view (main window or iframe) but the
            // inner object is not, keep track of the coordinate transformation to apply to
            // future nested calculations.
            scrollPosition = scrollableArea->scrollPosition();
            offsetX -= (scrollPosition.x() + point.x());
            offsetY -= (scrollPosition.y() + point.y());
            point.move(scrollPosition.x() - innerRect.x(), scrollPosition.y() - innerRect.y());
        } else if (inner->isAXScrollView()) {
            // Otherwise, if the inner object is a scroll view, reset the coordinate transformation.
            offsetX = 0;
            offsetY = 0;
        }
    }
}

void AXObject::notifyIfIgnoredValueChanged()
{
    bool isIgnored = accessibilityIsIgnored();
    if (lastKnownIsIgnoredValue() != isIgnored) {
        axObjectCache()->childrenChanged(parentObject());
        setLastKnownIsIgnoredValue(isIgnored);
    }
}

void AXObject::selectionChanged()
{
    if (AXObject* parent = parentObjectIfExists())
        parent->selectionChanged();
}

int AXObject::lineForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull() || !node())
        return -1;

    // If the position is not in the same editable region as this AX object, return -1.
    Node* containerNode = visiblePos.deepEquivalent().containerNode();
    if (!containerNode->containsIncludingShadowDOM(node()) && !node()->containsIncludingShadowDOM(containerNode))
        return -1;

    int lineCount = -1;
    VisiblePosition currentVisiblePos = visiblePos;
    VisiblePosition savedVisiblePos;

    // move up until we get to the top
    // FIXME: This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    do {
        savedVisiblePos = currentVisiblePos;
        VisiblePosition prevVisiblePos = previousLinePosition(currentVisiblePos, 0, HasEditableAXRole);
        currentVisiblePos = prevVisiblePos;
        ++lineCount;
    }  while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos)));

    return lineCount;
}

bool AXObject::isARIAControl(AccessibilityRole ariaRole)
{
    return isARIAInput(ariaRole) || ariaRole == TextAreaRole || ariaRole == ButtonRole
    || ariaRole == ComboBoxRole || ariaRole == SliderRole;
}

bool AXObject::isARIAInput(AccessibilityRole ariaRole)
{
    return ariaRole == RadioButtonRole || ariaRole == CheckBoxRole || ariaRole == TextFieldRole;
}

AccessibilityRole AXObject::ariaRoleToWebCoreRole(const String& value)
{
    ASSERT(!value.isEmpty());

    static const ARIARoleMap* roleMap = createARIARoleMap();

    Vector<String> roleVector;
    value.split(' ', roleVector);
    AccessibilityRole role = UnknownRole;
    unsigned size = roleVector.size();
    for (unsigned i = 0; i < size; ++i) {
        String roleName = roleVector[i];
        role = roleMap->get(roleName);
        if (role)
            return role;
    }

    return role;
}

AccessibilityRole AXObject::buttonRoleType() const
{
    // If aria-pressed is present, then it should be exposed as a toggle button.
    // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
    if (ariaPressedIsPresent())
        return ToggleButtonRole;
    if (ariaHasPopup())
        return PopUpButtonRole;
    // We don't contemplate RadioButtonRole, as it depends on the input
    // type.

    return ButtonRole;
}

} // namespace WebCore
