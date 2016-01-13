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

#include "config.h"
#include "core/rendering/RenderThemeChromiumAndroid.h"

#include "core/CSSValueKeywords.h"
#include "core/InputTypeNames.h"
#include "core/UserAgentStyleSheets.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderMediaControls.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderProgress.h"
#include "core/rendering/RenderSlider.h"
#include "platform/LayoutTestSupport.h"
#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThemeEngine.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

PassRefPtr<RenderTheme> RenderThemeChromiumAndroid::create()
{
    return adoptRef(new RenderThemeChromiumAndroid());
}

RenderTheme& RenderTheme::theme()
{
    DEFINE_STATIC_REF(RenderTheme, renderTheme, (RenderThemeChromiumAndroid::create()));
    return *renderTheme;
}

RenderThemeChromiumAndroid::~RenderThemeChromiumAndroid()
{
}

String RenderThemeChromiumAndroid::extraMediaControlsStyleSheet()
{
    return String(mediaControlsAndroidUserAgentStyleSheet, sizeof(mediaControlsAndroidUserAgentStyleSheet));
}

String RenderThemeChromiumAndroid::extraDefaultStyleSheet()
{
    return RenderThemeChromiumDefault::extraDefaultStyleSheet() +
        String(themeChromiumAndroidUserAgentStyleSheet, sizeof(themeChromiumAndroidUserAgentStyleSheet));
}

void RenderThemeChromiumAndroid::adjustInnerSpinButtonStyle(RenderStyle* style, Element*) const
{
    if (isRunningLayoutTest()) {
        // Match Linux spin button style in layout tests.
        // FIXME: Consider removing the conditional if a future Android theme matches this.
        IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartInnerSpinButton);

        style->setWidth(Length(size.width(), Fixed));
        style->setMinWidth(Length(size.width(), Fixed));
    }
}

int RenderThemeChromiumAndroid::menuListArrowPadding() const
{
    // We cannot use the scrollbar thickness here, as it's width is 0 on Android.
    // Instead, use the width of the scrollbar down arrow.
    IntSize scrollbarSize = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartScrollbarDownArrow);
    return scrollbarSize.width();
}

} // namespace WebCore
