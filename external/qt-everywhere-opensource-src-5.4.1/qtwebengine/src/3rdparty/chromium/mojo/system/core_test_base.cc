// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core_test_base.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/constants.h"
#include "mojo/system/core.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/memory.h"

namespace mojo {
namespace system {
namespace test {

namespace {

// MockDispatcher --------------------------------------------------------------

class MockDispatcher : public Dispatcher {
 public:
  explicit MockDispatcher(CoreTestBase::MockHandleInfo* info)
      : info_(info) {
    CHECK(info_);
    info_->IncrementCtorCallCount();
  }

  // |Dispatcher| private methods:
  virtual Type GetType() const OVERRIDE {
    return kTypeUnknown;
  }

 private:
  virtual ~MockDispatcher() {
    info_->IncrementDtorCallCount();
  }

  // |Dispatcher| protected methods:
  virtual void CloseImplNoLock() OVERRIDE {
    info_->IncrementCloseCallCount();
    lock().AssertAcquired();
  }

  virtual MojoResult WriteMessageImplNoLock(
      const void* bytes,
      uint32_t num_bytes,
      std::vector<DispatcherTransport>* transports,
      MojoWriteMessageFlags /*flags*/) OVERRIDE {
    info_->IncrementWriteMessageCallCount();
    lock().AssertAcquired();

    if (!VerifyUserPointerWithSize<1>(bytes, num_bytes))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (num_bytes > kMaxMessageNumBytes)
      return MOJO_RESULT_RESOURCE_EXHAUSTED;

    if (transports)
      return MOJO_RESULT_UNIMPLEMENTED;

    return MOJO_RESULT_OK;
  }

  virtual MojoResult ReadMessageImplNoLock(
      void* bytes,
      uint32_t* num_bytes,
      DispatcherVector* /*dispatchers*/,
      uint32_t* /*num_dispatchers*/,
      MojoReadMessageFlags /*flags*/) OVERRIDE {
    info_->IncrementReadMessageCallCount();
    lock().AssertAcquired();

    if (num_bytes && !VerifyUserPointerWithSize<1>(bytes, *num_bytes))
      return MOJO_RESULT_INVALID_ARGUMENT;

    return MOJO_RESULT_OK;
  }

  virtual MojoResult WriteDataImplNoLock(
      const void* /*elements*/,
      uint32_t* /*num_bytes*/,
      MojoWriteDataFlags /*flags*/) OVERRIDE {
    info_->IncrementWriteDataCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  virtual MojoResult BeginWriteDataImplNoLock(
      void** /*buffer*/,
      uint32_t* /*buffer_num_bytes*/,
      MojoWriteDataFlags /*flags*/) OVERRIDE {
    info_->IncrementBeginWriteDataCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  virtual MojoResult EndWriteDataImplNoLock(
      uint32_t /*num_bytes_written*/) OVERRIDE {
    info_->IncrementEndWriteDataCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  virtual MojoResult ReadDataImplNoLock(void* /*elements*/,
                                        uint32_t* /*num_bytes*/,
                                        MojoReadDataFlags /*flags*/) OVERRIDE {
    info_->IncrementReadDataCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  virtual MojoResult BeginReadDataImplNoLock(
      const void** /*buffer*/,
      uint32_t* /*buffer_num_bytes*/,
      MojoReadDataFlags /*flags*/) OVERRIDE {
    info_->IncrementBeginReadDataCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  virtual MojoResult EndReadDataImplNoLock(
      uint32_t /*num_bytes_read*/) OVERRIDE {
    info_->IncrementEndReadDataCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  virtual MojoResult AddWaiterImplNoLock(Waiter* /*waiter*/,
                                         MojoHandleSignals /*signals*/,
                                         uint32_t /*context*/) OVERRIDE {
    info_->IncrementAddWaiterCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  virtual void RemoveWaiterImplNoLock(Waiter* /*waiter*/) OVERRIDE {
    info_->IncrementRemoveWaiterCallCount();
    lock().AssertAcquired();
  }

  virtual void CancelAllWaitersNoLock() OVERRIDE {
    info_->IncrementCancelAllWaitersCallCount();
    lock().AssertAcquired();
  }

  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE {
    return scoped_refptr<Dispatcher>(new MockDispatcher(info_));
  }

  CoreTestBase::MockHandleInfo* const info_;

  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

}  // namespace

// CoreTestBase ----------------------------------------------------------------

CoreTestBase::CoreTestBase() {
}

CoreTestBase::~CoreTestBase() {
}

void CoreTestBase::SetUp() {
  core_ = new Core();
}

void CoreTestBase::TearDown() {
  delete core_;
  core_ = NULL;
}

MojoHandle CoreTestBase::CreateMockHandle(CoreTestBase::MockHandleInfo* info) {
  CHECK(core_);
  scoped_refptr<MockDispatcher> dispatcher(new MockDispatcher(info));
  return core_->AddDispatcher(dispatcher);
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
      add_waiter_call_count_(0),
      remove_waiter_call_count_(0),
      cancel_all_waiters_call_count_(0) {
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

unsigned CoreTestBase_MockHandleInfo::GetAddWaiterCallCount() const {
  base::AutoLock locker(lock_);
  return add_waiter_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetRemoveWaiterCallCount() const {
  base::AutoLock locker(lock_);
  return remove_waiter_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetCancelAllWaitersCallCount() const {
  base::AutoLock locker(lock_);
  return cancel_all_waiters_call_count_;
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

void CoreTestBase_MockHandleInfo::IncrementAddWaiterCallCount() {
  base::AutoLock locker(lock_);
  add_waiter_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementRemoveWaiterCallCount() {
  base::AutoLock locker(lock_);
  remove_waiter_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementCancelAllWaitersCallCount() {
  base::AutoLock locker(lock_);
  cancel_all_waiters_call_count_++;
}

}  // namespace test
}  // namespace system
}  // namespace mojo
