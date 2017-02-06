// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'media-picker' handles showing the dropdown allowing users to select the
 * default camera/microphone.
 */
Polymer({
  is: 'media-picker',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The type of media picker, either 'camera' or 'mic'.
     */
    type: String,

    /**
     * The devices available to pick from.
     * @type {Array<MediaPickerEntry>}
     */
    devices: Array,
  },

  ready: function() {
    this.addWebUIListener('updateDevicesMenu',
        this.updateDevicesMenu_.bind(this));
    this.browserProxy.getDefaultCaptureDevices(this.type);
  },

  /**
   * Updates the microphone/camera devices menu with the given entries.
   * @param {string} type The device type.
   * @param {!Array<MediaPickerEntry>} devices List of available devices.
   * @param {string} defaultDevice The unique id of the current default device.
   */
  updateDevicesMenu_: function(type, devices, defaultDevice) {
    if (type != this.type)
      return;

    this.$.picker.hidden = devices.length == 0;
    if (devices.length > 0) {
      this.devices = devices;
      this.$.mediaPicker.selected = defaultDevice;
    }
  },

  /**
   * A handler for when an item is selected in the media picker.
   */
  onMediaPickerActivate_: function(event) {
    this.browserProxy.setDefaultCaptureDevice(this.type, event.detail.selected);
  },
});
