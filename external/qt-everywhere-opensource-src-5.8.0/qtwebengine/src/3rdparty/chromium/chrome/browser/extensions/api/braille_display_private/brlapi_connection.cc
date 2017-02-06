// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"

#include <errno.h>

#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_info.h"
#include "build/build_config.h"

namespace extensions {
using base::MessageLoopForIO;
namespace api {
namespace braille_display_private {

namespace {
// Default virtual terminal.  This can be overriden by setting the
// WINDOWPATH environment variable.  This is only used when not running
// under Crhome OS (that is in aura for a Linux desktop).
// TODO(plundblad): Find a way to detect the controlling terminal of the
// X server.
static const int kDefaultTtyLinux = 7;
#if defined(OS_CHROMEOS)
// The GUI is always running on vt1 in Chrome OS.
static const int kDefaultTtyChromeOS = 1;
#endif
}  // namespace

class BrlapiConnectionImpl : public BrlapiConnection,
                             MessageLoopForIO::Watcher {
 public:
  explicit BrlapiConnectionImpl(LibBrlapiLoader* loader) :
      libbrlapi_loader_(loader) {}

  ~BrlapiConnectionImpl() override { Disconnect(); }

  ConnectResult Connect(const OnDataReadyCallback& on_data_ready) override;
  void Disconnect() override;
  bool Connected() override { return handle_ != nullptr; }
  brlapi_error_t* BrlapiError() override;
  std::string BrlapiStrError() override;
  bool GetDisplaySize(size_t* size) override;
  bool WriteDots(const unsigned char* cells) override;
  int ReadKey(brlapi_keyCode_t* keyCode) override;

  // MessageLoopForIO::Watcher
  void OnFileCanReadWithoutBlocking(int fd) override { on_data_ready_.Run(); }

  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  bool CheckConnected();
  ConnectResult ConnectResultForError();

  LibBrlapiLoader* libbrlapi_loader_;
  std::unique_ptr<brlapi_handle_t, base::FreeDeleter> handle_;
  MessageLoopForIO::FileDescriptorWatcher fd_controller_;
  OnDataReadyCallback on_data_ready_;

  DISALLOW_COPY_AND_ASSIGN(BrlapiConnectionImpl);
};

BrlapiConnection::BrlapiConnection() {
}

BrlapiConnection::~BrlapiConnection() {
}

std::unique_ptr<BrlapiConnection> BrlapiConnection::Create(
    LibBrlapiLoader* loader) {
  DCHECK(loader->loaded());
  return std::unique_ptr<BrlapiConnection>(new BrlapiConnectionImpl(loader));
}

BrlapiConnection::ConnectResult BrlapiConnectionImpl::Connect(
    const OnDataReadyCallback& on_data_ready) {
  DCHECK(!handle_);
  handle_.reset((brlapi_handle_t*) malloc(
      libbrlapi_loader_->brlapi_getHandleSize()));
  int fd = libbrlapi_loader_->brlapi__openConnection(handle_.get(), NULL, NULL);
  if (fd < 0) {
    handle_.reset();
    VLOG(1) << "Error connecting to brlapi: " << BrlapiStrError();
    return ConnectResultForError();
  }
  int path[2] = {0, 0};
  int pathElements = 0;
#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS())
    path[pathElements++] = kDefaultTtyChromeOS;
#endif
  if (pathElements == 0 && getenv("WINDOWPATH") == NULL)
    path[pathElements++] = kDefaultTtyLinux;
  if (libbrlapi_loader_->brlapi__enterTtyModeWithPath(
          handle_.get(), path, pathElements, NULL) < 0) {
    LOG(ERROR) << "brlapi: couldn't enter tty mode: " << BrlapiStrError();
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  size_t size;
  if (!GetDisplaySize(&size)) {
    // Error already logged.
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  // A display size of 0 means no display connected.  We can't reliably
  // detect when a display gets connected, so fail and let the caller
  // retry connecting.
  if (size == 0) {
    VLOG(1) << "No braille display connected";
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  const brlapi_keyCode_t extraKeys[] = {
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_OFFLINE,
      // brltty 5.1 converts dot input to Unicode characters unless we
      // explicitly accept this command.
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_PASSDOTS,
  };
  if (libbrlapi_loader_->brlapi__acceptKeys(
          handle_.get(), brlapi_rangeType_command, extraKeys,
          arraysize(extraKeys)) < 0) {
    LOG(ERROR) << "Couldn't acceptKeys: " << BrlapiStrError();
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          fd, true, MessageLoopForIO::WATCH_READ, &fd_controller_, this)) {
    LOG(ERROR) << "Couldn't watch file descriptor " << fd;
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  on_data_ready_ = on_data_ready;

  return CONNECT_SUCCESS;
}

void BrlapiConnectionImpl::Disconnect() {
  if (!handle_) {
    return;
  }
  fd_controller_.StopWatchingFileDescriptor();
  libbrlapi_loader_->brlapi__closeConnection(
      handle_.get());
  handle_.reset();
}

brlapi_error_t* BrlapiConnectionImpl::BrlapiError() {
  return libbrlapi_loader_->brlapi_error_location();
}

std::string BrlapiConnectionImpl::BrlapiStrError() {
  return libbrlapi_loader_->brlapi_strerror(BrlapiError());
}

bool BrlapiConnectionImpl::GetDisplaySize(size_t* size) {
  if (!CheckConnected()) {
    return false;
  }
  unsigned int columns, rows;
  if (libbrlapi_loader_->brlapi__getDisplaySize(
          handle_.get(), &columns, &rows) < 0) {
    LOG(ERROR) << "Couldn't get braille display size " << BrlapiStrError();
    return false;
  }
  *size = columns * rows;
  return true;
}

bool BrlapiConnectionImpl::WriteDots(const unsigned char* cells) {
  if (!CheckConnected())
    return false;
  if (libbrlapi_loader_->brlapi__writeDots(handle_.get(), cells) < 0) {
    VLOG(1) << "Couldn't write to brlapi: " << BrlapiStrError();
    return false;
  }
  return true;
}

int BrlapiConnectionImpl::ReadKey(brlapi_keyCode_t* key_code) {
  if (!CheckConnected())
    return -1;
  return libbrlapi_loader_->brlapi__readKey(
      handle_.get(), 0 /*wait*/, key_code);
}

bool BrlapiConnectionImpl::CheckConnected() {
  if (!handle_) {
    BrlapiError()->brlerrno = BRLAPI_ERROR_ILLEGAL_INSTRUCTION;
    return false;
  }
  return true;
}

BrlapiConnection::ConnectResult BrlapiConnectionImpl::ConnectResultForError() {
  const brlapi_error_t* error = BrlapiError();
  // For the majority of users, the socket file will never exist because
  // the daemon is never run.  Avoid retrying in this case, relying on
  // the socket directory to change and trigger further tries if the
  // daemon comes up later on.
  if (error->brlerrno == BRLAPI_ERROR_LIBCERR
      && error->libcerrno == ENOENT) {
    return CONNECT_ERROR_NO_RETRY;
  }
  return CONNECT_ERROR_RETRY;
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
