/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 *
 */

#include "core/html/TextControlElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeList.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextBoundaries.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

TextControlElement::TextControlElement(const QualifiedName& tagName,
                                       Document& doc,
                                       HTMLFormElement* form)
    : HTMLFormControlElementWithState(tagName, doc, form),
      m_lastChangeWasUserEdit(false),
      m_cachedSelectionStart(0),
      m_cachedSelectionEnd(0) {
  m_cachedSelectionDirection =
      doc.frame() &&
              doc.frame()
                  ->editor()
                  .behavior()
                  .shouldConsiderSelectionAsDirectional()
          ? SelectionHasForwardDirection
          : SelectionHasNoDirection;
}

TextControlElement::~TextControlElement() {}

Node::InsertionNotificationRequest TextControlElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLFormControlElementWithState::insertedInto(insertionPoint);
  if (!insertionPoint->isConnected())
    return InsertionDone;
  String initialValue = value();
  setTextAsOfLastFormControlChangeEvent(initialValue.isNull() ? emptyString()
                                                              : initialValue);
  return InsertionDone;
}

void TextControlElement::dispatchFocusEvent(
    Element* oldFocusedElement,
    WebFocusType type,
    InputDeviceCapabilities* sourceCapabilities) {
  if (supportsPlaceholder())
    updatePlaceholderVisibility();
  handleFocusEvent(oldFocusedElement, type);
  HTMLFormControlElementWithState::dispatchFocusEvent(oldFocusedElement, type,
                                                      sourceCapabilities);
}

void TextControlElement::dispatchBlurEvent(
    Element* newFocusedElement,
    WebFocusType type,
    InputDeviceCapabilities* sourceCapabilities) {
  if (supportsPlaceholder())
    updatePlaceholderVisibility();
  handleBlurEvent();
  HTMLFormControlElementWithState::dispatchBlurEvent(newFocusedElement, type,
                                                     sourceCapabilities);
}

void TextControlElement::defaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::webkitEditableContentChanged &&
      layoutObject() && layoutObject()->isTextControl()) {
    m_lastChangeWasUserEdit = !document().isRunningExecCommand();
    subtreeHasChanged();
    return;
  }

  HTMLFormControlElementWithState::defaultEventHandler(event);
}

void TextControlElement::forwardEvent(Event* event) {
  if (event->type() == EventTypeNames::blur ||
      event->type() == EventTypeNames::focus)
    return;
  innerEditorElement()->defaultEventHandler(event);
}

String TextControlElement::strippedPlaceholder() const {
  // According to the HTML5 specification, we need to remove CR and LF from
  // the attribute value.
  const AtomicString& attributeValue = fastGetAttribute(placeholderAttr);
  if (!attributeValue.contains(newlineCharacter) &&
      !attributeValue.contains(carriageReturnCharacter))
    return attributeValue;

  StringBuilder stripped;
  unsigned length = attributeValue.length();
  stripped.reserveCapacity(length);
  for (unsigned i = 0; i < length; ++i) {
    UChar character = attributeValue[i];
    if (character == newlineCharacter || character == carriageReturnCharacter)
      continue;
    stripped.append(character);
  }
  return stripped.toString();
}

static bool isNotLineBreak(UChar ch) {
  return ch != newlineCharacter && ch != carriageReturnCharacter;
}

bool TextControlElement::isPlaceholderEmpty() const {
  const AtomicString& attributeValue = fastGetAttribute(placeholderAttr);
  return attributeValue.getString().find(isNotLineBreak) == kNotFound;
}

bool TextControlElement::placeholderShouldBeVisible() const {
  return supportsPlaceholder() && isEmptyValue() && isEmptySuggestedValue() &&
         !isPlaceholderEmpty();
}

HTMLElement* TextControlElement::placeholderElement() const {
  return toHTMLElementOrDie(
      userAgentShadowRoot()->getElementById(ShadowElementNames::placeholder()));
}

void TextControlElement::updatePlaceholderVisibility() {
  HTMLElement* placeholder = placeholderElement();
  if (!placeholder) {
    updatePlaceholderText();
    return;
  }

  bool placeHolderWasVisible = isPlaceholderVisible();
  setPlaceholderVisibility(placeholderShouldBeVisible());
  if (placeHolderWasVisible == isPlaceholderVisible())
    return;

  pseudoStateChanged(CSSSelector::PseudoPlaceholderShown);
  placeholder->setInlineStyleProperty(
      CSSPropertyDisplay, isPlaceholderVisible() ? CSSValueBlock : CSSValueNone,
      true);
}

void TextControlElement::setSelectionStart(int start) {
  setSelectionRangeForBinding(start, std::max(start, selectionEnd()),
                              selectionDirection());
}

void TextControlElement::setSelectionEnd(int end) {
  setSelectionRangeForBinding(std::min(end, selectionStart()), end,
                              selectionDirection());
}

void TextControlElement::setSelectionDirection(const String& direction) {
  setSelectionRangeForBinding(selectionStart(), selectionEnd(), direction);
}

void TextControlElement::select() {
  setSelectionRangeForBinding(0, std::numeric_limits<int>::max());
  // Avoid SelectionBehaviorOnFocus::Restore, which scrolls containers to show
  // the selection.
  focus(FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
  restoreCachedSelection();
}

void TextControlElement::setChangedSinceLastFormControlChangeEvent(
    bool changed) {
  m_wasChangedSinceLastFormControlChangeEvent = changed;
}

void TextControlElement::setFocused(bool flag) {
  HTMLFormControlElementWithState::setFocused(flag);

  if (!flag) {
    if (wasChangedSinceLastFormControlChangeEvent()) {
      dispatchFormControlChangeEvent();
    } else {
      // |value| IDL attribute setter haven't updated
      // textAsOfLastFormControlChangeEvent while this is focused. So we
      // synchronize now.
      setTextAsOfLastFormControlChangeEvent(value());
    }
  }
}

bool TextControlElement::shouldDispatchFormControlChangeEvent(
    String& oldValue,
    String& newValue) {
  return !equalIgnoringNullity(oldValue, newValue);
}

void TextControlElement::dispatchFormControlChangeEvent() {
  String newValue = value();
  if (shouldDispatchFormControlChangeEvent(m_textAsOfLastFormControlChangeEvent,
                                           newValue)) {
    setTextAsOfLastFormControlChangeEvent(newValue);
    dispatchChangeEvent();
  }
  setChangedSinceLastFormControlChangeEvent(false);
}

void TextControlElement::enqueueChangeEvent() {
  String newValue = value();
  if (shouldDispatchFormControlChangeEvent(m_textAsOfLastFormControlChangeEvent,
                                           newValue)) {
    setTextAsOfLastFormControlChangeEvent(newValue);
    Event* event = Event::createBubble(EventTypeNames::change);
    event->setTarget(this);
    document().enqueueAnimationFrameEvent(event);
  }
  setChangedSinceLastFormControlChangeEvent(false);
}

void TextControlElement::dispatchFormControlInputEvent() {
  setChangedSinceLastFormControlChangeEvent(true);
  HTMLFormControlElementWithState::dispatchInputEvent();
}

void TextControlElement::setRangeText(const String& replacement,
                                      ExceptionState& exceptionState) {
  setRangeText(replacement, selectionStart(), selectionEnd(), "preserve",
               exceptionState);
}

void TextControlElement::setRangeText(const String& replacement,
                                      unsigned start,
                                      unsigned end,
                                      const String& selectionMode,
                                      ExceptionState& exceptionState) {
  if (start > end) {
    exceptionState.throwDOMException(
        IndexSizeError, "The provided start value (" + String::number(start) +
                            ") is larger than the provided end value (" +
                            String::number(end) + ").");
    return;
  }
  if (openShadowRoot())
    return;

  String text = innerEditorValue();
  unsigned textLength = text.length();
  unsigned replacementLength = replacement.length();
  unsigned newSelectionStart = selectionStart();
  unsigned newSelectionEnd = selectionEnd();

  start = std::min(start, textLength);
  end = std::min(end, textLength);

  if (start < end)
    text.replace(start, end - start, replacement);
  else
    text.insert(replacement, start);

  setValue(text, TextFieldEventBehavior::DispatchNoEvent);

  if (selectionMode == "select") {
    newSelectionStart = start;
    newSelectionEnd = start + replacementLength;
  } else if (selectionMode == "start") {
    newSelectionStart = newSelectionEnd = start;
  } else if (selectionMode == "end") {
    newSelectionStart = newSelectionEnd = start + replacementLength;
  } else {
    DCHECK_EQ(selectionMode, "preserve");
    long delta = replacementLength - (end - start);

    if (newSelectionStart > end)
      newSelectionStart += delta;
    else if (newSelectionStart > start)
      newSelectionStart = start;

    if (newSelectionEnd > end)
      newSelectionEnd += delta;
    else if (newSelectionEnd > start)
      newSelectionEnd = start + replacementLength;
  }

  setSelectionRangeForBinding(newSelectionStart, newSelectionEnd);
}

void TextControlElement::setSelectionRangeForBinding(
    int start,
    int end,
    const String& directionString) {
  TextFieldSelectionDirection direction = SelectionHasNoDirection;
  if (directionString == "forward")
    direction = SelectionHasForwardDirection;
  else if (directionString == "backward")
    direction = SelectionHasBackwardDirection;
  if (setSelectionRange(start, end, direction))
    scheduleSelectEvent();
}

static Position positionForIndex(HTMLElement* innerEditor, int index) {
  DCHECK_GE(index, 0);
  if (index == 0) {
    Node* node = NodeTraversal::next(*innerEditor, innerEditor);
    if (node && node->isTextNode())
      return Position(node, 0);
    return Position(innerEditor, 0);
  }
  int remainingCharactersToMoveForward = index;
  Node* lastBrOrText = innerEditor;
  for (Node& node : NodeTraversal::descendantsOf(*innerEditor)) {
    DCHECK_GE(remainingCharactersToMoveForward, 0);
    if (node.hasTagName(brTag)) {
      if (remainingCharactersToMoveForward == 0)
        return Position::beforeNode(&node);
      --remainingCharactersToMoveForward;
      lastBrOrText = &node;
      continue;
    }

    if (node.isTextNode()) {
      Text& text = toText(node);
      if (remainingCharactersToMoveForward < static_cast<int>(text.length()))
        return Position(&text, remainingCharactersToMoveForward);
      remainingCharactersToMoveForward -= text.length();
      lastBrOrText = &node;
      continue;
    }

    NOTREACHED();
  }
  return lastPositionInOrAfterNode(lastBrOrText);
}

int TextControlElement::indexForPosition(HTMLElement* innerEditor,
                                         const Position& passedPosition) {
  if (!innerEditor || !innerEditor->contains(passedPosition.anchorNode()) ||
      passedPosition.isNull())
    return 0;

  if (Position::beforeNode(innerEditor) == passedPosition)
    return 0;

  int index = 0;
  Node* startNode = passedPosition.computeNodeBeforePosition();
  if (!startNode)
    startNode = passedPosition.computeContainerNode();
  if (startNode == innerEditor && passedPosition.isAfterAnchor())
    startNode = innerEditor->lastChild();
  DCHECK(startNode);
  DCHECK(innerEditor->contains(startNode));

  for (Node* node = startNode; node;
       node = NodeTraversal::previous(*node, innerEditor)) {
    if (node->isTextNode()) {
      int length = toText(*node).length();
      if (node == passedPosition.computeContainerNode())
        index += std::min(length, passedPosition.offsetInContainerNode());
      else
        index += length;
    } else if (node->hasTagName(brTag)) {
      ++index;
    }
  }

  DCHECK_GE(index, 0);
  return index;
}

bool TextControlElement::setSelectionRange(
    int start,
    int end,
    TextFieldSelectionDirection direction) {
  if (openShadowRoot() || !isTextControl())
    return false;
  const int editorValueLength = static_cast<int>(innerEditorValue().length());
  DCHECK_GE(editorValueLength, 0);
  end = std::max(std::min(end, editorValueLength), 0);
  start = std::min(std::max(start, 0), end);
  LocalFrame* frame = document().frame();
  if (direction == SelectionHasNoDirection && frame &&
      frame->editor().behavior().shouldConsiderSelectionAsDirectional())
    direction = SelectionHasForwardDirection;
  cacheSelection(start, end, direction);

  if (document().focusedElement() != this)
    return true;

  HTMLElement* innerEditor = innerEditorElement();
  if (!frame || !innerEditor)
    return true;

  Position startPosition = positionForIndex(innerEditor, start);
  Position endPosition =
      start == end ? startPosition : positionForIndex(innerEditor, end);

  DCHECK_EQ(start, indexForPosition(innerEditor, startPosition));
  DCHECK_EQ(end, indexForPosition(innerEditor, endPosition));

#if DCHECK_IS_ON()
  // startPosition and endPosition can be null position for example when
  // "-webkit-user-select: none" style attribute is specified.
  if (startPosition.isNotNull() && endPosition.isNotNull()) {
    DCHECK_EQ(startPosition.anchorNode()->ownerShadowHost(), this);
    DCHECK_EQ(endPosition.anchorNode()->ownerShadowHost(), this);
  }
#endif  // DCHECK_IS_ON()
  VisibleSelection newSelection;
  if (direction == SelectionHasBackwardDirection)
    newSelection.setWithoutValidation(endPosition, startPosition);
  else
    newSelection.setWithoutValidation(startPosition, endPosition);
  newSelection.setIsDirectional(direction != SelectionHasNoDirection);

  frame->selection().setSelection(
      newSelection,
      FrameSelection::DoNotAdjustInFlatTree | FrameSelection::CloseTyping |
          FrameSelection::ClearTypingStyle | FrameSelection::DoNotSetFocus);
  return true;
}

VisiblePosition TextControlElement::visiblePositionForIndex(int index) const {
  if (index <= 0)
    return VisiblePosition::firstPositionInNode(innerEditorElement());
  Position start, end;
  bool selected = Range::selectNodeContents(innerEditorElement(), start, end);
  if (!selected)
    return VisiblePosition();
  CharacterIterator it(start, end);
  it.advance(index - 1);
  return createVisiblePosition(it.endPosition(), TextAffinity::Upstream);
}

int TextControlElement::indexForVisiblePosition(
    const VisiblePosition& pos) const {
  Position indexPosition = pos.deepEquivalent().parentAnchoredEquivalent();
  if (enclosingTextControl(indexPosition) != this)
    return 0;
  DCHECK(indexPosition.document());
  Range* range = Range::create(*indexPosition.document());
  range->setStart(innerEditorElement(), 0, ASSERT_NO_EXCEPTION);
  range->setEnd(indexPosition.computeContainerNode(),
                indexPosition.offsetInContainerNode(), ASSERT_NO_EXCEPTION);
  return TextIterator::rangeLength(range->startPosition(),
                                   range->endPosition());
}

int TextControlElement::selectionStart() const {
  if (!isTextControl())
    return 0;
  if (document().focusedElement() != this)
    return m_cachedSelectionStart;

  return computeSelectionStart();
}

int TextControlElement::computeSelectionStart() const {
  DCHECK(isTextControl());
  if (LocalFrame* frame = document().frame())
    return indexForPosition(innerEditorElement(), frame->selection().start());
  return 0;
}

int TextControlElement::selectionEnd() const {
  if (!isTextControl())
    return 0;
  if (document().focusedElement() != this)
    return m_cachedSelectionEnd;
  return computeSelectionEnd();
}

int TextControlElement::computeSelectionEnd() const {
  DCHECK(isTextControl());
  if (LocalFrame* frame = document().frame())
    return indexForPosition(innerEditorElement(), frame->selection().end());
  return 0;
}

static const AtomicString& directionString(
    TextFieldSelectionDirection direction) {
  DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, forward, ("forward"));
  DEFINE_STATIC_LOCAL(const AtomicString, backward, ("backward"));

  switch (direction) {
    case SelectionHasNoDirection:
      return none;
    case SelectionHasForwardDirection:
      return forward;
    case SelectionHasBackwardDirection:
      return backward;
  }

  NOTREACHED();
  return none;
}

const AtomicString& TextControlElement::selectionDirection() const {
  // Ensured by HTMLInputElement::selectionDirectionForBinding().
  DCHECK(isTextControl());
  if (document().focusedElement() != this)
    return directionString(m_cachedSelectionDirection);
  return directionString(computeSelectionDirection());
}

TextFieldSelectionDirection TextControlElement::computeSelectionDirection()
    const {
  DCHECK(isTextControl());
  LocalFrame* frame = document().frame();
  if (!frame)
    return SelectionHasNoDirection;

  const VisibleSelection& selection = frame->selection().selection();
  return selection.isDirectional()
             ? (selection.isBaseFirst() ? SelectionHasForwardDirection
                                        : SelectionHasBackwardDirection)
             : SelectionHasNoDirection;
}

static inline void setContainerAndOffsetForRange(Node* node,
                                                 int offset,
                                                 Node*& containerNode,
                                                 int& offsetInContainer) {
  if (node->isTextNode()) {
    containerNode = node;
    offsetInContainer = offset;
  } else {
    containerNode = node->parentNode();
    offsetInContainer = node->nodeIndex() + offset;
  }
}

Range* TextControlElement::selection() const {
  if (!layoutObject() || !isTextControl())
    return nullptr;

  int start = m_cachedSelectionStart;
  int end = m_cachedSelectionEnd;

  DCHECK_LE(start, end);
  HTMLElement* innerText = innerEditorElement();
  if (!innerText)
    return nullptr;

  if (!innerText->hasChildren())
    return Range::create(document(), innerText, 0, innerText, 0);

  int offset = 0;
  Node* startNode = 0;
  Node* endNode = 0;
  for (Node& node : NodeTraversal::descendantsOf(*innerText)) {
    DCHECK(!node.hasChildren());
    DCHECK(node.isTextNode() || isHTMLBRElement(node));
    int length = node.isTextNode() ? Position::lastOffsetInNode(&node) : 1;

    if (offset <= start && start <= offset + length)
      setContainerAndOffsetForRange(&node, start - offset, startNode, start);

    if (offset <= end && end <= offset + length) {
      setContainerAndOffsetForRange(&node, end - offset, endNode, end);
      break;
    }

    offset += length;
  }

  if (!startNode || !endNode)
    return nullptr;

  return Range::create(document(), startNode, start, endNode, end);
}

const AtomicString& TextControlElement::autocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, off, ("off"));
  DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
  DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
  DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

  const AtomicString& value = fastGetAttribute(autocapitalizeAttr);
  if (equalIgnoringCase(value, none) || equalIgnoringCase(value, off))
    return none;
  if (equalIgnoringCase(value, characters))
    return characters;
  if (equalIgnoringCase(value, words))
    return words;
  if (equalIgnoringCase(value, sentences))
    return sentences;

  // Invalid or missing value.
  return defaultAutocapitalize();
}

void TextControlElement::setAutocapitalize(const AtomicString& autocapitalize) {
  setAttribute(autocapitalizeAttr, autocapitalize);
}

int TextControlElement::maxLength() const {
  int value;
  if (!parseHTMLInteger(fastGetAttribute(maxlengthAttr), value))
    return -1;
  return value >= 0 ? value : -1;
}

int TextControlElement::minLength() const {
  int value;
  if (!parseHTMLInteger(fastGetAttribute(minlengthAttr), value))
    return -1;
  return value >= 0 ? value : -1;
}

void TextControlElement::setMaxLength(int newValue,
                                      ExceptionState& exceptionState) {
  int min = minLength();
  if (newValue < 0) {
    exceptionState.throwDOMException(
        IndexSizeError, "The value provided (" + String::number(newValue) +
                            ") is not positive or 0.");
  } else if (min >= 0 && newValue < min) {
    exceptionState.throwDOMException(
        IndexSizeError, ExceptionMessages::indexExceedsMinimumBound(
                            "maxLength", newValue, min));
  } else {
    setIntegralAttribute(maxlengthAttr, newValue);
  }
}

void TextControlElement::setMinLength(int newValue,
                                      ExceptionState& exceptionState) {
  int max = maxLength();
  if (newValue < 0) {
    exceptionState.throwDOMException(
        IndexSizeError, "The value provided (" + String::number(newValue) +
                            ") is not positive or 0.");
  } else if (max >= 0 && newValue > max) {
    exceptionState.throwDOMException(
        IndexSizeError, ExceptionMessages::indexExceedsMaximumBound(
                            "minLength", newValue, max));
  } else {
    setIntegralAttribute(minlengthAttr, newValue);
  }
}

void TextControlElement::restoreCachedSelection() {
  if (setSelectionRange(m_cachedSelectionStart, m_cachedSelectionEnd,
                        m_cachedSelectionDirection))
    scheduleSelectEvent();
}

void TextControlElement::selectionChanged(bool userTriggered) {
  if (!layoutObject() || !isTextControl())
    return;

  // selectionStart() or selectionEnd() will return cached selection when this
  // node doesn't have focus.
  cacheSelection(computeSelectionStart(), computeSelectionEnd(),
                 computeSelectionDirection());

  if (LocalFrame* frame = document().frame()) {
    if (frame->selection().isRange() && userTriggered)
      dispatchEvent(Event::createBubble(EventTypeNames::select));
  }
}

void TextControlElement::scheduleSelectEvent() {
  Event* event = Event::createBubble(EventTypeNames::select);
  event->setTarget(this);
  document().enqueueUniqueAnimationFrameEvent(event);
}

void TextControlElement::parseAttribute(const QualifiedName& name,
                                        const AtomicString& oldValue,
                                        const AtomicString& value) {
  if (name == autocapitalizeAttr)
    UseCounter::count(document(), UseCounter::AutocapitalizeAttribute);

  if (name == placeholderAttr) {
    updatePlaceholderText();
    updatePlaceholderVisibility();
    UseCounter::count(document(), UseCounter::PlaceholderAttribute);
  } else {
    HTMLFormControlElementWithState::parseAttribute(name, oldValue, value);
  }
}

bool TextControlElement::lastChangeWasUserEdit() const {
  if (!isTextControl())
    return false;
  return m_lastChangeWasUserEdit;
}

Node* TextControlElement::createPlaceholderBreakElement() const {
  return HTMLBRElement::create(document());
}

void TextControlElement::addPlaceholderBreakElementIfNecessary() {
  HTMLElement* innerEditor = innerEditorElement();
  if (innerEditor->layoutObject() &&
      !innerEditor->layoutObject()->style()->preserveNewline())
    return;
  Node* lastChild = innerEditor->lastChild();
  if (!lastChild || !lastChild->isTextNode())
    return;
  if (toText(lastChild)->data().endsWith('\n') ||
      toText(lastChild)->data().endsWith('\r'))
    innerEditor->appendChild(createPlaceholderBreakElement());
}

void TextControlElement::setInnerEditorValue(const String& value) {
  DCHECK(!openShadowRoot());
  if (!isTextControl() || openShadowRoot())
    return;

  bool textIsChanged = value != innerEditorValue();
  HTMLElement* innerEditor = innerEditorElement();
  if (!textIsChanged && innerEditor->hasChildren())
    return;

  // If the last child is a trailing <br> that's appended below, remove it
  // first so as to enable setInnerText() fast path of updating a text node.
  if (isHTMLBRElement(innerEditor->lastChild()))
    innerEditor->removeChild(innerEditor->lastChild(), ASSERT_NO_EXCEPTION);

  // We don't use setTextContent.  It triggers unnecessary paint.
  if (value.isEmpty())
    innerEditor->removeChildren();
  else
    replaceChildrenWithText(innerEditor, value, ASSERT_NO_EXCEPTION);

  // Add <br> so that we can put the caret at the next line of the last
  // newline.
  addPlaceholderBreakElementIfNecessary();

  if (textIsChanged && layoutObject()) {
    if (AXObjectCache* cache = document().existingAXObjectCache())
      cache->handleTextFormControlChanged(this);
  }
}

String TextControlElement::innerEditorValue() const {
  DCHECK(!openShadowRoot());
  HTMLElement* innerEditor = innerEditorElement();
  if (!innerEditor || !isTextControl())
    return emptyString();

  StringBuilder result;
  for (Node& node : NodeTraversal::inclusiveDescendantsOf(*innerEditor)) {
    if (isHTMLBRElement(node)) {
      DCHECK_EQ(&node, innerEditor->lastChild());
      if (&node != innerEditor->lastChild())
        result.append(newlineCharacter);
    } else if (node.isTextNode()) {
      result.append(toText(node).data());
    }
  }
  return result.toString();
}

static void getNextSoftBreak(RootInlineBox*& line,
                             Node*& breakNode,
                             unsigned& breakOffset) {
  RootInlineBox* next;
  for (; line; line = next) {
    next = line->nextRootBox();
    if (next && !line->endsWithBreak()) {
      DCHECK(line->lineBreakObj());
      breakNode = line->lineBreakObj().node();
      breakOffset = line->lineBreakPos();
      line = next;
      return;
    }
  }
  breakNode = 0;
  breakOffset = 0;
}

String TextControlElement::valueWithHardLineBreaks() const {
  // FIXME: It's not acceptable to ignore the HardWrap setting when there is no
  // layoutObject.  While we have no evidence this has ever been a practical
  // problem, it would be best to fix it some day.
  HTMLElement* innerText = innerEditorElement();
  if (!innerText || !isTextControl())
    return value();

  LayoutBlockFlow* layoutObject = toLayoutBlockFlow(innerText->layoutObject());
  if (!layoutObject)
    return value();

  Node* breakNode;
  unsigned breakOffset;
  RootInlineBox* line = layoutObject->firstRootBox();
  if (!line)
    return value();

  getNextSoftBreak(line, breakNode, breakOffset);

  StringBuilder result;
  for (Node& node : NodeTraversal::descendantsOf(*innerText)) {
    if (isHTMLBRElement(node)) {
      DCHECK_EQ(&node, innerText->lastChild());
      if (&node != innerText->lastChild())
        result.append(newlineCharacter);
    } else if (node.isTextNode()) {
      String data = toText(node).data();
      unsigned length = data.length();
      unsigned position = 0;
      while (breakNode == node && breakOffset <= length) {
        if (breakOffset > position) {
          result.append(data, position, breakOffset - position);
          position = breakOffset;
          result.append(newlineCharacter);
        }
        getNextSoftBreak(line, breakNode, breakOffset);
      }
      result.append(data, position, length - position);
    }
    while (breakNode == node)
      getNextSoftBreak(line, breakNode, breakOffset);
  }
  return result.toString();
}

TextControlElement* enclosingTextControl(const Position& position) {
  DCHECK(position.isNull() || position.isOffsetInAnchor() ||
         position.computeContainerNode() ||
         !position.anchorNode()->ownerShadowHost() ||
         (position.anchorNode()->parentNode() &&
          position.anchorNode()->parentNode()->isShadowRoot()));
  return enclosingTextControl(position.computeContainerNode());
}

TextControlElement* enclosingTextControl(Node* container) {
  if (!container)
    return nullptr;
  Element* ancestor = container->ownerShadowHost();
  return ancestor && isTextControlElement(*ancestor) &&
                 container->containingShadowRoot()->type() ==
                     ShadowRootType::UserAgent
             ? toTextControlElement(ancestor)
             : 0;
}

String TextControlElement::directionForFormData() const {
  for (const HTMLElement* element = this; element;
       element = Traversal<HTMLElement>::firstAncestor(*element)) {
    const AtomicString& dirAttributeValue = element->fastGetAttribute(dirAttr);
    if (dirAttributeValue.isNull())
      continue;

    if (equalIgnoringCase(dirAttributeValue, "rtl") ||
        equalIgnoringCase(dirAttributeValue, "ltr"))
      return dirAttributeValue;

    if (equalIgnoringCase(dirAttributeValue, "auto")) {
      bool isAuto;
      TextDirection textDirection =
          element->directionalityIfhasDirAutoAttribute(isAuto);
      return textDirection == RTL ? "rtl" : "ltr";
    }
  }

  return "ltr";
}

HTMLElement* TextControlElement::innerEditorElement() const {
  return toHTMLElementOrDie(
      userAgentShadowRoot()->getElementById(ShadowElementNames::innerEditor()));
}

static Position innerNodePosition(const Position& innerPosition) {
  DCHECK(!innerPosition.isBeforeAnchor());
  DCHECK(!innerPosition.isAfterAnchor());
  HTMLElement* element = toHTMLElement(innerPosition.anchorNode());
  DCHECK(element);
  NodeList* childNodes = element->childNodes();
  if (!childNodes->length())
    return Position(element, 0);

  unsigned offset = 0;

  if (innerPosition.isOffsetInAnchor()) {
    offset = std::max(0, std::min(innerPosition.offsetInContainerNode(),
                                  static_cast<int>(childNodes->length())));
  } else if (innerPosition.isAfterChildren()) {
    offset = childNodes->length();
  }

  if (offset == childNodes->length())
    return Position(element->lastChild(), PositionAnchorType::AfterAnchor);

  Node* node = childNodes->item(offset);
  if (node->isTextNode())
    return Position(toText(node), 0);

  return Position(node, PositionAnchorType::BeforeAnchor);
}

enum FindOption { FindStart, FindEnd };

static Position findWordBoundary(const HTMLElement* innerEditor,
                                 const Position& startPosition,
                                 const Position& endPosition,
                                 FindOption findOption) {
  StringBuilder concatTexts;
  Vector<unsigned> lengthList;
  HeapVector<Member<Text>> textList;

  if (startPosition.anchorNode()->isTextNode())
    DCHECK(startPosition.isOffsetInAnchor());
  if (endPosition.anchorNode()->isTextNode())
    DCHECK(endPosition.isOffsetInAnchor());

  // Traverse text nodes.
  for (Node* node = startPosition.anchorNode(); node;
       node = NodeTraversal::next(*node, innerEditor)) {
    bool isStartNode = node == startPosition.anchorNode();
    bool isEndNode = node == endPosition.anchorNode();
    if (node->isTextNode()) {
      Text* text = toText(node);
      const unsigned start =
          isStartNode ? startPosition.offsetInContainerNode() : 0;
      const unsigned end = isEndNode ? endPosition.offsetInContainerNode()
                                     : text->data().length();
      const unsigned length = end - start;

      concatTexts.append(text->data(), start, length);
      lengthList.append(length);
      textList.append(text);
    }

    if (isEndNode)
      break;
  }

  if (concatTexts.length() == 0)
    return startPosition;

  int start, end;
  if (findOption == FindEnd && concatTexts[0] == '\n') {
    // findWordBoundary("\ntext", 0, &start, &end) assigns 1 to |end| but
    // we expect 0 at the case.
    start = 0;
    end = 0;
  } else {
    Vector<UChar> characters;
    concatTexts.toString().appendTo(characters);
    findWordBoundary(characters.data(), characters.size(),
                     findOption == FindStart ? characters.size() : 0, &start,
                     &end);
  }
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  unsigned remainingOffset = findOption == FindStart ? start : end;
  // Find position.
  for (unsigned i = 0; i < lengthList.size(); ++i) {
    if (remainingOffset <= lengthList[i]) {
      return Position(
          textList[i],
          (textList[i] == startPosition.anchorNode())
              ? remainingOffset + startPosition.offsetInContainerNode()
              : remainingOffset);
    }
    remainingOffset -= lengthList[i];
  }

  NOTREACHED();
  return Position();
}

Position TextControlElement::startOfWord(const Position& position) {
  const TextControlElement* textControl = enclosingTextControl(position);
  DCHECK(textControl);
  HTMLElement* innerEditor = textControl->innerEditorElement();

  const Position startPosition = startOfSentence(position);
  if (startPosition == position)
    return position;
  const Position endPosition = (position.anchorNode() == innerEditor)
                                   ? innerNodePosition(position)
                                   : position;

  return findWordBoundary(innerEditor, startPosition, endPosition, FindStart);
}

Position TextControlElement::endOfWord(const Position& position) {
  const TextControlElement* textControl = enclosingTextControl(position);
  DCHECK(textControl);
  HTMLElement* innerEditor = textControl->innerEditorElement();

  const Position endPosition = endOfSentence(position);
  if (endPosition == position)
    return position;
  const Position startPosition = (position.anchorNode() == innerEditor)
                                     ? innerNodePosition(position)
                                     : position;

  return findWordBoundary(innerEditor, startPosition, endPosition, FindEnd);
}

static Position endOfPrevious(const Node& node, HTMLElement* innerEditor) {
  Node* previousNode = NodeTraversal::previous(node, innerEditor);
  if (!previousNode)
    return Position();

  if (isHTMLBRElement(previousNode))
    return Position(previousNode, PositionAnchorType::AfterAnchor);

  if (previousNode->isTextNode())
    return Position(toText(previousNode), toText(previousNode)->length());

  return Position();
}

static Position previousIfPositionIsAfterLineBreak(const Position& position,
                                                   HTMLElement* innerEditor) {
  if (position.isNull())
    return Position();

  // Move back if position is just after line break.
  if (isHTMLBRElement(*position.anchorNode())) {
    if (position.isAfterAnchor())
      return Position(position.anchorNode(), PositionAnchorType::BeforeAnchor);
    if (position.isBeforeAnchor()) {
      return previousIfPositionIsAfterLineBreak(
          endOfPrevious(*position.anchorNode(), innerEditor), innerEditor);
    }
    // We don't place caret into BR element, since well-formed BR element
    // doesn't have child nodes.
    NOTREACHED();
    return position;
  }

  if (!position.anchorNode()->isTextNode())
    return position;

  Text* textNode = toText(position.anchorNode());
  unsigned offset = position.offsetInContainerNode();
  if (textNode->length() == 0 || offset == 0) {
    return previousIfPositionIsAfterLineBreak(
        endOfPrevious(*position.anchorNode(), innerEditor), innerEditor);
  }

  if (offset <= textNode->length() && textNode->data()[offset - 1] == '\n')
    return Position(textNode, offset - 1);

  return position;
}

static inline Position startOfInnerText(
    const TextControlElement* textFormControl) {
  return Position(textFormControl->innerEditorElement(), 0);
}

Position TextControlElement::startOfSentence(const Position& position) {
  TextControlElement* textControl = enclosingTextControl(position);
  DCHECK(textControl);

  HTMLElement* innerEditor = textControl->innerEditorElement();
  if (!innerEditor->childNodes()->length())
    return startOfInnerText(textControl);

  const Position innerPosition = position.anchorNode() == innerEditor
                                     ? innerNodePosition(position)
                                     : position;
  const Position pivotPosition =
      previousIfPositionIsAfterLineBreak(innerPosition, innerEditor);
  if (pivotPosition.isNull())
    return startOfInnerText(textControl);

  for (Node* node = pivotPosition.anchorNode(); node;
       node = NodeTraversal::previous(*node, innerEditor)) {
    bool isPivotNode = (node == pivotPosition.anchorNode());

    if (isHTMLBRElement(node) &&
        (!isPivotNode || pivotPosition.isAfterAnchor()))
      return Position(node, PositionAnchorType::AfterAnchor);

    if (node->isTextNode()) {
      Text* textNode = toText(node);
      size_t lastLineBreak =
          textNode->data()
              .substring(0, isPivotNode ? pivotPosition.offsetInContainerNode()
                                        : textNode->length())
              .reverseFind('\n');
      if (lastLineBreak != kNotFound)
        return Position(textNode, lastLineBreak + 1);
    }
  }
  return startOfInnerText(textControl);
}

static Position endOfInnerText(const TextControlElement* textFormControl) {
  HTMLElement* innerEditor = textFormControl->innerEditorElement();
  return Position(innerEditor, innerEditor->childNodes()->length());
}

Position TextControlElement::endOfSentence(const Position& position) {
  TextControlElement* textControl = enclosingTextControl(position);
  DCHECK(textControl);

  HTMLElement* innerEditor = textControl->innerEditorElement();
  if (innerEditor->childNodes()->length() == 0)
    return startOfInnerText(textControl);

  const Position pivotPosition = position.anchorNode() == innerEditor
                                     ? innerNodePosition(position)
                                     : position;
  if (pivotPosition.isNull())
    return startOfInnerText(textControl);

  for (Node* node = pivotPosition.anchorNode(); node;
       node = NodeTraversal::next(*node, innerEditor)) {
    bool isPivotNode = node == pivotPosition.anchorNode();

    if (isHTMLBRElement(node))
      return Position(node, PositionAnchorType::AfterAnchor);

    if (node->isTextNode()) {
      Text* textNode = toText(node);
      size_t firstLineBreak = textNode->data().find(
          '\n', isPivotNode ? pivotPosition.offsetInContainerNode() : 0);
      if (firstLineBreak != kNotFound)
        return Position(textNode, firstLineBreak + 1);
    }
  }
  return endOfInnerText(textControl);
}

void TextControlElement::copyNonAttributePropertiesFromElement(
    const Element& source) {
  const TextControlElement& sourceElement =
      static_cast<const TextControlElement&>(source);
  m_lastChangeWasUserEdit = sourceElement.m_lastChangeWasUserEdit;
  HTMLFormControlElement::copyNonAttributePropertiesFromElement(source);
}

}  // namespace blink
