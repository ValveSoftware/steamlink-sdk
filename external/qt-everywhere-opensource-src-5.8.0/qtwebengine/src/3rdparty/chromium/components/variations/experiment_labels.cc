// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/experiment_labels.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_experiment_util.h"

namespace variations {

namespace {

const char kVariationPrefix[] = "CrVar";

// This method builds a single experiment label for a Chrome Variation,
// including a timestamp that is a year in the future from |current_time|. Since
// multiple headers can be transmitted, |count| is a number that is appended
// after the label key to differentiate the labels.
base::string16 CreateSingleExperimentLabel(int count,
                                           variations::VariationID id,
                                           const base::Time& current_time) {
  // Build the parts separately so they can be validated.
  const base::string16 key =
      base::ASCIIToUTF16(kVariationPrefix) + base::IntToString16(count);
  DCHECK_LE(key.size(), 8U);
  const base::string16 value = base::IntToString16(id);
  DCHECK_LE(value.size(), 8U);
  base::string16 label(key);
  label += base::ASCIIToUTF16("=");
  label += value;
  label += base::ASCIIToUTF16("|");
  label += variations::BuildExperimentDateString(current_time);
  return label;
}

}  // namespace

base::string16 BuildGoogleUpdateExperimentLabel(
    const base::FieldTrial::ActiveGroups& active_groups) {
  base::string16 experiment_labels;
  int counter = 0;

  const base::Time current_time(base::Time::Now());

  // Find all currently active VariationIDs associated with Google Update.
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    const variations::VariationID id =
        variations::GetGoogleVariationID(variations::GOOGLE_UPDATE_SERVICE,
                                         it->trial_name, it->group_name);

    if (id == variations::EMPTY_ID)
      continue;

    if (!experiment_labels.empty())
      experiment_labels += variations::kExperimentLabelSeparator;
    experiment_labels += CreateSingleExperimentLabel(++counter, id,
                                                     current_time);
  }

  return experiment_labels;
}

base::string16 ExtractNonVariationLabels(const base::string16& labels) {
  // First, split everything by the label separator.
  std::vector<base::StringPiece16> entries = base::SplitStringPiece(
      labels, base::StringPiece16(&variations::kExperimentLabelSeparator, 1),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // For each label, keep the ones that do not look like a Variations label.
  base::string16 non_variation_labels;
  for (const base::StringPiece16& entry : entries) {
    if (entry.empty() ||
        base::StartsWith(entry,
                         base::ASCIIToUTF16(kVariationPrefix),
                         base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    // Dump the whole thing, including the timestamp.
    if (!non_variation_labels.empty())
      non_variation_labels += variations::kExperimentLabelSeparator;
    entry.AppendToString(&non_variation_labels);
  }

  return non_variation_labels;
}

base::string16 CombineExperimentLabels(const base::string16& variation_labels,
                                       const base::string16& other_labels) {
  base::StringPiece16 separator(&variations::kExperimentLabelSeparator, 1);
  DCHECK(!base::StartsWith(variation_labels, separator,
                           base::CompareCase::SENSITIVE));
  DCHECK(!base::EndsWith(variation_labels, separator,
                         base::CompareCase::SENSITIVE));
  DCHECK(!base::StartsWith(other_labels, separator,
                           base::CompareCase::SENSITIVE));
  DCHECK(!base::EndsWith(other_labels, separator,
                         base::CompareCase::SENSITIVE));
  // Note that if either label is empty, a separator is not necessary.
  base::string16 combined_labels = other_labels;
  if (!other_labels.empty() && !variation_labels.empty())
    combined_labels += variations::kExperimentLabelSeparator;
  combined_labels += variation_labels;
  return combined_labels;
}

}  // namespace variations
