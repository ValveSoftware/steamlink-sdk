// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_EXPERIMENTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_EXPERIMENTS_H_

namespace password_manager {

// True iff the force-saving of passwords is enabled.
bool ForceSavingExperimentEnabled();

// True iff the manual password generation is enabled.
bool ManualPasswordGenerationEnabled();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_EXPERIMENTS_H_
