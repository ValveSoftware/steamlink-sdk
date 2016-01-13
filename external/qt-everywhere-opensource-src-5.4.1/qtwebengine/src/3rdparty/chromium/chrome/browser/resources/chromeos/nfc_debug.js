// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nfcDebug', function() {
  'use strict';

  function NfcDebugUI() {
    this.adapterData_ = {};
    this.peerData_ = {};
    this.tagData_ = {};
  }

  NfcDebugUI.prototype = {
    setAdapterData: function(data) {
      this.adapterData_ = data;
    },

    setPeerData: function(data) {
      this.peerData_ = data;
    },

    setTagData: function(data) {
      this.tagData_ = data;
    },

    /**
     * Powers the NFC adapter ON or OFF.
     */
    toggleAdapterPower: function() {
      chrome.send('setAdapterPower', [!this.adapterData_.powered]);
    },

    /**
     * Tells the NFC adapter to start or stop polling.
     */
    toggleAdapterPolling: function() {
      chrome.send('setAdapterPolling', [!this.adapterData_.polling]);
    },

    /**
     * Notifies the UI that the user made an NDEF type selection and the
     * appropriate form should be displayed.
     */
    recordTypeChanged: function() {
      this.updateRecordFormContents();
    },

    /**
     * Creates a table element and populates it for each record contained
     * in the given list of records and adds them as a child of the given
     * DOMElement. This method will replace the contents of the given element
     * with the tables.
     *
     * @param {DOMElement} div The container that the records should be rendered
     *                         to.
     * @param {Array} records List of NDEF record data.
     */
    renderRecords: function(div, records) {
      div.textContent = '';
      if (records.length == 0) {
        return;
      }
      var self = this;
      records.forEach(function(record) {
        var recordDiv = document.createElement('div');
        recordDiv.setAttribute('class', 'record-div');
        for (var key in record) {
          if (!record.hasOwnProperty(key))
            continue;

          var rowDiv = document.createElement('div');
          rowDiv.setAttribute('class', 'record-key-value-div');

          var keyElement, valueElement;
          if (key == 'titles') {
            keyElement = document.createElement('div');
            keyElement.setAttribute('class', 'record-key-div');
            keyElement.appendChild(document.createTextNode(key));
            valueElement = document.createElement('div');
            valueElement.setAttribute('class', 'record-value-div');
            self.renderRecords(valueElement, record[key]);
          } else {
            keyElement = document.createElement('span');
            keyElement.setAttribute('class', 'record-key-span');
            keyElement.appendChild(document.createTextNode(key));
            valueElement = document.createElement('span');
            valueElement.setAttribute('class', 'record-value-span');
            valueElement.appendChild(document.createTextNode(record[key]));
          }
          rowDiv.appendChild(keyElement);
          rowDiv.appendChild(valueElement);
          recordDiv.appendChild(rowDiv);
        }
        div.appendChild(recordDiv);
        if (records[records.length - 1] !== record)
          div.appendChild(document.createElement('hr'));
      });
    },

    /**
     * Updates which record type form is displayed based on the currently
     * selected type.
     */
    updateRecordFormContents: function() {
      var recordTypeMenu = $('record-type-menu');
      var selectedType =
          recordTypeMenu.options[recordTypeMenu.selectedIndex].value;
      this.updateRecordFormContentsFromType(selectedType);
    },

    /**
     * Updates which record type form is displayed based on the passed in
     * type string.
     *
     * @param {string} type The record type.
     */
    updateRecordFormContentsFromType: function(type) {
      $('text-form').hidden = (type != 'text');
      $('uri-form').hidden = (type != 'uri');
      $('smart-poster-form').hidden = (type != 'smart-poster');
    },

    /**
     * Tries to push or write the record to the remote tag or device based on
     * the contents of the record form fields.
     */
    submitRecordForm: function() {
      var recordTypeMenu = $('record-type-menu');
      var selectedType =
          recordTypeMenu.options[recordTypeMenu.selectedIndex].value;
      var recordData = {};
      if (selectedType == 'text') {
        recordData.type = 'TEXT';
        if ($('text-form-text').value)
          recordData.text = $('text-form-text').value;
        if ($('text-form-encoding').value)
          recordData.encoding = $('text-form-encoding').value;
        if ($('text-form-language-code').value)
          recordData.languageCode = $('text-form-language-code').value;
      } else if (selectedType == 'uri') {
        recordData.type = 'URI';
        if ($('uri-form-uri').value)
          recordData.uri = $('uri-form-uri').value;
        if ($('uri-form-mime-type').value)
          recordData.mimeType = $('uri-form-mime-type').value;
        if ($('uri-form-target-size').value) {
          var targetSize = $('uri-form-target-size').value;
          targetSize = parseFloat(targetSize);
          recordData.targetSize = isNaN(targetSize) ? 0.0 : targetSize;
        }
      } else if (selectedType == 'smart-poster') {
        recordData.type = 'SMART_POSTER';
        if ($('smart-poster-form-uri').value)
          recordData.uri = $('smart-poster-form-uri').value;
        if ($('smart-poster-form-mime-type').value)
          recordData.mimeType = $('smart-poster-form-mime-type').value;
        if ($('smart-poster-form-target-size').value) {
          var targetSize = $('smart-poster-form-target-size').value;
          targetSize = parseFloat(targetSize);
          recordData.targetSize = isNaN(targetSize) ? 0.0 : targetSize;
        }
        var title = {};
        if ($('smart-poster-form-title-text').value)
          title.text = $('smart-poster-form-title-text').value;
        if ($('smart-poster-form-title-encoding').value)
          title.encoding = $('smart-poster-form-title-encoding').value;
        if ($('smart-poster-form-title-language-code').value)
          title.languageCode =
              $('smart-poster-form-title-language-code').value;
        if (Object.keys(title).length != 0)
          recordData.titles = [title];
      }
      chrome.send('submitRecordForm', [recordData]);
    },

    /**
     * Given a dictionary |data|, builds a table where each row contains the
     * a key and its value. The resulting table is then added as the sole child
     * of |div|. |data| contains information about an adapter, tag, or peer and
     * this method creates a table for display, thus the value of some keys
     * will be processed.
     *
     * @param {DOMElement} div The container that the table should be rendered
     *                         to.
     * @param {dictionary} data Data to generate the table from.
     */
    createTableFromData: function(div, data) {
      div.textContent = '';
      var table = document.createElement('table');
      table.classList.add('parameters-table');
      for (var key in data) {
        var row = document.createElement('tr');
        var col = document.createElement('td');
        col.textContent = key;
        row.appendChild(col);

        col = document.createElement('td');
        var value = data[key];
        if (key == 'records')
          value = value.length;
        else if (key == 'supportedTechnologies')
          value = value.join(', ');
        col.textContent = value;
        row.appendChild(col);
        table.appendChild(row);
      }
      div.appendChild(table);
    },
  };

  cr.addSingletonGetter(NfcDebugUI);

  /**
   * Initializes the page after the content has loaded.
   */
  NfcDebugUI.initialize = function() {
    $('nfc-adapter-info').hidden = true;
    $('adapter-toggles').hidden = true;
    $('nfc-adapter-info').classList.add('transition-out');
    $('ndef-record-form').classList.add('transition-out');
    $('nfc-peer-info').classList.add('transition-out');
    $('nfc-tag-info').classList.add('transition-out');
    $('power-toggle').onclick = function() {
      NfcDebugUI.getInstance().toggleAdapterPower();
    };
    $('poll-toggle').onclick = function() {
      NfcDebugUI.getInstance().toggleAdapterPolling();
    };
    $('record-type-menu').onchange = function() {
      NfcDebugUI.getInstance().recordTypeChanged();
    };
    $('record-form-submit-button').onclick = function() {
      NfcDebugUI.getInstance().submitRecordForm();
    };
    $('record-form-submit-button').hidden = true;
    NfcDebugUI.getInstance().updateRecordFormContents();
    chrome.send('initialize');
  };

  /**
   * Updates the UI based on the NFC availability on the current platform.
   *
   * @param {bool} available If true, NFC is supported on the current platform.
   */
  NfcDebugUI.onNfcAvailabilityDetermined = function(available) {
    $('nfc-not-supported').hidden = available;
  };

  /**
   * Notifies the UI that information about the NFC adapter has been received.
   *
   * @param {dictionary} data Properties of the NFC adapter.
   */
  NfcDebugUI.onNfcAdapterInfoChanged = function(data) {
    NfcDebugUI.getInstance().setAdapterData(data);

    $('nfc-adapter-info').hidden = false;
    NfcDebugUI.getInstance().createTableFromData($('adapter-parameters'), data);

    $('nfc-adapter-info').classList.toggle('transition-out', !data.present);
    $('nfc-adapter-info').classList.toggle('transition-in', data.present);
    $('ndef-record-form').classList.toggle('transition-out', !data.present);
    $('ndef-record-form').classList.toggle('transition-in', data.present);

    $('adapter-toggles').hidden = !data.present;
    $('ndef-record-form').hidden = !data.present;

    $('power-toggle').textContent = loadTimeData.getString(
        data.powered ? 'adapterPowerOffText' : 'adapterPowerOnText');
    $('poll-toggle').textContent = loadTimeData.getString(
        data.polling ? 'adapterStopPollText' : 'adapterStartPollText');
  };

  /**
   * Notifies the UI that information about an NFC peer has been received.
   *
   * @param {dictionary} data Properties of the NFC peer device.
   */
  NfcDebugUI.onNfcPeerDeviceInfoChanged = function(data) {
    NfcDebugUI.getInstance().setPeerData(data);

    if (Object.keys(data).length == 0) {
      $('nfc-peer-info').classList.add('transition-out');
      $('nfc-peer-info').classList.remove('transition-in');
      $('record-form-submit-button').hidden = true;
      return;
    }

    $('nfc-peer-info').classList.remove('transition-out');
    $('nfc-peer-info').classList.add('transition-in');

    NfcDebugUI.getInstance().createTableFromData($('peer-parameters'), data);

    $('record-form-submit-button').hidden = false;
    $('record-form-submit-button').textContent =
        loadTimeData.getString('ndefFormPushButtonText');

    if (data.records.length == 0) {
      $('peer-records-entry').hidden = true;
      return;
    }

    $('peer-records-entry').hidden = false;
    NfcDebugUI.getInstance().renderRecords($('peer-records-container'),
                                           data.records);
  };

  /**
   * Notifies the UI that information about an NFC tag has been received.
   *
   * @param {dictionary} data Properties of the NFC tag.
   */
  NfcDebugUI.onNfcTagInfoChanged = function(data) {
    NfcDebugUI.getInstance().setTagData(data);

    if (Object.keys(data).length == 0) {
      $('nfc-tag-info').classList.add('transition-out');
      $('nfc-tag-info').classList.remove('transition-in');
      $('record-form-submit-button').hidden = true;
      return;
    }

    $('nfc-tag-info').classList.remove('transition-out');
    $('nfc-tag-info').classList.add('transition-in');

    NfcDebugUI.getInstance().createTableFromData($('tag-parameters'), data);

    $('record-form-submit-button').hidden = false;
    $('record-form-submit-button').textContent =
        loadTimeData.getString('ndefFormWriteButtonText');

    if (data.records.length == 0) {
      $('tag-records-entry').hidden = true;
      return;
    }

    $('tag-records-entry').hidden = false;
    NfcDebugUI.getInstance().renderRecords($('tag-records-container'),
                                           data.records);
  };

  /**
   * Notifies the UI that a call to "setAdapterPower" failed. Displays an
   * alert.
   */
  NfcDebugUI.onSetAdapterPowerFailed = function() {
    alert(loadTimeData.getString('errorFailedToSetPowerText'));
  };

  /**
   * Notifies the UI that a call to "setAdapterPolling" failed. Displays an
   * alert.
   */
  NfcDebugUI.onSetAdapterPollingFailed = function() {
    alert(loadTimeData.getString('errorFailedToSetPollingText'));
  };

  /**
   * Notifies the UI that an error occurred while submitting an NDEF record
   * form.
   * @param {string} errorMessage An error message, describing the failure.
   */
  NfcDebugUI.onSubmitRecordFormFailed = function(errorMessage) {
    alert(loadTimeData.getString('errorFailedToSubmitPrefixText') +
          ' ' + errorMessage);
  };

  // Export
  return {
    NfcDebugUI: NfcDebugUI
  };
});

document.addEventListener('DOMContentLoaded', nfcDebug.NfcDebugUI.initialize);
