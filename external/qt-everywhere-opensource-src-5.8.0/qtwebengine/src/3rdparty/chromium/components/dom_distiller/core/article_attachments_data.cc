// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/article_attachments_data.h"

#include "base/logging.h"

namespace dom_distiller {

syncer::AttachmentIdList GetAttachmentIds(
    const sync_pb::ArticleAttachments& attachments) {
  syncer::AttachmentIdList ids;
  ids.push_back(
      syncer::AttachmentId::CreateFromProto(attachments.distilled_article()));
  return ids;
}

std::unique_ptr<ArticleAttachmentsData>
ArticleAttachmentsData::GetFromAttachmentMap(
    const sync_pb::ArticleAttachments& attachments_key,
    const syncer::AttachmentMap& attachment_map) {
  std::unique_ptr<ArticleAttachmentsData> data(new ArticleAttachmentsData);
  syncer::AttachmentMap::const_iterator iter =
      attachment_map.find(syncer::AttachmentId::CreateFromProto(
          attachments_key.distilled_article()));
  CHECK(iter != attachment_map.end());
  scoped_refptr<base::RefCountedMemory> attachment_bytes =
      iter->second.GetData();
  data->distilled_article_.ParseFromArray(attachment_bytes->front(),
                                          attachment_bytes->size());
  return data;
}

void ArticleAttachmentsData::CreateSyncAttachments(
    syncer::AttachmentList* attachment_list,
    sync_pb::ArticleAttachments* attachments_key) const {
  CHECK(attachment_list);
  CHECK(attachments_key);
  std::string serialized_article(distilled_article_.SerializeAsString());
  syncer::Attachment attachment(
      syncer::Attachment::Create(
          base::RefCountedString::TakeString(&serialized_article)));
  *attachments_key->mutable_distilled_article() =
      attachment.GetId().GetProto();
  attachment_list->push_back(attachment);
}

std::string ArticleAttachmentsData::ToString() const {
  return distilled_article_.SerializeAsString();
}

}  // namespace dom_distiller
