// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLTextFormControlElement.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/OwnPtr.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class HTMLTextFormControlElementTest : public ::testing::Test {
protected:
    virtual void SetUp() OVERRIDE;

    HTMLTextFormControlElement& textControl() const { return *m_textControl; }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;

    RefPtrWillBePersistent<HTMLTextFormControlElement> m_textControl;
};

void HTMLTextFormControlElementTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    HTMLDocument& document = toHTMLDocument(m_dummyPageHolder->document());
    document.documentElement()->setInnerHTML("<body><textarea id=textarea></textarea></body>", ASSERT_NO_EXCEPTION);
    document.view()->updateLayoutAndStyleIfNeededRecursive();
    m_textControl = toHTMLTextFormControlElement(document.getElementById("textarea"));
    m_textControl->focus();
}

TEST_F(HTMLTextFormControlElementTest, SetSelectionRange)
{
    EXPECT_EQ(0, textControl().selectionStart());
    EXPECT_EQ(0, textControl().selectionEnd());

    textControl().setInnerEditorValue("Hello, text form.");
    EXPECT_EQ(0, textControl().selectionStart());
    EXPECT_EQ(0, textControl().selectionEnd());

    textControl().setSelectionRange(1, 3);
    EXPECT_EQ(1, textControl().selectionStart());
    EXPECT_EQ(3, textControl().selectionEnd());
}

}
