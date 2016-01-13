// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WallpaperUtil = {};

/**
 * Saves value to local storage that associates with key.
 * @param {string} key The key that associates with value.
 * @param {string} value The value to save to local storage.
 * @param {boolen} sync True if the value is saved to sync storage.
 * @param {function=} opt_callback The callback on success, or on failure.
 */
WallpaperUtil.saveToStorage = function(key, value, sync, opt_callback) {
  var items = {};
  items[key] = value;
  if (sync)
    Constants.WallpaperSyncStorage.set(items, opt_callback);
  else
    Constants.WallpaperLocalStorage.set(items, opt_callback);
};

/**
 * Saves user's wallpaper infomation to local and sync storage. Note that local
 * value should be saved first.
 * @param {string} url The url address of wallpaper. For custom wallpaper, it is
 *     the file name.
 * @param {string} layout The wallpaper layout.
 * @param {string} source The wallpaper source.
 */
WallpaperUtil.saveWallpaperInfo = function(url, layout, source) {
  var wallpaperInfo = {
      url: url,
      layout: layout,
      source: source
  };
  WallpaperUtil.saveToStorage(Constants.AccessLocalWallpaperInfoKey,
                              wallpaperInfo, false, function() {
    WallpaperUtil.saveToStorage(Constants.AccessSyncWallpaperInfoKey,
                                wallpaperInfo, true);
  });
};

/**
 * Downloads resources from url. Calls onSuccess and opt_onFailure accordingly.
 * @param {string} url The url address where we should fetch resources.
 * @param {string} type The response type of XMLHttprequest.
 * @param {function} onSuccess The success callback. It must be called with
 *     current XMLHttprequest object.
 * @param {function} onFailure The failure callback.
 * @param {XMLHttpRequest=} opt_xhr The XMLHttpRequest object.
 */
WallpaperUtil.fetchURL = function(url, type, onSuccess, onFailure, opt_xhr) {
  var xhr;
  if (opt_xhr)
    xhr = opt_xhr;
  else
    xhr = new XMLHttpRequest();

  try {
    // Do not use loadend here to handle both success and failure case. It gets
    // complicated with abortion. Unexpected error message may show up. See
    // http://crbug.com/242581.
    xhr.addEventListener('load', function(e) {
      if (this.status == 200) {
        onSuccess(this);
      } else {
        onFailure();
      }
    });
    xhr.addEventListener('error', onFailure);
    xhr.open('GET', url, true);
    xhr.responseType = type;
    xhr.send(null);
  } catch (e) {
    onFailure();
  }
};

/**
 * Sets wallpaper to online wallpaper specified by url and layout
 * @param {string} url The url address where we should fetch resources.
 * @param {string} layout The layout of online wallpaper.
 * @param {function} onSuccess The success callback.
 * @param {function} onFailure The failure callback.
 */
WallpaperUtil.setOnlineWallpaper = function(url, layout, onSuccess, onFailure) {
  var self = this;
  chrome.wallpaperPrivate.setWallpaperIfExists(url, layout, function(exists) {
    if (exists) {
      onSuccess();
      return;
    }

    self.fetchURL(url, 'arraybuffer', function(xhr) {
      if (xhr.response != null) {
        chrome.wallpaperPrivate.setWallpaper(xhr.response, layout, url,
                                             onSuccess);
        self.saveWallpaperInfo(url, layout,
                               Constants.WallpaperSourceEnum.Online);
      } else {
        onFailure();
      }
    }, onFailure);
  });
};
