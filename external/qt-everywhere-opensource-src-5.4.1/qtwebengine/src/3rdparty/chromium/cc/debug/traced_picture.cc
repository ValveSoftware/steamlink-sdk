// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/traced_picture.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"

namespace cc {

TracedPicture::TracedPicture(scoped_refptr<const Picture> picture)
    : picture_(picture), is_alias_(false) {}

TracedPicture::~TracedPicture() {
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
TracedPicture::AsTraceablePicture(const Picture* picture) {
  return scoped_refptr<base::debug::ConvertableToTraceFormat>(
      new TracedPicture(picture));
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
TracedPicture::AsTraceablePictureAlias(const Picture* original) {
  scoped_refptr<TracedPicture> ptr = new TracedPicture(original);
  ptr->is_alias_ = true;
  return scoped_refptr<base::debug::ConvertableToTraceFormat>(ptr);
}

void TracedPicture::AppendAsTraceFormat(std::string* out) const {
  if (is_alias_)
    AppendPictureAlias(out);
  else
    AppendPicture(out);
}

void TracedPicture::AppendPictureAlias(std::string* out) const {
  scoped_ptr<base::DictionaryValue> alias(new base::DictionaryValue());
  alias->SetString("id_ref", base::StringPrintf("%p", picture_.get()));

  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->Set("alias", alias.release());

  std::string tmp;
  base::JSONWriter::Write(res.get(), &tmp);
  out->append(tmp);
}

void TracedPicture::AppendPicture(std::string* out) const {
  scoped_ptr<base::Value> value = picture_->AsValue();
  std::string tmp;
  base::JSONWriter::Write(value.get(), &tmp);
  out->append(tmp);
}

}  // namespace cc
