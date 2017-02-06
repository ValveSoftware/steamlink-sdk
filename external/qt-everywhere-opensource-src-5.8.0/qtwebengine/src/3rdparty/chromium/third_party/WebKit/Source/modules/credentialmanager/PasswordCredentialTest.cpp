// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/PasswordCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/URLSearchParams.h"
#include "core/frame/FrameView.h"
#include "core/html/FormData.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/forms/FormController.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

class PasswordCredentialTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_dummyPageHolder = DummyPageHolder::create();
        m_document = toHTMLDocument(&m_dummyPageHolder->document());
    }

    HTMLDocument& document() const { return *m_document; }

    HTMLFormElement* populateForm(const char* enctype, const char* html)
    {
        StringBuilder b;
        b.append("<!DOCTYPE html><html><body><form id='theForm' enctype='");
        b.append(enctype);
        b.append("'>");
        b.append(html);
        b.append("</form></body></html>");
        document().documentElement()->setInnerHTML(b.toString(), ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
        HTMLFormElement* form = toHTMLFormElement(document().getElementById("theForm"));
        EXPECT_NE(nullptr, form);
        return form;
    }

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLDocument> m_document;
};

TEST_F(PasswordCredentialTest, CreateFromMultipartForm)
{
    HTMLFormElement* form = populateForm("multipart/form-data",
        "<input type='text' name='theId' value='musterman' autocomplete='username'>"
        "<input type='text' name='thePassword' value='sekrit' autocomplete='current-password'>"
        "<input type='text' name='theIcon' value='https://example.com/photo' autocomplete='photo'>"
        "<input type='text' name='theExtraField' value='extra'>"
        "<input type='text' name='theName' value='friendly name' autocomplete='name'>");
    PasswordCredential* credential = PasswordCredential::create(form, ASSERT_NO_EXCEPTION);
    ASSERT_NE(nullptr, credential);
    EXPECT_EQ("theId", credential->idName());
    EXPECT_EQ("thePassword", credential->passwordName());

    EXPECT_EQ("musterman", credential->id());
    EXPECT_EQ("sekrit", credential->password());
    EXPECT_EQ(KURL(ParsedURLString, "https://example.com/photo"), credential->iconURL());
    EXPECT_EQ("friendly name", credential->name());
    EXPECT_EQ("password", credential->type());

    FormDataOrURLSearchParams additionalData;
    credential->additionalData(additionalData);
    ASSERT_TRUE(additionalData.isFormData());
    EXPECT_TRUE(additionalData.getAsFormData()->has("theId"));
    EXPECT_TRUE(additionalData.getAsFormData()->has("thePassword"));
    EXPECT_TRUE(additionalData.getAsFormData()->has("theIcon"));
    EXPECT_TRUE(additionalData.getAsFormData()->has("theName"));
    EXPECT_TRUE(additionalData.getAsFormData()->has("theExtraField"));
}

TEST_F(PasswordCredentialTest, CreateFromURLEncodedForm)
{
    HTMLFormElement* form = populateForm("application/x-www-form-urlencoded",
        "<input type='text' name='theId' value='musterman' autocomplete='username'>"
        "<input type='text' name='thePassword' value='sekrit' autocomplete='current-password'>"
        "<input type='text' name='theIcon' value='https://example.com/photo' autocomplete='photo'>"
        "<input type='text' name='theExtraField' value='extra'>"
        "<input type='text' name='theName' value='friendly name' autocomplete='name'>");
    PasswordCredential* credential = PasswordCredential::create(form, ASSERT_NO_EXCEPTION);
    ASSERT_NE(nullptr, credential);
    EXPECT_EQ("theId", credential->idName());
    EXPECT_EQ("thePassword", credential->passwordName());

    EXPECT_EQ("musterman", credential->id());
    EXPECT_EQ("sekrit", credential->password());
    EXPECT_EQ(KURL(ParsedURLString, "https://example.com/photo"), credential->iconURL());
    EXPECT_EQ("friendly name", credential->name());
    EXPECT_EQ("password", credential->type());

    FormDataOrURLSearchParams additionalData;
    credential->additionalData(additionalData);
    ASSERT_TRUE(additionalData.isURLSearchParams());
    EXPECT_TRUE(additionalData.getAsURLSearchParams()->has("theId"));
    EXPECT_TRUE(additionalData.getAsURLSearchParams()->has("thePassword"));
    EXPECT_TRUE(additionalData.getAsURLSearchParams()->has("theIcon"));
    EXPECT_TRUE(additionalData.getAsURLSearchParams()->has("theName"));
    EXPECT_TRUE(additionalData.getAsURLSearchParams()->has("theExtraField"));
}

TEST_F(PasswordCredentialTest, CreateFromFormNoPassword)
{
    HTMLFormElement* form = populateForm("multipart/form-data",
        "<input type='text' name='theId' value='musterman' autocomplete='username'>"
        "<!-- No password field -->"
        "<input type='text' name='theIcon' value='https://example.com/photo' autocomplete='photo'>"
        "<input type='text' name='theName' value='friendly name' autocomplete='name'>");
    TrackExceptionState exceptionState;
    PasswordCredential* credential = PasswordCredential::create(form, exceptionState);
    EXPECT_EQ(nullptr, credential);
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(V8TypeError, exceptionState.code());
    EXPECT_EQ("'password' must not be empty.", exceptionState.message());
}

TEST_F(PasswordCredentialTest, CreateFromFormNoId)
{
    HTMLFormElement* form = populateForm("multipart/form-data",
        "<!-- No username field. -->"
        "<input type='text' name='thePassword' value='sekrit' autocomplete='current-password'>"
        "<input type='text' name='theIcon' value='https://example.com/photo' autocomplete='photo'>"
        "<input type='text' name='theName' value='friendly name' autocomplete='name'>");
    TrackExceptionState exceptionState;
    PasswordCredential* credential = PasswordCredential::create(form, exceptionState);
    EXPECT_EQ(nullptr, credential);
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(V8TypeError, exceptionState.code());
    EXPECT_EQ("'id' must not be empty.", exceptionState.message());
}

} // namespace blink
