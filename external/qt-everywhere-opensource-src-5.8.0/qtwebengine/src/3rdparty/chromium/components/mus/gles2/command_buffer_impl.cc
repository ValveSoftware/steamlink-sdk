// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/gpu_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"

namespace mus {

namespace {

uint64_t g_next_command_buffer_id = 0;

void RunInitializeCallback(
    const mojom::CommandBuffer::InitializeCallback& callback,
    mojom::CommandBufferInitializeResultPtr result) {
  callback.Run(std::move(result));
}

void RunMakeProgressCallback(
    const mojom::CommandBuffer::MakeProgressCallback& callback,
    const gpu::CommandBuffer::State& state) {
  callback.Run(state);
}

}  // namespace

CommandBufferImpl::CommandBufferImpl(
    mojo::InterfaceRequest<mus::mojom::CommandBuffer> request,
    scoped_refptr<GpuState> gpu_state)
    : gpu_state_(gpu_state) {
  // Bind |CommandBufferImpl| to the |request| in the GPU control thread.
  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferImpl::BindToRequest,
                 base::Unretained(this), base::Passed(&request)));
}

void CommandBufferImpl::DidLoseContext(uint32_t reason) {
  driver_->set_client(nullptr);
  client_->Destroyed(reason, gpu::error::kLostContext);
}

void CommandBufferImpl::UpdateVSyncParameters(const base::TimeTicks& timebase,
                                              const base::TimeDelta& interval) {
}

void CommandBufferImpl::OnGpuCompletedSwapBuffers(gfx::SwapResult result) {}

CommandBufferImpl::~CommandBufferImpl() {
}

void CommandBufferImpl::Initialize(
    mus::mojom::CommandBufferClientPtr client,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs,
    const mojom::CommandBuffer::InitializeCallback& callback) {
  gpu_state_->command_buffer_task_runner()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferImpl::InitializeOnGpuThread,
                 base::Unretained(this), base::Passed(&client),
                 base::Passed(&shared_state), base::Passed(&attribs),
                 base::Bind(&RunInitializeCallback, callback)));
}

void CommandBufferImpl::SetGetBuffer(int32_t buffer) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferImpl::SetGetBufferOnGpuThread,
                                base::Unretained(this), buffer));
}

void CommandBufferImpl::Flush(int32_t put_offset) {
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  const uint32_t order_num = driver_->sync_point_order_data()
      ->GenerateUnprocessedOrderNumber(sync_point_manager);
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferImpl::FlushOnGpuThread,
                                base::Unretained(this), put_offset, order_num));
}

void CommandBufferImpl::MakeProgress(
    int32_t last_get_offset,
    const mojom::CommandBuffer::MakeProgressCallback& callback) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferImpl::MakeProgressOnGpuThread,
                                base::Unretained(this), last_get_offset,
                                base::Bind(&RunMakeProgressCallback,
                                           callback)));
}

void CommandBufferImpl::RegisterTransferBuffer(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::RegisterTransferBufferOnGpuThread,
                 base::Unretained(this), id, base::Passed(&transfer_buffer),
                 size));
}

void CommandBufferImpl::DestroyTransferBuffer(int32_t id) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::DestroyTransferBufferOnGpuThread,
                 base::Unretained(this), id));
}

void CommandBufferImpl::CreateImage(int32_t id,
                                    mojo::ScopedHandle memory_handle,
                                    int32_t type,
                                    const gfx::Size& size,
                                    int32_t format,
                                    int32_t internal_format) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::CreateImageOnGpuThread,
                 base::Unretained(this), id, base::Passed(&memory_handle), type,
                 size, format, internal_format));
}

void CommandBufferImpl::DestroyImage(int32_t id) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferImpl::DestroyImageOnGpuThread,
                                base::Unretained(this), id));
}

void CommandBufferImpl::CreateStreamTexture(
    uint32_t client_texture_id,
    const mojom::CommandBuffer::CreateStreamTextureCallback& callback) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::TakeFrontBuffer(const gpu::Mailbox& mailbox) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                                          bool is_lost) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::SignalQuery(uint32_t query, uint32_t signal_id) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::SignalSyncToken(const gpu::SyncToken& sync_token,
                                        uint32_t signal_id) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::WaitForGetOffsetInRange(
    int32_t start, int32_t end,
    const mojom::CommandBuffer::WaitForGetOffsetInRangeCallback& callback) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::WaitForTokenInRange(
    int32_t start, int32_t end,
    const mojom::CommandBuffer::WaitForGetOffsetInRangeCallback& callback) {
  NOTIMPLEMENTED();
}

void CommandBufferImpl::BindToRequest(
    mojo::InterfaceRequest<mus::mojom::CommandBuffer> request) {
  binding_.reset(
      new mojo::Binding<mus::mojom::CommandBuffer>(this, std::move(request)));
  binding_->set_connection_error_handler(
      base::Bind(&CommandBufferImpl::OnConnectionError,
                 base::Unretained(this)));
}

void CommandBufferImpl::InitializeOnGpuThread(
    mojom::CommandBufferClientPtr client,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs,
    const base::Callback<
        void(mojom::CommandBufferInitializeResultPtr)>& callback) {
  DCHECK(!driver_);
  driver_.reset(new CommandBufferDriver(
      gpu::CommandBufferNamespace::MOJO,
      gpu::CommandBufferId::FromUnsafeValue(++g_next_command_buffer_id),
      gfx::kNullAcceleratedWidget, gpu_state_));
  driver_->set_client(this);
  client_ = mojo::MakeProxy(client.PassInterface());
  bool result =
      driver_->Initialize(std::move(shared_state), std::move(attribs));
  mojom::CommandBufferInitializeResultPtr initialize_result;
  if (result) {
    initialize_result = mojom::CommandBufferInitializeResult::New();
    initialize_result->command_buffer_namespace = driver_->GetNamespaceID();
    initialize_result->command_buffer_id =
        driver_->GetCommandBufferID().GetUnsafeValue();
    initialize_result->capabilities = driver_->GetCapabilities();
  }
  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&initialize_result)));
}

bool CommandBufferImpl::SetGetBufferOnGpuThread(int32_t buffer) {
  DCHECK(driver_->IsScheduled());
  driver_->SetGetBuffer(buffer);
  return true;
}

bool CommandBufferImpl::FlushOnGpuThread(int32_t put_offset,
                                         uint32_t order_num) {
  DCHECK(driver_->IsScheduled());
  driver_->sync_point_order_data()->BeginProcessingOrderNumber(order_num);
  driver_->Flush(put_offset);

  // Return false if the Flush is not finished, so the CommandBufferTaskRunner
  // will not remove this task from the task queue.
  const bool complete = !driver_->HasUnprocessedCommands();
  if (!complete)
    driver_->sync_point_order_data()->PauseProcessingOrderNumber(order_num);
  else
    driver_->sync_point_order_data()->FinishProcessingOrderNumber(order_num);
  return complete;
}

bool CommandBufferImpl::MakeProgressOnGpuThread(
    int32_t last_get_offset,
    const base::Callback<void(const gpu::CommandBuffer::State&)>& callback) {
  DCHECK(driver_->IsScheduled());
  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE, base::Bind(callback, driver_->GetLastState()));
  return true;
}

bool CommandBufferImpl::RegisterTransferBufferOnGpuThread(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(driver_->IsScheduled());
  driver_->RegisterTransferBuffer(id, std::move(transfer_buffer), size);
  return true;
}

bool CommandBufferImpl::DestroyTransferBufferOnGpuThread(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyTransferBuffer(id);
  return true;
}

bool CommandBufferImpl::CreateImageOnGpuThread(int32_t id,
                                               mojo::ScopedHandle memory_handle,
                                               int32_t type,
                                               const gfx::Size& size,
                                               int32_t format,
                                               int32_t internal_format) {
  DCHECK(driver_->IsScheduled());
  driver_->CreateImage(id, std::move(memory_handle), type, std::move(size),
                       format, internal_format);
  return true;
}

bool CommandBufferImpl::DestroyImageOnGpuThread(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyImage(id);
  return true;
}

void CommandBufferImpl::OnConnectionError() {
  // OnConnectionError() is called on the control thread |control_task_runner|.

  // Before deleting, we need to delete |binding_| because it is bound to the
  // current thread (|control_task_runner|).
  binding_.reset();

  // Objects we own (such as CommandBufferDriver) need to be destroyed on the
  // thread we were created on. It's entirely possible we haven't or are in the
  // process of creating |driver_|.
  if (driver_) {
    gpu_state_->command_buffer_task_runner()->PostTask(
        driver_.get(), base::Bind(&CommandBufferImpl::DeleteOnGpuThread,
                                  base::Unretained(this)));
  } else {
    gpu_state_->command_buffer_task_runner()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&CommandBufferImpl::DeleteOnGpuThread2,
                              base::Unretained(this)));
  }
}

bool CommandBufferImpl::DeleteOnGpuThread() {
  delete this;
  return true;
}

void CommandBufferImpl::DeleteOnGpuThread2() {
  delete this;
}

}  // namespace mus
