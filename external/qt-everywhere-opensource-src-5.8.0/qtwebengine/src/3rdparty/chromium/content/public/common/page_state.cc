// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/page_state.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/page_state_serialization.h"

namespace content {
namespace {

base::NullableString16 ToNullableString16(const std::string& utf8) {
  return base::NullableString16(base::UTF8ToUTF16(utf8), false);
}

base::FilePath ToFilePath(const base::NullableString16& s) {
  return base::FilePath::FromUTF16Unsafe(s.string());
}

void ToFilePathVector(const std::vector<base::NullableString16>& input,
                      std::vector<base::FilePath>* output) {
  output->clear();
  output->reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i)
    output->push_back(ToFilePath(input[i]));
}

PageState ToPageState(const ExplodedPageState& state) {
  std::string encoded_data;
  if (!EncodePageState(state, &encoded_data))
    return PageState();

  return PageState::CreateFromEncodedData(encoded_data);
}

void RecursivelyRemovePasswordData(ExplodedFrameState* state) {
  if (state->http_body.contains_passwords)
    state->http_body = ExplodedHttpBody();
}

void RecursivelyRemoveScrollOffset(ExplodedFrameState* state) {
  state->scroll_offset = gfx::Point();
  state->visual_viewport_scroll_offset = gfx::PointF();
}

void RecursivelyRemoveReferrer(ExplodedFrameState* state) {
  state->referrer = base::NullableString16();
  state->referrer_policy = blink::WebReferrerPolicyDefault;
  for (std::vector<ExplodedFrameState>::iterator it = state->children.begin();
       it != state->children.end();
       ++it) {
    RecursivelyRemoveReferrer(&*it);
  }
}

}  // namespace

// static
PageState PageState::CreateFromEncodedData(const std::string& data) {
  return PageState(data);
}

// static
PageState PageState::CreateFromURL(const GURL& url) {
  ExplodedPageState state;

  state.top.url_string = ToNullableString16(url.possibly_invalid_spec());

  return ToPageState(state);
}

// static
PageState PageState::CreateForTesting(
    const GURL& url,
    bool body_contains_password_data,
    const char* optional_body_data,
    const base::FilePath* optional_body_file_path) {
  ExplodedPageState state;

  state.top.url_string = ToNullableString16(url.possibly_invalid_spec());

  if (optional_body_data || optional_body_file_path) {
    if (optional_body_data) {
      std::string body_data(optional_body_data);
      state.top.http_body.request_body = new ResourceRequestBodyImpl();
      state.top.http_body.request_body->AppendBytes(body_data.data(),
                                                    body_data.size());
    }
    if (optional_body_file_path) {
      state.top.http_body.request_body = new ResourceRequestBodyImpl();
      state.top.http_body.request_body->AppendFileRange(
          *optional_body_file_path,
          0, std::numeric_limits<uint64_t>::max(),
          base::Time());
      state.referenced_files.push_back(base::NullableString16(
          optional_body_file_path->AsUTF16Unsafe(), false));
    }
    state.top.http_body.contains_passwords =
        body_contains_password_data;
  }

  return ToPageState(state);
}

PageState::PageState() {
}

bool PageState::IsValid() const {
  return !data_.empty();
}

bool PageState::Equals(const PageState& other) const {
  return data_ == other.data_;
}

const std::string& PageState::ToEncodedData() const {
  return data_;
}

std::vector<base::FilePath> PageState::GetReferencedFiles() const {
  std::vector<base::FilePath> results;

  ExplodedPageState state;
  if (DecodePageState(data_, &state))
    ToFilePathVector(state.referenced_files, &results);

  return results;
}

PageState PageState::RemovePasswordData() const {
  ExplodedPageState state;
  if (!DecodePageState(data_, &state))
    return PageState();  // Oops!

  RecursivelyRemovePasswordData(&state.top);

  return ToPageState(state);
}

PageState PageState::RemoveScrollOffset() const {
  ExplodedPageState state;
  if (!DecodePageState(data_, &state))
    return PageState();  // Oops!

  RecursivelyRemoveScrollOffset(&state.top);

  return ToPageState(state);
}

PageState PageState::RemoveReferrer() const {
  if (data_.empty())
    return *this;

  ExplodedPageState state;
  if (!DecodePageState(data_, &state))
    return PageState();  // Oops!

  RecursivelyRemoveReferrer(&state.top);

  return ToPageState(state);
}

PageState::PageState(const std::string& data)
    : data_(data) {
  // TODO(darin): Enable this DCHECK once tests have been fixed up to not pass
  // bogus encoded data to CreateFromEncodedData.
  //DCHECK(IsValid());
}

}  // namespace content
