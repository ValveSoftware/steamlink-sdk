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

#ifndef ImageBitmapFactories_h
#define ImageBitmapFactories_h

#include "bindings/core/v8/HTMLImageElementOrHTMLVideoElementOrHTMLCanvasElementOrBlobOrImageDataOrImageBitmap.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "platform/Supplementable.h"
#include "platform/geometry/IntRect.h"

class SkImage;

namespace blink {

class Blob;
class EventTarget;
class ExceptionState;
class ExecutionContext;
class ImageBitmapSource;
class ImageBitmapOptions;
class ImageDecoder;
class WebTaskRunner;

typedef HTMLImageElementOrHTMLVideoElementOrHTMLCanvasElementOrBlobOrImageDataOrImageBitmap ImageBitmapSourceUnion;

class ImageBitmapFactories final : public GarbageCollectedFinalized<ImageBitmapFactories>, public Supplement<LocalDOMWindow>, public Supplement<WorkerGlobalScope> {
    USING_GARBAGE_COLLECTED_MIXIN(ImageBitmapFactories);
public:
    static ScriptPromise createImageBitmap(ScriptState*, EventTarget&, const ImageBitmapSourceUnion&, const ImageBitmapOptions&, ExceptionState&);
    static ScriptPromise createImageBitmap(ScriptState*, EventTarget&, const ImageBitmapSourceUnion&, int sx, int sy, int sw, int sh, const ImageBitmapOptions&, ExceptionState&);
    static ScriptPromise createImageBitmap(ScriptState*, EventTarget&, ImageBitmapSource*, int sx, int sy, int sw, int sh, const ImageBitmapOptions&, ExceptionState&);

    virtual ~ImageBitmapFactories() { }

    DECLARE_TRACE();

protected:
    static const char* supplementName();

private:
    class ImageBitmapLoader final : public GarbageCollectedFinalized<ImageBitmapLoader>, public FileReaderLoaderClient {
    public:
        static ImageBitmapLoader* create(ImageBitmapFactories& factory, const IntRect& cropRect, const ImageBitmapOptions& options, ScriptState* scriptState)
        {
            return new ImageBitmapLoader(factory, cropRect, scriptState, options);
        }

        void loadBlobAsync(ExecutionContext*, Blob*);
        ScriptPromise promise() { return m_resolver->promise(); }

        DECLARE_TRACE();

        ~ImageBitmapLoader() override { }

    private:
        ImageBitmapLoader(ImageBitmapFactories&, const IntRect&, ScriptState*, const ImageBitmapOptions&);

        void rejectPromise();

        void scheduleAsyncImageBitmapDecoding(DOMArrayBuffer*);
        void decodeImageOnDecoderThread(WebTaskRunner*, DOMArrayBuffer*, const String& premultiplyAlphaOption, const String& colorSpaceConversionOption);
        void resolvePromiseOnOriginalThread(PassRefPtr<SkImage>);

        // FileReaderLoaderClient
        void didStartLoading() override { }
        void didReceiveData() override { }
        void didFinishLoading() override;
        void didFail(FileError::ErrorCode) override;

        FileReaderLoader m_loader;
        Member<ImageBitmapFactories> m_factory;
        Member<ScriptPromiseResolver> m_resolver;
        IntRect m_cropRect;
        ImageBitmapOptions m_options;
    };

    static ImageBitmapFactories& from(EventTarget&);

    template<class GlobalObject>
    static ImageBitmapFactories& fromInternal(GlobalObject&);

    void addLoader(ImageBitmapLoader*);
    void didFinishLoading(ImageBitmapLoader*);

    HeapHashSet<Member<ImageBitmapLoader>> m_pendingLoaders;
};

} // namespace blink

#endif // ImageBitmapFactories_h
