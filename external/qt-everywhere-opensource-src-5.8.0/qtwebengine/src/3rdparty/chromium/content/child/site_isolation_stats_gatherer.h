// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SITE_ISOLATION_STATS_GATHERER_H_
#define CONTENT_CHILD_SITE_ISOLATION_STATS_GATHERER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/common/cross_site_document_classifier.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace content {

struct ResourceResponseInfo;

// SiteIsolationStatsGatherer monitors responses to gather various UMA stats to
// see the compatibility impact of actual deployment of the policy. The UMA stat
// categories SiteIsolationStatsGatherer gathers are as follows:
//
// SiteIsolation.AllResponses : # of all network responses.
// SiteIsolation.XSD.DataLength : the length of the first packet of a response.
// SiteIsolation.XSD.MimeType (enum):
//   # of responses from other sites, tagged with a document mime type.
//   0:HTML, 1:XML, 2:JSON, 3:Plain, 4:Others
// SiteIsolation.XSD.[%MIMETYPE].Blocked :
//   blocked # of cross-site document responses grouped by sniffed MIME type.
// SiteIsolation.XSD.[%MIMETYPE].Blocked.RenderableStatusCode2 :
//   # of responses with renderable status code,
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].Blocked.NonRenderableStatusCode :
//   # of responses with non-renderable status code,
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NoSniffBlocked.RenderableStatusCode2 :
//   # of responses failed to be sniffed for its MIME type, but blocked by
//   "X-Content-Type-Options: nosniff" header, and with renderable status code
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NoSniffBlocked.NonRenderableStatusCode :
//   # of responses failed to be sniffed for its MIME type, but blocked by
//   "X-Content-Type-Options: nosniff" header, and with non-renderable status
//   code out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NotBlocked :
//   # of responses, but not blocked due to failure of mime sniffing.
// SiteIsolation.XSD.[%MIMETYPE].NotBlocked.MaybeJS :
//   # of responses that are plausibly sniffed to be JavaScript.

struct SiteIsolationResponseMetaData {
  SiteIsolationResponseMetaData();

  std::string frame_origin;
  GURL response_url;
  ResourceType resource_type;
  CrossSiteDocumentMimeType canonical_mime_type;
  int http_status_code;
  bool no_sniff;
};

class CONTENT_EXPORT SiteIsolationStatsGatherer {
 public:
  // Set activation flag for the UMA data collection for this renderer process.
  static void SetEnabled(bool enabled);

  // Returns any bookkeeping data about the HTTP header information for the
  // request identified by |request_id|. Any data returned should then be
  // passed to OnReceivedFirstChunk() with the first data chunk.
  static std::unique_ptr<SiteIsolationResponseMetaData> OnReceivedResponse(
      const GURL& frame_origin,
      const GURL& response_url,
      ResourceType resource_type,
      int origin_pid,
      const ResourceResponseInfo& info);

  // Examines the first chunk of network data in case response_url is registered
  // as a cross-site document by OnReceivedResponse(). This records various
  // kinds of UMA data stats. This function is called only if the length of
  // received data is non-zero.
  static bool OnReceivedFirstChunk(
      const std::unique_ptr<SiteIsolationResponseMetaData>& resp_data,
      const char* payload,
      int length);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationStatsGathererTest, SniffForJS);

  SiteIsolationStatsGatherer();  // Not instantiable.

  // Imprecise JS sniffing; only appropriate for collecting UMA stat.
  static bool SniffForJS(base::StringPiece data);

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationStatsGatherer);
};

}  // namespace content

#endif  // CONTENT_CHILD_SITE_ISOLATION_STATS_GATHERER_H_
