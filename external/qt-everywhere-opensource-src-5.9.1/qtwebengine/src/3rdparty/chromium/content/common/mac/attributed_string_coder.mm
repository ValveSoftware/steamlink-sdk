// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/attributed_string_coder.h"

#include <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/render_process_messages.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_utils.h"

namespace mac {

// static
const AttributedStringCoder::EncodedString* AttributedStringCoder::Encode(
    NSAttributedString* str) {
  // Create the return value.
  EncodedString* encoded_string =
      new EncodedString(base::SysNSStringToUTF16([str string]));
  // Iterate over all the attributes in the string.
  NSUInteger length = [str length];
  for (NSUInteger i = 0; i < length; ) {
    NSRange effective_range;
    NSDictionary* ns_attributes = [str attributesAtIndex:i
                                          effectiveRange:&effective_range];
    // Convert the attributes to IPC-friendly types.
    FontAttribute attrs(ns_attributes, gfx::Range(effective_range));
    // Only encode the attributes if the filtered set contains font information.
    if (attrs.ShouldEncode()) {
      encoded_string->attributes()->push_back(attrs);
    }
    // Advance the iterator to the position outside of the effective range.
    i = NSMaxRange(effective_range);
  }
  return encoded_string;
}

// static
NSAttributedString* AttributedStringCoder::Decode(
    const AttributedStringCoder::EncodedString* str) {
  // Create the return value.
  NSString* plain_text = base::SysUTF16ToNSString(str->string());
  base::scoped_nsobject<NSMutableAttributedString> decoded_string(
      [[NSMutableAttributedString alloc] initWithString:plain_text]);
  // Iterate over all the encoded attributes, attaching each to the string.
  const std::vector<FontAttribute> attributes = str->attributes();
  for (std::vector<FontAttribute>::const_iterator it = attributes.begin();
       it != attributes.end(); ++it) {
    // Protect against ranges that are outside the range of the string.
    const gfx::Range& range = it->effective_range();
    if (range.GetMin() > [plain_text length] ||
        range.GetMax() > [plain_text length]) {
      continue;
    }
    [decoded_string addAttributes:it->ToAttributesDictionary()
                            range:range.ToNSRange()];
  }
  return [decoded_string.release() autorelease];
}

// Data Types //////////////////////////////////////////////////////////////////

AttributedStringCoder::EncodedString::EncodedString(base::string16 string)
    : string_(string) {
}

AttributedStringCoder::EncodedString::EncodedString()
    : string_() {
}

AttributedStringCoder::EncodedString::~EncodedString() {
}

AttributedStringCoder::FontAttribute::FontAttribute(NSDictionary* dict,
                                                    gfx::Range effective_range)
    : font_descriptor_(),
      effective_range_(effective_range) {
  NSFont* font = [dict objectForKey:NSFontAttributeName];
  if (font) {
    font_descriptor_ = FontDescriptor(font);
  }
}

AttributedStringCoder::FontAttribute::FontAttribute(FontDescriptor font,
                                                    gfx::Range range)
    : font_descriptor_(font),
      effective_range_(range) {
}

AttributedStringCoder::FontAttribute::FontAttribute()
    : font_descriptor_(),
      effective_range_() {
}

AttributedStringCoder::FontAttribute::~FontAttribute() {
}

NSDictionary*
AttributedStringCoder::FontAttribute::ToAttributesDictionary() const {
  DCHECK(ShouldEncode());
  NSFont* font = font_descriptor_.ToNSFont();
  if (!font)
    return [NSDictionary dictionary];
  return [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
}

bool AttributedStringCoder::FontAttribute::ShouldEncode() const {
  return !font_descriptor_.font_name.empty();
}

}  // namespace mac

// IPC ParamTraits specialization //////////////////////////////////////////////

namespace IPC {

using mac::AttributedStringCoder;

void ParamTraits<AttributedStringCoder::EncodedString>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.string());
  WriteParam(m, p.attributes());
}

bool ParamTraits<AttributedStringCoder::EncodedString>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  bool success = true;

  base::string16 result;
  success &= ReadParam(m, iter, &result);
  *p = AttributedStringCoder::EncodedString(result);

  success &= ReadParam(m, iter, p->attributes());
  return success;
}

void ParamTraits<AttributedStringCoder::EncodedString>::Log(
    const param_type& p, std::string* l) {
  l->append(base::UTF16ToUTF8(p.string()));
}

void ParamTraits<AttributedStringCoder::FontAttribute>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.font_descriptor());
  WriteParam(m, p.effective_range());
}

bool ParamTraits<AttributedStringCoder::FontAttribute>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  bool success = true;

  FontDescriptor font;
  success &= ReadParam(m, iter, &font);

  gfx::Range range;
  success &= ReadParam(m, iter, &range);

  if (success) {
    *p = AttributedStringCoder::FontAttribute(font, range);
  }
  return success;
}

void ParamTraits<AttributedStringCoder::FontAttribute>::Log(
    const param_type& p, std::string* l) {
}

}  // namespace IPC
