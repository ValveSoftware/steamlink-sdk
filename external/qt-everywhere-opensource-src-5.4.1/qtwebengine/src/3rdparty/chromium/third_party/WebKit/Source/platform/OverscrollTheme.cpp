/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "platform/OverscrollTheme.h"

namespace WebCore {

OverscrollTheme::OverscrollTheme()
{
#if USE(RUBBER_BANDING)
    // Load the shadow and pattern for the overhang.
    m_overhangShadow = Image::loadPlatformResource("overhangShadow");
    m_overhangPattern = Image::loadPlatformResource("overhangPattern");
#endif
}

PassRefPtr<Image> OverscrollTheme::getOverhangImage()
{
    return m_overhangPattern;
}

void OverscrollTheme::setUpOverhangShadowLayer(GraphicsLayer* overhangShadowLayer)
{
    // The shadow texture is has a 1-pixel aperture in the center, so the division by
    // two is doing an intentional round-down.
    overhangShadowLayer->setContentsToNinePatch(
        m_overhangShadow.get(),
        IntRect(m_overhangShadow->width() / 2, m_overhangShadow->height() / 2, 1, 1));
}

void OverscrollTheme::updateOverhangShadowLayer(GraphicsLayer* shadowLayer, GraphicsLayer* rootContentLayer)
{
    // Note that for the position, the division m_overhangShadow->width() / 2 is an intentional
    // round-down, and that for the width and height, the 1-pixel aperture is being replaced
    // by the root contents layer, hence subtracting 1 and adding the rootContentsLayer size.
    IntRect shadowRect(
        static_cast<int>(rootContentLayer->position().x()) - m_overhangShadow->width() / 2,
        static_cast<int>(rootContentLayer->position().y()) -  m_overhangShadow->height() / 2,
        static_cast<int>(rootContentLayer->size().width()) + m_overhangShadow->width() - 1,
        static_cast<int>(rootContentLayer->size().height()) + m_overhangShadow->height() - 1);
    shadowLayer->setContentsRect(shadowRect);
}

OverscrollTheme* OverscrollTheme::theme()
{
    DEFINE_STATIC_LOCAL(OverscrollTheme, theme, ());
    return &theme;
}

}
