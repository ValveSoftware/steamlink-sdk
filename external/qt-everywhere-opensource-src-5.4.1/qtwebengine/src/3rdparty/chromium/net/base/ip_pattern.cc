// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_pattern.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"

namespace net {

class IPPattern::ComponentPattern {
 public:
  ComponentPattern();
  void AppendRange(uint32 min, uint32 max);
  bool Match(uint32 value) const;

 private:
  struct Range {
   public:
    Range(uint32 min, uint32 max) : minimum(min), maximum(max) {}
    uint32 minimum;
    uint32 maximum;
  };
  typedef std::vector<Range> RangeVector;

  RangeVector ranges_;

  DISALLOW_COPY_AND_ASSIGN(ComponentPattern);
};

IPPattern::ComponentPattern::ComponentPattern() {}

void IPPattern::ComponentPattern::AppendRange(uint32 min, uint32 max) {
  ranges_.push_back(Range(min, max));
}

bool IPPattern::ComponentPattern::Match(uint32 value) const {
  // Simple linear search should be fine, as we usually only have very few
  // distinct ranges to test.
  for (RangeVector::const_iterator range_it = ranges_.begin();
       range_it != ranges_.end(); ++range_it) {
    if (range_it->maximum >= value && range_it->minimum <= value)
      return true;
  }
  return false;
}

IPPattern::IPPattern() : is_ipv4_(true) {}

IPPattern::~IPPattern() {
  STLDeleteElements(&component_patterns_);
}

bool IPPattern::Match(const IPAddressNumber& address) const {
  if (ip_mask_.empty())
    return false;
  bool address_is_ipv4 = address.size() == kIPv4AddressSize;
  if (address_is_ipv4 != is_ipv4_)
    return false;

  ComponentPatternList::const_iterator pattern_it(component_patterns_.begin());
  int fixed_value_index = 0;
  // IPv6 |address| vectors have 16 pieces, while our  |ip_mask_| has only
  // 8, so it is easier to count separately.
  int address_index = 0;
  for (size_t i = 0; i < ip_mask_.size(); ++i) {
    uint32 value_to_test = address[address_index++];
    if (!is_ipv4_) {
      value_to_test = (value_to_test << 8) + address[address_index++];
    }
    if (ip_mask_[i]) {
      if (component_values_[fixed_value_index++] != value_to_test)
        return false;
      continue;
    }
    if (!(*pattern_it)->Match(value_to_test))
      return false;
    ++pattern_it;
  }
  return true;
}

bool IPPattern::ParsePattern(const std::string& ip_pattern) {
  DCHECK(ip_mask_.empty());
  if (ip_pattern.find(':') != std::string::npos) {
    is_ipv4_ = false;
  }
  Strings components;
  base::SplitString(ip_pattern, is_ipv4_ ? '.' : ':', &components);
  if (components.size() != (is_ipv4_ ? 4u : 8u)) {
    DVLOG(1) << "Invalid component count: " << ip_pattern;
    return false;
  }
  for (Strings::iterator component_it = components.begin();
       component_it != components.end(); ++component_it) {
    if (component_it->empty()) {
      DVLOG(1) << "Empty component: " << ip_pattern;
      return false;
    }
    if (*component_it == "*") {
      // Let standard code handle this below.
      *component_it = is_ipv4_ ? "[0-255]" : "[0-FFFF]";
    } else if ((*component_it)[0] != '[') {
      // This value will just have a specific integer to match.
      uint32 value;
      if (!ValueTextToInt(*component_it, &value))
        return false;
      ip_mask_.push_back(true);
      component_values_.push_back(value);
      continue;
    }
    if ((*component_it)[component_it->size() - 1] != ']') {
      DVLOG(1) << "Missing close bracket: " << ip_pattern;
      return false;
    }
    // Now we know the size() is at least 2.
    if (component_it->size() == 2) {
      DVLOG(1) << "Empty bracket: " << ip_pattern;
      return false;
    }
    // We'll need a pattern to match this bracketed component.
    scoped_ptr<ComponentPattern> component_pattern(new ComponentPattern);
    // Trim leading and trailing bracket before calling for parsing.
    if (!ParseComponentPattern(base::StringPiece(component_it->data() + 1,
        component_it->size() - 2), component_pattern.get())) {
      return false;
    }
    ip_mask_.push_back(false);
    component_patterns_.push_back(component_pattern.release());
  }
  return true;
}

bool IPPattern::ParseComponentPattern(const base::StringPiece& text,
                                      ComponentPattern* pattern) const {
  // We're given a comma separated set of ranges, some of which may be simple
  // constants.
  Strings ranges;
  base::SplitString(text.as_string(), ',', &ranges);
  for (Strings::iterator range_it = ranges.begin();
        range_it != ranges.end(); ++range_it) {
    base::StringTokenizer range_pair(*range_it, "-");
    uint32 min = 0;
    range_pair.GetNext();
    if (!ValueTextToInt(range_pair.token(), &min))
      return false;
    uint32 max = min;  // Sometimes we have no distinct max.
    if (range_pair.GetNext()) {
      if (!ValueTextToInt(range_pair.token(), &max))
        return false;
    }
    if (range_pair.GetNext()) {
      // Too many "-" in this range specifier.
      DVLOG(1) << "Too many hyphens in range: ";
      return false;
    }
    pattern->AppendRange(min, max);
  }
  return true;
}

bool IPPattern::ValueTextToInt(const base::StringPiece& input,
                               uint32* output) const {
  bool ok = is_ipv4_ ? base::StringToUint(input, output) :
                       base::HexStringToUInt(input, output);
  if (!ok) {
    DVLOG(1) << "Could not convert value to number: " << input;
    return false;
  }
  if (is_ipv4_ && *output > 255u) {
    DVLOG(1) << "IPv4 component greater than 255";
    return false;
  }
  if (!is_ipv4_ && *output > 0xFFFFu) {
    DVLOG(1) << "IPv6 component greater than 0xFFFF";
    return false;
  }
  return ok;
}

}  // namespace net
