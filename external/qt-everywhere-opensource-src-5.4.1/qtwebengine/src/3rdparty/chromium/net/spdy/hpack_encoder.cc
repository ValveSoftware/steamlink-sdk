// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoder.h"

#include <algorithm>

#include "base/logging.h"
#include "net/spdy/hpack_header_table.h"
#include "net/spdy/hpack_huffman_table.h"
#include "net/spdy/hpack_output_stream.h"

namespace net {

using base::StringPiece;
using std::string;

namespace {

const uint8 kNoState = 0;
// Set on a referenced HpackEntry which is part of the current header set.
const uint8 kReferencedImplicitOn = 1;
// Set on a referenced HpackEntry which is not part of the current header set.
const uint8 kReferencedExplicitOff = 2;
// Set on a entries added to the reference set during this encoding.
const uint8 kReferencedThisEncoding = 3;

}  // namespace

HpackEncoder::HpackEncoder(const HpackHuffmanTable& table)
    : output_stream_(),
      allow_huffman_compression_(true),
      huffman_table_(table),
      char_counts_(NULL),
      total_char_counts_(NULL) {}

HpackEncoder::~HpackEncoder() {}

bool HpackEncoder::EncodeHeaderSet(const std::map<string, string>& header_set,
                                   string* output) {
  // Walk the set of entries to encode, which are not already implied by the
  // header table's reference set. They must be explicitly emitted.
  Representations explicit_set(DetermineEncodingDelta(header_set));
  for (Representations::const_iterator it = explicit_set.begin();
       it != explicit_set.end(); ++it) {
    // Try to find an exact match. Note that dynamic entries are preferred
    // by the header table index.
    HpackEntry* entry = header_table_.GetByNameAndValue(it->first, it->second);
    if (entry != NULL && !entry->IsStatic()) {
      // Already in the dynamic table. Simply toggle on.
      CHECK_EQ(kNoState, entry->state());
      EmitDynamicIndex(entry);
      continue;
    }

    // Walk the set of entries to be evicted by this insertion.
    HpackHeaderTable::EntryTable::iterator evict_begin, evict_end, evict_it;
    header_table_.EvictionSet(it->first, it->second, &evict_begin, &evict_end);

    for (evict_it = evict_begin; evict_it != evict_end; ++evict_it) {
      HpackEntry* evictee = &(*evict_it);

      if (evict_it->state() == kReferencedImplicitOn) {
        // Issue twice to explicitly emit.
        EmitDynamicIndex(evictee);
        EmitDynamicIndex(evictee);
      } else if (evictee->state() == kReferencedExplicitOff) {
        // Eviction saves us from having to explicitly toggle off.
        evictee->set_state(kNoState);
      } else if (evictee->state() == kReferencedThisEncoding) {
        // Already emitted. No action required.
        evictee->set_state(kNoState);
      }
    }
    if (entry != NULL) {
      EmitStaticIndex(entry);
    } else {
      EmitIndexedLiteral(*it);
    }
  }
  // Walk the reference set, toggling off as needed and clearing encoding state.
  for (HpackHeaderTable::OrderedEntrySet::const_iterator it =
           header_table_.reference_set().begin();
       it != header_table_.reference_set().end();) {
    HpackEntry* entry = *(it++);  // Step to prevent invalidation.
    CHECK_NE(kNoState, entry->state());

    if (entry->state() == kReferencedExplicitOff) {
      // Explicitly toggle off.
      EmitDynamicIndex(entry);
    }
    entry->set_state(kNoState);
  }
  output_stream_.TakeString(output);
  return true;
}

bool HpackEncoder::EncodeHeaderSetWithoutCompression(
    const std::map<string, string>& header_set,
    string* output) {

  allow_huffman_compression_ = false;
  for (std::map<string, string>::const_iterator it = header_set.begin();
       it != header_set.end(); ++it) {
    // Note that cookies are not crumbled in this case.
    EmitNonIndexedLiteral(*it);
  }
  allow_huffman_compression_ = true;
  output_stream_.TakeString(output);
  return true;
}

void HpackEncoder::EmitDynamicIndex(HpackEntry* entry) {
  DCHECK(!entry->IsStatic());
  output_stream_.AppendPrefix(kIndexedOpcode);
  output_stream_.AppendUint32(header_table_.IndexOf(entry));

  entry->set_state(kNoState);
  if (header_table_.Toggle(entry)) {
    // Was added to the reference set.
    entry->set_state(kReferencedThisEncoding);
  }
}

void HpackEncoder::EmitStaticIndex(HpackEntry* entry) {
  DCHECK(entry->IsStatic());
  output_stream_.AppendPrefix(kIndexedOpcode);
  output_stream_.AppendUint32(header_table_.IndexOf(entry));

  HpackEntry* new_entry = header_table_.TryAddEntry(entry->name(),
                                                    entry->value());
  if (new_entry) {
    // This is a static entry: no need to pin.
    header_table_.Toggle(new_entry);
    new_entry->set_state(kReferencedThisEncoding);
  }
}

void HpackEncoder::EmitIndexedLiteral(const Representation& representation) {
  output_stream_.AppendPrefix(kLiteralIncrementalIndexOpcode);
  EmitLiteral(representation);

  HpackEntry* new_entry = header_table_.TryAddEntry(representation.first,
                                                    representation.second);
  if (new_entry) {
    header_table_.Toggle(new_entry);
    new_entry->set_state(kReferencedThisEncoding);
  }
}

void HpackEncoder::EmitNonIndexedLiteral(
    const Representation& representation) {
  output_stream_.AppendPrefix(kLiteralNoIndexOpcode);
  output_stream_.AppendUint32(0);
  EmitString(representation.first);
  EmitString(representation.second);
}

void HpackEncoder::EmitLiteral(const Representation& representation) {
  const HpackEntry* name_entry = header_table_.GetByName(representation.first);
  if (name_entry != NULL) {
    output_stream_.AppendUint32(header_table_.IndexOf(name_entry));
  } else {
    output_stream_.AppendUint32(0);
    EmitString(representation.first);
  }
  EmitString(representation.second);
}

void HpackEncoder::EmitString(StringPiece str) {
  size_t encoded_size = (!allow_huffman_compression_ ? str.size()
                         : huffman_table_.EncodedSize(str));
  if (encoded_size < str.size()) {
    output_stream_.AppendPrefix(kStringLiteralHuffmanEncoded);
    output_stream_.AppendUint32(encoded_size);
    huffman_table_.EncodeString(str, &output_stream_);
  } else {
    output_stream_.AppendPrefix(kStringLiteralIdentityEncoded);
    output_stream_.AppendUint32(str.size());
    output_stream_.AppendBytes(str);
  }
  UpdateCharacterCounts(str);
}

// static
HpackEncoder::Representations HpackEncoder::DetermineEncodingDelta(
    const std::map<string, string>& header_set) {
  // Flatten & crumble headers into an ordered list of representations.
  Representations full_set;
  for (std::map<string, string>::const_iterator it = header_set.begin();
       it != header_set.end(); ++it) {
    if (it->first == "cookie") {
      // |CookieToCrumbs()| produces ordered crumbs.
      CookieToCrumbs(*it, &full_set);
    } else {
      // Note std::map guarantees representations are ordered.
      full_set.push_back(make_pair(
          StringPiece(it->first), StringPiece(it->second)));
    }
  }
  // Perform a linear merge of ordered representations with the (also ordered)
  // reference set. Mark each referenced entry with current membership state,
  // and gather representations which must be explicitly emitted.
  Representations::const_iterator r_it = full_set.begin();
  HpackHeaderTable::OrderedEntrySet::const_iterator s_it =
      header_table_.reference_set().begin();

  Representations explicit_set;
  while (r_it != full_set.end() &&
         s_it != header_table_.reference_set().end()) {
    // Compare on name, then value.
    int result = r_it->first.compare((*s_it)->name());
    if (result == 0) {
      result = r_it->second.compare((*s_it)->value());
    }

    if (result < 0) {
      explicit_set.push_back(*r_it);
      ++r_it;
    } else if (result > 0) {
      (*s_it)->set_state(kReferencedExplicitOff);
      ++s_it;
    } else {
      (*s_it)->set_state(kReferencedImplicitOn);
      ++s_it;
      ++r_it;
    }
  }
  while (r_it != full_set.end()) {
    explicit_set.push_back(*r_it);
    ++r_it;
  }
  while (s_it != header_table_.reference_set().end()) {
    (*s_it)->set_state(kReferencedExplicitOff);
    ++s_it;
  }
  return explicit_set;
}

void HpackEncoder::SetCharCountsStorage(std::vector<size_t>* char_counts,
                                        size_t* total_char_counts) {
  CHECK_LE(256u, char_counts->size());
  char_counts_ = char_counts;
  total_char_counts_ = total_char_counts;
}

void HpackEncoder::UpdateCharacterCounts(base::StringPiece str) {
  if (char_counts_ == NULL || total_char_counts_ == NULL) {
    return;
  }
  for (StringPiece::const_iterator it = str.begin(); it != str.end(); ++it) {
    ++(*char_counts_)[static_cast<uint8>(*it)];
  }
  (*total_char_counts_) += str.size();
}

// static
void HpackEncoder::CookieToCrumbs(const Representation& cookie,
                                  Representations* out) {
  size_t prior_size = out->size();

  // See Section 8.1.3.4 "Compressing the Cookie Header Field" in the HTTP/2
  // specification at http://tools.ietf.org/html/draft-ietf-httpbis-http2-11
  // Cookie values are split into individually-encoded HPACK representations.
  for (size_t pos = 0;;) {
    size_t end = cookie.second.find(";", pos);

    if (end == StringPiece::npos) {
      out->push_back(make_pair(
          cookie.first,
          cookie.second.substr(pos)));
      break;
    }
    out->push_back(make_pair(
        cookie.first,
        cookie.second.substr(pos, end - pos)));

    // Consume next space if present.
    pos = end + 1;
    if (pos != cookie.second.size() && cookie.second[pos] == ' ') {
      pos++;
    }
  }
  // Sort crumbs and remove duplicates.
  std::sort(out->begin() + prior_size, out->end());
  out->erase(std::unique(out->begin() + prior_size, out->end()),
             out->end());
}

}  // namespace net
