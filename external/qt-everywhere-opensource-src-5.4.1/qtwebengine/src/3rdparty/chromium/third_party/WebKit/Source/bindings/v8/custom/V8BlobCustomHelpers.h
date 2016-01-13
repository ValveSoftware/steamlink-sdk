/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef V8BlobCustomHelpers_h
#define V8BlobCustomHelpers_h

#include "bindings/v8/V8Binding.h"

namespace WebCore {

class BlobData;
class ExceptionState;

// Shared code between the custom constructor bindings for Blob and File.
namespace V8BlobCustomHelpers {

// Parsed properties from a BlobPropertyBag or a FilePropertyBag.
//
// Instances are stack-allocated by the File and Blob constructor.
//
// parseBlobPropertyBag is only called when constructors receive a value for
// the optional BlobPropertyBag / FilePropertyBag argument. The defaults set in
// the constructor are used otherwise.
class ParsedProperties {
public:
    explicit ParsedProperties(bool hasFileProperties);

    const String& contentType() const { return m_contentType; }
    bool normalizeLineEndingsToNative() const { return m_normalizeLineEndingsToNative; }
    void setLastModified(double);
    void setDefaultLastModified();
    double lastModified() const
    {
        ASSERT(m_hasFileProperties);
        ASSERT(m_hasLastModified);
        return m_lastModified;
    }

    // Extracts the "type" and "endings" properties out of the BlobPropertyBag passed to a Blob constructor.
    // http://www.w3.org/TR/FileAPI/#constructorParams
    // Returns true if everything went well, false if a JS exception was thrown.
    bool parseBlobPropertyBag(v8::Local<v8::Value> propertyBag, const char* blobClassName, ExceptionState&, v8::Isolate*);

private:
    String m_contentType;
    bool m_normalizeLineEndingsToNative;

    // False if this contains the properties of a BlobPropertyBag.
    bool m_hasFileProperties;

    double m_lastModified;
#ifndef NDEBUG
    bool m_hasLastModified;
#endif // NDEBUG
};

// Appends the blobParts passed to a Blob or File constructor into a BlobData.
// http://www.w3.org/TR/FileAPI/#constructorParams
// Returns true if everything went well, false if a JS exception was thrown.
bool processBlobParts(v8::Local<v8::Object> blobParts, uint32_t blobPartsLength, bool normalizeLineEndingsToNative, BlobData&, v8::Isolate*);

} // namespace V8BlobCustomHelpers

} // namespace WebCore

#endif // V8BlobCustomHelpers_h
