// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component_patcher.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/update_client.h"

namespace update_client {

namespace {

// Deserialize the commands file (present in delta update packages). The top
// level must be a list.
base::ListValue* ReadCommands(const base::FilePath& unpack_path) {
  const base::FilePath commands =
      unpack_path.Append(FILE_PATH_LITERAL("commands.json"));
  if (!base::PathExists(commands))
    return NULL;

  JSONFileValueDeserializer deserializer(commands);
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, NULL);

  return (root.get() && root->IsType(base::Value::TYPE_LIST))
             ? static_cast<base::ListValue*>(root.release())
             : NULL;
}

}  // namespace

ComponentPatcher::ComponentPatcher(
    const base::FilePath& input_dir,
    const base::FilePath& unpack_dir,
    scoped_refptr<CrxInstaller> installer,
    scoped_refptr<OutOfProcessPatcher> out_of_process_patcher,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : input_dir_(input_dir),
      unpack_dir_(unpack_dir),
      installer_(installer),
      out_of_process_patcher_(out_of_process_patcher),
      task_runner_(task_runner) {
}

ComponentPatcher::~ComponentPatcher() {
}

void ComponentPatcher::Start(const ComponentUnpacker::Callback& callback) {
  callback_ = callback;
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&ComponentPatcher::StartPatching,
                                    scoped_refptr<ComponentPatcher>(this)));
}

void ComponentPatcher::StartPatching() {
  commands_.reset(ReadCommands(input_dir_));
  if (!commands_.get()) {
    DonePatching(ComponentUnpacker::kDeltaBadCommands, 0);
  } else {
    next_command_ = commands_->begin();
    PatchNextFile();
  }
}

void ComponentPatcher::PatchNextFile() {
  if (next_command_ == commands_->end()) {
    DonePatching(ComponentUnpacker::kNone, 0);
    return;
  }
  const base::DictionaryValue* command_args;
  if (!(*next_command_)->GetAsDictionary(&command_args)) {
    DonePatching(ComponentUnpacker::kDeltaBadCommands, 0);
    return;
  }

  std::string operation;
  if (command_args->GetString(kOp, &operation)) {
    current_operation_ =
        CreateDeltaUpdateOp(operation, out_of_process_patcher_);
  }

  if (!current_operation_.get()) {
    DonePatching(ComponentUnpacker::kDeltaUnsupportedCommand, 0);
    return;
  }
  current_operation_->Run(command_args, input_dir_, unpack_dir_, installer_,
                          base::Bind(&ComponentPatcher::DonePatchingFile,
                                     scoped_refptr<ComponentPatcher>(this)),
                          task_runner_);
}

void ComponentPatcher::DonePatchingFile(ComponentUnpacker::Error error,
                                        int extended_error) {
  if (error != ComponentUnpacker::kNone) {
    DonePatching(error, extended_error);
  } else {
    ++next_command_;
    PatchNextFile();
  }
}

void ComponentPatcher::DonePatching(ComponentUnpacker::Error error,
                                    int extended_error) {
  current_operation_ = NULL;
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback_, error, extended_error));
  callback_.Reset();
}

}  // namespace update_client
