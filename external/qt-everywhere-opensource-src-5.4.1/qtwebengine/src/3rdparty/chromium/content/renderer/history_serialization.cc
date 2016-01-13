// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/history_serialization.h"

#include "content/common/page_state_serialization.h"
#include "content/public/common/page_state.h"
#include "content/renderer/history_entry.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"

using blink::WebHTTPBody;
using blink::WebHistoryItem;
using blink::WebSerializedScriptValue;
using blink::WebString;
using blink::WebVector;

namespace content {
namespace {

void ToNullableString16Vector(const WebVector<WebString>& input,
                              std::vector<base::NullableString16>* output) {
  output->reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i)
    output->push_back(input[i]);
}

void ToExplodedHttpBodyElement(const WebHTTPBody::Element& input,
                               ExplodedHttpBodyElement* output) {
  switch (input.type) {
    case WebHTTPBody::Element::TypeData:
      output->data.assign(input.data.data(), input.data.size());
      break;
    case WebHTTPBody::Element::TypeFile:
      output->file_path = input.filePath;
      output->file_start = input.fileStart;
      output->file_length = input.fileLength;
      output->file_modification_time = input.modificationTime;
      break;
    case WebHTTPBody::Element::TypeFileSystemURL:
      output->filesystem_url = input.fileSystemURL;
      output->file_start = input.fileStart;
      output->file_length = input.fileLength;
      output->file_modification_time = input.modificationTime;
      break;
    case WebHTTPBody::Element::TypeBlob:
      output->blob_uuid = input.blobUUID.utf8();
      break;
  }
}

void AppendHTTPBodyElement(const ExplodedHttpBodyElement& element,
                           WebHTTPBody* http_body) {
  switch (element.type) {
    case WebHTTPBody::Element::TypeData:
      http_body->appendData(element.data);
      break;
    case WebHTTPBody::Element::TypeFile:
      http_body->appendFileRange(
          element.file_path,
          element.file_start,
          element.file_length,
          element.file_modification_time);
      break;
    case WebHTTPBody::Element::TypeFileSystemURL:
      http_body->appendFileSystemURLRange(
          element.filesystem_url,
          element.file_start,
          element.file_length,
          element.file_modification_time);
      break;
    case WebHTTPBody::Element::TypeBlob:
      http_body->appendBlob(WebString::fromUTF8(element.blob_uuid));
      break;
  }
}

void GenerateFrameStateFromItem(const WebHistoryItem& item,
                                ExplodedFrameState* state) {
  state->url_string = item.urlString();
  state->referrer = item.referrer();
  state->referrer_policy = item.referrerPolicy();
  state->target = item.target();
  if (!item.stateObject().isNull())
    state->state_object = item.stateObject().toString();
  state->pinch_viewport_scroll_offset = item.pinchViewportScrollOffset();
  state->scroll_offset = item.scrollOffset();
  state->item_sequence_number = item.itemSequenceNumber();
  state->document_sequence_number =
      item.documentSequenceNumber();
  state->page_scale_factor = item.pageScaleFactor();
  ToNullableString16Vector(item.documentState(), &state->document_state);

  state->http_body.http_content_type = item.httpContentType();
  const WebHTTPBody& http_body = item.httpBody();
  if (!(state->http_body.is_null = http_body.isNull())) {
    state->http_body.identifier = http_body.identifier();
    state->http_body.elements.resize(http_body.elementCount());
    for (size_t i = 0; i < http_body.elementCount(); ++i) {
      WebHTTPBody::Element element;
      http_body.elementAt(i, element);
      ToExplodedHttpBodyElement(element, &state->http_body.elements[i]);
    }
    state->http_body.contains_passwords = http_body.containsPasswordData();
  }
}

void RecursivelyGenerateFrameState(HistoryEntry::HistoryNode* node,
                                   ExplodedFrameState* state) {
  GenerateFrameStateFromItem(node->item(), state);

  std::vector<HistoryEntry::HistoryNode*>& children = node->children();
  state->children.resize(children.size());
  for (size_t i = 0; i < children.size(); ++i)
    RecursivelyGenerateFrameState(children[i], &state->children[i]);
}

void RecursivelyGenerateHistoryItem(const ExplodedFrameState& state,
                                    HistoryEntry::HistoryNode* node) {
  WebHistoryItem item;
  item.initialize();
  item.setURLString(state.url_string);
  item.setReferrer(state.referrer, state.referrer_policy);
  item.setTarget(state.target);
  if (!state.state_object.is_null()) {
    item.setStateObject(
        WebSerializedScriptValue::fromString(state.state_object));
  }
  item.setDocumentState(state.document_state);
  item.setPinchViewportScrollOffset(state.pinch_viewport_scroll_offset);
  item.setScrollOffset(state.scroll_offset);
  item.setPageScaleFactor(state.page_scale_factor);

  // These values are generated at WebHistoryItem construction time, and we
  // only want to override those new values with old values if the old values
  // are defined.  A value of 0 means undefined in this context.
  if (state.item_sequence_number)
    item.setItemSequenceNumber(state.item_sequence_number);
  if (state.document_sequence_number)
    item.setDocumentSequenceNumber(state.document_sequence_number);

  item.setHTTPContentType(state.http_body.http_content_type);
  if (!state.http_body.is_null) {
    WebHTTPBody http_body;
    http_body.initialize();
    http_body.setIdentifier(state.http_body.identifier);
    for (size_t i = 0; i < state.http_body.elements.size(); ++i)
      AppendHTTPBodyElement(state.http_body.elements[i], &http_body);
    item.setHTTPBody(http_body);
  }
  node->set_item(item);

  for (size_t i = 0; i < state.children.size(); ++i)
    RecursivelyGenerateHistoryItem(state.children[i], node->AddChild());
}

}  // namespace

PageState HistoryEntryToPageState(HistoryEntry* entry) {
  ExplodedPageState state;
  ToNullableString16Vector(entry->root().getReferencedFilePaths(),
                           &state.referenced_files);

  RecursivelyGenerateFrameState(entry->root_history_node(), &state.top);

  std::string encoded_data;
  if (!EncodePageState(state, &encoded_data))
    return PageState();

  return PageState::CreateFromEncodedData(encoded_data);
}

PageState SingleHistoryItemToPageState(const WebHistoryItem& item) {
  ExplodedPageState state;
  ToNullableString16Vector(item.getReferencedFilePaths(),
                           &state.referenced_files);
  GenerateFrameStateFromItem(item, &state.top);

  std::string encoded_data;
  if (!EncodePageState(state, &encoded_data))
    return PageState();

  return PageState::CreateFromEncodedData(encoded_data);
}

scoped_ptr<HistoryEntry> PageStateToHistoryEntry(const PageState& page_state) {
  ExplodedPageState state;
  if (!DecodePageState(page_state.ToEncodedData(), &state))
    return scoped_ptr<HistoryEntry>();

  scoped_ptr<HistoryEntry> entry(new HistoryEntry());
  RecursivelyGenerateHistoryItem(state.top, entry->root_history_node());

  return entry.Pass();
}

}  // namespace content
