// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An object to store user feedback to spellcheck suggestions from spelling
// service.
//
// Stores feedback for the spelling service in |Misspelling| objects. Each
// |Misspelling| object is identified by a |hash| and corresponds to a document
// marker with the same |hash| identifier in the renderer.

#ifndef CHROME_BROWSER_SPELLCHECKER_FEEDBACK_H_
#define CHROME_BROWSER_SPELLCHECKER_FEEDBACK_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/spellchecker/misspelling.h"

namespace spellcheck {

// Stores user feedback to spellcheck suggestions. Sample usage:
//    Feedback feedback;
//    feedback.AddMisspelling(renderer_process_id, Misspelling(
//        base::ASCIIToUTF16("Helllo world"), 0, 6,
//        std::vector<base::string16>(), GenerateRandomHash()));
//    feedback.FinalizeRemovedMisspellings(renderer_process_id,
//                                         std::vector<uint32_t>());
//    ProcessFeedback(feedback.GetMisspellingsInRenderer(renderer_process_id));
//    feedback.EraseFinalizedMisspellings(renderer_process_id);
class Feedback {
 public:
  explicit Feedback(size_t max_total_text_size);
  ~Feedback();

  // Returns the misspelling identified by |hash|. Returns NULL if there's no
  // misspelling identified by |hash|. Retains the ownership of the result. The
  // caller should not modify the hash in the returned misspelling.
  Misspelling* GetMisspelling(uint32_t hash);

  // Finalizes the user actions on misspellings that are removed from the
  // renderer process with ID |renderer_process_id|.
  void FinalizeRemovedMisspellings(
      int renderer_process_id,
      const std::vector<uint32_t>& remaining_markers);

  // Returns true if the renderer with process ID |renderer_process_id| has
  // misspellings.
  bool RendererHasMisspellings(int renderer_process_id) const;

  // Returns a copy of the misspellings in renderer with process ID
  // |renderer_process_id|.
  std::vector<Misspelling> GetMisspellingsInRenderer(
      int renderer_process_id) const;

  // Erases the misspellings with final user actions in the renderer with
  // process ID |renderer_process_id|.
  void EraseFinalizedMisspellings(int renderer_process_id);

  // Returns true if there's a misspelling with |hash| identifier.
  bool HasMisspelling(uint32_t hash) const;

  // Adds the |misspelling| to feedback data. If the |misspelling| has a
  // duplicate hash, then replaces the existing misspelling with the same hash.
  void AddMisspelling(int renderer_process_id, const Misspelling& misspelling);

  // Returns true if there're no misspellings.
  bool Empty() const;

  // Returns a list of process identifiers for renderers that have misspellings.
  std::vector<int> GetRendersWithMisspellings() const;

  // Finalizes all misspellings.
  void FinalizeAllMisspellings();

  // Returns a copy of all misspellings.
  std::vector<Misspelling> GetAllMisspellings() const;

  // Removes all misspellings.
  void Clear();

  // Returns a list of all misspelling identifiers for |misspelled_text|.
  const std::set<uint32_t>& FindMisspellings(
      const base::string16& misspelled_text) const;

 private:
  typedef std::map<uint32_t, Misspelling> HashMisspellingMap;
  typedef std::set<uint32_t> HashCollection;
  typedef std::map<int, HashCollection> RendererHashesMap;
  typedef std::map<base::string16, HashCollection> TextHashesMap;

  // An empty hash collection to return when FindMisspellings() does not find
  // misspellings.
  const HashCollection empty_hash_collection_;

  // The limit on the amount of data to send to the server at once.
  const size_t max_total_text_size_;

  // Size of all feedback text, used to limit the estimated amount of data sent
  // to the server at once.
  size_t total_text_size_;

  // A map of hashes that identify document markers to feedback data to be sent
  // to spelling service.
  HashMisspellingMap misspellings_;

  // A map of renderer process ID to hashes that identify misspellings.
  RendererHashesMap renderers_;

  // A map of misspelled text to hashes that identify misspellings.
  TextHashesMap text_;

  DISALLOW_COPY_AND_ASSIGN(Feedback);
};

}  // namespace spellcheck

#endif  // CHROME_BROWSER_SPELLCHECKER_FEEDBACK_H_
