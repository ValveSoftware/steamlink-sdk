// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/spdy/hpack_huffman_aggregator.h"

#include "base/metrics/bucket_ranges.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sample_vector.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/spdy/hpack_encoder.h"
#include "net/spdy/spdy_http_utils.h"

namespace net {

namespace {

const char kHistogramName[] = "Net.SpdyHpackEncodedCharacterFrequency";

const size_t kTotalCountsPublishThreshold = 50000;

// Each encoder uses the default dynamic table size of 4096 total bytes.
const size_t kMaxEncoders = 20;

}  // namespace

HpackHuffmanAggregator::HpackHuffmanAggregator()
  : counts_(256, 0),
    total_counts_(0),
    max_encoders_(kMaxEncoders) {
}

HpackHuffmanAggregator::~HpackHuffmanAggregator() {
  STLDeleteContainerPairSecondPointers(encoders_.begin(), encoders_.end());
  encoders_.clear();
}

void HpackHuffmanAggregator::AggregateTransactionCharacterCounts(
    const HttpRequestInfo& request,
    const HttpRequestHeaders& request_headers,
    const ProxyServer& proxy,
    const HttpResponseHeaders& response_headers) {
  if (IsCrossOrigin(request)) {
    return;
  }
  HostPortPair endpoint = HostPortPair(request.url.HostNoBrackets(),
                                       request.url.EffectiveIntPort());
  HpackEncoder* encoder = ObtainEncoder(
      SpdySessionKey(endpoint, proxy, request.privacy_mode));

  // Convert and encode the request and response header sets.
  {
    SpdyHeaderBlock headers;
    CreateSpdyHeadersFromHttpRequest(
        request, request_headers, &headers, SPDY4, false);

    std::string tmp_out;
    encoder->EncodeHeaderSet(headers, &tmp_out);
  }
  {
    SpdyHeaderBlock headers;
    CreateSpdyHeadersFromHttpResponse(response_headers, &headers);

    std::string tmp_out;
    encoder->EncodeHeaderSet(headers, &tmp_out);
  }
  if (total_counts_ >= kTotalCountsPublishThreshold) {
    PublishCounts();
  }
}

// static
bool HpackHuffmanAggregator::UseAggregator() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("HpackHuffmanAggregator");
  if (group_name == "Enabled") {
    return true;
  }
  return false;
}

// static
void HpackHuffmanAggregator::CreateSpdyHeadersFromHttpResponse(
    const HttpResponseHeaders& headers,
    SpdyHeaderBlock* headers_out) {
  // Lower-case header names, and coalesce multiple values delimited by \0.
  // Also add the fixed status header.
  std::string name, value;
  void* it = NULL;
  while (headers.EnumerateHeaderLines(&it, &name, &value)) {
    StringToLowerASCII(&name);
    if (headers_out->find(name) == headers_out->end()) {
      (*headers_out)[name] = value;
    } else {
      (*headers_out)[name] += std::string(1, '\0') + value;
    }
  }
  (*headers_out)[":status"] = base::IntToString(headers.response_code());
}

// static
bool HpackHuffmanAggregator::IsCrossOrigin(const HttpRequestInfo& request) {
  // Require that the request is top-level, or that it shares
  // an origin with its referer.
  HostPortPair endpoint = HostPortPair(request.url.HostNoBrackets(),
                                       request.url.EffectiveIntPort());
  if ((request.load_flags & LOAD_MAIN_FRAME) == 0) {
    std::string referer_str;
    if (!request.extra_headers.GetHeader(HttpRequestHeaders::kReferer,
                                         &referer_str)) {
      // Require a referer.
      return true;
    }
    GURL referer(referer_str);
    HostPortPair referer_endpoint = HostPortPair(referer.HostNoBrackets(),
                                                 referer.EffectiveIntPort());
    if (!endpoint.Equals(referer_endpoint)) {
      // Cross-origin request.
      return true;
    }
  }
  return false;
}

HpackEncoder* HpackHuffmanAggregator::ObtainEncoder(const SpdySessionKey& key) {
  for (OriginEncoders::iterator it = encoders_.begin();
       it != encoders_.end(); ++it) {
    if (key.Equals(it->first)) {
      // Move to head of list and return.
      OriginEncoder origin_encoder = *it;
      encoders_.erase(it);
      encoders_.push_front(origin_encoder);
      return origin_encoder.second;
    }
  }
  // Not found. Create a new encoder, evicting one if needed.
  encoders_.push_front(std::make_pair(
      key, new HpackEncoder(ObtainHpackHuffmanTable())));
  if (encoders_.size() > max_encoders_) {
    delete encoders_.back().second;
    encoders_.pop_back();
  }
  encoders_.front().second->SetCharCountsStorage(&counts_, &total_counts_);
  return encoders_.front().second;
}

void HpackHuffmanAggregator::PublishCounts() {
  // base::Histogram requires that values be 1-indexed.
  const size_t kRangeMin = 1;
  const size_t kRangeMax = counts_.size() + 1;
  const size_t kBucketCount = kRangeMax + 1;

  base::BucketRanges ranges(kBucketCount + 1);
  for (size_t i = 0; i != ranges.size(); ++i) {
    ranges.set_range(i, i);
  }
  ranges.ResetChecksum();

  // Copy |counts_| into a SampleVector.
  base::SampleVector samples(&ranges);
  for (size_t i = 0; i != counts_.size(); ++i) {
    samples.Accumulate(i + 1, counts_[i]);
  }

  STATIC_HISTOGRAM_POINTER_BLOCK(
      kHistogramName,
      AddSamples(samples),
      base::LinearHistogram::FactoryGet(
          kHistogramName, kRangeMin, kRangeMax, kBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  // Clear counts.
  counts_.assign(counts_.size(), 0);
  total_counts_ = 0;
}

}  // namespace net
