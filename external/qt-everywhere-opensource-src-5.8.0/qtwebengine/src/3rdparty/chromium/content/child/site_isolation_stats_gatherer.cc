// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/site_isolation_stats_gatherer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/public/common/resource_response_info.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {

// The gathering of UMA stats for site isolation is deactivated by default, and
// only activated in renderer processes.
static bool g_stats_gathering_enabled = false;

bool IsRenderableStatusCode(int status_code) {
  // Chrome only uses the content of a response with one of these status codes
  // for CSS/JavaScript. For images, Chrome just ignores status code.
  const int renderable_status_code[] = {
      200, 201, 202, 203, 206, 300, 301, 302, 303, 305, 306, 307};
  for (size_t i = 0; i < arraysize(renderable_status_code); ++i) {
    if (renderable_status_code[i] == status_code)
      return true;
  }
  return false;
}

void IncrementHistogramCount(const std::string& name) {
  // The default value of min, max, bucket_count are copied from histogram.h.
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryGet(
      name, 1, 100000, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(1);
}

void IncrementHistogramEnum(const std::string& name,
                            uint32_t sample,
                            uint32_t boundary_value) {
  // The default value of min, max, bucket_count are copied from histogram.h.
  base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
      name, 1, boundary_value, boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(sample);
}

void HistogramCountBlockedResponse(
    const std::string& bucket_prefix,
    const std::unique_ptr<SiteIsolationResponseMetaData>& resp_data,
    bool nosniff_block) {
  std::string block_label(nosniff_block ? ".NoSniffBlocked" : ".Blocked");
  IncrementHistogramCount(bucket_prefix + block_label);

  // The content is blocked if it is sniffed as HTML/JSON/XML. When
  // the blocked response is with an error status code, it is not
  // disruptive for the following reasons : 1) the blocked content is
  // not a binary object (such as an image) since it is sniffed as
  // text; 2) then, this blocking only breaks the renderer behavior
  // only if it is either JavaScript or CSS. However, the renderer
  // doesn't use the contents of JS/CSS with unaffected status code
  // (e.g, 404). 3) the renderer is expected not to use the cross-site
  // document content for purposes other than JS/CSS (e.g, XHR).
  bool renderable_status_code =
      IsRenderableStatusCode(resp_data->http_status_code);

  if (renderable_status_code) {
    IncrementHistogramEnum(
        bucket_prefix + block_label + ".RenderableStatusCode2",
        resp_data->resource_type, RESOURCE_TYPE_LAST_TYPE);
  } else {
    IncrementHistogramCount(bucket_prefix + block_label +
                            ".NonRenderableStatusCode");
  }
}

void HistogramCountNotBlockedResponse(const std::string& bucket_prefix,
                                      bool sniffed_as_js) {
  IncrementHistogramCount(bucket_prefix + ".NotBlocked");
  if (sniffed_as_js)
    IncrementHistogramCount(bucket_prefix + ".NotBlocked.MaybeJS");
}

}  // namespace

SiteIsolationResponseMetaData::SiteIsolationResponseMetaData() {
}

void SiteIsolationStatsGatherer::SetEnabled(bool enabled) {
  g_stats_gathering_enabled = enabled;
}

std::unique_ptr<SiteIsolationResponseMetaData>
SiteIsolationStatsGatherer::OnReceivedResponse(
    const GURL& frame_origin,
    const GURL& response_url,
    ResourceType resource_type,
    int origin_pid,
    const ResourceResponseInfo& info) {
  if (!g_stats_gathering_enabled)
    return nullptr;

  // if |origin_pid| is non-zero, it means that this response is for a plugin
  // spawned from this renderer process. We exclude responses for plugins for
  // now, but eventually, we're going to make plugin processes directly talk to
  // the browser process so that we don't apply cross-site document blocking to
  // them.
  if (origin_pid)
    return nullptr;

  UMA_HISTOGRAM_COUNTS("SiteIsolation.AllResponses", 1);

  // See if this is for navigation. If it is, don't block it, under the
  // assumption that we will put it in an appropriate process.
  if (IsResourceTypeFrame(resource_type))
    return nullptr;

  if (!CrossSiteDocumentClassifier::IsBlockableScheme(response_url))
    return nullptr;

  if (CrossSiteDocumentClassifier::IsSameSite(frame_origin, response_url))
    return nullptr;

  CrossSiteDocumentMimeType canonical_mime_type =
      CrossSiteDocumentClassifier::GetCanonicalMimeType(info.mime_type);

  if (canonical_mime_type == CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS)
    return nullptr;

  // Every CORS request should have the Access-Control-Allow-Origin header even
  // if it is preceded by a pre-flight request. Therefore, if this is a CORS
  // request, it has this header.  response.httpHeaderField() internally uses
  // case-insensitive matching for the header name.
  std::string access_control_origin;

  // We can use a case-insensitive header name for EnumerateHeader().
  info.headers->EnumerateHeader(NULL, "access-control-allow-origin",
                                &access_control_origin);
  if (CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
          frame_origin, response_url, access_control_origin))
    return nullptr;

  // Real XSD data collection starts from here.
  std::string no_sniff;
  info.headers->EnumerateHeader(NULL, "x-content-type-options", &no_sniff);

  std::unique_ptr<SiteIsolationResponseMetaData> resp_data(
      new SiteIsolationResponseMetaData);
  resp_data->frame_origin = frame_origin.spec();
  resp_data->response_url = response_url;
  resp_data->resource_type = resource_type;
  resp_data->canonical_mime_type = canonical_mime_type;
  resp_data->http_status_code = info.headers->response_code();
  resp_data->no_sniff = base::LowerCaseEqualsASCII(no_sniff, "nosniff");

  return resp_data;
}

bool SiteIsolationStatsGatherer::OnReceivedFirstChunk(
    const std::unique_ptr<SiteIsolationResponseMetaData>& resp_data,
    const char* raw_data,
    int raw_length) {
  if (!g_stats_gathering_enabled)
    return false;

  DCHECK(resp_data.get());

  base::StringPiece data(raw_data, raw_length);

  // Record the length of the first received chunk of data to see if it's enough
  // for sniffing.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.XSD.DataLength", raw_length);

  // Record the number of cross-site document responses with a specific mime
  // type (text/html, text/xml, etc).
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.MimeType",
                            resp_data->canonical_mime_type,
                            CROSS_SITE_DOCUMENT_MIME_TYPE_MAX);

  // Store the result of cross-site document blocking analysis.
  bool would_block = false;
  bool sniffed_as_js = SniffForJS(data);

  // Record the number of responses whose content is sniffed for what its mime
  // type claims it to be. For example, we apply a HTML sniffer for a document
  // tagged with text/html here. Whenever this check becomes true, we'll block
  // the response.
  if (resp_data->canonical_mime_type != CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN) {
    std::string bucket_prefix;
    bool sniffed_as_target_document = false;
    if (resp_data->canonical_mime_type == CROSS_SITE_DOCUMENT_MIME_TYPE_HTML) {
      bucket_prefix = "SiteIsolation.XSD.HTML";
      sniffed_as_target_document =
          CrossSiteDocumentClassifier::SniffForHTML(data);
    } else if (resp_data->canonical_mime_type ==
               CROSS_SITE_DOCUMENT_MIME_TYPE_XML) {
      bucket_prefix = "SiteIsolation.XSD.XML";
      sniffed_as_target_document =
          CrossSiteDocumentClassifier::SniffForXML(data);
    } else if (resp_data->canonical_mime_type ==
               CROSS_SITE_DOCUMENT_MIME_TYPE_JSON) {
      bucket_prefix = "SiteIsolation.XSD.JSON";
      sniffed_as_target_document =
          CrossSiteDocumentClassifier::SniffForJSON(data);
    } else {
      NOTREACHED() << "Not a blockable mime type: "
                   << resp_data->canonical_mime_type;
    }

    if (sniffed_as_target_document) {
      would_block = true;
      HistogramCountBlockedResponse(bucket_prefix, resp_data, false);
    } else {
      if (resp_data->no_sniff) {
        would_block = true;
        HistogramCountBlockedResponse(bucket_prefix, resp_data, true);
      } else {
        HistogramCountNotBlockedResponse(bucket_prefix, sniffed_as_js);
      }
    }
  } else {
    // This block is for plain text documents. We apply our HTML, XML,
    // and JSON sniffer to a text document in the order, and block it
    // if any of them succeeds in sniffing.
    std::string bucket_prefix;
    if (CrossSiteDocumentClassifier::SniffForHTML(data))
      bucket_prefix = "SiteIsolation.XSD.Plain.HTML";
    else if (CrossSiteDocumentClassifier::SniffForXML(data))
      bucket_prefix = "SiteIsolation.XSD.Plain.XML";
    else if (CrossSiteDocumentClassifier::SniffForJSON(data))
      bucket_prefix = "SiteIsolation.XSD.Plain.JSON";

    if (bucket_prefix.size() > 0) {
      would_block = true;
      HistogramCountBlockedResponse(bucket_prefix, resp_data, false);
    } else if (resp_data->no_sniff) {
      would_block = true;
      HistogramCountBlockedResponse("SiteIsolation.XSD.Plain", resp_data, true);
    } else {
      HistogramCountNotBlockedResponse("SiteIsolation.XSD.Plain",
                                       sniffed_as_js);
    }
  }

  return would_block;
}

bool SiteIsolationStatsGatherer::SniffForJS(base::StringPiece data) {
  // The purpose of this function is to try to see if there's any possibility
  // that this data can be JavaScript (superset of JS). Search for "var " for JS
  // detection. This is a real hack and should only be used for stats gathering.
  return data.find("var ") != base::StringPiece::npos;
}

}  // namespace content
