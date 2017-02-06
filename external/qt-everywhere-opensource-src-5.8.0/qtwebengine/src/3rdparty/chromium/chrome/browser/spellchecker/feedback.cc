// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The |Feedback| object keeps track of each instance of user feedback in a map
// |misspellings_|. This is a map from uint32_t hashes to |Misspelling| objects.
//
// Each misspelling should be present in only one renderer process. The
// |Feedback| objects keeps track of misspelling-renderer relationship in the
// |renderers_| map of renderer process identifiers to a set of hashes.
//
// When the user adds a misspelling to their custom dictionary, all of the
// |Misspelling| objects with the same misspelled string are updated. The
// |Feedback| object facilitates efficient access to these misspellings through
// a |text_| map of misspelled strings to a set of hashes.

#include "chrome/browser/spellchecker/feedback.h"

#include <algorithm>
#include <iterator>
#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"

namespace spellcheck {

Feedback::Feedback(size_t max_total_text_size)
    : max_total_text_size_(max_total_text_size), total_text_size_(0) {
  DCHECK_GE(max_total_text_size, 1024U);
}

Feedback::~Feedback() {}

Misspelling* Feedback::GetMisspelling(uint32_t hash) {
  HashMisspellingMap::iterator misspelling_it = misspellings_.find(hash);
  if (misspelling_it == misspellings_.end())
    return nullptr;
  return &misspelling_it->second;
}

void Feedback::FinalizeRemovedMisspellings(
    int renderer_process_id,
    const std::vector<uint32_t>& remaining_markers) {
  RendererHashesMap::iterator renderer_it =
      renderers_.find(renderer_process_id);
  if (renderer_it == renderers_.end() || renderer_it->second.empty())
    return;
  HashCollection& renderer_hashes = renderer_it->second;
  HashCollection remaining_hashes(remaining_markers.begin(),
                                  remaining_markers.end());
  std::vector<HashCollection::value_type> removed_hashes =
      base::STLSetDifference<std::vector<HashCollection::value_type>>(
          renderer_hashes, remaining_hashes);
  for (auto hash : removed_hashes) {
    HashMisspellingMap::iterator misspelling_it = misspellings_.find(hash);
    if (misspelling_it != misspellings_.end() &&
        !misspelling_it->second.action.IsFinal()) {
      misspelling_it->second.action.Finalize();
    }
  }
}

bool Feedback::RendererHasMisspellings(int renderer_process_id) const {
  RendererHashesMap::const_iterator renderer_it =
      renderers_.find(renderer_process_id);
  return renderer_it != renderers_.end() && !renderer_it->second.empty();
}

std::vector<Misspelling> Feedback::GetMisspellingsInRenderer(
    int renderer_process_id) const {
  std::vector<Misspelling> misspellings_in_renderer;
  RendererHashesMap::const_iterator renderer_it =
      renderers_.find(renderer_process_id);
  if (renderer_it == renderers_.end() || renderer_it->second.empty())
    return misspellings_in_renderer;
  const HashCollection& renderer_hashes = renderer_it->second;
  for (HashCollection::const_iterator hash_it = renderer_hashes.begin();
       hash_it != renderer_hashes.end(); ++hash_it) {
    HashMisspellingMap::const_iterator misspelling_it =
        misspellings_.find(*hash_it);
    if (misspelling_it != misspellings_.end())
      misspellings_in_renderer.push_back(misspelling_it->second);
  }
  return misspellings_in_renderer;
}

void Feedback::EraseFinalizedMisspellings(int renderer_process_id) {
  RendererHashesMap::iterator renderer_it =
      renderers_.find(renderer_process_id);
  if (renderer_it == renderers_.end())
    return;
  HashCollection& renderer_hashes = renderer_it->second;
  for (HashCollection::const_iterator hash_it = renderer_hashes.begin();
       hash_it != renderer_hashes.end();) {
    HashMisspellingMap::iterator misspelling_it = misspellings_.find(*hash_it);
    HashCollection::iterator erasable_hash_it = hash_it;
    ++hash_it;
    if (misspelling_it == misspellings_.end())
      continue;
    const Misspelling& misspelling = misspelling_it->second;
    if (!misspelling.action.IsFinal())
      continue;
    renderer_hashes.erase(erasable_hash_it);
    text_[GetMisspelledString(misspelling)].erase(misspelling.hash);
    size_t approximate_size = ApproximateSerializedSize(misspelling_it->second);
    // Prevent underlfow.
    if (total_text_size_ >= approximate_size)
      total_text_size_ -= approximate_size;
    else
      total_text_size_ = 0;
    misspellings_.erase(misspelling_it);
  }
  if (renderer_hashes.empty())
    renderers_.erase(renderer_it);
}

bool Feedback::HasMisspelling(uint32_t hash) const {
  return !!misspellings_.count(hash);
}

void Feedback::AddMisspelling(int renderer_process_id,
                              const Misspelling& misspelling) {
  HashMisspellingMap::iterator misspelling_it =
      misspellings_.find(misspelling.hash);
  if (misspelling_it != misspellings_.end()) {
    const Misspelling& existing_misspelling = misspelling_it->second;
    text_[GetMisspelledString(existing_misspelling)].erase(misspelling.hash);
    for (RendererHashesMap::iterator renderer_it = renderers_.begin();
         renderer_it != renderers_.end();) {
      HashCollection& renderer_hashes = renderer_it->second;
      RendererHashesMap::iterator erasable_renderer_it = renderer_it;
      ++renderer_it;
      renderer_hashes.erase(misspelling.hash);
      if (renderer_hashes.empty())
        renderers_.erase(erasable_renderer_it);
    }
  } else {
    size_t approximate_size = ApproximateSerializedSize(misspelling);
    // Prevent overflow.
    if (total_text_size_ <=
        std::numeric_limits<size_t>::max() - approximate_size) {
      total_text_size_ += approximate_size;
    }
    if (total_text_size_ >= max_total_text_size_)
      return;
  }
  misspellings_[misspelling.hash] = misspelling;
  text_[GetMisspelledString(misspelling)].insert(misspelling.hash);
  renderers_[renderer_process_id].insert(misspelling.hash);
}

bool Feedback::Empty() const {
  return misspellings_.empty();
}

std::vector<int> Feedback::GetRendersWithMisspellings() const {
  std::vector<int> renderers_with_misspellings;
  for (const auto& renderer : renderers_) {
    if (!renderer.second.empty())
      renderers_with_misspellings.push_back(renderer.first);
  }
  return renderers_with_misspellings;
}

void Feedback::FinalizeAllMisspellings() {
  for (auto& misspelling : misspellings_) {
    if (!misspelling.second.action.IsFinal())
      misspelling.second.action.Finalize();
  }
}

std::vector<Misspelling> Feedback::GetAllMisspellings() const {
  std::vector<Misspelling> all_misspellings;
  for (const auto& misspelling : misspellings_)
    all_misspellings.push_back(misspelling.second);
  return all_misspellings;
}

void Feedback::Clear() {
  total_text_size_ = 0;
  misspellings_.clear();
  text_.clear();
  renderers_.clear();
}

const std::set<uint32_t>& Feedback::FindMisspellings(
    const base::string16& misspelled_text) const {
  const TextHashesMap::const_iterator text_it = text_.find(misspelled_text);
  return text_it == text_.end() ? empty_hash_collection_ : text_it->second;
}

}  // namespace spellcheck
