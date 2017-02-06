// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_EXPERIMENT_LABELS_H_
#define COMPONENTS_VARIATIONS_EXPERIMENT_LABELS_H_

#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"

namespace variations {

// Takes the list of active groups and builds the label for the ones that have
// Google Update VariationID associated with them. This will return an empty
// string if there are no such groups.
base::string16 BuildGoogleUpdateExperimentLabel(
    const base::FieldTrial::ActiveGroups& active_groups);

// Creates a final combined experiment labels string with |variation_labels|
// and |other_labels|, appropriately appending a separator based on their
// contents. It is assumed that |variation_labels| and |other_labels| do not
// have leading or trailing separators.
base::string16 CombineExperimentLabels(const base::string16& variation_labels,
                                       const base::string16& other_labels);

// Takes the value of experiment_labels from the registry and returns a valid
// experiment_labels string value containing only the labels that are not
// associated with Chrome Variations.
base::string16 ExtractNonVariationLabels(const base::string16& labels);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_EXPERIMENT_LABELS_H_
