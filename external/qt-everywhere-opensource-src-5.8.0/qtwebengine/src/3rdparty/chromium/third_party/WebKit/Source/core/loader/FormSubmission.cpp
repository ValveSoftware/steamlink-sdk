/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/FormSubmission.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/FormDataEncoder.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

using namespace HTMLNames;

static int64_t generateFormDataIdentifier()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static int64_t nextIdentifier = static_cast<int64_t>(currentTime() * 1000000.0);
    return ++nextIdentifier;
}

static void appendMailtoPostFormDataToURL(KURL& url, const EncodedFormData& data, const String& encodingType)
{
    String body = data.flattenToString();

    if (equalIgnoringCase(encodingType, "text/plain")) {
        // Convention seems to be to decode, and s/&/\r\n/. Also, spaces are encoded as %20.
        body = decodeURLEscapeSequences(body.replace('&', "\r\n").replace('+', ' ') + "\r\n");
    }

    Vector<char> bodyData;
    bodyData.append("body=", 5);
    FormDataEncoder::encodeStringAsFormData(bodyData, body.utf8(), FormDataEncoder::NormalizeCRLF);
    body = String(bodyData.data(), bodyData.size()).replace('+', "%20");

    StringBuilder query;
    query.append(url.query());
    if (!query.isEmpty())
        query.append('&');
    query.append(body);
    url.setQuery(query.toString());
}

void FormSubmission::Attributes::parseAction(const String& action)
{
    // m_action cannot be converted to KURL (bug https://crbug.com/388664)
    m_action = stripLeadingAndTrailingHTMLSpaces(action);
}

AtomicString FormSubmission::Attributes::parseEncodingType(const String& type)
{
    if (equalIgnoringCase(type, "multipart/form-data"))
        return AtomicString("multipart/form-data");
    if (equalIgnoringCase(type, "text/plain"))
        return AtomicString("text/plain");
    return AtomicString("application/x-www-form-urlencoded");
}

void FormSubmission::Attributes::updateEncodingType(const String& type)
{
    m_encodingType = parseEncodingType(type);
    m_isMultiPartForm = (m_encodingType == "multipart/form-data");
}

FormSubmission::SubmitMethod FormSubmission::Attributes::parseMethodType(const String& type)
{
    if (equalIgnoringCase(type, "post"))
        return FormSubmission::PostMethod;
    if (equalIgnoringCase(type, "dialog"))
        return FormSubmission::DialogMethod;
    return FormSubmission::GetMethod;
}

void FormSubmission::Attributes::updateMethodType(const String& type)
{
    m_method = parseMethodType(type);
}

String FormSubmission::Attributes::methodString(SubmitMethod method)
{
    switch (method) {
    case GetMethod:
        return "get";
    case PostMethod:
        return "post";
    case DialogMethod:
        return "dialog";
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

void FormSubmission::Attributes::copyFrom(const Attributes& other)
{
    m_method = other.m_method;
    m_isMultiPartForm = other.m_isMultiPartForm;

    m_action = other.m_action;
    m_target = other.m_target;
    m_encodingType = other.m_encodingType;
    m_acceptCharset = other.m_acceptCharset;
}

inline FormSubmission::FormSubmission(SubmitMethod method, const KURL& action, const AtomicString& target, const AtomicString& contentType, HTMLFormElement* form, PassRefPtr<EncodedFormData> data, const String& boundary, Event* event)
    : m_method(method)
    , m_action(action)
    , m_target(target)
    , m_contentType(contentType)
    , m_form(form)
    , m_formData(data)
    , m_boundary(boundary)
    , m_event(event)
{
}

inline FormSubmission::FormSubmission(const String& result)
    : m_method(DialogMethod)
    , m_result(result)
{
}

FormSubmission* FormSubmission::create(HTMLFormElement* form, const Attributes& attributes, Event* event)
{
    ASSERT(form);

    HTMLFormControlElement* submitButton = 0;
    if (event && event->target()) {
        for (Node* node = event->target()->toNode(); node; node = node->parentOrShadowHostNode()) {
            if (node->isElementNode() && toElement(node)->isFormControlElement()) {
                submitButton = toHTMLFormControlElement(node);
                break;
            }
        }
    }

    FormSubmission::Attributes copiedAttributes;
    copiedAttributes.copyFrom(attributes);
    if (submitButton) {
        AtomicString attributeValue;
        if (!(attributeValue = submitButton->fastGetAttribute(formactionAttr)).isNull())
            copiedAttributes.parseAction(attributeValue);
        if (!(attributeValue = submitButton->fastGetAttribute(formenctypeAttr)).isNull())
            copiedAttributes.updateEncodingType(attributeValue);
        if (!(attributeValue = submitButton->fastGetAttribute(formmethodAttr)).isNull())
            copiedAttributes.updateMethodType(attributeValue);
        if (!(attributeValue = submitButton->fastGetAttribute(formtargetAttr)).isNull())
            copiedAttributes.setTarget(attributeValue);
    }

    if (copiedAttributes.method() == DialogMethod) {
        if (submitButton)
            return new FormSubmission(submitButton->resultForDialogSubmit());
        return new FormSubmission("");
    }

    Document& document = form->document();
    KURL actionURL = document.completeURL(copiedAttributes.action().isEmpty() ? document.url().getString() : copiedAttributes.action());

    if (document.getInsecureRequestPolicy() & kUpgradeInsecureRequests && actionURL.protocolIs("http")) {
        UseCounter::count(document, UseCounter::UpgradeInsecureRequestsUpgradedRequest);
        actionURL.setProtocol("https");
        if (actionURL.port() == 80)
            actionURL.setPort(443);
    }

    bool isMailtoForm = actionURL.protocolIs("mailto");
    bool isMultiPartForm = false;
    AtomicString encodingType = copiedAttributes.encodingType();

    if (copiedAttributes.method() == PostMethod) {
        isMultiPartForm = copiedAttributes.isMultiPartForm();
        if (isMultiPartForm && isMailtoForm) {
            encodingType = AtomicString("application/x-www-form-urlencoded");
            isMultiPartForm = false;
        }
    }
    WTF::TextEncoding dataEncoding = isMailtoForm ? UTF8Encoding() : FormDataEncoder::encodingFromAcceptCharset(copiedAttributes.acceptCharset(), document.encoding());
    FormData* domFormData = FormData::create(dataEncoding.encodingForFormSubmission());

    bool containsPasswordData = false;
    for (unsigned i = 0; i < form->associatedElements().size(); ++i) {
        FormAssociatedElement* control = form->associatedElements()[i];
        ASSERT(control);
        HTMLElement& element = toHTMLElement(*control);
        if (!element.isDisabledFormControl())
            control->appendToFormData(*domFormData);
        if (isHTMLInputElement(element)) {
            HTMLInputElement& input = toHTMLInputElement(element);
            if (input.type() == InputTypeNames::password && !input.value().isEmpty())
                containsPasswordData = true;
        }
    }

    RefPtr<EncodedFormData> formData;
    String boundary;

    if (isMultiPartForm) {
        formData = domFormData->encodeMultiPartFormData();
        boundary = formData->boundary().data();
    } else {
        formData = domFormData->encodeFormData(attributes.method() == GetMethod ? EncodedFormData::FormURLEncoded : EncodedFormData::parseEncodingType(encodingType));
        if (copiedAttributes.method() == PostMethod && isMailtoForm) {
            // Convert the form data into a string that we put into the URL.
            appendMailtoPostFormDataToURL(actionURL, *formData, encodingType);
            formData = EncodedFormData::create();
        }
    }

    formData->setIdentifier(generateFormDataIdentifier());
    formData->setContainsPasswordData(containsPasswordData);
    AtomicString targetOrBaseTarget = copiedAttributes.target().isEmpty() ? document.baseTarget() : copiedAttributes.target();
    return new FormSubmission(copiedAttributes.method(), actionURL, targetOrBaseTarget, encodingType, form, formData.release(), boundary, event);
}

DEFINE_TRACE(FormSubmission)
{
    visitor->trace(m_form);
    visitor->trace(m_event);
}

KURL FormSubmission::requestURL() const
{
    if (m_method == FormSubmission::PostMethod)
        return m_action;

    KURL requestURL(m_action);
    requestURL.setQuery(m_formData->flattenToString());
    return requestURL;
}

FrameLoadRequest FormSubmission::createFrameLoadRequest(Document* originDocument)
{
    FrameLoadRequest frameRequest(originDocument);

    if (!m_target.isEmpty())
        frameRequest.setFrameName(m_target);

    if (m_method == FormSubmission::PostMethod) {
        frameRequest.resourceRequest().setHTTPMethod("POST");
        frameRequest.resourceRequest().setHTTPBody(m_formData);

        // construct some user headers if necessary
        if (m_boundary.isEmpty())
            frameRequest.resourceRequest().setHTTPContentType(m_contentType);
        else
            frameRequest.resourceRequest().setHTTPContentType(m_contentType + "; boundary=" + m_boundary);
    }

    frameRequest.resourceRequest().setURL(requestURL());

    frameRequest.setTriggeringEvent(m_event);
    frameRequest.setForm(m_form);

    return frameRequest;
}

} // namespace blink
