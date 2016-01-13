// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/site_isolation_policy.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"

using base::StringPiece;

namespace content {

namespace {

// The cross-site document blocking/UMA data collection is deactivated by
// default, and only activated in renderer processes.
static bool g_policy_enabled = false;

// MIME types
const char kTextHtml[] = "text/html";
const char kTextXml[] = "text/xml";
const char xAppRssXml[] = "application/rss+xml";
const char kAppXml[] = "application/xml";
const char kAppJson[] = "application/json";
const char kTextJson[] = "text/json";
const char kTextXjson[] = "text/x-json";
const char kTextPlain[] = "text/plain";

// TODO(dsjang): this is only needed for collecting UMA stat. Will be deleted
// when this class is used for actual blocking.
bool IsRenderableStatusCode(int status_code) {
  // Chrome only uses the content of a response with one of these status codes
  // for CSS/JavaScript. For images, Chrome just ignores status code.
  const int renderable_status_code[] = {200, 201, 202, 203, 206, 300,
                                        301, 302, 303, 305, 306, 307};
  for (size_t i = 0; i < arraysize(renderable_status_code); ++i) {
    if (renderable_status_code[i] == status_code)
      return true;
  }
  return false;
}

bool MatchesSignature(StringPiece data,
                      const StringPiece signatures[],
                      size_t arr_size) {

  size_t offset = data.find_first_not_of(" \t\r\n");
  // There is no not-whitespace character in this document.
  if (offset == base::StringPiece::npos)
    return false;

  data.remove_prefix(offset);
  size_t length = data.length();

  for (size_t sig_index = 0; sig_index < arr_size; ++sig_index) {
    const StringPiece& signature = signatures[sig_index];
    size_t signature_length = signature.length();
    if (length < signature_length)
      continue;

    if (LowerCaseEqualsASCII(
            data.begin(), data.begin() + signature_length, signature.data()))
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
                          uint32 sample,
                          uint32 boundary_value) {
  // The default value of min, max, bucket_count are copied from histogram.h.
  base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
      name,
      1,
      boundary_value,
      boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(sample);
}

void HistogramCountBlockedResponse(
    const std::string& bucket_prefix,
    linked_ptr<SiteIsolationResponseMetaData>& resp_data,
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
        bucket_prefix + block_label + ".RenderableStatusCode",
        resp_data->resource_type,
        ResourceType::LAST_TYPE);
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

SiteIsolationResponseMetaData::SiteIsolationResponseMetaData() {}

void SiteIsolationPolicy::SetPolicyEnabled(bool enabled) {
  g_policy_enabled = enabled;
}

linked_ptr<SiteIsolationResponseMetaData>
SiteIsolationPolicy::OnReceivedResponse(const GURL& frame_origin,
                                        const GURL& response_url,
                                        ResourceType::Type resource_type,
                                        int origin_pid,
                                        const ResourceResponseInfo& info) {
  if (!g_policy_enabled)
    return linked_ptr<SiteIsolationResponseMetaData>();

  // if |origin_pid| is non-zero, it means that this response is for a plugin
  // spawned from this renderer process. We exclude responses for plugins for
  // now, but eventually, we're going to make plugin processes directly talk to
  // the browser process so that we don't apply cross-site document blocking to
  // them.
  if (origin_pid)
    return linked_ptr<SiteIsolationResponseMetaData>();

  UMA_HISTOGRAM_COUNTS("SiteIsolation.AllResponses", 1);

  // See if this is for navigation. If it is, don't block it, under the
  // assumption that we will put it in an appropriate process.
  if (ResourceType::IsFrame(resource_type))
    return linked_ptr<SiteIsolationResponseMetaData>();

  if (!IsBlockableScheme(response_url))
    return linked_ptr<SiteIsolationResponseMetaData>();

  if (IsSameSite(frame_origin, response_url))
    return linked_ptr<SiteIsolationResponseMetaData>();

  SiteIsolationResponseMetaData::CanonicalMimeType canonical_mime_type =
      GetCanonicalMimeType(info.mime_type);

  if (canonical_mime_type == SiteIsolationResponseMetaData::Others)
    return linked_ptr<SiteIsolationResponseMetaData>();

  // Every CORS request should have the Access-Control-Allow-Origin header even
  // if it is preceded by a pre-flight request. Therefore, if this is a CORS
  // request, it has this header.  response.httpHeaderField() internally uses
  // case-insensitive matching for the header name.
  std::string access_control_origin;

  // We can use a case-insensitive header name for EnumerateHeader().
  info.headers->EnumerateHeader(
      NULL, "access-control-allow-origin", &access_control_origin);
  if (IsValidCorsHeaderSet(frame_origin, response_url, access_control_origin))
    return linked_ptr<SiteIsolationResponseMetaData>();

  // Real XSD data collection starts from here.
  std::string no_sniff;
  info.headers->EnumerateHeader(NULL, "x-content-type-options", &no_sniff);

  linked_ptr<SiteIsolationResponseMetaData> resp_data(
      new SiteIsolationResponseMetaData);
  resp_data->frame_origin = frame_origin.spec();
  resp_data->response_url = response_url;
  resp_data->resource_type = resource_type;
  resp_data->canonical_mime_type = canonical_mime_type;
  resp_data->http_status_code = info.headers->response_code();
  resp_data->no_sniff = LowerCaseEqualsASCII(no_sniff, "nosniff");

  return resp_data;
}

bool SiteIsolationPolicy::ShouldBlockResponse(
    linked_ptr<SiteIsolationResponseMetaData>& resp_data,
    const char* raw_data,
    int raw_length,
    std::string* alternative_data) {
  if (!g_policy_enabled)
    return false;

  DCHECK(resp_data.get());

  StringPiece data(raw_data, raw_length);

  // Record the length of the first received network packet to see if it's
  // enough for sniffing.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.XSD.DataLength", raw_length);

  // Record the number of cross-site document responses with a specific mime
  // type (text/html, text/xml, etc).
  UMA_HISTOGRAM_ENUMERATION(
      "SiteIsolation.XSD.MimeType",
      resp_data->canonical_mime_type,
      SiteIsolationResponseMetaData::MaxCanonicalMimeType);

  // Store the result of cross-site document blocking analysis.
  bool is_blocked = false;
  bool sniffed_as_js = SniffForJS(data);

  // Record the number of responses whose content is sniffed for what its mime
  // type claims it to be. For example, we apply a HTML sniffer for a document
  // tagged with text/html here. Whenever this check becomes true, we'll block
  // the response.
  if (resp_data->canonical_mime_type !=
          SiteIsolationResponseMetaData::Plain) {
    std::string bucket_prefix;
    bool sniffed_as_target_document = false;
    if (resp_data->canonical_mime_type ==
            SiteIsolationResponseMetaData::HTML) {
      bucket_prefix = "SiteIsolation.XSD.HTML";
      sniffed_as_target_document = SniffForHTML(data);
    } else if (resp_data->canonical_mime_type ==
                   SiteIsolationResponseMetaData::XML) {
      bucket_prefix = "SiteIsolation.XSD.XML";
      sniffed_as_target_document = SniffForXML(data);
    } else if (resp_data->canonical_mime_type ==
                   SiteIsolationResponseMetaData::JSON) {
      bucket_prefix = "SiteIsolation.XSD.JSON";
      sniffed_as_target_document = SniffForJSON(data);
    } else {
      NOTREACHED() << "Not a blockable mime type: "
                   << resp_data->canonical_mime_type;
    }

    if (sniffed_as_target_document) {
      is_blocked = true;
      HistogramCountBlockedResponse(bucket_prefix, resp_data, false);
    } else {
      if (resp_data->no_sniff) {
        is_blocked = true;
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
    if (SniffForHTML(data))
      bucket_prefix = "SiteIsolation.XSD.Plain.HTML";
    else if (SniffForXML(data))
      bucket_prefix = "SiteIsolation.XSD.Plain.XML";
    else if (SniffForJSON(data))
      bucket_prefix = "SiteIsolation.XSD.Plain.JSON";

    if (bucket_prefix.size() > 0) {
      is_blocked = true;
      HistogramCountBlockedResponse(bucket_prefix, resp_data, false);
    } else if (resp_data->no_sniff) {
      is_blocked = true;
      HistogramCountBlockedResponse("SiteIsolation.XSD.Plain", resp_data, true);
    } else {
      HistogramCountNotBlockedResponse("SiteIsolation.XSD.Plain",
                                       sniffed_as_js);
    }
  }

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kBlockCrossSiteDocuments))
    is_blocked = false;

  if (is_blocked) {
    alternative_data->erase();
    alternative_data->insert(0, " ");
    LOG(ERROR) << resp_data->response_url
               << " is blocked as an illegal cross-site document from "
               << resp_data->frame_origin;
  }
  return is_blocked;
}

SiteIsolationResponseMetaData::CanonicalMimeType
SiteIsolationPolicy::GetCanonicalMimeType(const std::string& mime_type) {
  if (LowerCaseEqualsASCII(mime_type, kTextHtml)) {
    return SiteIsolationResponseMetaData::HTML;
  }

  if (LowerCaseEqualsASCII(mime_type, kTextPlain)) {
    return SiteIsolationResponseMetaData::Plain;
  }

  if (LowerCaseEqualsASCII(mime_type, kAppJson) ||
      LowerCaseEqualsASCII(mime_type, kTextJson) ||
      LowerCaseEqualsASCII(mime_type, kTextXjson)) {
    return SiteIsolationResponseMetaData::JSON;
  }

  if (LowerCaseEqualsASCII(mime_type, kTextXml) ||
      LowerCaseEqualsASCII(mime_type, xAppRssXml) ||
      LowerCaseEqualsASCII(mime_type, kAppXml)) {
    return SiteIsolationResponseMetaData::XML;
  }

 return SiteIsolationResponseMetaData::Others;
}

bool SiteIsolationPolicy::IsBlockableScheme(const GURL& url) {
  // We exclude ftp:// from here. FTP doesn't provide a Content-Type
  // header which our policy depends on, so we cannot protect any
  // document from FTP servers.
  return url.SchemeIs("http") || url.SchemeIs("https");
}

bool SiteIsolationPolicy::IsSameSite(const GURL& frame_origin,
                                     const GURL& response_url) {

  if (!frame_origin.is_valid() || !response_url.is_valid())
    return false;

  if (frame_origin.scheme() != response_url.scheme())
    return false;

  // SameDomainOrHost() extracts the effective domains (public suffix plus one)
  // from the two URLs and compare them.
  return net::registry_controlled_domains::SameDomainOrHost(
      frame_origin,
      response_url,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// We don't use Webkit's existing CORS policy implementation since
// their policy works in terms of origins, not sites. For example,
// when frame is sub.a.com and it is not allowed to access a document
// with sub1.a.com. But under Site Isolation, it's allowed.
bool SiteIsolationPolicy::IsValidCorsHeaderSet(
    const GURL& frame_origin,
    const GURL& website_origin,
    const std::string& access_control_origin) {
  // Many websites are sending back "\"*\"" instead of "*". This is
  // non-standard practice, and not supported by Chrome. Refer to
  // CrossOriginAccessControl::passesAccessControlCheck().

  // TODO(dsjang): * is not allowed for the response from a request
  // with cookies. This allows for more than what the renderer will
  // eventually be able to receive, so we won't see illegal cross-site
  // documents allowed by this. We have to find a way to see if this
  // response is from a cookie-tagged request or not in the future.
  if (access_control_origin == "*")
    return true;

  // TODO(dsjang): The CORS spec only treats a fully specified URL, except for
  // "*", but many websites are using just a domain for access_control_origin,
  // and this is blocked by Webkit's CORS logic here :
  // CrossOriginAccessControl::passesAccessControlCheck(). GURL is set
  // is_valid() to false when it is created from a URL containing * in the
  // domain part.

  GURL cors_origin(access_control_origin);
  return IsSameSite(frame_origin, cors_origin);
}

// This function is a slight modification of |net::SniffForHTML|.
bool SiteIsolationPolicy::SniffForHTML(StringPiece data) {
  // The content sniffer used by Chrome and Firefox are using "<!--"
  // as one of the HTML signatures, but it also appears in valid
  // JavaScript, considered as well-formed JS by the browser.  Since
  // we do not want to block any JS, we exclude it from our HTML
  // signatures. This can weaken our document block policy, but we can
  // break less websites.
  // TODO(dsjang): parameterize |net::SniffForHTML| with an option
  // that decides whether to include <!-- or not, so that we can
  // remove this function.
  // TODO(dsjang): Once SiteIsolationPolicy is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  static const StringPiece kHtmlSignatures[] = {
    StringPiece("<!DOCTYPE html"),  // HTML5 spec
    StringPiece("<script"),  // HTML5 spec, Mozilla
    StringPiece("<html"),    // HTML5 spec, Mozilla
    StringPiece("<head"),    // HTML5 spec, Mozilla
    StringPiece("<iframe"),  // Mozilla
    StringPiece("<h1"),      // Mozilla
    StringPiece("<div"),     // Mozilla
    StringPiece("<font"),    // Mozilla
    StringPiece("<table"),   // Mozilla
    StringPiece("<a"),       // Mozilla
    StringPiece("<style"),   // Mozilla
    StringPiece("<title"),   // Mozilla
    StringPiece("<b"),       // Mozilla
    StringPiece("<body"),    // Mozilla
    StringPiece("<br"),      // Mozilla
    StringPiece("<p"),       // Mozilla
    StringPiece("<?xml")     // Mozilla
  };

  while (data.length() > 0) {
    if (MatchesSignature(
          data, kHtmlSignatures, arraysize(kHtmlSignatures)))
      return true;

    // If we cannot find "<!--", we fail sniffing this as HTML.
    static const StringPiece kCommentBegins[] = { StringPiece("<!--") };
    if (!MatchesSignature(data, kCommentBegins, arraysize(kCommentBegins)))
      break;

    // Search for --> and do SniffForHTML after that. If we can find the
    // comment's end, we start HTML sniffing from there again.
    static const char kEndComment[] = "-->";
    size_t offset = data.find(kEndComment);
    if (offset == base::StringPiece::npos)
      break;

    // Proceed to the index next to the ending comment (-->).
    data.remove_prefix(offset + strlen(kEndComment));
  }

  return false;
}

bool SiteIsolationPolicy::SniffForXML(base::StringPiece data) {
  // TODO(dsjang): Chrome's mime_sniffer is using strncasecmp() for
  // this signature. However, XML is case-sensitive. Don't we have to
  // be more lenient only to block documents starting with the exact
  // string <?xml rather than <?XML ?
  // TODO(dsjang): Once SiteIsolationPolicy is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  static const StringPiece kXmlSignatures[] = { StringPiece("<?xml") };
  return MatchesSignature(data, kXmlSignatures, arraysize(kXmlSignatures));
}

bool SiteIsolationPolicy::SniffForJSON(base::StringPiece data) {
  // TODO(dsjang): We have to come up with a better way to sniff
  // JSON. However, even RE cannot help us that much due to the fact
  // that we don't do full parsing.  This DFA starts with state 0, and
  // finds {, "/' and : in that order. We're avoiding adding a
  // dependency on a regular expression library.
  enum {
    kStartState,
    kLeftBraceState,
    kLeftQuoteState,
    kColonState,
    kTerminalState,
  } state = kStartState;

  size_t length = data.length();
  for (size_t i = 0; i < length && state < kColonState; ++i) {
    const char c = data[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      continue;

    switch (state) {
      case kStartState:
        if (c == '{')
          state = kLeftBraceState;
        else
          state = kTerminalState;
        break;
      case kLeftBraceState:
        if (c == '\"' || c == '\'')
          state = kLeftQuoteState;
        else
          state = kTerminalState;
        break;
      case kLeftQuoteState:
        if (c == ':')
          state = kColonState;
        break;
      case kColonState:
      case kTerminalState:
        NOTREACHED();
        break;
    }
  }
  return state == kColonState;
}

bool SiteIsolationPolicy::SniffForJS(StringPiece data) {
  // TODO(dsjang): This is a real hack. The only purpose of this function is to
  // try to see if there's any possibility that this data can be JavaScript
  // (superset of JS). This function will be removed once UMA stats are
  // gathered.

  // Search for "var " for JS detection.
  return data.find("var ") != base::StringPiece::npos;
}

}  // namespace content
