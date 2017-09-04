/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights
 * reserved.
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

#include "core/html/HTMLTableRowElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowsCollection.h"
#include "core/html/HTMLTableSectionElement.h"

namespace blink {

using namespace HTMLNames;

inline HTMLTableRowElement::HTMLTableRowElement(Document& document)
    : HTMLTablePartElement(trTag, document) {}

DEFINE_NODE_FACTORY(HTMLTableRowElement)

bool HTMLTableRowElement::hasLegalLinkAttribute(
    const QualifiedName& name) const {
  return name == backgroundAttr ||
         HTMLTablePartElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLTableRowElement::subResourceAttributeName() const {
  return backgroundAttr;
}

static int findIndexInRowCollection(const HTMLCollection& rows,
                                    const HTMLTableRowElement& target) {
  Element* candidate = rows.item(0);
  for (int i = 0; candidate; i++, candidate = rows.item(i)) {
    if (target == candidate)
      return i;
  }
  return -1;
}

int HTMLTableRowElement::rowIndex() const {
  ContainerNode* maybeTable = parentNode();
  if (maybeTable && isHTMLTableSectionElement(maybeTable)) {
    // Skip THEAD, TBODY and TFOOT.
    maybeTable = maybeTable->parentNode();
  }
  if (!(maybeTable && isHTMLTableElement(maybeTable)))
    return -1;
  return findIndexInRowCollection(*toHTMLTableElement(maybeTable)->rows(),
                                  *this);
}

int HTMLTableRowElement::sectionRowIndex() const {
  ContainerNode* maybeTable = parentNode();
  if (!maybeTable)
    return -1;
  HTMLCollection* rows = nullptr;
  if (isHTMLTableSectionElement(maybeTable))
    rows = toHTMLTableSectionElement(maybeTable)->rows();
  else if (isHTMLTableElement(maybeTable))
    rows = toHTMLTableElement(maybeTable)->rows();
  if (!rows)
    return -1;
  return findIndexInRowCollection(*rows, *this);
}

HTMLElement* HTMLTableRowElement::insertCell(int index,
                                             ExceptionState& exceptionState) {
  HTMLCollection* children = cells();
  int numCells = children ? children->length() : 0;
  if (index < -1 || index > numCells) {
    exceptionState.throwDOMException(
        IndexSizeError, "The value provided (" + String::number(index) +
                            ") is outside the range [-1, " +
                            String::number(numCells) + "].");
    return nullptr;
  }

  HTMLTableCellElement* cell = HTMLTableCellElement::create(tdTag, document());
  if (numCells == index || index == -1)
    appendChild(cell, exceptionState);
  else
    insertBefore(cell, children->item(index), exceptionState);
  return cell;
}

void HTMLTableRowElement::deleteCell(int index,
                                     ExceptionState& exceptionState) {
  HTMLCollection* children = cells();
  int numCells = children ? children->length() : 0;
  // 1. If index is less than −1 or greater than or equal to the number of
  // elements in the cells collection, then throw "IndexSizeError".
  if (index < -1 || index >= numCells) {
    exceptionState.throwDOMException(
        IndexSizeError, "The value provided (" + String::number(index) +
                            ") is outside the range [0, " +
                            String::number(numCells) + ").");
    return;
  }
  // 2. If index is −1, remove the last element in the cells collection
  // from its parent, or do nothing if the cells collection is empty.
  if (index == -1) {
    if (numCells == 0)
      return;
    index = numCells - 1;
  }
  // 3. Remove the indexth element in the cells collection from its parent.
  Element* cell = children->item(index);
  HTMLElement::removeChild(cell, exceptionState);
}

HTMLCollection* HTMLTableRowElement::cells() {
  return ensureCachedCollection<HTMLCollection>(TRCells);
}

}  // namespace blink
