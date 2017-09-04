// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_REVISION_GENERATOR_H_
#define BLIMP_HELIUM_REVISION_GENERATOR_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/version_vector.h"

namespace blimp {
namespace helium {

// Responsible for generating monotonically increasing values
// of Revisions. This generator is used one per host
// (client or engine) to ensure they have a single clock domain.
class BLIMP_HELIUM_EXPORT RevisionGenerator {
 public:
  RevisionGenerator() = default;

  const Revision& current() { return revision_; }

  const Revision& GetNextRevision();

  static RevisionGenerator* GetInstance();

 private:
  static base::LazyInstance<RevisionGenerator> instance_;

  Revision revision_ = 0;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(RevisionGenerator);
};

Revision BLIMP_HELIUM_EXPORT GetNextRevision();

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_REVISION_GENERATOR_H_
