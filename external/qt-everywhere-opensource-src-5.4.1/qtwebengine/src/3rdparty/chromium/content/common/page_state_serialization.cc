// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/page_state_serialization.h"

#include <algorithm>
#include <limits>

#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/screen.h"

namespace content {
namespace {

#if defined(OS_ANDROID)
float g_device_scale_factor_for_testing = 0.0;
#endif

//-----------------------------------------------------------------------------

void AppendDataToHttpBody(ExplodedHttpBody* http_body, const char* data,
                          int data_length) {
  ExplodedHttpBodyElement element;
  element.type = blink::WebHTTPBody::Element::TypeData;
  element.data.assign(data, data_length);
  http_body->elements.push_back(element);
}

void AppendFileRangeToHttpBody(ExplodedHttpBody* http_body,
                               const base::NullableString16& file_path,
                               int file_start,
                               int file_length,
                               double file_modification_time) {
  ExplodedHttpBodyElement element;
  element.type = blink::WebHTTPBody::Element::TypeFile;
  element.file_path = file_path;
  element.file_start = file_start;
  element.file_length = file_length;
  element.file_modification_time = file_modification_time;
  http_body->elements.push_back(element);
}

void AppendURLRangeToHttpBody(ExplodedHttpBody* http_body,
                              const GURL& url,
                              int file_start,
                              int file_length,
                              double file_modification_time) {
  ExplodedHttpBodyElement element;
  element.type = blink::WebHTTPBody::Element::TypeFileSystemURL;
  element.filesystem_url = url;
  element.file_start = file_start;
  element.file_length = file_length;
  element.file_modification_time = file_modification_time;
  http_body->elements.push_back(element);
}

void AppendBlobToHttpBody(ExplodedHttpBody* http_body,
                          const std::string& uuid) {
  ExplodedHttpBodyElement element;
  element.type = blink::WebHTTPBody::Element::TypeBlob;
  element.blob_uuid = uuid;
  http_body->elements.push_back(element);
}

//----------------------------------------------------------------------------

void AppendReferencedFilesFromHttpBody(
    const std::vector<ExplodedHttpBodyElement>& elements,
    std::vector<base::NullableString16>* referenced_files) {
  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i].type == blink::WebHTTPBody::Element::TypeFile)
      referenced_files->push_back(elements[i].file_path);
  }
}

bool AppendReferencedFilesFromDocumentState(
    const std::vector<base::NullableString16>& document_state,
    std::vector<base::NullableString16>* referenced_files) {
  if (document_state.empty())
    return true;

  // This algorithm is adapted from Blink's core/html/FormController.cpp code.
  // We only care about how that code worked when this code snapshot was taken
  // as this code is only needed for backwards compat.
  //
  // For reference, see FormController::formStatesFromStateVector at:
  // http://src.chromium.org/viewvc/blink/trunk/Source/core/html/FormController.cpp?pathrev=152274

  size_t index = 0;

  if (document_state.size() < 3)
    return false;

  index++;  // Skip over magic signature.
  index++;  // Skip over form key.

  size_t item_count;
  if (!base::StringToSizeT(document_state[index++].string(), &item_count))
    return false;

  while (item_count--) {
    if (index + 1 >= document_state.size())
      return false;

    index++;  // Skip over name.
    const base::NullableString16& type = document_state[index++];

    if (index >= document_state.size())
      return false;

    size_t value_size;
    if (!base::StringToSizeT(document_state[index++].string(), &value_size))
      return false;

    if (index + value_size > document_state.size() ||
        index + value_size < index)  // Check for overflow.
      return false;

    if (EqualsASCII(type.string(), "file")) {
      if (value_size != 2)
        return false;

      referenced_files->push_back(document_state[index++]);
      index++;  // Skip over display name.
    } else {
      index += value_size;
    }
  }

  return true;
}

bool RecursivelyAppendReferencedFiles(
    const ExplodedFrameState& frame_state,
    std::vector<base::NullableString16>* referenced_files) {
  if (!frame_state.http_body.is_null) {
    AppendReferencedFilesFromHttpBody(frame_state.http_body.elements,
                                      referenced_files);
  }

  if (!AppendReferencedFilesFromDocumentState(frame_state.document_state,
                                              referenced_files))
    return false;

  for (size_t i = 0; i < frame_state.children.size(); ++i) {
    if (!RecursivelyAppendReferencedFiles(frame_state.children[i],
                                          referenced_files))
      return false;
  }

  return true;
}

//----------------------------------------------------------------------------

struct SerializeObject {
  SerializeObject()
      : version(0),
        parse_error(false) {
  }

  SerializeObject(const char* data, int len)
      : pickle(data, len),
        version(0),
        parse_error(false) {
    iter = PickleIterator(pickle);
  }

  std::string GetAsString() {
    return std::string(static_cast<const char*>(pickle.data()), pickle.size());
  }

  Pickle pickle;
  PickleIterator iter;
  int version;
  bool parse_error;
};

// Version ID of serialized format.
// 11: Min version
// 12: Adds support for contains_passwords in HTTP body
// 13: Adds support for URL (FileSystem URL)
// 14: Adds list of referenced files, version written only for first item.
// 15: Removes a bunch of values we defined but never used.
// 16: Switched from blob urls to blob uuids.
// 17: Add a target frame id number.
// 18: Add referrer policy.
// 19: Remove target frame id, which was a bad idea, and original url string,
//         which is no longer used.
// 20: Add pinch viewport scroll offset, the offset of the pinched zoomed
//     viewport within the unzoomed main frame.
//
// NOTE: If the version is -1, then the pickle contains only a URL string.
// See ReadPageState.
//
const int kMinVersion = 11;
const int kCurrentVersion = 20;

// A bunch of convenience functions to read/write to SerializeObjects.  The
// de-serializers assume the input data will be in the correct format and fall
// back to returning safe defaults when not.

void WriteData(const void* data, int length, SerializeObject* obj) {
  obj->pickle.WriteData(static_cast<const char*>(data), length);
}

void ReadData(SerializeObject* obj, const void** data, int* length) {
  const char* tmp;
  if (obj->pickle.ReadData(&obj->iter, &tmp, length)) {
    *data = tmp;
  } else {
    obj->parse_error = true;
    *data = NULL;
    *length = 0;
  }
}

void WriteInteger(int data, SerializeObject* obj) {
  obj->pickle.WriteInt(data);
}

int ReadInteger(SerializeObject* obj) {
  int tmp;
  if (obj->pickle.ReadInt(&obj->iter, &tmp))
    return tmp;
  obj->parse_error = true;
  return 0;
}

void ConsumeInteger(SerializeObject* obj) {
  int unused ALLOW_UNUSED = ReadInteger(obj);
}

void WriteInteger64(int64 data, SerializeObject* obj) {
  obj->pickle.WriteInt64(data);
}

int64 ReadInteger64(SerializeObject* obj) {
  int64 tmp = 0;
  if (obj->pickle.ReadInt64(&obj->iter, &tmp))
    return tmp;
  obj->parse_error = true;
  return 0;
}

void ConsumeInteger64(SerializeObject* obj) {
  int64 unused ALLOW_UNUSED = ReadInteger64(obj);
}

void WriteReal(double data, SerializeObject* obj) {
  WriteData(&data, sizeof(double), obj);
}

double ReadReal(SerializeObject* obj) {
  const void* tmp = NULL;
  int length = 0;
  double value = 0.0;
  ReadData(obj, &tmp, &length);
  if (length == static_cast<int>(sizeof(double))) {
    // Use memcpy, as tmp may not be correctly aligned.
    memcpy(&value, tmp, sizeof(double));
  } else {
    obj->parse_error = true;
  }
  return value;
}

void ConsumeReal(SerializeObject* obj) {
  double unused ALLOW_UNUSED = ReadReal(obj);
}

void WriteBoolean(bool data, SerializeObject* obj) {
  obj->pickle.WriteInt(data ? 1 : 0);
}

bool ReadBoolean(SerializeObject* obj) {
  bool tmp;
  if (obj->pickle.ReadBool(&obj->iter, &tmp))
    return tmp;
  obj->parse_error = true;
  return false;
}

void ConsumeBoolean(SerializeObject* obj) {
  bool unused ALLOW_UNUSED = ReadBoolean(obj);
}

void WriteGURL(const GURL& url, SerializeObject* obj) {
  obj->pickle.WriteString(url.possibly_invalid_spec());
}

GURL ReadGURL(SerializeObject* obj) {
  std::string spec;
  if (obj->pickle.ReadString(&obj->iter, &spec))
    return GURL(spec);
  obj->parse_error = true;
  return GURL();
}

void WriteStdString(const std::string& s, SerializeObject* obj) {
  obj->pickle.WriteString(s);
}

std::string ReadStdString(SerializeObject* obj) {
  std::string s;
  if (obj->pickle.ReadString(&obj->iter, &s))
    return s;
  obj->parse_error = true;
  return std::string();
}

// WriteString pickles the NullableString16 as <int length><char16* data>.
// If length == -1, then the NullableString16 itself is null.  Otherwise the
// length is the number of char16 (not bytes) in the NullableString16.
void WriteString(const base::NullableString16& str, SerializeObject* obj) {
  if (str.is_null()) {
    obj->pickle.WriteInt(-1);
  } else {
    const base::char16* data = str.string().data();
    size_t length_in_bytes = str.string().length() * sizeof(base::char16);

    CHECK_LT(length_in_bytes,
             static_cast<size_t>(std::numeric_limits<int>::max()));
    obj->pickle.WriteInt(length_in_bytes);
    obj->pickle.WriteBytes(data, length_in_bytes);
  }
}

// This reads a serialized NullableString16 from obj. If a string can't be
// read, NULL is returned.
const base::char16* ReadStringNoCopy(SerializeObject* obj, int* num_chars) {
  int length_in_bytes;
  if (!obj->pickle.ReadInt(&obj->iter, &length_in_bytes)) {
    obj->parse_error = true;
    return NULL;
  }

  if (length_in_bytes < 0)
    return NULL;

  const char* data;
  if (!obj->pickle.ReadBytes(&obj->iter, &data, length_in_bytes)) {
    obj->parse_error = true;
    return NULL;
  }

  if (num_chars)
    *num_chars = length_in_bytes / sizeof(base::char16);
  return reinterpret_cast<const base::char16*>(data);
}

base::NullableString16 ReadString(SerializeObject* obj) {
  int num_chars;
  const base::char16* chars = ReadStringNoCopy(obj, &num_chars);
  return chars ?
      base::NullableString16(base::string16(chars, num_chars), false) :
      base::NullableString16();
}

void ConsumeString(SerializeObject* obj) {
  const base::char16* unused ALLOW_UNUSED = ReadStringNoCopy(obj, NULL);
}

template <typename T>
void WriteAndValidateVectorSize(const std::vector<T>& v, SerializeObject* obj) {
  CHECK_LT(v.size(), std::numeric_limits<int>::max() / sizeof(T));
  WriteInteger(static_cast<int>(v.size()), obj);
}

size_t ReadAndValidateVectorSize(SerializeObject* obj, size_t element_size) {
  size_t num_elements = static_cast<size_t>(ReadInteger(obj));

  // Ensure that resizing a vector to size num_elements makes sense.
  if (std::numeric_limits<int>::max() / element_size <= num_elements) {
    obj->parse_error = true;
    return 0;
  }

  // Ensure that it is plausible for the pickle to contain num_elements worth
  // of data.
  if (obj->pickle.payload_size() <= num_elements) {
    obj->parse_error = true;
    return 0;
  }

  return num_elements;
}

// Writes a Vector of strings into a SerializeObject for serialization.
void WriteStringVector(
    const std::vector<base::NullableString16>& data, SerializeObject* obj) {
  WriteAndValidateVectorSize(data, obj);
  for (size_t i = 0; i < data.size(); ++i) {
    WriteString(data[i], obj);
  }
}

void ReadStringVector(SerializeObject* obj,
                      std::vector<base::NullableString16>* result) {
  size_t num_elements =
      ReadAndValidateVectorSize(obj, sizeof(base::NullableString16));

  result->resize(num_elements);
  for (size_t i = 0; i < num_elements; ++i)
    (*result)[i] = ReadString(obj);
}

// Writes an ExplodedHttpBody object into a SerializeObject for serialization.
void WriteHttpBody(const ExplodedHttpBody& http_body, SerializeObject* obj) {
  WriteBoolean(!http_body.is_null, obj);

  if (http_body.is_null)
    return;

  WriteAndValidateVectorSize(http_body.elements, obj);
  for (size_t i = 0; i < http_body.elements.size(); ++i) {
    const ExplodedHttpBodyElement& element = http_body.elements[i];
    WriteInteger(element.type, obj);
    if (element.type == blink::WebHTTPBody::Element::TypeData) {
      WriteData(element.data.data(), static_cast<int>(element.data.size()),
                obj);
    } else if (element.type == blink::WebHTTPBody::Element::TypeFile) {
      WriteString(element.file_path, obj);
      WriteInteger64(element.file_start, obj);
      WriteInteger64(element.file_length, obj);
      WriteReal(element.file_modification_time, obj);
    } else if (element.type ==
               blink::WebHTTPBody::Element::TypeFileSystemURL) {
      WriteGURL(element.filesystem_url, obj);
      WriteInteger64(element.file_start, obj);
      WriteInteger64(element.file_length, obj);
      WriteReal(element.file_modification_time, obj);
    } else {
      DCHECK(element.type == blink::WebHTTPBody::Element::TypeBlob);
      WriteStdString(element.blob_uuid, obj);
    }
  }
  WriteInteger64(http_body.identifier, obj);
  WriteBoolean(http_body.contains_passwords, obj);
}

void ReadHttpBody(SerializeObject* obj, ExplodedHttpBody* http_body) {
  // An initial boolean indicates if we have an HTTP body.
  if (!ReadBoolean(obj))
    return;
  http_body->is_null = false;

  int num_elements = ReadInteger(obj);

  for (int i = 0; i < num_elements; ++i) {
    int type = ReadInteger(obj);
    if (type == blink::WebHTTPBody::Element::TypeData) {
      const void* data;
      int length = -1;
      ReadData(obj, &data, &length);
      if (length >= 0) {
        AppendDataToHttpBody(http_body, static_cast<const char*>(data),
                             length);
      }
    } else if (type == blink::WebHTTPBody::Element::TypeFile) {
      base::NullableString16 file_path = ReadString(obj);
      int64 file_start = ReadInteger64(obj);
      int64 file_length = ReadInteger64(obj);
      double file_modification_time = ReadReal(obj);
      AppendFileRangeToHttpBody(http_body, file_path, file_start, file_length,
                                file_modification_time);
    } else if (type == blink::WebHTTPBody::Element::TypeFileSystemURL) {
      GURL url = ReadGURL(obj);
      int64 file_start = ReadInteger64(obj);
      int64 file_length = ReadInteger64(obj);
      double file_modification_time = ReadReal(obj);
      AppendURLRangeToHttpBody(http_body, url, file_start, file_length,
                               file_modification_time);
    } else if (type == blink::WebHTTPBody::Element::TypeBlob) {
      if (obj->version >= 16) {
        std::string blob_uuid = ReadStdString(obj);
        AppendBlobToHttpBody(http_body, blob_uuid);
      } else {
        ReadGURL(obj); // Skip the obsolete blob url value.
      }
    }
  }
  http_body->identifier = ReadInteger64(obj);

  if (obj->version >= 12)
    http_body->contains_passwords = ReadBoolean(obj);
}

// Writes the ExplodedFrameState data into the SerializeObject object for
// serialization.
void WriteFrameState(
    const ExplodedFrameState& state, SerializeObject* obj, bool is_top) {
  // WARNING: This data may be persisted for later use. As such, care must be
  // taken when changing the serialized format. If a new field needs to be
  // written, only adding at the end will make it easier to deal with loading
  // older versions. Similarly, this should NOT save fields with sensitive
  // data, such as password fields.

  WriteString(state.url_string, obj);
  WriteString(state.target, obj);
  WriteInteger(state.scroll_offset.x(), obj);
  WriteInteger(state.scroll_offset.y(), obj);
  WriteString(state.referrer, obj);

  WriteStringVector(state.document_state, obj);

  WriteReal(state.page_scale_factor, obj);
  WriteInteger64(state.item_sequence_number, obj);
  WriteInteger64(state.document_sequence_number, obj);
  WriteInteger(state.referrer_policy, obj);
  WriteReal(state.pinch_viewport_scroll_offset.x(), obj);
  WriteReal(state.pinch_viewport_scroll_offset.y(), obj);

  bool has_state_object = !state.state_object.is_null();
  WriteBoolean(has_state_object, obj);
  if (has_state_object)
    WriteString(state.state_object, obj);

  WriteHttpBody(state.http_body, obj);

  // NOTE: It is a quirk of the format that we still have to write the
  // http_content_type field when the HTTP body is null.  That's why this code
  // is here instead of inside WriteHttpBody.
  WriteString(state.http_body.http_content_type, obj);

  // Subitems
  const std::vector<ExplodedFrameState>& children = state.children;
  WriteAndValidateVectorSize(children, obj);
  for (size_t i = 0; i < children.size(); ++i)
    WriteFrameState(children[i], obj, false);
}

void ReadFrameState(SerializeObject* obj, bool is_top,
                    ExplodedFrameState* state) {
  if (obj->version < 14 && !is_top)
    ConsumeInteger(obj);  // Skip over redundant version field.

  state->url_string = ReadString(obj);

  if (obj->version < 19)
    ConsumeString(obj);  // Skip obsolete original url string field.

  state->target = ReadString(obj);
  if (obj->version < 15) {
    ConsumeString(obj);  // Skip obsolete parent field.
    ConsumeString(obj);  // Skip obsolete title field.
    ConsumeString(obj);  // Skip obsolete alternate title field.
    ConsumeReal(obj);    // Skip obsolete visited time field.
  }

  int x = ReadInteger(obj);
  int y = ReadInteger(obj);
  state->scroll_offset = gfx::Point(x, y);

  if (obj->version < 15) {
    ConsumeBoolean(obj);  // Skip obsolete target item flag.
    ConsumeInteger(obj);  // Skip obsolete visit count field.
  }
  state->referrer = ReadString(obj);

  ReadStringVector(obj, &state->document_state);

  state->page_scale_factor = ReadReal(obj);
  state->item_sequence_number = ReadInteger64(obj);
  state->document_sequence_number = ReadInteger64(obj);

  if (obj->version >= 17 && obj->version < 19)
    ConsumeInteger64(obj); // Skip obsolete target frame id number.

  if (obj->version >= 18) {
    state->referrer_policy =
        static_cast<blink::WebReferrerPolicy>(ReadInteger(obj));
  }

  if (obj->version >= 20) {
    double x = ReadReal(obj);
    double y = ReadReal(obj);
    state->pinch_viewport_scroll_offset = gfx::PointF(x, y);
  } else {
    state->pinch_viewport_scroll_offset = gfx::PointF(-1, -1);
  }

  bool has_state_object = ReadBoolean(obj);
  if (has_state_object)
    state->state_object = ReadString(obj);

  ReadHttpBody(obj, &state->http_body);

  // NOTE: It is a quirk of the format that we still have to read the
  // http_content_type field when the HTTP body is null.  That's why this code
  // is here instead of inside ReadHttpBody.
  state->http_body.http_content_type = ReadString(obj);

  if (obj->version < 14)
    ConsumeString(obj);  // Skip unused referrer string.

#if defined(OS_ANDROID)
  if (obj->version == 11) {
    // Now-unused values that shipped in this version of Chrome for Android when
    // it was on a private branch.
    ReadReal(obj);
    ReadBoolean(obj);

    // In this version, page_scale_factor included device_scale_factor and
    // scroll offsets were premultiplied by pageScaleFactor.
    if (state->page_scale_factor) {
      float device_scale_factor = g_device_scale_factor_for_testing;
      if (!device_scale_factor) {
        device_scale_factor =
            gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().
                device_scale_factor();
      }
      state->scroll_offset =
          gfx::Point(state->scroll_offset.x() / state->page_scale_factor,
                     state->scroll_offset.y() / state->page_scale_factor);
      state->page_scale_factor /= device_scale_factor;
    }
  }
#endif

  // Subitems
  size_t num_children =
      ReadAndValidateVectorSize(obj, sizeof(ExplodedFrameState));
  state->children.resize(num_children);
  for (size_t i = 0; i < num_children; ++i)
    ReadFrameState(obj, false, &state->children[i]);
}

void WritePageState(const ExplodedPageState& state, SerializeObject* obj) {
  WriteInteger(obj->version, obj);
  WriteStringVector(state.referenced_files, obj);
  WriteFrameState(state.top, obj, true);
}

void ReadPageState(SerializeObject* obj, ExplodedPageState* state) {
  obj->version = ReadInteger(obj);

  if (obj->version == -1) {
    GURL url = ReadGURL(obj);
    // NOTE: GURL::possibly_invalid_spec() always returns valid UTF-8.
    state->top.url_string =
        base::NullableString16(
            base::UTF8ToUTF16(url.possibly_invalid_spec()), false);
    return;
  }

  if (obj->version > kCurrentVersion || obj->version < kMinVersion) {
    obj->parse_error = true;
    return;
  }

  if (obj->version >= 14)
    ReadStringVector(obj, &state->referenced_files);

  ReadFrameState(obj, true, &state->top);

  if (obj->version < 14)
    RecursivelyAppendReferencedFiles(state->top, &state->referenced_files);

  // De-dupe
  state->referenced_files.erase(
      std::unique(state->referenced_files.begin(),
                  state->referenced_files.end()),
      state->referenced_files.end());
}

}  // namespace

ExplodedHttpBodyElement::ExplodedHttpBodyElement()
    : type(blink::WebHTTPBody::Element::TypeData),
      file_start(0),
      file_length(-1),
      file_modification_time(std::numeric_limits<double>::quiet_NaN()) {
}

ExplodedHttpBodyElement::~ExplodedHttpBodyElement() {
}

ExplodedHttpBody::ExplodedHttpBody()
    : identifier(0),
      contains_passwords(false),
      is_null(true) {
}

ExplodedHttpBody::~ExplodedHttpBody() {
}

ExplodedFrameState::ExplodedFrameState()
    : item_sequence_number(0),
      document_sequence_number(0),
      page_scale_factor(0.0),
      referrer_policy(blink::WebReferrerPolicyDefault) {
}

ExplodedFrameState::~ExplodedFrameState() {
}

ExplodedPageState::ExplodedPageState() {
}

ExplodedPageState::~ExplodedPageState() {
}

bool DecodePageState(const std::string& encoded, ExplodedPageState* exploded) {
  *exploded = ExplodedPageState();

  if (encoded.empty())
    return true;

  SerializeObject obj(encoded.data(), static_cast<int>(encoded.size()));
  ReadPageState(&obj, exploded);
  return !obj.parse_error;
}

bool EncodePageState(const ExplodedPageState& exploded, std::string* encoded) {
  SerializeObject obj;
  obj.version = kCurrentVersion;
  WritePageState(exploded, &obj);
  *encoded = obj.GetAsString();
  return true;
}

#if defined(OS_ANDROID)
bool DecodePageStateWithDeviceScaleFactorForTesting(
    const std::string& encoded,
    float device_scale_factor,
    ExplodedPageState* exploded) {
  g_device_scale_factor_for_testing = device_scale_factor;
  bool rv = DecodePageState(encoded, exploded);
  g_device_scale_factor_for_testing = 0.0;
  return rv;
}
#endif

}  // namespace content
