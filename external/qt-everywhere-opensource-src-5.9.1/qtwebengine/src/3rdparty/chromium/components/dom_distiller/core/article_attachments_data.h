// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ATTACHMENTS_DATA_H_
#define COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ATTACHMENTS_DATA_H_

#include <memory>
#include <string>

#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/protocol/article_specifics.pb.h"

namespace dom_distiller {

// When stored, article attachments are split into two pieces, the actual data
// is stored by a set of ids in some attachment storage. The mapping of what
// each id corresponds to is stored in the ArticleEntry's
// sync_pb::ArticleAttachments.  This class handles splitting into those two
// pieces (::CreateSyncAttachments) and reconstructing the data from the two
// pieces (::GetFromAttachmentMap and ::GetAttachmentIds). Outside of this
// class, the structure of sync_pb::ArticleAttachments (the id mapping) should
// be rather opaque.
class ArticleAttachmentsData {
 public:
  static std::unique_ptr<ArticleAttachmentsData> GetFromAttachmentMap(
      const sync_pb::ArticleAttachments& attachments_key,
      const syncer::AttachmentMap& attachment_map);

  // Creates sync attachments and the sync_pb::ArticleAttachments id map.
  void CreateSyncAttachments(
      syncer::AttachmentList* attachment_list,
      sync_pb::ArticleAttachments* attachments_key) const;

  const DistilledArticleProto& distilled_article() const {
    return distilled_article_;
  }
  void set_distilled_article(const DistilledArticleProto& article) {
    distilled_article_ = article;
  }

  std::string ToString() const;
 private:
  DistilledArticleProto distilled_article_;
};

syncer::AttachmentIdList GetAttachmentIds(
    const sync_pb::ArticleAttachments& attachments);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ATTACHMENTS_DATA_H_
