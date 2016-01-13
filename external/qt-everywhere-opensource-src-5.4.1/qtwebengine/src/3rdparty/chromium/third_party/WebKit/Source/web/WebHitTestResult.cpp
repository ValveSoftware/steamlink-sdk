/*
* Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "public/web/WebHitTestResult.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/editing/VisiblePosition.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderObject.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebURL.h"
#include "public/web/WebElement.h"
#include "public/web/WebNode.h"

using namespace WebCore;

namespace blink {

WebNode WebHitTestResult::node() const
{
    return WebNode(m_private->innerNode());
}

WebPoint WebHitTestResult::localPoint() const
{
    return roundedIntPoint(m_private->localPoint());
}

WebElement WebHitTestResult::urlElement() const
{
    return WebElement(m_private->URLElement());
}

WebURL WebHitTestResult::absoluteImageURL() const
{
    return m_private->absoluteImageURL();
}

WebURL WebHitTestResult::absoluteLinkURL() const
{
    return m_private->absoluteLinkURL();
}

bool WebHitTestResult::isContentEditable() const
{
    return m_private->isContentEditable();
}

WebHitTestResult::WebHitTestResult(const HitTestResult& result)
{
    m_private.reset(new HitTestResult(result));
}

WebHitTestResult& WebHitTestResult::operator=(const HitTestResult& result)
{
    m_private.reset(new HitTestResult(result));
    return *this;
}

WebHitTestResult::operator HitTestResult() const
{
    return *m_private.get();
}

bool WebHitTestResult::isNull() const
{
    return !m_private.get();
}

void WebHitTestResult::assign(const WebHitTestResult& info)
{
    m_private.reset(new HitTestResult(info));
}

void WebHitTestResult::reset()
{
    m_private.reset(0);
}

} // namespace blink
