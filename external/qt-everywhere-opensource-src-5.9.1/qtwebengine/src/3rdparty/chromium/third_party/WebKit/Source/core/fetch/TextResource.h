// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextResource_h
#define TextResource_h

#include "core/CoreExport.h"
#include "core/fetch/Resource.h"
#include <memory>

namespace blink {

class TextResourceDecoder;

class CORE_EXPORT TextResource : public Resource {
 public:
  // Returns the decoded data in text form. The data has to be available at
  // call time.
  String decodedText() const;

  void setEncoding(const String&) override;
  String encoding() const override;

 protected:
  TextResource(const ResourceRequest&,
               Type,
               const ResourceLoaderOptions&,
               const String& mimeType,
               const String& charset);
  ~TextResource() override;

 private:
  std::unique_ptr<TextResourceDecoder> m_decoder;
};

}  // namespace blink

#endif  // TextResource_h
