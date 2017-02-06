// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLTableRowElement.h"

#include "core/dom/Document.h"
#include "core/html/HTMLParagraphElement.h"
#include "core/html/HTMLTableElement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using namespace blink;

// rowIndex
// https://html.spec.whatwg.org/multipage/tables.html#dom-tr-rowindex

TEST(HTMLTableRowElementTest, rowIndex_notInTable)
{
    Document* document = Document::create();
    HTMLTableRowElement* row =
        HTMLTableRowElement::create(*document);
    EXPECT_EQ(-1, row->rowIndex())
        << "rows not in tables should have row index -1";
}

TEST(HTMLTableRowElementTest, rowIndex_directChildOfTable)
{
    Document* document = Document::create();
    HTMLTableElement* table =
        HTMLTableElement::create(*document);
    HTMLTableRowElement* row =
        HTMLTableRowElement::create(*document);
    table->appendChild(row);
    EXPECT_EQ(0, row->rowIndex())
        << "rows that are direct children of a table should have a row index";
}

TEST(HTMLTableRowElementTest, rowIndex_inUnrelatedElementInTable)
{
    Document* document = Document::create();
    HTMLTableElement* table =
        HTMLTableElement::create(*document);
    // Almost any element will do; what's pertinent is that this is not
    // THEAD, TBODY or TFOOT.
    HTMLParagraphElement* paragraph =
        HTMLParagraphElement::create(*document);
    HTMLTableRowElement* row =
        HTMLTableRowElement::create(*document);
    table->appendChild(paragraph);
    paragraph->appendChild(row);
    EXPECT_EQ(-1, row->rowIndex())
        << "rows in a table, but within an unrelated element, should have "
        << "row index -1";
}

} // namespace
