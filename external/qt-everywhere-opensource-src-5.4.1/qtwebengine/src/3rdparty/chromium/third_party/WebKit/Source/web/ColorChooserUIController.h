/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef ColorChooserUIController_h
#define ColorChooserUIController_h

#include "platform/ColorChooser.h"
#include "platform/text/PlatformLocale.h"
#include "public/web/WebColorChooserClient.h"
#include "wtf/OwnPtr.h"

namespace WebCore {
class ColorChooserClient;
class LocalFrame;
}

namespace blink {

class WebColorChooser;

class ColorChooserUIController : public WebColorChooserClient, public WebCore::ColorChooser {
public:
    ColorChooserUIController(WebCore::LocalFrame*, WebCore::ColorChooserClient*);
    virtual ~ColorChooserUIController();

    virtual void openUI();

    // ColorChooser functions:
    virtual void setSelectedColor(const WebCore::Color&) OVERRIDE FINAL;
    virtual void endChooser() OVERRIDE;

    // WebColorChooserClient functions:
    virtual void didChooseColor(const WebColor&) OVERRIDE FINAL;
    virtual void didEndChooser() OVERRIDE FINAL;

protected:
    void openColorChooser();
    OwnPtr<WebColorChooser> m_chooser;

private:

    WebCore::LocalFrame* m_frame;
    WebCore::ColorChooserClient* m_client;
};

}

#endif // ColorChooserUIController_h
