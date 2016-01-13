// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextResource_h
#define TextResource_h

#include "core/fetch/ResourcePtr.h"

namespace WebCore {

class ResourceFetcher;
class TextResourceDecoder;

class TextResource : public Resource {
public:
    // Returns the decoded data in text form. The data has to be available at
    // call time.
    String decodedText() const;

    virtual void setEncoding(const String&) OVERRIDE;
    virtual String encoding() const OVERRIDE;

protected:
    TextResource(const ResourceRequest&, Type, const String& mimeType, const String& charset);
    virtual ~TextResource();

private:
    OwnPtr<TextResourceDecoder> m_decoder;
};

}

#endif // TextResource_h
