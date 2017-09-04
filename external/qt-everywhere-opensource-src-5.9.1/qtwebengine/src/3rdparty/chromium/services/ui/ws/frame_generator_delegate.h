// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_DELEGATE_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_DELEGATE_H_

namespace ui {
namespace ws {

class FrameGeneratorDelegate {
 public:
  virtual bool IsInHighContrastMode() = 0;

 protected:
  virtual ~FrameGeneratorDelegate() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_DELEGATE_H_
