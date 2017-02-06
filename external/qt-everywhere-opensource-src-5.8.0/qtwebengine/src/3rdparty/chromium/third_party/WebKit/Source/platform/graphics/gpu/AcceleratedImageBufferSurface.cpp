/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"

#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"

namespace blink {

AcceleratedImageBufferSurface::AcceleratedImageBufferSurface(const IntSize& size, OpacityMode opacityMode)
    : ImageBufferSurface(size, opacityMode)
{
    m_contextProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!m_contextProvider)
        return;
    GrContext* grContext = m_contextProvider->grContext();
    if (!grContext)
        return;

    SkAlphaType alphaType = (Opaque == opacityMode) ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
    SkImageInfo info = SkImageInfo::MakeN32(size.width(), size.height(), alphaType);
    SkSurfaceProps disableLCDProps(0, kUnknown_SkPixelGeometry);
    m_surface = SkSurface::MakeRenderTarget(grContext, SkBudgeted::kYes, info, 0 /* sampleCount */,
        Opaque == opacityMode ? nullptr : &disableLCDProps);
    if (!m_surface.get())
        return;
    clear();
}

PassRefPtr<SkImage> AcceleratedImageBufferSurface::newImageSnapshot(AccelerationHint, SnapshotReason)
{
    return fromSkSp(m_surface->makeImageSnapshot());
}

GLuint AcceleratedImageBufferSurface::getBackingTextureHandleForOverwrite()
{
    if (!m_surface)
        return 0;
    return skia::GrBackendObjectToGrGLTextureInfo(m_surface->getTextureHandle(SkSurface::kDiscardWrite_TextureHandleAccess))->fID;
}

} // namespace blink
