// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_FAKE_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_FAKE_DISPLAY_SNAPSHOT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display {

// A display snapshot that doesn't correspond to a physical display, used when
// running off device.
class DISPLAY_EXPORT FakeDisplaySnapshot : public ui::DisplaySnapshot {
 public:
  class Builder {
   public:
    Builder();
    ~Builder();

    // Builds new FakeDisplaySnapshot. At the very minimum you must set id and
    // native display mode before building or it will fail.
    std::unique_ptr<FakeDisplaySnapshot> Build();

    Builder& SetId(int64_t id);
    // Adds display mode with |size| and set as native mode. If a display mode
    // with |size| already exists then it will be reused.
    Builder& SetNativeMode(const gfx::Size& size);
    // Adds display mode and set as native mode. If an existing display mode is
    // equivalent to |mode| it will be set as native mode instead.
    Builder& SetNativeMode(std::unique_ptr<ui::DisplayMode> mode);
    // Adds display mode with |size| and set as current mode. If a display mode
    // with |size| already exists then it will be reused.
    Builder& SetCurrentMode(const gfx::Size& size);
    // Adds display mode and set as current mode. If an existing display mode is
    // equivalent to |mode| it will be set as current mode instead.
    Builder& SetCurrentMode(std::unique_ptr<ui::DisplayMode> mode);
    // Adds display mode with |size| if necessary. If a display mode with |size|
    // already exists then it will be reused.
    Builder& AddMode(const gfx::Size& size);
    // Adds |mode| if necessary. If an existing display mode is equivalent to
    // |mode| it will not be added.
    Builder& AddMode(std::unique_ptr<ui::DisplayMode> mode);
    Builder& SetOrigin(const gfx::Point& origin);
    Builder& SetType(ui::DisplayConnectionType type);
    Builder& SetIsAspectPerservingScaling(bool is_aspect_preserving_scaling);
    Builder& SetHasOverscan(bool has_overscan);
    Builder& SetHasColorCorrectionMatrix(bool val);
    Builder& SetName(const std::string& name);
    Builder& SetProductId(int64_t product_id);
    // Sets physical_size so that the screen has the specified DPI using the
    // native resolution.
    Builder& SetDPI(int dpi);
    // Sets physical_size for low DPI display.
    Builder& SetLowDPI();
    // Sets physical_size for high DPI display.
    Builder& SetHighDPI();

   private:
    // Returns a display mode with |size|. If there is no existing mode, insert
    // a display mode with |size| first.
    const ui::DisplayMode* AddOrFindDisplayMode(const gfx::Size& size);
    // Returns a display mode equivalent to |mode|. If there is no equivalent
    // display mode, insert |mode| first.
    const ui::DisplayMode* AddOrFindDisplayMode(
        std::unique_ptr<ui::DisplayMode> mode);

    int64_t id_ = Display::kInvalidDisplayID;
    gfx::Point origin_;
    float dpi_ = 96.0;
    ui::DisplayConnectionType type_ = ui::DISPLAY_CONNECTION_TYPE_UNKNOWN;
    bool is_aspect_preserving_scaling_ = false;
    bool has_overscan_ = false;
    bool has_color_correction_matrix_ = false;
    std::string name_ = "Fake Display";
    int64_t product_id_ = DisplaySnapshot::kInvalidProductID;
    DisplayModeList modes_;
    const ui::DisplayMode* current_mode_ = nullptr;
    const ui::DisplayMode* native_mode_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  FakeDisplaySnapshot(int64_t display_id,
                      const gfx::Point& origin,
                      const gfx::Size& physical_size,
                      ui::DisplayConnectionType type,
                      bool is_aspect_preserving_scaling,
                      bool has_overscan,
                      bool has_color_correction_matrix,
                      std::string display_name,
                      int64_t product_id,
                      DisplayModeList modes,
                      const ui::DisplayMode* current_mode,
                      const ui::DisplayMode* native_mode);
  ~FakeDisplaySnapshot() override;

  // Creates a display snapshot from the provided |spec| string. Returns null if
  // |spec| is invalid. See fake_display_delegate.h for |spec| format
  // description.
  static std::unique_ptr<ui::DisplaySnapshot> CreateFromSpec(
      int64_t id,
      const std::string& spec);

  // DisplaySnapshot:
  std::string ToString() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDisplaySnapshot);
};

}  // namespace display

#endif  // UI_DISPLAY_FAKE_DISPLAY_SNAPSHOT_H_
