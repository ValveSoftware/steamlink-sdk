// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/core_test_base.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/message_for_transit.h"

namespace mojo {
namespace edk {
namespace test {

namespace {

// MockDispatcher --------------------------------------------------------------

class MockDispatcher : public Dispatcher {
 public:
  static scoped_refptr<MockDispatcher> Create(
      CoreTestBase::MockHandleInfo* info) {
    return make_scoped_refptr(new MockDispatcher(info));
  }

  // Dispatcher:
  Type GetType() const override { return Type::UNKNOWN; }

  MojoResult Close() override {
    info_->IncrementCloseCallCount();
    return MOJO_RESULT_OK;
  }

  MojoResult WriteMessage(
      std::unique_ptr<MessageForTransit> message,
      MojoWriteMessageFlags /*flags*/) override {
    info_->IncrementWriteMessageCallCount();

    if (message->num_bytes() > GetConfiguration().max_message_num_bytes)
      return MOJO_RESULT_RESOURCE_EXHAUSTED;

    if (message->num_handles())
      return MOJO_RESULT_UNIMPLEMENTED;

    return MOJO_RESULT_OK;
  }

  MojoResult ReadMessage(std::unique_ptr<MessageForTransit>* message,
                         uint32_t* num_bytes,
                         MojoHandle* handle,
                         uint32_t* num_handles,
                         MojoReadMessageFlags /*flags*/,
                         bool ignore_num_bytes) override {
    info_->IncrementReadMessageCallCount();

    if (num_handles)
      *num_handles = 1;

    return MOJO_RESULT_OK;
  }

  MojoResult WriteData(const void* elements,
                       uint32_t* num_bytes,
                       MojoWriteDataFlags flags) override {
    info_->IncrementWriteDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult BeginWriteData(void** buffer,
                            uint32_t* buffer_num_bytes,
                            MojoWriteDataFlags flags) override {
    info_->IncrementBeginWriteDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult EndWriteData(uint32_t num_bytes_written) override {
    info_->IncrementEndWriteDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult ReadData(void* elements,
                      uint32_t* num_bytes,
                      MojoReadDataFlags flags) override {
    info_->IncrementReadDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult BeginReadData(const void** buffer,
                           uint32_t* buffer_num_bytes,
                           MojoReadDataFlags flags) override {
    info_->IncrementBeginReadDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult EndReadData(uint32_t num_bytes_read) override {
    info_->IncrementEndReadDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult AddAwakable(Awakable* awakable,
                         MojoHandleSignals /*signals*/,
                         uintptr_t /*context*/,
                         HandleSignalsState* signals_state) override {
    info_->IncrementAddAwakableCallCount();
    if (signals_state)
      *signals_state = HandleSignalsState();
    if (info_->IsAddAwakableAllowed()) {
      info_->AwakableWasAdded(awakable);
      return MOJO_RESULT_OK;
    }

    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  void RemoveAwakable(Awakable* /*awakable*/,
                      HandleSignalsState* signals_state) override {
    info_->IncrementRemoveAwakableCallCount();
    if (signals_state)
      *signals_state = HandleSignalsState();
  }

 private:
  explicit MockDispatcher(CoreTestBase::MockHandleInfo* info) : info_(info) {
    CHECK(info_);
    info_->IncrementCtorCallCount();
  }

  ~MockDispatcher() override { info_->IncrementDtorCallCount(); }

  CoreTestBase::MockHandleInfo* const info_;

  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

}  // namespace

// CoreTestBase ----------------------------------------------------------------

CoreTestBase::CoreTestBase() {
}

CoreTestBase::~CoreTestBase() {
}

MojoHandle CoreTestBase::CreateMockHandle(CoreTestBase::MockHandleInfo* info) {
  scoped_refptr<MockDispatcher> dispatcher = MockDispatcher::Create(info);
  return core()->AddDispatcher(dispatcher);
}

Core* CoreTestBase::core() {
  return mojo::edk::internal::g_core;
}

// CoreTestBase_MockHandleInfo -------------------------------------------------

CoreTestBase_MockHandleInfo::CoreTestBase_MockHandleInfo()
    : ctor_call_count_(0),
      dtor_call_count_(0),
      close_call_count_(0),
      write_message_call_count_(0),
      read_message_call_count_(0),
      write_data_call_count_(0),
      begin_write_data_call_count_(0),
      end_write_data_call_count_(0),
      read_data_call_count_(0),
      begin_read_data_call_count_(0),
      end_read_data_call_count_(0),
      add_awakable_call_count_(0),
      remove_awakable_call_count_(0),
      add_awakable_allowed_(false) {
}

CoreTestBase_MockHandleInfo::~CoreTestBase_MockHandleInfo() {
}

unsigned CoreTestBase_MockHandleInfo::GetCtorCallCount() const {
  base::AutoLock locker(lock_);
  return ctor_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetDtorCallCount() const {
  base::AutoLock locker(lock_);
  return dtor_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetCloseCallCount() const {
  base::AutoLock locker(lock_);
  return close_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetWriteMessageCallCount() const {
  base::AutoLock locker(lock_);
  return write_message_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetReadMessageCallCount() const {
  base::AutoLock locker(lock_);
  return read_message_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetWriteDataCallCount() const {
  base::AutoLock locker(lock_);
  return write_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetBeginWriteDataCallCount() const {
  base::AutoLock locker(lock_);
  return begin_write_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetEndWriteDataCallCount() const {
  base::AutoLock locker(lock_);
  return end_write_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetReadDataCallCount() const {
  base::AutoLock locker(lock_);
  return read_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetBeginReadDataCallCount() const {
  base::AutoLock locker(lock_);
  return begin_read_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetEndReadDataCallCount() const {
  base::AutoLock locker(lock_);
  return end_read_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetAddAwakableCallCount() const {
  base::AutoLock locker(lock_);
  return add_awakable_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetRemoveAwakableCallCount() const {
  base::AutoLock locker(lock_);
  return remove_awakable_call_count_;
}

size_t CoreTestBase_MockHandleInfo::GetAddedAwakableSize() const {
  base::AutoLock locker(lock_);
  return added_awakables_.size();
}

Awakable* CoreTestBase_MockHandleInfo::GetAddedAwakableAt(unsigned i) const {
  base::AutoLock locker(lock_);
  return added_awakables_[i];
}

void CoreTestBase_MockHandleInfo::IncrementCtorCallCount() {
  base::AutoLock locker(lock_);
  ctor_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementDtorCallCount() {
  base::AutoLock locker(lock_);
  dtor_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementCloseCallCount() {
  base::AutoLock locker(lock_);
  close_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementWriteMessageCallCount() {
  base::AutoLock locker(lock_);
  write_message_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementReadMessageCallCount() {
  base::AutoLock locker(lock_);
  read_message_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementWriteDataCallCount() {
  base::AutoLock locker(lock_);
  write_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementBeginWriteDataCallCount() {
  base::AutoLock locker(lock_);
  begin_write_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementEndWriteDataCallCount() {
  base::AutoLock locker(lock_);
  end_write_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementReadDataCallCount() {
  base::AutoLock locker(lock_);
  read_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementBeginReadDataCallCount() {
  base::AutoLock locker(lock_);
  begin_read_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementEndReadDataCallCount() {
  base::AutoLock locker(lock_);
  end_read_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementAddAwakableCallCount() {
  base::AutoLock locker(lock_);
  add_awakable_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementRemoveAwakableCallCount() {
  base::AutoLock locker(lock_);
  remove_awakable_call_count_++;
}

void CoreTestBase_MockHandleInfo::AllowAddAwakable(bool alllow) {
  base::AutoLock locker(lock_);
  add_awakable_allowed_ = alllow;
}

bool CoreTestBase_MockHandleInfo::IsAddAwakableAllowed() const {
  base::AutoLock locker(lock_);
  return add_awakable_allowed_;
}

void CoreTestBase_MockHandleInfo::AwakableWasAdded(Awakable* awakable) {
  base::AutoLock locker(lock_);
  added_awakables_.push_back(awakable);
}

}  // namespace test
}  // namespace edk
}  // namespace mojo
