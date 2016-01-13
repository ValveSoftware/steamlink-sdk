// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jhawkins): Use hidden instead of showInline* and display:none.

/**
 * Sets the display style of a node.
 * @param {!Element} node The target element to show or hide.
 * @param {boolean} isShow Should the target element be visible.
 */
function showInline(node, isShow) {
  node.style.display = isShow ? 'inline' : 'none';
}

/**
 * Sets the display style of a node.
 * @param {!Element} node The target element to show or hide.
 * @param {boolean} isShow Should the target element be visible.
 */
function showInlineBlock(node, isShow) {
  node.style.display = isShow ? 'inline-block' : 'none';
}

/**
 * Creates a link with a specified onclick handler and content.
 * @param {function()} onclick The onclick handler.
 * @param {string} value The link text.
 * @return {Element} The created link element.
 */
function createLink(onclick, value) {
  var link = document.createElement('a');
  link.onclick = onclick;
  link.href = '#';
  link.textContent = value;
  link.oncontextmenu = function() { return false; };
  return link;
}

/**
 * Creates a button with a specified onclick handler and content.
 * @param {function()} onclick The onclick handler.
 * @param {string} value The button text.
 * @return {Element} The created button.
 */
function createButton(onclick, value) {
  var button = document.createElement('input');
  button.type = 'button';
  button.value = value;
  button.onclick = onclick;
  return button;
}

///////////////////////////////////////////////////////////////////////////////
// Downloads
/**
 * Class to hold all the information about the visible downloads.
 * @constructor
 */
function Downloads() {
  this.downloads_ = {};
  this.node_ = $('downloads-display');
  this.summary_ = $('downloads-summary-text');
  this.searchText_ = '';

  // Keep track of the dates of the newest and oldest downloads so that we
  // know where to insert them.
  this.newestTime_ = -1;

  // Icon load request queue.
  this.iconLoadQueue_ = [];
  this.isIconLoading_ = false;
}

/**
 * Called when a download has been updated or added.
 * @param {Object} download A backend download object (see downloads_ui.cc)
 */
Downloads.prototype.updated = function(download) {
  var id = download.id;
  if (!!this.downloads_[id]) {
    this.downloads_[id].update(download);
  } else {
    this.downloads_[id] = new Download(download);
    // We get downloads in display order, so we don't have to worry about
    // maintaining correct order - we can assume that any downloads not in
    // display order are new ones and so we can add them to the top of the
    // list.
    if (download.started > this.newestTime_) {
      this.node_.insertBefore(this.downloads_[id].node, this.node_.firstChild);
      this.newestTime_ = download.started;
    } else {
      this.node_.appendChild(this.downloads_[id].node);
    }
  }
  // Download.prototype.update may change its nodeSince_ and nodeDate_, so
  // update all the date displays.
  // TODO(benjhayden) Only do this if its nodeSince_ or nodeDate_ actually did
  // change since this may touch 150 elements and Downloads.prototype.updated
  // may be called 150 times.
  this.updateDateDisplay_();
};

/**
 * Set our display search text.
 * @param {string} searchText The string we're searching for.
 */
Downloads.prototype.setSearchText = function(searchText) {
  this.searchText_ = searchText;
};

/**
 * Update the summary block above the results
 */
Downloads.prototype.updateSummary = function() {
  if (this.searchText_) {
    this.summary_.textContent = loadTimeData.getStringF('searchresultsfor',
                                                        this.searchText_);
  } else {
    this.summary_.textContent = loadTimeData.getString('downloads');
  }

  var hasDownloads = false;
  for (var i in this.downloads_) {
    hasDownloads = true;
    break;
  }
};

/**
 * Returns the number of downloads in the model. Used by tests.
 * @return {integer} Returns the number of downloads shown on the page.
 */
Downloads.prototype.size = function() {
  return Object.keys(this.downloads_).length;
};

/**
 * Update the date visibility in our nodes so that no date is
 * repeated.
 * @private
 */
Downloads.prototype.updateDateDisplay_ = function() {
  var dateContainers = document.getElementsByClassName('date-container');
  var displayed = {};
  for (var i = 0, container; container = dateContainers[i]; i++) {
    var dateString = container.getElementsByClassName('date')[0].innerHTML;
    if (!!displayed[dateString]) {
      container.style.display = 'none';
    } else {
      displayed[dateString] = true;
      container.style.display = 'block';
    }
  }
};

/**
 * Remove a download.
 * @param {number} id The id of the download to remove.
 */
Downloads.prototype.remove = function(id) {
  this.node_.removeChild(this.downloads_[id].node);
  delete this.downloads_[id];
  this.updateDateDisplay_();
};

/**
 * Clear all downloads and reset us back to a null state.
 */
Downloads.prototype.clear = function() {
  for (var id in this.downloads_) {
    this.downloads_[id].clear();
    this.remove(id);
  }
};

/**
 * Schedule icon load.
 * @param {HTMLImageElement} elem Image element that should contain the icon.
 * @param {string} iconURL URL to the icon.
 */
Downloads.prototype.scheduleIconLoad = function(elem, iconURL) {
  var self = this;

  // Sends request to the next icon in the queue and schedules
  // call to itself when the icon is loaded.
  function loadNext() {
    self.isIconLoading_ = true;
    while (self.iconLoadQueue_.length > 0) {
      var request = self.iconLoadQueue_.shift();
      var oldSrc = request.element.src;
      request.element.onabort = request.element.onerror =
          request.element.onload = loadNext;
      request.element.src = request.url;
      if (oldSrc != request.element.src)
        return;
    }
    self.isIconLoading_ = false;
  }

  // Create new request
  var loadRequest = {element: elem, url: iconURL};
  this.iconLoadQueue_.push(loadRequest);

  // Start loading if none scheduled yet
  if (!this.isIconLoading_)
    loadNext();
};

/**
 * Returns whether the displayed list needs to be updated or not.
 * @param {Array} downloads Array of download nodes.
 * @return {boolean} Returns true if the displayed list is to be updated.
 */
Downloads.prototype.isUpdateNeeded = function(downloads) {
  var size = 0;
  for (var i in this.downloads_)
    size++;
  if (size != downloads.length)
    return true;
  // Since there are the same number of items in the incoming list as
  // |this.downloads_|, there won't be any removed downloads without some
  // downloads having been inserted.  So check only for new downloads in
  // deciding whether to update.
  for (var i = 0; i < downloads.length; i++) {
    if (!this.downloads_[downloads[i].id])
      return true;
  }
  return false;
};

///////////////////////////////////////////////////////////////////////////////
// Download
/**
 * A download and the DOM representation for that download.
 * @param {Object} download A backend download object (see downloads_ui.cc)
 * @constructor
 */
function Download(download) {
  // Create DOM
  this.node = createElementWithClassName(
      'div', 'download' + (download.otr ? ' otr' : ''));

  // Dates
  this.dateContainer_ = createElementWithClassName('div', 'date-container');
  this.node.appendChild(this.dateContainer_);

  this.nodeSince_ = createElementWithClassName('div', 'since');
  this.nodeDate_ = createElementWithClassName('div', 'date');
  this.dateContainer_.appendChild(this.nodeSince_);
  this.dateContainer_.appendChild(this.nodeDate_);

  // Container for all 'safe download' UI.
  this.safe_ = createElementWithClassName('div', 'safe');
  this.safe_.ondragstart = this.drag_.bind(this);
  this.node.appendChild(this.safe_);

  if (download.state != Download.States.COMPLETE) {
    this.nodeProgressBackground_ =
        createElementWithClassName('div', 'progress background');
    this.safe_.appendChild(this.nodeProgressBackground_);

    this.nodeProgressForeground_ =
        createElementWithClassName('canvas', 'progress');
    this.nodeProgressForeground_.width = Download.Progress.width;
    this.nodeProgressForeground_.height = Download.Progress.height;
    this.canvasProgress_ = this.nodeProgressForeground_.getContext('2d');

    this.safe_.appendChild(this.nodeProgressForeground_);
  }

  this.nodeImg_ = createElementWithClassName('img', 'icon');
  this.safe_.appendChild(this.nodeImg_);

  // FileLink is used for completed downloads, otherwise we show FileName.
  this.nodeTitleArea_ = createElementWithClassName('div', 'title-area');
  this.safe_.appendChild(this.nodeTitleArea_);

  this.nodeFileLink_ = createLink(this.openFile_.bind(this), '');
  this.nodeFileLink_.className = 'name';
  this.nodeFileLink_.style.display = 'none';
  this.nodeTitleArea_.appendChild(this.nodeFileLink_);

  this.nodeFileName_ = createElementWithClassName('span', 'name');
  this.nodeFileName_.style.display = 'none';
  this.nodeTitleArea_.appendChild(this.nodeFileName_);

  this.nodeStatus_ = createElementWithClassName('span', 'status');
  this.nodeTitleArea_.appendChild(this.nodeStatus_);

  var nodeURLDiv = createElementWithClassName('div', 'url-container');
  this.safe_.appendChild(nodeURLDiv);

  this.nodeURL_ = createElementWithClassName('a', 'src-url');
  this.nodeURL_.target = '_blank';
  nodeURLDiv.appendChild(this.nodeURL_);

  // Controls.
  this.nodeControls_ = createElementWithClassName('div', 'controls');
  this.safe_.appendChild(this.nodeControls_);

  // We don't need 'show in folder' in chromium os. See download_ui.cc and
  // http://code.google.com/p/chromium-os/issues/detail?id=916.
  if (loadTimeData.valueExists('control_showinfolder')) {
    this.controlShow_ = createLink(this.show_.bind(this),
        loadTimeData.getString('control_showinfolder'));
    this.nodeControls_.appendChild(this.controlShow_);
  } else {
    this.controlShow_ = null;
  }

  this.controlRetry_ = document.createElement('a');
  this.controlRetry_.download = '';
  this.controlRetry_.textContent = loadTimeData.getString('control_retry');
  this.nodeControls_.appendChild(this.controlRetry_);

  // Pause/Resume are a toggle.
  this.controlPause_ = createLink(this.pause_.bind(this),
      loadTimeData.getString('control_pause'));
  this.nodeControls_.appendChild(this.controlPause_);

  this.controlResume_ = createLink(this.resume_.bind(this),
      loadTimeData.getString('control_resume'));
  this.nodeControls_.appendChild(this.controlResume_);

  // Anchors <a> don't support the "disabled" property.
  if (loadTimeData.getBoolean('allow_deleting_history')) {
    this.controlRemove_ = createLink(this.remove_.bind(this),
        loadTimeData.getString('control_removefromlist'));
    this.controlRemove_.classList.add('control-remove-link');
  } else {
    this.controlRemove_ = document.createElement('span');
    this.controlRemove_.classList.add('disabled-link');
    var text = document.createTextNode(
        loadTimeData.getString('control_removefromlist'));
    this.controlRemove_.appendChild(text);
  }
  if (!loadTimeData.getBoolean('show_delete_history'))
    this.controlRemove_.hidden = true;

  this.nodeControls_.appendChild(this.controlRemove_);

  this.controlCancel_ = createLink(this.cancel_.bind(this),
      loadTimeData.getString('control_cancel'));
  this.nodeControls_.appendChild(this.controlCancel_);

  this.controlByExtension_ = document.createElement('span');
  this.nodeControls_.appendChild(this.controlByExtension_);

  // Container for 'unsafe download' UI.
  this.danger_ = createElementWithClassName('div', 'show-dangerous');
  this.node.appendChild(this.danger_);

  this.dangerNodeImg_ = createElementWithClassName('img', 'icon');
  this.danger_.appendChild(this.dangerNodeImg_);

  this.dangerDesc_ = document.createElement('div');
  this.danger_.appendChild(this.dangerDesc_);

  // Buttons for the malicious case.
  this.malwareNodeControls_ = createElementWithClassName('div', 'controls');
  this.malwareSave_ = createLink(
      this.saveDangerous_.bind(this),
      loadTimeData.getString('danger_restore'));
  this.malwareNodeControls_.appendChild(this.malwareSave_);
  this.malwareDiscard_ = createLink(
      this.discardDangerous_.bind(this),
      loadTimeData.getString('control_removefromlist'));
  this.malwareNodeControls_.appendChild(this.malwareDiscard_);
  this.danger_.appendChild(this.malwareNodeControls_);

  // Buttons for the dangerous but not malicious case.
  this.dangerSave_ = createButton(
      this.saveDangerous_.bind(this),
      loadTimeData.getString('danger_save'));
  this.danger_.appendChild(this.dangerSave_);

  this.dangerDiscard_ = createButton(
      this.discardDangerous_.bind(this),
      loadTimeData.getString('danger_discard'));
  this.danger_.appendChild(this.dangerDiscard_);

  // Update member vars.
  this.update(download);
}

/**
 * The states a download can be in. These correspond to states defined in
 * DownloadsDOMHandler::CreateDownloadItemValue
 */
Download.States = {
  IN_PROGRESS: 'IN_PROGRESS',
  CANCELLED: 'CANCELLED',
  COMPLETE: 'COMPLETE',
  PAUSED: 'PAUSED',
  DANGEROUS: 'DANGEROUS',
  INTERRUPTED: 'INTERRUPTED',
};

/**
 * Explains why a download is in DANGEROUS state.
 */
Download.DangerType = {
  NOT_DANGEROUS: 'NOT_DANGEROUS',
  DANGEROUS_FILE: 'DANGEROUS_FILE',
  DANGEROUS_URL: 'DANGEROUS_URL',
  DANGEROUS_CONTENT: 'DANGEROUS_CONTENT',
  UNCOMMON_CONTENT: 'UNCOMMON_CONTENT',
  DANGEROUS_HOST: 'DANGEROUS_HOST',
  POTENTIALLY_UNWANTED: 'POTENTIALLY_UNWANTED',
};

/**
 * @param {number} a Some float.
 * @param {number} b Some float.
 * @param {number} opt_pct Percent of min(a,b).
 * @return {boolean} true if a is within opt_pct percent of b.
 */
function floatEq(a, b, opt_pct) {
  return Math.abs(a - b) < (Math.min(a, b) * (opt_pct || 1.0) / 100.0);
}

/**
 * Constants and "constants" for the progress meter.
 */
Download.Progress = {
  START_ANGLE: -0.5 * Math.PI,
  SIDE: 48,
};

/***/
Download.Progress.HALF = Download.Progress.SIDE / 2;

function computeDownloadProgress() {
  if (floatEq(Download.Progress.scale, window.devicePixelRatio)) {
    // Zooming in or out multiple times then typing Ctrl+0 resets the zoom level
    // directly to 1x, which fires the matchMedia event multiple times.
    return;
  }
  Download.Progress.scale = window.devicePixelRatio;
  Download.Progress.width = Download.Progress.SIDE * Download.Progress.scale;
  Download.Progress.height = Download.Progress.SIDE * Download.Progress.scale;
  Download.Progress.radius = Download.Progress.HALF * Download.Progress.scale;
  Download.Progress.centerX = Download.Progress.HALF * Download.Progress.scale;
  Download.Progress.centerY = Download.Progress.HALF * Download.Progress.scale;
}
computeDownloadProgress();

// Listens for when device-pixel-ratio changes between any zoom level.
[0.3, 0.4, 0.6, 0.7, 0.8, 0.95, 1.05, 1.2, 1.4, 1.6, 1.9, 2.2, 2.7, 3.5, 4.5
].forEach(function(scale) {
  matchMedia('(-webkit-min-device-pixel-ratio:' + scale + ')').addListener(
    function() {
      computeDownloadProgress();
  });
});

var ImageCache = {};
function getCachedImage(src) {
  if (!ImageCache[src]) {
    ImageCache[src] = new Image();
    ImageCache[src].src = src;
  }
  return ImageCache[src];
}

/**
 * Updates the download to reflect new data.
 * @param {Object} download A backend download object (see downloads_ui.cc)
 */
Download.prototype.update = function(download) {
  this.id_ = download.id;
  this.filePath_ = download.file_path;
  this.fileUrl_ = download.file_url;
  this.fileName_ = download.file_name;
  this.url_ = download.url;
  this.state_ = download.state;
  this.fileExternallyRemoved_ = download.file_externally_removed;
  this.dangerType_ = download.danger_type;
  this.lastReasonDescription_ = download.last_reason_text;
  this.byExtensionId_ = download.by_ext_id;
  this.byExtensionName_ = download.by_ext_name;

  this.since_ = download.since_string;
  this.date_ = download.date_string;

  // See DownloadItem::PercentComplete
  this.percent_ = Math.max(download.percent, 0);
  this.progressStatusText_ = download.progress_status_text;
  this.received_ = download.received;

  if (this.state_ == Download.States.DANGEROUS) {
    this.updateDangerousFile();
  } else {
    downloads.scheduleIconLoad(this.nodeImg_,
                               'chrome://fileicon/' +
                                   encodeURIComponent(this.filePath_) +
                                   '?scale=' + window.devicePixelRatio + 'x');

    if (this.state_ == Download.States.COMPLETE &&
        !this.fileExternallyRemoved_) {
      this.nodeFileLink_.textContent = this.fileName_;
      this.nodeFileLink_.href = this.fileUrl_;
      this.nodeFileLink_.oncontextmenu = null;
    } else if (this.nodeFileName_.textContent != this.fileName_) {
      this.nodeFileName_.textContent = this.fileName_;
    }
    if (this.state_ == Download.States.INTERRUPTED) {
      this.nodeFileName_.classList.add('interrupted');
    } else if (this.nodeFileName_.classList.contains('interrupted')) {
      this.nodeFileName_.classList.remove('interrupted');
    }

    showInline(this.nodeFileLink_,
               this.state_ == Download.States.COMPLETE &&
                   !this.fileExternallyRemoved_);
    // nodeFileName_ has to be inline-block to avoid the 'interaction' with
    // nodeStatus_. If both are inline, it appears that their text contents
    // are merged before the bidi algorithm is applied leading to an
    // undesirable reordering. http://crbug.com/13216
    showInlineBlock(this.nodeFileName_,
                    this.state_ != Download.States.COMPLETE ||
                        this.fileExternallyRemoved_);

    if (this.state_ == Download.States.IN_PROGRESS) {
      this.nodeProgressForeground_.style.display = 'block';
      this.nodeProgressBackground_.style.display = 'block';
      this.nodeProgressForeground_.width = Download.Progress.width;
      this.nodeProgressForeground_.height = Download.Progress.height;

      var foregroundImage = getCachedImage(
          'chrome://theme/IDR_DOWNLOAD_PROGRESS_FOREGROUND_32@' +
          window.devicePixelRatio + 'x');

      // Draw a pie-slice for the progress.
      this.canvasProgress_.globalCompositeOperation = 'copy';
      this.canvasProgress_.drawImage(
          foregroundImage,
          0, 0,  // sx, sy
          foregroundImage.width,
          foregroundImage.height,
          0, 0,  // x, y
          Download.Progress.width, Download.Progress.height);
      this.canvasProgress_.globalCompositeOperation = 'destination-in';
      this.canvasProgress_.beginPath();
      this.canvasProgress_.moveTo(Download.Progress.centerX,
                                  Download.Progress.centerY);

      // Draw an arc CW for both RTL and LTR. http://crbug.com/13215
      this.canvasProgress_.arc(Download.Progress.centerX,
                               Download.Progress.centerY,
                               Download.Progress.radius,
                               Download.Progress.START_ANGLE,
                               Download.Progress.START_ANGLE + Math.PI * 0.02 *
                               Number(this.percent_),
                               false);

      this.canvasProgress_.lineTo(Download.Progress.centerX,
                                  Download.Progress.centerY);
      this.canvasProgress_.fill();
      this.canvasProgress_.closePath();
    } else if (this.nodeProgressBackground_) {
      this.nodeProgressForeground_.style.display = 'none';
      this.nodeProgressBackground_.style.display = 'none';
    }

    if (this.controlShow_) {
      showInline(this.controlShow_,
                 this.state_ == Download.States.COMPLETE &&
                     !this.fileExternallyRemoved_);
    }
    showInline(this.controlRetry_, download.retry);
    this.controlRetry_.href = this.url_;
    showInline(this.controlPause_, this.state_ == Download.States.IN_PROGRESS);
    showInline(this.controlResume_, download.resume);
    var showCancel = this.state_ == Download.States.IN_PROGRESS ||
                     this.state_ == Download.States.PAUSED;
    showInline(this.controlCancel_, showCancel);
    showInline(this.controlRemove_, !showCancel);

    if (this.byExtensionId_ && this.byExtensionName_) {
      // Format 'control_by_extension' with a link instead of plain text by
      // splitting the formatted string into pieces.
      var slug = 'XXXXX';
      var formatted = loadTimeData.getStringF('control_by_extension', slug);
      var slugIndex = formatted.indexOf(slug);
      this.controlByExtension_.textContent = formatted.substr(0, slugIndex);
      this.controlByExtensionLink_ = document.createElement('a');
      this.controlByExtensionLink_.href =
          'chrome://extensions#' + this.byExtensionId_;
      this.controlByExtensionLink_.textContent = this.byExtensionName_;
      this.controlByExtension_.appendChild(this.controlByExtensionLink_);
      if (slugIndex < (formatted.length - slug.length))
        this.controlByExtension_.appendChild(document.createTextNode(
            formatted.substr(slugIndex + 1)));
    }

    this.nodeSince_.textContent = this.since_;
    this.nodeDate_.textContent = this.date_;
    // Don't unnecessarily update the url, as doing so will remove any
    // text selection the user has started (http://crbug.com/44982).
    if (this.nodeURL_.textContent != this.url_) {
      this.nodeURL_.textContent = this.url_;
      this.nodeURL_.href = this.url_;
    }
    this.nodeStatus_.textContent = this.getStatusText_();

    this.danger_.style.display = 'none';
    this.safe_.style.display = 'block';
  }
};

/**
 * Decorates the icons, strings, and buttons for a download to reflect the
 * danger level of a file. Dangerous & malicious files are treated differently.
 */
Download.prototype.updateDangerousFile = function() {
  switch (this.dangerType_) {
    case Download.DangerType.DANGEROUS_FILE: {
      this.dangerDesc_.textContent = loadTimeData.getStringF(
          'danger_file_desc', this.fileName_);
      break;
    }
    case Download.DangerType.DANGEROUS_URL: {
      this.dangerDesc_.textContent = loadTimeData.getString('danger_url_desc');
      break;
    }
    case Download.DangerType.DANGEROUS_CONTENT:  // Fall through.
    case Download.DangerType.DANGEROUS_HOST: {
      this.dangerDesc_.textContent = loadTimeData.getStringF(
          'danger_content_desc', this.fileName_);
      break;
    }
    case Download.DangerType.UNCOMMON_CONTENT: {
      this.dangerDesc_.textContent = loadTimeData.getStringF(
          'danger_uncommon_desc', this.fileName_);
      break;
    }
    case Download.DangerType.POTENTIALLY_UNWANTED: {
      this.dangerDesc_.textContent = loadTimeData.getStringF(
          'danger_settings_desc', this.fileName_);
      break;
    }
  }

  if (this.dangerType_ == Download.DangerType.DANGEROUS_FILE) {
    downloads.scheduleIconLoad(
        this.dangerNodeImg_,
        'chrome://theme/IDR_WARNING?scale=' + window.devicePixelRatio + 'x');
  } else {
    downloads.scheduleIconLoad(
        this.dangerNodeImg_,
        'chrome://theme/IDR_SAFEBROWSING_WARNING?scale=' +
            window.devicePixelRatio + 'x');
    this.dangerDesc_.className = 'malware-description';
  }

  if (this.dangerType_ == Download.DangerType.DANGEROUS_CONTENT ||
      this.dangerType_ == Download.DangerType.DANGEROUS_HOST ||
      this.dangerType_ == Download.DangerType.DANGEROUS_URL ||
      this.dangerType_ == Download.DangerType.POTENTIALLY_UNWANTED) {
    this.malwareNodeControls_.style.display = 'block';
    this.dangerDiscard_.style.display = 'none';
    this.dangerSave_.style.display = 'none';
  } else {
    this.malwareNodeControls_.style.display = 'none';
    this.dangerDiscard_.style.display = 'inline';
    this.dangerSave_.style.display = 'inline';
  }

  this.danger_.style.display = 'block';
  this.safe_.style.display = 'none';
};

/**
 * Removes applicable bits from the DOM in preparation for deletion.
 */
Download.prototype.clear = function() {
  this.safe_.ondragstart = null;
  this.nodeFileLink_.onclick = null;
  if (this.controlShow_) {
    this.controlShow_.onclick = null;
  }
  this.controlCancel_.onclick = null;
  this.controlPause_.onclick = null;
  this.controlResume_.onclick = null;
  this.dangerDiscard_.onclick = null;
  this.dangerSave_.onclick = null;
  this.malwareDiscard_.onclick = null;
  this.malwareSave_.onclick = null;

  this.node.innerHTML = '';
};

/**
 * @private
 * @return {string} User-visible status update text.
 */
Download.prototype.getStatusText_ = function() {
  switch (this.state_) {
    case Download.States.IN_PROGRESS:
      return this.progressStatusText_;
    case Download.States.CANCELLED:
      return loadTimeData.getString('status_cancelled');
    case Download.States.PAUSED:
      return loadTimeData.getString('status_paused');
    case Download.States.DANGEROUS:
      // danger_url_desc is also used by DANGEROUS_CONTENT.
      var desc = this.dangerType_ == Download.DangerType.DANGEROUS_FILE ?
          'danger_file_desc' : 'danger_url_desc';
      return loadTimeData.getString(desc);
    case Download.States.INTERRUPTED:
      return this.lastReasonDescription_;
    case Download.States.COMPLETE:
      return this.fileExternallyRemoved_ ?
          loadTimeData.getString('status_removed') : '';
  }
};

/**
 * Tells the backend to initiate a drag, allowing users to drag
 * files from the download page and have them appear as native file
 * drags.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.drag_ = function() {
  chrome.send('drag', [this.id_.toString()]);
  return false;
};

/**
 * Tells the backend to open this file.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.openFile_ = function() {
  chrome.send('openFile', [this.id_.toString()]);
  return false;
};

/**
 * Tells the backend that the user chose to save a dangerous file.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.saveDangerous_ = function() {
  chrome.send('saveDangerous', [this.id_.toString()]);
  return false;
};

/**
 * Tells the backend that the user chose to discard a dangerous file.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.discardDangerous_ = function() {
  chrome.send('discardDangerous', [this.id_.toString()]);
  downloads.remove(this.id_);
  return false;
};

/**
 * Tells the backend to show the file in explorer.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.show_ = function() {
  chrome.send('show', [this.id_.toString()]);
  return false;
};

/**
 * Tells the backend to pause this download.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.pause_ = function() {
  chrome.send('pause', [this.id_.toString()]);
  return false;
};

/**
 * Tells the backend to resume this download.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.resume_ = function() {
  chrome.send('resume', [this.id_.toString()]);
  return false;
};

/**
 * Tells the backend to remove this download from history and download shelf.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
 Download.prototype.remove_ = function() {
   if (loadTimeData.getBoolean('allow_deleting_history')) {
    chrome.send('remove', [this.id_.toString()]);
  }
  return false;
};

/**
 * Tells the backend to cancel this download.
 * @return {boolean} Returns false to prevent the default action.
 * @private
 */
Download.prototype.cancel_ = function() {
  chrome.send('cancel', [this.id_.toString()]);
  return false;
};

///////////////////////////////////////////////////////////////////////////////
// Page:
var downloads, resultsTimeout;

// TODO(benjhayden): Rename Downloads to DownloadManager, downloads to
// downloadManager or theDownloadManager or DownloadManager.get() to prevent
// confusing Downloads with Download.

/**
 * The FIFO array that stores updates of download files to be appeared
 * on the download page. It is guaranteed that the updates in this array
 * are reflected to the download page in a FIFO order.
*/
var fifoResults;

function load() {
  chrome.send('onPageLoaded');
  fifoResults = [];
  downloads = new Downloads();
  $('term').focus();
  setSearch('');

  var clearAllHolder = $('clear-all-holder');
  var clearAllElement;
  if (loadTimeData.getBoolean('allow_deleting_history')) {
    clearAllElement = createLink(clearAll, loadTimeData.getString('clear_all'));
    clearAllElement.classList.add('clear-all-link');
    clearAllHolder.classList.remove('disabled-link');
  } else {
    clearAllElement = document.createTextNode(
        loadTimeData.getString('clear_all'));
    clearAllHolder.classList.add('disabled-link');
  }
  if (!loadTimeData.getBoolean('show_delete_history'))
    clearAllHolder.hidden = true;

  clearAllHolder.appendChild(clearAllElement);
  clearAllElement.oncontextmenu = function() { return false; };

  // TODO(jhawkins): Use a link-button here.
  var openDownloadsFolderLink = $('open-downloads-folder');
  openDownloadsFolderLink.onclick = function() {
    chrome.send('openDownloadsFolder');
  };
  openDownloadsFolderLink.oncontextmenu = function() { return false; };

  $('search-link').onclick = function(e) {
    setSearch('');
    e.preventDefault();
    $('term').value = '';
    return false;
  };

  $('term').onsearch = function(e) {
    setSearch(this.value);
  };
}

function setSearch(searchText) {
  fifoResults.length = 0;
  downloads.setSearchText(searchText);
  searchText = searchText.toString().match(/(?:[^\s"]+|"[^"]*")+/g);
  if (searchText) {
    searchText = searchText.map(function(term) {
      // strip quotes
      return (term.match(/\s/) &&
              term[0].match(/["']/) &&
              term[term.length - 1] == term[0]) ?
        term.substr(1, term.length - 2) : term;
    });
  } else {
    searchText = [];
  }
  chrome.send('getDownloads', searchText);
}

function clearAll() {
  if (!loadTimeData.getBoolean('allow_deleting_history'))
    return;

  fifoResults.length = 0;
  downloads.clear();
  downloads.setSearchText('');
  chrome.send('clearAll');
}

///////////////////////////////////////////////////////////////////////////////
// Chrome callbacks:
/**
 * Our history system calls this function with results from searches or when
 * downloads are added or removed.
 * @param {Array.<Object>} results List of updates.
 */
function downloadsList(results) {
  if (downloads && downloads.isUpdateNeeded(results)) {
    if (resultsTimeout)
      clearTimeout(resultsTimeout);
    fifoResults.length = 0;
    downloads.clear();
    downloadUpdated(results);
  }
  downloads.updateSummary();
}

/**
 * When a download is updated (progress, state change), this is called.
 * @param {Array.<Object>} results List of updates for the download process.
 */
function downloadUpdated(results) {
  // Sometimes this can get called too early.
  if (!downloads)
    return;

  fifoResults = fifoResults.concat(results);
  tryDownloadUpdatedPeriodically();
}

/**
 * Try to reflect as much updates as possible within 50ms.
 * This function is scheduled again and again until all updates are reflected.
 */
function tryDownloadUpdatedPeriodically() {
  var start = Date.now();
  while (fifoResults.length) {
    var result = fifoResults.shift();
    downloads.updated(result);
    // Do as much as we can in 50ms.
    if (Date.now() - start > 50) {
      clearTimeout(resultsTimeout);
      resultsTimeout = setTimeout(tryDownloadUpdatedPeriodically, 5);
      break;
    }
  }
}

// Add handlers to HTML elements.
window.addEventListener('DOMContentLoaded', load);
