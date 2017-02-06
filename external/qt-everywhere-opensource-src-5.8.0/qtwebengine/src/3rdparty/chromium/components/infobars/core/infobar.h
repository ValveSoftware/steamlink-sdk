// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/infobars/core/infobar_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/size.h"

namespace infobars {

class InfoBarContainer;
class InfoBarManager;

// InfoBar is a cross-platform base class for an infobar "view" (in the MVC
// sense), which owns a corresponding InfoBarDelegate "model".  Typically,
// a caller will call XYZInfoBarDelegate::Create() and pass in the
// InfoBarManager for the relevant tab.  This will create an XYZInfoBarDelegate,
// create a platform-specific subclass of InfoBar to own it, and then call
// InfoBarManager::AddInfoBar() to give it ownership of the infobar.
// During its life, the InfoBar may be shown and hidden as the owning tab is
// switched between the foreground and background.  Eventually, InfoBarManager
// will instruct the InfoBar to close itself.  At this point, the InfoBar will
// optionally animate closed; once it's no longer visible, it deletes itself,
// destroying the InfoBarDelegate in the process.
//
// Thus, InfoBarDelegate and InfoBar implementations can assume they share
// lifetimes, and not NULL-check each other; but if one needs to reach back into
// the owning InfoBarManager, it must check whether that's still possible.
class InfoBar : public gfx::AnimationDelegate {
 public:
  // These are the types passed as Details for infobar-related notifications.
  typedef InfoBar AddedDetails;
  typedef std::pair<InfoBar*, bool> RemovedDetails;

  explicit InfoBar(std::unique_ptr<InfoBarDelegate> delegate);
  ~InfoBar() override;

  static SkColor GetTopColor(InfoBarDelegate::Type infobar_type);
  static SkColor GetBottomColor(InfoBarDelegate::Type infobar_type);

  InfoBarManager* owner() { return owner_; }
  InfoBarDelegate* delegate() const { return delegate_.get(); }
  void set_container(InfoBarContainer* container) { container_ = container; }

  // Sets |owner_|.  This also sets the nav entry ID on |delegate_|.  This must
  // only be called once as there's no way to extract an infobar from its owner
  // without deleting it, for reparenting in another tab.
  void SetOwner(InfoBarManager* owner);

  // Makes the infobar visible.  If |animate| is true, the infobar is then
  // animated to full size.
  void Show(bool animate);

  // Makes the infobar hidden.  If |animate| is false, the infobar is
  // immediately removed from the container, and, if now unowned, deleted.  If
  // |animate| is true, the infobar is animated to zero size, ultimately
  // triggering a call to AnimationEnded().
  void Hide(bool animate);

  // Changes the target height of the arrow portion of the infobar.  This has no
  // effect once the infobar is animating closed.
  void SetArrowTargetHeight(int height);

  // Notifies the infobar that it is no longer owned and should delete itself
  // once it is invisible.
  void CloseSoon();

  // Forwards a close request to our owner.  This is a no-op if we're already
  // unowned.
  void RemoveSelf();

  // Changes the target height of the main ("bar") portion of the infobar.
  void SetBarTargetHeight(int height);

  const gfx::SlideAnimation& animation() const { return animation_; }
  int arrow_height() const { return arrow_height_; }
  int arrow_target_height() const { return arrow_target_height_; }
  int arrow_half_width() const { return arrow_half_width_; }
  int total_height() const { return arrow_height_ + bar_height_; }

 protected:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  const InfoBarContainer* container() const { return container_; }
  InfoBarContainer* container() { return container_; }
  gfx::SlideAnimation* animation() { return &animation_; }
  int bar_height() const { return bar_height_; }
  int bar_target_height() const { return bar_target_height_; }

  // Platforms may optionally override these if they need to do work during
  // processing of the given calls.
  virtual void PlatformSpecificSetOwner() {}
  virtual void PlatformSpecificShow(bool animate) {}
  virtual void PlatformSpecificHide(bool animate) {}
  virtual void PlatformSpecificOnCloseSoon() {}
  virtual void PlatformSpecificOnHeightsRecalculated() {}

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

  // Finds the new desired arrow and bar heights, and if they differ from the
  // current ones, calls PlatformSpecificOnHeightRecalculated().  Informs our
  // container our state has changed if either the heights have changed or
  // |force_notify| is set.
  void RecalculateHeights(bool force_notify);

  // Checks whether the infobar is unowned and done with all animations.  If so,
  // notifies the container that it should remove this infobar, and deletes
  // itself.
  void MaybeDelete();

  InfoBarManager* owner_;
  std::unique_ptr<InfoBarDelegate> delegate_;
  InfoBarContainer* container_;
  gfx::SlideAnimation animation_;

  // The current and target heights of the arrow and bar portions, and half the
  // current arrow width.  (It's easier to work in half-widths as we draw the
  // arrow as two halves on either side of a center point.)  All these values
  // scale in unison to the container delegate's default and maximum values.
  int arrow_height_;         // Includes both fill and top stroke.
  int arrow_target_height_;  // Should always be set by the time we read it.
  int arrow_half_width_;     // Includes only fill.
  int bar_height_;           // Includes both fill and bottom separator.
  int bar_target_height_;    // May be left as -1, meaning "use default".

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_H_
