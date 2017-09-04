// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityIterator_h
#define NGLayoutOpportunityIterator_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_opportunity_tree_node.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include "wtf/Optional.h"
#include "wtf/Vector.h"

namespace blink {

typedef NGLogicalRect NGLayoutOpportunity;
typedef Vector<NGLayoutOpportunity> NGLayoutOpportunities;

class CORE_EXPORT NGLayoutOpportunityIterator final
    : public GarbageCollectedFinalized<NGLayoutOpportunityIterator> {
 public:
  // Default constructor.
  //
  // @param space Constraint space with exclusions for which this iterator needs
  //              to generate layout opportunities.
  // @param opt_origin_point Optional origin_point parameter that is used as a
  //                     default start point for layout opportunities.
  // @param opt_leader_point Optional 'leader' parameter that is used to specify
  // the
  //                     ending point of temporary excluded rectangle which
  //                     starts from 'origin'. This rectangle may represent a
  //                     text fragment for example.
  NGLayoutOpportunityIterator(
      NGConstraintSpace* space,
      const WTF::Optional<NGLogicalOffset>& opt_origin_point = WTF::nullopt,
      const WTF::Optional<NGLogicalOffset>& opt_leader_point = WTF::nullopt);

  // Gets the next Layout Opportunity or nullptr if the search is exhausted.
  // TODO(chrome-layout-team): Refactor with using C++ <iterator> library.
  const NGLayoutOpportunity Next();

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(constraint_space_);
    visitor->trace(opportunity_tree_root_);
  }

 private:
  // Mutable Getters.
  NGLayoutOpportunityTreeNode* MutableOpportunityTreeRoot() {
    return opportunity_tree_root_.get();
  }

  // Read-only Getters.
  const NGLayoutOpportunityTreeNode* OpportunityTreeRoot() const {
    return opportunity_tree_root_.get();
  }

  Member<NGConstraintSpace> constraint_space_;

  NGLayoutOpportunities opportunities_;
  NGLayoutOpportunities::const_iterator opportunity_iter_;
  Member<NGLayoutOpportunityTreeNode> opportunity_tree_root_;
};

}  // namespace blink

#endif  // NGLayoutOpportunityIterator_h
