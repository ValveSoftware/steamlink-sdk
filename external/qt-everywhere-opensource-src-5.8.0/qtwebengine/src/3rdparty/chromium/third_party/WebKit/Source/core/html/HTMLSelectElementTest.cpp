// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLSelectElement.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/forms/FormController.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class HTMLSelectElementTest : public::testing::Test {
protected:
    void SetUp() override;
    HTMLDocument& document() const { return *m_document; }

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLDocument> m_document;
};

void HTMLSelectElementTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    m_document->setMimeType("text/html");
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectSingleFormControlState)
{
    document().documentElement()->setInnerHTML(String("<!DOCTYPE HTML><select id='sel'>"
        "<option value='111' id='0'>111</option>"
        "<option value='222'>222</option>"
        "<option value='111' selected id='2'>!666</option>"
        "<option value='999'>999</option></select>"), ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("sel");
    HTMLFormControlElementWithState* select = toHTMLSelectElement(element);
    HTMLOptionElement* opt0 = toHTMLOptionElement(document().getElementById("0"));
    HTMLOptionElement* opt2 = toHTMLOptionElement(document().getElementById("2"));

    // Save the select element state, and then restore again.
    // Test passes if the restored state is not changed.
    EXPECT_EQ(2, toHTMLSelectElement(element)->selectedIndex());
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
    FormControlState selectState = select->saveFormControlState();
    EXPECT_EQ(2U, selectState.valueSize());

    // Clear the selected state, to be restored by restoreFormControlState.
    toHTMLSelectElement(select)->setSelectedIndex(-1);
    ASSERT_FALSE(opt2->selected());

    // Restore
    select->restoreFormControlState(selectState);
    EXPECT_EQ(2, toHTMLSelectElement(element)->selectedIndex());
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectMultipleFormControlState)
{
    document().documentElement()->setInnerHTML(String("<!DOCTYPE HTML><select id='sel' multiple>"
        "<option value='111' id='0'>111</option>"
        "<option value='222'>222</option>"
        "<option value='111' selected id='2'>!666</option>"
        "<option value='999' selected id='3'>999</option></select>"), ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLFormControlElementWithState* select = toHTMLSelectElement(document().getElementById("sel"));

    HTMLOptionElement* opt0 = toHTMLOptionElement(document().getElementById("0"));
    HTMLOptionElement* opt2 = toHTMLOptionElement(document().getElementById("2"));
    HTMLOptionElement* opt3 = toHTMLOptionElement(document().getElementById("3"));

    // Save the select element state, and then restore again.
    // Test passes if the selected options are not changed.
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
    EXPECT_TRUE(opt3->selected());
    FormControlState selectState = select->saveFormControlState();
    EXPECT_EQ(4U, selectState.valueSize());

    // Clear the selected state, to be restored by restoreFormControlState.
    opt2->setSelected(false);
    opt3->setSelected(false);
    ASSERT_FALSE(opt2->selected());
    ASSERT_FALSE(opt3->selected());

    // Restore
    select->restoreFormControlState(selectState);
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
    EXPECT_TRUE(opt3->selected());
}

TEST_F(HTMLSelectElementTest, ElementRectRelativeToViewport)
{
    document().documentElement()->setInnerHTML("<select style='position:fixed; top:12.3px; height:24px; -webkit-appearance:none;'><option>o1</select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    ASSERT(select);
    IntRect bounds = select->elementRectRelativeToViewport();
    EXPECT_EQ(24, bounds.height());
}

TEST_F(HTMLSelectElementTest, PopupIsVisible)
{
    document().documentElement()->setInnerHTML("<select><option>o1</option></select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    ASSERT(select);
    EXPECT_FALSE(select->popupIsVisible());
    select->showPopup();
    EXPECT_TRUE(select->popupIsVisible());
    document().detach();
    EXPECT_FALSE(select->popupIsVisible());
}

TEST_F(HTMLSelectElementTest, FirstSelectableOption)
{
    {
        document().documentElement()->setInnerHTML("<select></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ(nullptr, select->firstSelectableOption());
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->firstSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1 disabled></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->firstSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1 style='display:none'></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->firstSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><optgroup><option id=o1></option><option id=o2></option></optgroup></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->firstSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
}

TEST_F(HTMLSelectElementTest, LastSelectableOption)
{
    {
        document().documentElement()->setInnerHTML("<select></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ(nullptr, select->lastSelectableOption());
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->lastSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2 disabled></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->lastSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2 style='display:none'></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->lastSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><optgroup><option id=o1></option><option id=o2></option></optgroup></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->lastSelectableOption()->fastGetAttribute(HTMLNames::idAttr));
    }
}

TEST_F(HTMLSelectElementTest, NextSelectableOption)
{
    {
        document().documentElement()->setInnerHTML("<select></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ(nullptr, select->nextSelectableOption(nullptr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->nextSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1 disabled></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->nextSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1 style='display:none'></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->nextSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><optgroup><option id=o1></option><option id=o2></option></optgroup></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->nextSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        HTMLOptionElement* option = toHTMLOptionElement(document().getElementById("o1"));
        EXPECT_EQ("o2", select->nextSelectableOption(option)->fastGetAttribute(HTMLNames::idAttr));

        EXPECT_EQ(nullptr, select->nextSelectableOption(toHTMLOptionElement(document().getElementById("o2"))));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><optgroup><option id=o2></option></optgroup></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        HTMLOptionElement* option = toHTMLOptionElement(document().getElementById("o1"));
        EXPECT_EQ("o2", select->nextSelectableOption(option)->fastGetAttribute(HTMLNames::idAttr));
    }
}

TEST_F(HTMLSelectElementTest, PreviousSelectableOption)
{
    {
        document().documentElement()->setInnerHTML("<select></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ(nullptr, select->previousSelectableOption(nullptr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->previousSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2 disabled></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->previousSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2 style='display:none'></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o1", select->previousSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><optgroup><option id=o1></option><option id=o2></option></optgroup></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        EXPECT_EQ("o2", select->previousSelectableOption(nullptr)->fastGetAttribute(HTMLNames::idAttr));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><option id=o2></option></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        HTMLOptionElement* option = toHTMLOptionElement(document().getElementById("o2"));
        EXPECT_EQ("o1", select->previousSelectableOption(option)->fastGetAttribute(HTMLNames::idAttr));

        EXPECT_EQ(nullptr, select->previousSelectableOption(toHTMLOptionElement(document().getElementById("o1"))));
    }
    {
        document().documentElement()->setInnerHTML("<select><option id=o1></option><optgroup><option id=o2></option></optgroup></select>", ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
        HTMLOptionElement* option = toHTMLOptionElement(document().getElementById("o2"));
        EXPECT_EQ("o1", select->previousSelectableOption(option)->fastGetAttribute(HTMLNames::idAttr));
    }
}

TEST_F(HTMLSelectElementTest, ActiveSelectionEndAfterOptionRemoval)
{
    document().documentElement()->setInnerHTML("<select><optgroup><option selected>o1</option></optgroup></select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    HTMLOptionElement* option = toHTMLOptionElement(select->firstChild()->firstChild());
    EXPECT_EQ(1, select->activeSelectionEndListIndex());
    select->firstChild()->removeChild(option, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(-1, select->activeSelectionEndListIndex());
    select->appendChild(option, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(1, select->activeSelectionEndListIndex());
}

TEST_F(HTMLSelectElementTest, DefaultToolTip)
{
    document().documentElement()->setInnerHTML("<select size=4><option value="">Placeholder</option><optgroup><option>o2</option></optgroup></select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    Element* option = toElement(select->firstChild());
    Element* optgroup = toElement(option->nextSibling());

    EXPECT_EQ(String(), select->defaultToolTip()) << "defaultToolTip for SELECT without FORM and without required attribute should return null string.";
    EXPECT_EQ(select->defaultToolTip(), option->defaultToolTip());
    EXPECT_EQ(select->defaultToolTip(), optgroup->defaultToolTip());

    select->setBooleanAttribute(HTMLNames::requiredAttr, true);
    EXPECT_EQ("<<ValidationValueMissingForSelect>>", select->defaultToolTip()) << "defaultToolTip for SELECT without FORM and with required attribute should return a valueMissing message.";
    EXPECT_EQ(select->defaultToolTip(), option->defaultToolTip());
    EXPECT_EQ(select->defaultToolTip(), optgroup->defaultToolTip());

    HTMLFormElement* form = HTMLFormElement::create(document());
    document().body()->appendChild(form);
    form->appendChild(select);
    EXPECT_EQ("<<ValidationValueMissingForSelect>>", select->defaultToolTip()) << "defaultToolTip for SELECT with FORM and required attribute should return a valueMissing message.";
    EXPECT_EQ(select->defaultToolTip(), option->defaultToolTip());
    EXPECT_EQ(select->defaultToolTip(), optgroup->defaultToolTip());

    form->setBooleanAttribute(HTMLNames::novalidateAttr, true);
    EXPECT_EQ(String(), select->defaultToolTip()) << "defaultToolTip for SELECT with FORM[novalidate] and required attribute should return null string.";
    EXPECT_EQ(select->defaultToolTip(), option->defaultToolTip());
    EXPECT_EQ(select->defaultToolTip(), optgroup->defaultToolTip());

    option->remove();
    optgroup->remove();
    EXPECT_EQ(String(), option->defaultToolTip());
    EXPECT_EQ(String(), optgroup->defaultToolTip());
}

TEST_F(HTMLSelectElementTest, SetRecalcListItemsByOptgroupRemoval)
{
    document().documentElement()->setInnerHTML("<select><optgroup><option>sub1</option><option>sub2</option></optgroup></select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    select->setInnerHTML("", ASSERT_NO_EXCEPTION);
    // PASS if setInnerHTML didn't have a check failure.
}

} // namespace blink
