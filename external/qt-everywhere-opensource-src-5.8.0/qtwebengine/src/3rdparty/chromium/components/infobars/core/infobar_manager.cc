// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_manager.h"

#include <utility>

#include "base/command_line.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobars_switches.h"

namespace infobars {


// InfoBarManager::Observer ---------------------------------------------------

InfoBarManager::Observer::~Observer() {
}

void InfoBarManager::Observer::OnInfoBarAdded(InfoBar* infobar) {
}

void InfoBarManager::Observer::OnInfoBarRemoved(InfoBar* infobar,
                                                bool animate) {
}

void InfoBarManager::Observer::OnInfoBarReplaced(InfoBar* old_infobar,
                                                 InfoBar* new_infobar) {
}

void InfoBarManager::Observer::OnManagerShuttingDown(InfoBarManager* manager) {
}


// InfoBarManager --------------------------------------------------------------

InfoBar* InfoBarManager::AddInfoBar(std::unique_ptr<InfoBar> infobar) {
  DCHECK(infobar);
  if (!infobars_enabled_)
    return NULL;

  for (InfoBars::const_iterator i(infobars_.begin()); i != infobars_.end();
       ++i) {
    if ((*i)->delegate()->EqualsDelegate(infobar->delegate())) {
      DCHECK_NE((*i)->delegate(), infobar->delegate());
      return NULL;
    }
  }

  InfoBar* infobar_ptr = infobar.release();
  infobars_.push_back(infobar_ptr);
  infobar_ptr->SetOwner(this);

  NotifyInfoBarAdded(infobar_ptr);

  return infobar_ptr;
}

void InfoBarManager::RemoveInfoBar(InfoBar* infobar) {
  RemoveInfoBarInternal(infobar, true);
}

void InfoBarManager::RemoveAllInfoBars(bool animate) {
  while (!infobars_.empty())
    RemoveInfoBarInternal(infobars_.back(), animate);
}

InfoBar* InfoBarManager::ReplaceInfoBar(InfoBar* old_infobar,
                                        std::unique_ptr<InfoBar> new_infobar) {
  DCHECK(old_infobar);
  if (!infobars_enabled_)
    return AddInfoBar(std::move(new_infobar));  // Deletes the infobar.
  DCHECK(new_infobar);

  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(),
                                 old_infobar));
  DCHECK(i != infobars_.end());

  InfoBar* new_infobar_ptr = new_infobar.release();
  i = infobars_.insert(i, new_infobar_ptr);
  new_infobar_ptr->SetOwner(this);

  // Remove the old infobar before notifying, so that if any observers call back
  // to AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(++i);

  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnInfoBarReplaced(old_infobar, new_infobar_ptr));

  old_infobar->CloseSoon();
  return new_infobar_ptr;
}

void InfoBarManager::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void InfoBarManager::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

InfoBarManager::InfoBarManager()
    : infobars_enabled_(true) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInfoBars))
    infobars_enabled_ = false;
}

InfoBarManager::~InfoBarManager() {}

void InfoBarManager::ShutDown() {
  // Destroy all remaining InfoBars.  It's important to not animate here so that
  // we guarantee that we'll delete all delegates before we do anything else.
  RemoveAllInfoBars(false);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnManagerShuttingDown(this));
}

void InfoBarManager::OnNavigation(
    const InfoBarDelegate::NavigationDetails& details) {
  // NOTE: It is not safe to change the following code to count upwards or
  // use iterators, as the RemoveInfoBar() call synchronously modifies our
  // delegate list.
  for (size_t i = infobars_.size(); i > 0; --i) {
    InfoBar* infobar = infobars_[i - 1];
    if (infobar->delegate()->ShouldExpire(details))
      RemoveInfoBar(infobar);
  }
}

void InfoBarManager::NotifyInfoBarAdded(InfoBar* infobar) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnInfoBarAdded(infobar));
}

void InfoBarManager::NotifyInfoBarRemoved(InfoBar* infobar, bool animate) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnInfoBarRemoved(infobar, animate));
}

void InfoBarManager::RemoveInfoBarInternal(InfoBar* infobar, bool animate) {
  DCHECK(infobar);
  if (!infobars_enabled_) {
    DCHECK(infobars_.empty());
    return;
  }

  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(), infobar));
  DCHECK(i != infobars_.end());

  // Remove the infobar before notifying, so that if any observers call back to
  // AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(i);

  // This notification must happen before the call to CloseSoon() below, since
  // observers may want to access |infobar| and that call can delete it.
  NotifyInfoBarRemoved(infobar, animate);

  infobar->CloseSoon();
}

}  // namespace infobars
