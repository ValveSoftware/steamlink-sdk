# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'WIDGET_ROOT': '<(DEPTH)/components/chrome_apps/webstore_widget/cws_widget',
    'cws_widget_container': [
      '<(DEPTH)/ui/webui/resources/js/cr.js',
      '<(DEPTH)/ui/webui/resources/js/cr/event_target.js',
      '<(DEPTH)/ui/webui/resources/js/cr/ui/dialogs.js',
      '<(WIDGET_ROOT)/app_installer.js',
      '<(WIDGET_ROOT)/cws_webview_client.js',
      '<(WIDGET_ROOT)/cws_widget_container.js',
      '<(WIDGET_ROOT)/cws_widget_container_error_dialog.js',
    ]
  }
}
