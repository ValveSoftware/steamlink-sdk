// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "secondary_tile.h"

#include <windows.ui.startscreen.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"
#include "win8/metro_driver/chrome_app_view.h"
#include "win8/metro_driver/winrt_utils.h"

namespace {

using base::win::MetroPinUmaResultCallback;

// Callback for asynchronous pin requests.
class TileRequestCompleter {
 public:
  enum PinType {
    PIN,
    UNPIN
  };
  TileRequestCompleter(PinType type, const MetroPinUmaResultCallback& callback)
      : type_(type), callback_(callback) {}

  void Complete(mswr::ComPtr<winfoundtn::IAsyncOperation<bool>>& completion);

 private:
  // Callback that responds to user input on the pin request pop-up. This will
  // run |callback_|, then delete |this| before returning.
  HRESULT Respond(winfoundtn::IAsyncOperation<bool>* async,
                  AsyncStatus status);

  PinType type_;
  MetroPinUmaResultCallback callback_;
};

void TileRequestCompleter::Complete(
    mswr::ComPtr<winfoundtn::IAsyncOperation<bool>>& completion) {
  typedef winfoundtn::IAsyncOperationCompletedHandler<bool> RequestDoneType;
  mswr::ComPtr<RequestDoneType> handler(mswr::Callback<RequestDoneType>(
      this, &TileRequestCompleter::Respond));
  DCHECK(handler.Get() != NULL);
  HRESULT hr = completion->put_Completed(handler.Get());
  CheckHR(hr, "Failed to put_Completed");
}

HRESULT TileRequestCompleter::Respond(winfoundtn::IAsyncOperation<bool>* async,
                                 AsyncStatus status) {
  base::win::MetroSecondaryTilePinUmaResult pin_state =
      base::win::METRO_PIN_STATE_NONE;

  if (status == Completed) {
    unsigned char result;
    CheckHR(async->GetResults(&result));
    LOG(INFO) << __FUNCTION__ << " result " << static_cast<int>(result);
    switch (result) {
      case 0:
        pin_state = type_ == PIN ?
            base::win::METRO_PIN_RESULT_CANCEL :
            base::win::METRO_UNPIN_RESULT_CANCEL;
        break;
      case 1:
        pin_state = type_ == PIN ?
            base::win::METRO_PIN_RESULT_OK :
            base::win::METRO_UNPIN_RESULT_OK;
        break;
      default:
        pin_state = type_ == PIN ?
            base::win::METRO_PIN_RESULT_OTHER :
            base::win::METRO_UNPIN_RESULT_OTHER;
        break;
    }
  } else {
    LOG(ERROR) << __FUNCTION__ << " Unexpected async status "
               << static_cast<int>(status);
    pin_state = type_ == PIN ?
        base::win::METRO_PIN_RESULT_ERROR :
        base::win::METRO_UNPIN_RESULT_ERROR;
  }
  callback_.Run(pin_state);

  delete this;
  return S_OK;
}

void DeleteTileFromStartScreen(const base::string16& tile_id,
                               const MetroPinUmaResultCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  mswr::ComPtr<winui::StartScreen::ISecondaryTileFactory> tile_factory;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_StartScreen_SecondaryTile,
      tile_factory.GetAddressOf());
  CheckHR(hr, "Failed to create instance of ISecondaryTileFactory");

  mswrw::HString id;
  id.Attach(MakeHString(tile_id));

  mswr::ComPtr<winui::StartScreen::ISecondaryTile> tile;
  hr = tile_factory->CreateWithId(id.Get(), tile.GetAddressOf());
  CheckHR(hr, "Failed to create tile");

  mswr::ComPtr<winfoundtn::IAsyncOperation<bool>> completion;
  hr = tile->RequestDeleteAsync(completion.GetAddressOf());
  CheckHR(hr, "RequestDeleteAsync failed");

  if (FAILED(hr)) {
    callback.Run(base::win::METRO_UNPIN_REQUEST_SHOW_ERROR);
    return;
  }

  // Deleted in TileRequestCompleter::Respond when the async operation
  // completes.
  TileRequestCompleter* completer =
      new TileRequestCompleter(TileRequestCompleter::UNPIN, callback);
  completer->Complete(completion);
}

void CreateTileOnStartScreen(const base::string16& tile_id,
                             const base::string16& title_str,
                             const base::string16& url_str,
                             const base::FilePath& logo_path,
                             const MetroPinUmaResultCallback& callback) {
  VLOG(1) << __FUNCTION__;

  mswr::ComPtr<winui::StartScreen::ISecondaryTileFactory> tile_factory;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_StartScreen_SecondaryTile,
      tile_factory.GetAddressOf());
  CheckHR(hr, "Failed to create instance of ISecondaryTileFactory");

  winui::StartScreen::TileOptions options =
      winui::StartScreen::TileOptions_ShowNameOnLogo;
  mswrw::HString title;
  title.Attach(MakeHString(title_str));

  mswrw::HString id;
  id.Attach(MakeHString(tile_id));

  mswrw::HString args;
  // The url is just passed into the tile agruments as is. Metro and desktop
  // chrome will see the arguments as command line parameters.
  // A GURL is used to ensure any spaces are properly escaped.
  GURL url(url_str);
  args.Attach(MakeHString(base::UTF8ToUTF16(url.spec())));

  mswr::ComPtr<winfoundtn::IUriRuntimeClassFactory> uri_factory;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Foundation_Uri,
      uri_factory.GetAddressOf());
  CheckHR(hr, "Failed to create URIFactory");

  mswrw::HString logo_url;
  logo_url.Attach(
      MakeHString(base::string16(L"file:///").append(logo_path.value())));
  mswr::ComPtr<winfoundtn::IUriRuntimeClass> uri;
  hr = uri_factory->CreateUri(logo_url.Get(), &uri);
  CheckHR(hr, "Failed to create URI");

  mswr::ComPtr<winui::StartScreen::ISecondaryTile> tile;
  hr = tile_factory->CreateTile(id.Get(),
                                title.Get(),
                                title.Get(),
                                args.Get(),
                                options,
                                uri.Get(),
                                tile.GetAddressOf());
  CheckHR(hr, "Failed to create tile");

  hr = tile->put_ForegroundText(winui::StartScreen::ForegroundText_Light);
  CheckHR(hr, "Failed to change foreground text color");

  mswr::ComPtr<winfoundtn::IAsyncOperation<bool>> completion;
  hr = tile->RequestCreateAsync(completion.GetAddressOf());
  CheckHR(hr, "RequestCreateAsync failed");

  if (FAILED(hr)) {
    callback.Run(base::win::METRO_PIN_REQUEST_SHOW_ERROR);
    return;
  }

  // Deleted in TileRequestCompleter::Respond when the async operation
  // completes.
  TileRequestCompleter* completer =
      new TileRequestCompleter(TileRequestCompleter::PIN, callback);
  completer->Complete(completion);
}

}  // namespace

BOOL MetroIsPinnedToStartScreen(const base::string16& tile_id) {
  mswr::ComPtr<winui::StartScreen::ISecondaryTileStatics> tile_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_StartScreen_SecondaryTile,
      tile_statics.GetAddressOf());
  CheckHR(hr, "Failed to create instance of ISecondaryTileStatics");

  boolean exists;
  hr = tile_statics->Exists(MakeHString(tile_id), &exists);
  CheckHR(hr, "ISecondaryTileStatics.Exists failed");
  return exists;
}

void MetroUnPinFromStartScreen(const base::string16& tile_id,
                               const MetroPinUmaResultCallback& callback) {
  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&DeleteTileFromStartScreen,
                            tile_id,
                            callback));
}

void MetroPinToStartScreen(const base::string16& tile_id,
                           const base::string16& title,
                           const base::string16& url,
                           const base::FilePath& logo_path,
                           const MetroPinUmaResultCallback& callback) {
  globals.appview_msg_loop->PostTask(
    FROM_HERE, base::Bind(&CreateTileOnStartScreen,
                          tile_id,
                          title,
                          url,
                          logo_path,
                          callback));
}
