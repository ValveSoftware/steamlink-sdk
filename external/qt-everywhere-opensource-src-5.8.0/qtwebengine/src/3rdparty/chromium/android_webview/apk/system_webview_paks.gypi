# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file defines the name of webviewchromium.pak and the set of locales that
# should be packed inside the System WebView apk, these files also are used in
# downstream.
# TODO: consider unifying this list with the one in chrome_android_paks.gypi
# once Chrome includes all the locales that the WebView needs.
{
  'variables': {
    'webview_licenses_path': '<(PRODUCT_DIR)/android_webview_assets/webview_licenses.notice',
    'webview_chromium_pak_path': '<(PRODUCT_DIR)/android_webview_assets/webviewchromium.pak',
    'webview_locales_input_paks_folder': '<(PRODUCT_DIR)/android_webview_assets/locales/',
    # The list of locale are only supported by WebView.
    'webview_locales_input_individual_paks': [
      '<(webview_locales_input_paks_folder)/bn.pak',
      '<(webview_locales_input_paks_folder)/et.pak',
      '<(webview_locales_input_paks_folder)/gu.pak',
      '<(webview_locales_input_paks_folder)/kn.pak',
      '<(webview_locales_input_paks_folder)/ml.pak',
      '<(webview_locales_input_paks_folder)/mr.pak',
      '<(webview_locales_input_paks_folder)/ms.pak',
      '<(webview_locales_input_paks_folder)/ta.pak',
      '<(webview_locales_input_paks_folder)/te.pak',
    ],
    # The list of locale are supported by chrome too.
    'webview_locales_input_common_paks': [
      '<(webview_locales_input_paks_folder)/am.pak',
      '<(webview_locales_input_paks_folder)/ar.pak',
      '<(webview_locales_input_paks_folder)/bg.pak',
      '<(webview_locales_input_paks_folder)/ca.pak',
      '<(webview_locales_input_paks_folder)/cs.pak',
      '<(webview_locales_input_paks_folder)/da.pak',
      '<(webview_locales_input_paks_folder)/de.pak',
      '<(webview_locales_input_paks_folder)/el.pak',
      '<(webview_locales_input_paks_folder)/en-GB.pak',
      '<(webview_locales_input_paks_folder)/en-US.pak',
      '<(webview_locales_input_paks_folder)/es.pak',
      '<(webview_locales_input_paks_folder)/es-419.pak',
      '<(webview_locales_input_paks_folder)/fa.pak',
      '<(webview_locales_input_paks_folder)/fi.pak',
      '<(webview_locales_input_paks_folder)/fil.pak',
      '<(webview_locales_input_paks_folder)/fr.pak',
      '<(webview_locales_input_paks_folder)/he.pak',
      '<(webview_locales_input_paks_folder)/hi.pak',
      '<(webview_locales_input_paks_folder)/hr.pak',
      '<(webview_locales_input_paks_folder)/hu.pak',
      '<(webview_locales_input_paks_folder)/id.pak',
      '<(webview_locales_input_paks_folder)/it.pak',
      '<(webview_locales_input_paks_folder)/ja.pak',
      '<(webview_locales_input_paks_folder)/ko.pak',
      '<(webview_locales_input_paks_folder)/lt.pak',
      '<(webview_locales_input_paks_folder)/lv.pak',
      '<(webview_locales_input_paks_folder)/nb.pak',
      '<(webview_locales_input_paks_folder)/nl.pak',
      '<(webview_locales_input_paks_folder)/pl.pak',
      '<(webview_locales_input_paks_folder)/pt-BR.pak',
      '<(webview_locales_input_paks_folder)/pt-PT.pak',
      '<(webview_locales_input_paks_folder)/ro.pak',
      '<(webview_locales_input_paks_folder)/ru.pak',
      '<(webview_locales_input_paks_folder)/sk.pak',
      '<(webview_locales_input_paks_folder)/sl.pak',
      '<(webview_locales_input_paks_folder)/sr.pak',
      '<(webview_locales_input_paks_folder)/sv.pak',
      '<(webview_locales_input_paks_folder)/sw.pak',
      '<(webview_locales_input_paks_folder)/th.pak',
      '<(webview_locales_input_paks_folder)/tr.pak',
      '<(webview_locales_input_paks_folder)/uk.pak',
      '<(webview_locales_input_paks_folder)/vi.pak',
      '<(webview_locales_input_paks_folder)/zh-CN.pak',
      '<(webview_locales_input_paks_folder)/zh-TW.pak',
    ],
  },
}

