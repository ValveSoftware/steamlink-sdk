// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_keycode_map.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
using content::BrowserThread;
using base::Time;
using base::TimeDelta;
namespace api {
namespace braille_display_private {

namespace {
// Delay between detecting a directory update and trying to connect
// to the brlapi.
const int64_t kConnectionDelayMs = 500;
// How long to periodically retry connecting after a brltty restart.
// Some displays are slow to connect.
const int64_t kConnectRetryTimeout = 20000;
}  // namespace

BrailleController::BrailleController() {
}

BrailleController::~BrailleController() {
}

// static
BrailleController* BrailleController::GetInstance() {
  return BrailleControllerImpl::GetInstance();
}

// static
BrailleControllerImpl* BrailleControllerImpl::GetInstance() {
  return base::Singleton<
      BrailleControllerImpl,
      base::LeakySingletonTraits<BrailleControllerImpl>>::get();
}

BrailleControllerImpl::BrailleControllerImpl()
    : started_connecting_(false),
      connect_scheduled_(false) {
  create_brlapi_connection_function_ = base::Bind(
      &BrailleControllerImpl::CreateBrlapiConnection,
      base::Unretained(this));
}

BrailleControllerImpl::~BrailleControllerImpl() {
}

void BrailleControllerImpl::TryLoadLibBrlApi() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (libbrlapi_loader_.loaded())
    return;
  // These versions of libbrlapi work the same for the functions we
  // are using.  (0.6.0 adds brlapi_writeWText).
  static const char* const kSupportedVersions[] = {
    "libbrlapi.so.0.5",
    "libbrlapi.so.0.6"
  };
  for (size_t i = 0; i < arraysize(kSupportedVersions); ++i) {
    if (libbrlapi_loader_.Load(kSupportedVersions[i]))
      return;
  }
  LOG(WARNING) << "Couldn't load libbrlapi: " << strerror(errno);
}

std::unique_ptr<DisplayState> BrailleControllerImpl::GetDisplayState() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartConnecting();
  std::unique_ptr<DisplayState> display_state(new DisplayState);
  if (connection_.get() && connection_->Connected()) {
    size_t size;
    if (!connection_->GetDisplaySize(&size)) {
      Disconnect();
    } else if (size > 0) {  // size == 0 means no display present.
      display_state->available = true;
      display_state->text_cell_count.reset(new int(size));
    }
  }
  return display_state;
}

void BrailleControllerImpl::WriteDots(const std::vector<char>& cells) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (connection_ && connection_->Connected()) {
    size_t size;
    if (!connection_->GetDisplaySize(&size)) {
      Disconnect();
    }
    std::vector<unsigned char> sizedCells(size);
    std::memcpy(&sizedCells[0], cells.data(), std::min(cells.size(), size));
    if (size > cells.size())
      std::fill(sizedCells.begin() + cells.size(), sizedCells.end(), 0);
    if (!connection_->WriteDots(&sizedCells[0]))
      Disconnect();
  }
}

void BrailleControllerImpl::AddObserver(BrailleObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                               base::Bind(
                                   &BrailleControllerImpl::StartConnecting,
                                   base::Unretained(this)))) {
    NOTREACHED();
  }
  observers_.AddObserver(observer);
}

void BrailleControllerImpl::RemoveObserver(BrailleObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void BrailleControllerImpl::SetCreateBrlapiConnectionForTesting(
    const CreateBrlapiConnectionFunction& function) {
  if (function.is_null()) {
    create_brlapi_connection_function_ = base::Bind(
        &BrailleControllerImpl::CreateBrlapiConnection,
        base::Unretained(this));
  } else {
    create_brlapi_connection_function_ = function;
  }
}

void BrailleControllerImpl::PokeSocketDirForTesting() {
  OnSocketDirChangedOnIOThread();
}

void BrailleControllerImpl::StartConnecting() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (started_connecting_)
    return;
  started_connecting_ = true;
  TryLoadLibBrlApi();
  if (!libbrlapi_loader_.loaded()) {
    return;
  }
  // Only try to connect after we've started to watch the
  // socket directory.  This is necessary to avoid a race condition
  // and because we don't retry to connect after errors that will
  // persist until there's a change to the socket directory (i.e.
  // ENOENT).
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &BrailleControllerImpl::StartWatchingSocketDirOnFileThread,
          base::Unretained(this)),
      base::Bind(
          &BrailleControllerImpl::TryToConnect,
          base::Unretained(this)));
  ResetRetryConnectHorizon();
}

void BrailleControllerImpl::StartWatchingSocketDirOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath brlapi_dir(BRLAPI_SOCKETPATH);
  if (!file_path_watcher_.Watch(
          brlapi_dir, false, base::Bind(
              &BrailleControllerImpl::OnSocketDirChangedOnFileThread,
              base::Unretained(this)))) {
    LOG(WARNING) << "Couldn't watch brlapi directory " << BRLAPI_SOCKETPATH;
  }
}

void BrailleControllerImpl::OnSocketDirChangedOnFileThread(
    const base::FilePath& path, bool error) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (error) {
    LOG(ERROR) << "Error watching brlapi directory: " << path.value();
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(
          &BrailleControllerImpl::OnSocketDirChangedOnIOThread,
          base::Unretained(this)));
}

void BrailleControllerImpl::OnSocketDirChangedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "BrlAPI directory changed";
  // Every directory change resets the max retry time to the appropriate delay
  // into the future.
  ResetRetryConnectHorizon();
  // Try after an initial delay to give the driver a chance to connect.
  ScheduleTryToConnect();
}

void BrailleControllerImpl::TryToConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(libbrlapi_loader_.loaded());
  connect_scheduled_ = false;
  if (!connection_.get())
    connection_ = create_brlapi_connection_function_.Run();
  if (connection_.get() && !connection_->Connected()) {
    VLOG(1) << "Trying to connect to brlapi";
    BrlapiConnection::ConnectResult result = connection_->Connect(base::Bind(
        &BrailleControllerImpl::DispatchKeys,
        base::Unretained(this)));
    switch (result) {
      case BrlapiConnection::CONNECT_SUCCESS:
        DispatchOnDisplayStateChanged(GetDisplayState());
        break;
      case BrlapiConnection::CONNECT_ERROR_NO_RETRY:
        break;
      case BrlapiConnection::CONNECT_ERROR_RETRY:
        ScheduleTryToConnect();
        break;
      default:
        NOTREACHED();
    }
  }
}

void BrailleControllerImpl::ResetRetryConnectHorizon() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  retry_connect_horizon_ = Time::Now() + TimeDelta::FromMilliseconds(
      kConnectRetryTimeout);
}

void BrailleControllerImpl::ScheduleTryToConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TimeDelta delay(TimeDelta::FromMilliseconds(kConnectionDelayMs));
  // Don't reschedule if there's already a connect scheduled or
  // the next attempt would fall outside of the retry limit.
  if (connect_scheduled_)
    return;
  if (Time::Now() + delay > retry_connect_horizon_) {
    VLOG(1) << "Stopping to retry to connect to brlapi";
    return;
  }
  VLOG(1) << "Scheduling connection retry to brlapi";
  connect_scheduled_ = true;
  BrowserThread::PostDelayedTask(BrowserThread::IO, FROM_HERE,
                                 base::Bind(
                                     &BrailleControllerImpl::TryToConnect,
                                     base::Unretained(this)),
                                 delay);
}

void BrailleControllerImpl::Disconnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!connection_ || !connection_->Connected())
    return;
  connection_->Disconnect();
  DispatchOnDisplayStateChanged(
      std::unique_ptr<DisplayState>(new DisplayState()));
}

std::unique_ptr<BrlapiConnection>
BrailleControllerImpl::CreateBrlapiConnection() {
  DCHECK(libbrlapi_loader_.loaded());
  return BrlapiConnection::Create(&libbrlapi_loader_);
}

void BrailleControllerImpl::DispatchKeys() {
  DCHECK(connection_.get());
  brlapi_keyCode_t code;
  while (true) {
    int result = connection_->ReadKey(&code);
    if (result < 0) {  // Error.
      brlapi_error_t* err = connection_->BrlapiError();
      if (err->brlerrno == BRLAPI_ERROR_LIBCERR && err->libcerrno == EINTR)
        continue;
      // Disconnect on other errors.
      VLOG(1) << "BrlAPI error: " << connection_->BrlapiStrError();
      Disconnect();
      return;
    } else if (result == 0) {  // No more data.
      return;
    }
    std::unique_ptr<KeyEvent> event = BrlapiKeyCodeToEvent(code);
    if (event)
      DispatchKeyEvent(std::move(event));
  }
}

void BrailleControllerImpl::DispatchKeyEvent(std::unique_ptr<KeyEvent> event) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(
                                &BrailleControllerImpl::DispatchKeyEvent,
                                base::Unretained(this),
                                base::Passed(&event)));
    return;
  }
  VLOG(1) << "Dispatching key event: " << *event->ToValue();
  FOR_EACH_OBSERVER(BrailleObserver, observers_, OnBrailleKeyEvent(*event));
}

void BrailleControllerImpl::DispatchOnDisplayStateChanged(
    std::unique_ptr<DisplayState> new_state) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (!BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&BrailleControllerImpl::DispatchOnDisplayStateChanged,
                       base::Unretained(this),
                       base::Passed(&new_state)))) {
      NOTREACHED();
    }
    return;
  }
  FOR_EACH_OBSERVER(BrailleObserver, observers_,
                    OnBrailleDisplayStateChanged(*new_state));
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
