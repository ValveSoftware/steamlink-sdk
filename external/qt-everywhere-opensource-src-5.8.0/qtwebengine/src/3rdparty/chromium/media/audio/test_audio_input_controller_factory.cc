// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/test_audio_input_controller_factory.h"
#include "media/audio/audio_io.h"

namespace media {

TestAudioInputController::TestAudioInputController(
    TestAudioInputControllerFactory* factory,
    AudioManager* audio_manager,
    const AudioParameters& audio_parameters,
    EventHandler* event_handler,
    SyncWriter* sync_writer,
    UserInputMonitor* user_input_monitor)
    : AudioInputController(event_handler,
                           sync_writer,
                           user_input_monitor,
                           false),
      audio_parameters_(audio_parameters),
      factory_(factory),
      event_handler_(event_handler) {
  task_runner_ = audio_manager->GetTaskRunner();
}

TestAudioInputController::~TestAudioInputController() {
  // Inform the factory so that it allows creating new instances in future.
  factory_->OnTestAudioInputControllerDestroyed(this);
}

void TestAudioInputController::Record() {
  if (factory_->delegate_)
    factory_->delegate_->TestAudioControllerOpened(this);
}

void TestAudioInputController::Close(const base::Closure& closed_task) {
  task_runner_->PostTask(FROM_HERE, closed_task);
  if (factory_->delegate_)
    factory_->delegate_->TestAudioControllerClosed(this);
}

TestAudioInputControllerFactory::TestAudioInputControllerFactory()
    : controller_(NULL),
      delegate_(NULL) {
}

TestAudioInputControllerFactory::~TestAudioInputControllerFactory() {
  DCHECK(!controller_);
}

AudioInputController* TestAudioInputControllerFactory::Create(
    AudioManager* audio_manager,
    AudioInputController::EventHandler* event_handler,
    AudioParameters params,
    UserInputMonitor* user_input_monitor) {
  DCHECK(!controller_);  // Only one test instance managed at a time.
  controller_ = new TestAudioInputController(
      this, audio_manager, params, event_handler, NULL, user_input_monitor);
  return controller_;
}

void TestAudioInputControllerFactory::OnTestAudioInputControllerDestroyed(
    TestAudioInputController* controller) {
  DCHECK_EQ(controller_, controller);
  controller_ = NULL;
}

}  // namespace media
