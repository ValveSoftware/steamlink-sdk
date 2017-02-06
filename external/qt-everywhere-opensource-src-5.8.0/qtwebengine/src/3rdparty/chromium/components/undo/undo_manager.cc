// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/undo_manager.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "components/undo/undo_manager_observer.h"
#include "components/undo/undo_operation.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Maximum number of changes that can be undone.
const size_t kMaxUndoGroups = 100;

}  // namespace

// UndoGroup ------------------------------------------------------------------

UndoGroup::UndoGroup()
    : undo_label_id_(IDS_BOOKMARK_BAR_UNDO),
      redo_label_id_(IDS_BOOKMARK_BAR_REDO) {
}

UndoGroup::~UndoGroup() {
}

void UndoGroup::AddOperation(std::unique_ptr<UndoOperation> operation) {
  if (operations_.empty()) {
    set_undo_label_id(operation->GetUndoLabelId());
    set_redo_label_id(operation->GetRedoLabelId());
  }
  operations_.push_back(operation.release());
}

void UndoGroup::Undo() {
  for (ScopedVector<UndoOperation>::reverse_iterator ri = operations_.rbegin();
       ri != operations_.rend(); ++ri) {
    (*ri)->Undo();
  }
}

// UndoManager ----------------------------------------------------------------

UndoManager::UndoManager()
    : group_actions_count_(0),
      undo_in_progress_action_(NULL),
      undo_suspended_count_(0),
      performing_undo_(false),
      performing_redo_(false) {
}

UndoManager::~UndoManager() {
  DCHECK_EQ(0, group_actions_count_);
  DCHECK_EQ(0, undo_suspended_count_);
  DCHECK(!performing_undo_);
  DCHECK(!performing_redo_);
}

void UndoManager::Undo() {
  Undo(&performing_undo_, &undo_actions_);
}

void UndoManager::Redo() {
  Undo(&performing_redo_, &redo_actions_);
}

base::string16 UndoManager::GetUndoLabel() const {
  return l10n_util::GetStringUTF16(
      undo_actions_.empty() ? IDS_BOOKMARK_BAR_UNDO
                            : undo_actions_.back()->get_undo_label_id());
}

base::string16 UndoManager::GetRedoLabel() const {
  return l10n_util::GetStringUTF16(
      redo_actions_.empty() ? IDS_BOOKMARK_BAR_REDO
                            : redo_actions_.back()->get_redo_label_id());
}

void UndoManager::AddUndoOperation(std::unique_ptr<UndoOperation> operation) {
  if (IsUndoTrakingSuspended()) {
    RemoveAllOperations();
    operation.reset();
    return;
  }

  if (group_actions_count_) {
    pending_grouped_action_->AddOperation(std::move(operation));
  } else {
    UndoGroup* new_action = new UndoGroup();
    new_action->AddOperation(std::move(operation));
    AddUndoGroup(new_action);
  }
}

void UndoManager::StartGroupingActions() {
  if (!group_actions_count_)
    pending_grouped_action_.reset(new UndoGroup());
  ++group_actions_count_;
}

void UndoManager::EndGroupingActions() {
  --group_actions_count_;
  if (group_actions_count_ > 0)
    return;

  // Check that StartGroupingActions and EndGroupingActions are paired.
  DCHECK_GE(group_actions_count_, 0);

  bool is_user_action = !performing_undo_ && !performing_redo_;
  if (!pending_grouped_action_->undo_operations().empty()) {
    AddUndoGroup(pending_grouped_action_.release());
  } else {
    // No changes were executed since we started grouping actions, so the
    // pending UndoGroup should be discarded.
    pending_grouped_action_.reset();

    // This situation is only expected when it is a user initiated action.
    // Undo/Redo should have at least one operation performed.
    DCHECK(is_user_action);
  }
}

void UndoManager::SuspendUndoTracking() {
  ++undo_suspended_count_;
}

void UndoManager::ResumeUndoTracking() {
  DCHECK_GT(undo_suspended_count_, 0);
  --undo_suspended_count_;
}

bool UndoManager::IsUndoTrakingSuspended() const {
  return undo_suspended_count_ > 0;
}

std::vector<UndoOperation*> UndoManager::GetAllUndoOperations() const {
  std::vector<UndoOperation*> result;
  for (size_t i = 0; i < undo_actions_.size(); ++i) {
    const std::vector<UndoOperation*>& operations =
        undo_actions_[i]->undo_operations();
    result.insert(result.end(), operations.begin(), operations.end());
  }
  for (size_t i = 0; i < redo_actions_.size(); ++i) {
    const std::vector<UndoOperation*>& operations =
        redo_actions_[i]->undo_operations();
    result.insert(result.end(), operations.begin(), operations.end());
  }
  // Ensure that if an Undo is in progress the UndoOperations part of that
  // UndoGroup are included in the returned set. This will ensure that any
  // changes (such as renumbering) will be applied to any potentially
  // unprocessed UndoOperations.
  if (undo_in_progress_action_) {
    const std::vector<UndoOperation*>& operations =
        undo_in_progress_action_->undo_operations();
    result.insert(result.end(), operations.begin(), operations.end());
  }

  return result;
}

void UndoManager::RemoveAllOperations() {
  DCHECK(!group_actions_count_);
  undo_actions_.clear();
  redo_actions_.clear();

  NotifyOnUndoManagerStateChange();
}

void UndoManager::AddObserver(UndoManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void UndoManager::RemoveObserver(UndoManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UndoManager::Undo(bool* performing_indicator,
                       ScopedVector<UndoGroup>* active_undo_group) {
  // Check that action grouping has been correctly ended.
  DCHECK(!group_actions_count_);

  if (active_undo_group->empty())
    return;

  base::AutoReset<bool> incoming_changes(performing_indicator, true);
  std::unique_ptr<UndoGroup> action(active_undo_group->back());
  base::AutoReset<UndoGroup*> action_context(&undo_in_progress_action_,
      action.get());
  active_undo_group->weak_erase(
      active_undo_group->begin() + active_undo_group->size() - 1);

  StartGroupingActions();
  action->Undo();
  EndGroupingActions();

  NotifyOnUndoManagerStateChange();
}

void UndoManager::NotifyOnUndoManagerStateChange() {
  FOR_EACH_OBSERVER(
      UndoManagerObserver, observers_, OnUndoManagerStateChange());
}

void UndoManager::AddUndoGroup(UndoGroup* new_undo_group) {
  GetActiveUndoGroup()->push_back(new_undo_group);

  // User actions invalidate any available redo actions.
  if (is_user_action())
    redo_actions_.clear();

  // Limit the number of undo levels so the undo stack does not grow unbounded.
  if (GetActiveUndoGroup()->size() > kMaxUndoGroups)
    GetActiveUndoGroup()->erase(GetActiveUndoGroup()->begin());

  NotifyOnUndoManagerStateChange();
}

ScopedVector<UndoGroup>* UndoManager::GetActiveUndoGroup() {
  return performing_undo_ ? &redo_actions_ : &undo_actions_;
}
