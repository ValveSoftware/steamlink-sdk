// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/revision_generator.h"

namespace blimp {
namespace helium {

base::LazyInstance<RevisionGenerator> RevisionGenerator::instance_ =
    LAZY_INSTANCE_INITIALIZER;

const Revision& RevisionGenerator::GetNextRevision() {
  DCHECK(sequence_checker_.CalledOnValidSequence())
      << "GetNextRevision() must be called on the same sequenced thread.";
  return ++revision_;
}

RevisionGenerator* RevisionGenerator::GetInstance() {
  return instance_.Pointer();
}

Revision GetNextRevision() {
  return RevisionGenerator::GetInstance()->GetNextRevision();
}

}  // namespace helium
}  // namespace blimp
