// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Provides the UI for dump creation.
 */
var DumpCreator = (function() {
  /**
   * @param {Element} containerElement The parent element of the dump creation
   *     UI.
   * @constructor
   */
  function DumpCreator(containerElement) {
    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    this.root_ = document.createElement('details');

    this.root_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.root_);
    var summary = document.createElement('summary');
    this.root_.appendChild(summary);
    summary.textContent = 'Create Dump';
    var content = document.createElement('div');
    this.root_.appendChild(content);

    content.innerHTML = '<div><a><button>' +
        'Download the PeerConnection updates and stats data' +
        '</button></a></div>' +
        '<p><label><input type=checkbox>' +
        'Enable diagnostic audio recordings</label></p>' +
        '<p class=audio-recordings-info>A diagnostic audio recording is used' +
        ' for analyzing audio problems. It consists of two files and contains' +
        ' the audio played out from the speaker and recorded from the' +
        ' microphone and is saved to the local disk. Checking this box will' +
        ' enable the recording for ongoing WebRTC calls and for future WebRTC' +
        ' calls. When the box is unchecked or this page is closed, all' +
        ' ongoing recordings will be stopped and this recording' +
        ' functionality will be disabled for future WebRTC calls. Recordings' +
        ' in multiple tabs are supported as well as multiple recordings in' +
        ' the same tab. When enabling, you select a base filename to which' +
        ' suffixes will be appended as</p>' +
        '<p><div>&lt;base filename&gt;.&lt;render process ID&gt;' +
        '.aec_dump.&lt;recording ID&gt;</div>' +
        '<div>&lt;base filename&gt;.&lt;render process ID&gt;' +
        '.source_input.&lt;stream ID&gt;.wav</div></p>' +
        '<p class=audio-recordings-info>If recordings are disabled and then' +
        ' enabled using the same base filename, the microphone recording file' +
        ' will be overwritten, and the AEC dump file will be appended to and' +
        ' may become invalid. It is recommended to choose a new base filename' +
        ' each time or move the produced files before enabling again.</p>' +
        '<p><label><input type=checkbox>' +
        'Enable diagnostic packet and event recording</label></p>' +
        '<p class=audio-recordings-info>A diagnostic packet and event' +
        ' recording can be used for analyzing various issues related to' +
        ' thread starvation, jitter buffers or bandwidth estimation. Two' +
        ' types of data are logged. First, incoming and outgoing RTP headers' +
        ' and RTCP packets are logged. These do not include any audio or' +
        ' video information, nor any other types of personally identifiable' +
        ' information (so no IP addresses or URLs). Checking this box will' +
        ' enable the recording for currently ongoing WebRTC calls. When' +
        ' the box is unchecked or this page is closed, all active recordings' +
        ' will be stopped. Recording in multiple tabs or multiple recordings' +
        ' in the same tab is currently not supported. When enabling, a' +
        ' filename for the recording can be selected. If an existing file is' +
        ' selected, it will be overwritten. </p>';
    content.getElementsByTagName('a')[0].addEventListener(
        'click', this.onDownloadData_.bind(this));
    content.getElementsByTagName('input')[0].addEventListener(
        'click', this.onAudioDebugRecordingsChanged_.bind(this));
    content.getElementsByTagName('input')[1].addEventListener(
        'click', this.onEventLogRecordingsChanged_.bind(this));
  }

  DumpCreator.prototype = {
    // Mark the diagnostic audio recording checkbox checked.
    enableAudioDebugRecordings: function() {
      this.root_.getElementsByTagName('input')[0].checked = true;
    },

    // Mark the diagnostic audio recording checkbox unchecked.
    disableAudioDebugRecordings: function() {
      this.root_.getElementsByTagName('input')[0].checked = false;
    },

    // Mark the event log recording checkbox checked.
    enableEventLogRecordings: function() {
      this.root_.getElementsByTagName('input')[1].checked = true;
    },

    // Mark the event log recording checkbox unchecked.
    disableEventLogRecordings: function() {
      this.root_.getElementsByTagName('input')[1].checked = false;
    },

    /**
     * Downloads the PeerConnection updates and stats data as a file.
     *
     * @private
     */
    onDownloadData_: function() {
      var dump_object =
      {
        'getUserMedia': userMediaRequests,
        'PeerConnections': peerConnectionDataStore,
      };
      var textBlob = new Blob([JSON.stringify(dump_object, null, ' ')],
                              {type: 'octet/stream'});
      var URL = window.URL.createObjectURL(textBlob);

      var anchor = this.root_.getElementsByTagName('a')[0];
      anchor.href = URL;
      anchor.download = 'webrtc_internals_dump.txt';
      // The default action of the anchor will download the URL.
    },

    /**
     * Handles the event of toggling the audio debug recordings state.
     *
     * @private
     */
    onAudioDebugRecordingsChanged_: function() {
      var enabled = this.root_.getElementsByTagName('input')[0].checked;
      if (enabled) {
        chrome.send('enableAudioDebugRecordings');
      } else {
        chrome.send('disableAudioDebugRecordings');
      }
    },

    /**
     * Handles the event of toggling the event log recordings state.
     *
     * @private
     */
    onEventLogRecordingsChanged_: function() {
      var enabled = this.root_.getElementsByTagName('input')[1].checked;
      if (enabled) {
        chrome.send('enableEventLogRecordings');
      } else {
        chrome.send('disableEventLogRecordings');
      }
    },
  };
  return DumpCreator;
})();
