// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var Grid = cr.ui.Grid;
  /** @const */ var GridItem = cr.ui.GridItem;
  /** @const */ var GridSelectionController = cr.ui.GridSelectionController;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Number of frames recorded by takeVideo().
   * @const
   */
  var RECORD_FRAMES = 48;

  /**
   * FPS at which camera stream is recorded.
   * @const
   */
  var RECORD_FPS = 16;

   /**
    * Dimensions for camera capture.
    * @const
    */
  var CAPTURE_SIZE = {
    height: 480,
    width: 480
  };

  /**
   * Path for internal URLs.
   * @const
   */
  var CHROME_THEME_PATH = 'chrome://theme';

  /**
   * Creates a new user images grid item.
   * @param {{url: string, title: string=, decorateFn: function=,
   *     clickHandler: function=}} imageInfo User image URL, optional title,
   *     decorator callback and click handler.
   * @constructor
   * @extends {cr.ui.GridItem}
   */
  function UserImagesGridItem(imageInfo) {
    var el = new GridItem(imageInfo);
    el.__proto__ = UserImagesGridItem.prototype;
    return el;
  }

  UserImagesGridItem.prototype = {
    __proto__: GridItem.prototype,

    /** @override */
    decorate: function() {
      GridItem.prototype.decorate.call(this);
      var imageEl = cr.doc.createElement('img');
      // Force 1x scale for chrome://theme URLs. Grid elements are much smaller
      // than actual images so there is no need in full scale on HDPI.
      var url = this.dataItem.url;
      if (url.slice(0, CHROME_THEME_PATH.length) == CHROME_THEME_PATH)
        imageEl.src = this.dataItem.url + '@1x';
      else
        imageEl.src = this.dataItem.url;
      imageEl.title = this.dataItem.title || '';
      if (typeof this.dataItem.clickHandler == 'function')
        imageEl.addEventListener('mousedown', this.dataItem.clickHandler);
      // Remove any garbage added by GridItem and ListItem decorators.
      this.textContent = '';
      this.appendChild(imageEl);
      if (typeof this.dataItem.decorateFn == 'function')
        this.dataItem.decorateFn(this);
      this.setAttribute('role', 'option');
      this.oncontextmenu = function(e) { e.preventDefault(); };
    }
  };

  /**
   * Creates a selection controller that wraps selection on grid ends
   * and translates Enter presses into 'activate' events.
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {cr.ui.Grid} grid The grid to interact with.
   * @constructor
   * @extends {cr.ui.GridSelectionController}
   */
  function UserImagesGridSelectionController(selectionModel, grid) {
    GridSelectionController.call(this, selectionModel, grid);
  }

  UserImagesGridSelectionController.prototype = {
    __proto__: GridSelectionController.prototype,

    /** @override */
    getIndexBefore: function(index) {
      var result =
          GridSelectionController.prototype.getIndexBefore.call(this, index);
      return result == -1 ? this.getLastIndex() : result;
    },

    /** @override */
    getIndexAfter: function(index) {
      var result =
          GridSelectionController.prototype.getIndexAfter.call(this, index);
      return result == -1 ? this.getFirstIndex() : result;
    },

    /** @override */
    handleKeyDown: function(e) {
      if (e.keyIdentifier == 'Enter')
        cr.dispatchSimpleEvent(this.grid_, 'activate');
      else
        GridSelectionController.prototype.handleKeyDown.call(this, e);
    }
  };

  /**
   * Creates a new user images grid element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Grid}
   */
  var UserImagesGrid = cr.ui.define('grid');

  UserImagesGrid.prototype = {
    __proto__: Grid.prototype,

    /** @override */
    createSelectionController: function(sm) {
      return new UserImagesGridSelectionController(sm, this);
    },

    /** @override */
    decorate: function() {
      Grid.prototype.decorate.call(this);
      this.dataModel = new ArrayDataModel([]);
      this.itemConstructor = UserImagesGridItem;
      this.selectionModel = new ListSingleSelectionModel();
      this.inProgramSelection_ = false;
      this.addEventListener('dblclick', this.handleDblClick_.bind(this));
      this.addEventListener('change', this.handleChange_.bind(this));
      this.setAttribute('role', 'listbox');
      this.autoExpands = true;
    },

    /**
     * Handles double click on the image grid.
     * @param {Event} e Double click Event.
     * @private
     */
    handleDblClick_: function(e) {
      // If a child element is double-clicked and not the grid itself, handle
      // this as 'Enter' keypress.
      if (e.target != this)
        cr.dispatchSimpleEvent(this, 'activate');
    },

    /**
     * Handles selection change.
     * @param {Event} e Double click Event.
     * @private
     */
    handleChange_: function(e) {
      if (this.selectedItem === null)
        return;

      var oldSelectionType = this.selectionType;

      // Update current selection type.
      this.selectionType = this.selectedItem.type;

      // Show grey silhouette with the same border as stock images.
      if (/^chrome:\/\/theme\//.test(this.selectedItemUrl))
        this.previewElement.classList.add('default-image');

      this.updatePreview_();

      var e = new Event('select');
      e.oldSelectionType = oldSelectionType;
      this.dispatchEvent(e);
    },

    /**
     * Updates the preview image, if present.
     * @private
     */
    updatePreview_: function() {
      var url = this.selectedItemUrl;
      if (url && this.previewImage_) {
        if (url.slice(0, CHROME_THEME_PATH.length) == CHROME_THEME_PATH)
          this.previewImage_.src = url + '@' + window.devicePixelRatio + 'x';
        else
          this.previewImage_.src = url;
      }
    },

    /**
     * Whether a camera is present or not.
     * @type {boolean}
     */
    get cameraPresent() {
      return this.cameraPresent_;
    },
    set cameraPresent(value) {
      this.cameraPresent_ = value;
      if (this.cameraLive)
        this.cameraImage = null;
    },

    /**
     * Whether camera is actually streaming video. May be |false| even when
     * camera is present and shown but still initializing.
     * @type {boolean}
     */
    get cameraOnline() {
      return this.previewElement.classList.contains('online');
    },
    set cameraOnline(value) {
      this.previewElement.classList.toggle('online', value);
    },

    /**
     * Tries to starts camera stream capture.
     * @param {function(): boolean} onAvailable Callback that is called if
     *     camera is available. If it returns |true|, capture is started
     *     immediately.
     */
    startCamera: function(onAvailable, onAbsent) {
      this.stopCamera();
      this.cameraStartInProgress_ = true;
      navigator.webkitGetUserMedia(
          {video: true},
          this.handleCameraAvailable_.bind(this, onAvailable),
          this.handleCameraAbsent_.bind(this));
    },

    /**
     * Stops camera capture, if it's currently active.
     */
    stopCamera: function() {
      this.cameraOnline = false;
      if (this.cameraVideo_)
        this.cameraVideo_.src = '';
      if (this.cameraStream_)
        this.cameraStream_.stop();
      // Cancel any pending getUserMedia() checks.
      this.cameraStartInProgress_ = false;
    },

    /**
     * Handles successful camera check.
     * @param {function(): boolean} onAvailable Callback to call. If it returns
     *     |true|, capture is started immediately.
     * @param {MediaStream} stream Stream object as returned by getUserMedia.
     * @private
     */
    handleCameraAvailable_: function(onAvailable, stream) {
      if (this.cameraStartInProgress_ && onAvailable()) {
        this.cameraVideo_.src = URL.createObjectURL(stream);
        this.cameraStream_ = stream;
      } else {
        stream.stop();
      }
      this.cameraStartInProgress_ = false;
    },

    /**
     * Handles camera check failure.
     * @param {NavigatorUserMediaError=} err Error object.
     * @private
     */
    handleCameraAbsent_: function(err) {
      this.cameraPresent = false;
      this.cameraOnline = false;
      this.cameraStartInProgress_ = false;
    },

    /**
     * Handles successful camera capture start.
     * @private
     */
    handleVideoStarted_: function() {
      this.cameraOnline = true;
      this.handleVideoUpdate_();
    },

    /**
     * Handles camera stream update. Called regularly (at rate no greater then
     * 4/sec) while camera stream is live.
     * @private
     */
    handleVideoUpdate_: function() {
      this.lastFrameTime_ = new Date().getTime();
    },

    /**
     * Type of the selected image (one of 'default', 'profile', 'camera').
     * Setting it will update class list of |previewElement|.
     * @type {string}
     */
    get selectionType() {
      return this.selectionType_;
    },
    set selectionType(value) {
      this.selectionType_ = value;
      var previewClassList = this.previewElement.classList;
      previewClassList[value == 'default' ? 'add' : 'remove']('default-image');
      previewClassList[value == 'profile' ? 'add' : 'remove']('profile-image');
      previewClassList[value == 'camera' ? 'add' : 'remove']('camera');

      var setFocusIfLost = function() {
        // Set focus to the grid, if focus is not on UI.
        if (!document.activeElement ||
            document.activeElement.tagName == 'BODY') {
          $('user-image-grid').focus();
        }
      }
      // Timeout guarantees processing AFTER style changes display attribute.
      setTimeout(setFocusIfLost, 0);
    },

    /**
     * Current image captured from camera as data URL. Setting to null will
     * return to the live camera stream.
     * @type {string=}
     */
    get cameraImage() {
      return this.cameraImage_;
    },
    set cameraImage(imageUrl) {
      this.cameraLive = !imageUrl;
      if (this.cameraPresent && !imageUrl)
        imageUrl = UserImagesGrid.ButtonImages.TAKE_PHOTO;
      if (imageUrl) {
        this.cameraImage_ = this.cameraImage_ ?
            this.updateItem(this.cameraImage_, imageUrl, this.cameraTitle_) :
            this.addItem(imageUrl, this.cameraTitle_, undefined, 0);
        this.cameraImage_.type = 'camera';
      } else {
        this.removeItem(this.cameraImage_);
        this.cameraImage_ = null;
      }
    },

    /**
     * Updates the titles for the camera element.
     * @param {string} placeholderTitle Title when showing a placeholder.
     * @param {string} capturedImageTitle Title when showing a captured photo.
     */
    setCameraTitles: function(placeholderTitle, capturedImageTitle) {
      this.placeholderTitle_ = placeholderTitle;
      this.capturedImageTitle_ = capturedImageTitle;
      this.cameraTitle_ = this.placeholderTitle_;
    },

    /**
     * True when camera is in live mode (i.e. no still photo selected).
     * @type {boolean}
     */
    get cameraLive() {
      return this.cameraLive_;
    },
    set cameraLive(value) {
      this.cameraLive_ = value;
      this.previewElement.classList[value ? 'add' : 'remove']('live');
    },

    /**
     * Should only be queried from the 'change' event listener, true if the
     * change event was triggered by a programmatical selection change.
     * @type {boolean}
     */
    get inProgramSelection() {
      return this.inProgramSelection_;
    },

    /**
     * URL of the image selected.
     * @type {string?}
     */
    get selectedItemUrl() {
      var selectedItem = this.selectedItem;
      return selectedItem ? selectedItem.url : null;
    },
    set selectedItemUrl(url) {
      for (var i = 0, el; el = this.dataModel.item(i); i++) {
        if (el.url === url)
          this.selectedItemIndex = i;
      }
    },

    /**
     * Set index to the image selected.
     * @type {number} index The index of selected image.
     */
    set selectedItemIndex(index) {
      this.inProgramSelection_ = true;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
    },

    /** @override */
    get selectedItem() {
      var index = this.selectionModel.selectedIndex;
      return index != -1 ? this.dataModel.item(index) : null;
    },
    set selectedItem(selectedItem) {
      var index = this.indexOf(selectedItem);
      this.inProgramSelection_ = true;
      this.selectionModel.selectedIndex = index;
      this.selectionModel.leadIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * Element containing the preview image (the first IMG element) and the
     * camera live stream (the first VIDEO element).
     * @type {HTMLElement}
     */
    get previewElement() {
      // TODO(ivankr): temporary hack for non-HTML5 version.
      return this.previewElement_ || this;
    },
    set previewElement(value) {
      this.previewElement_ = value;
      this.previewImage_ = value.querySelector('img');
      this.cameraVideo_ = value.querySelector('video');
      this.cameraVideo_.addEventListener('canplay',
                                         this.handleVideoStarted_.bind(this));
      this.cameraVideo_.addEventListener('timeupdate',
                                         this.handleVideoUpdate_.bind(this));
      this.updatePreview_();
      // Initialize camera state and check for its presence.
      this.cameraLive = true;
      this.cameraPresent = false;
    },

    /**
     * Whether the camera live stream and photo should be flipped horizontally.
     * If setting this property results in photo update, 'photoupdated' event
     * will be fired with 'dataURL' property containing the photo encoded as
     * a data URL
     * @type {boolean}
     */
    get flipPhoto() {
      return this.flipPhoto_ || false;
    },
    set flipPhoto(value) {
      if (this.flipPhoto_ == value)
        return;
      this.flipPhoto_ = value;
      this.previewElement.classList.toggle('flip-x', value);
      /* TODO(merkulova): remove when webkit crbug.com/126479 is fixed. */
      this.flipPhotoElement.classList.toggle('flip-trick', value);
      if (!this.cameraLive) {
        // Flip current still photo.
        var e = new Event('photoupdated');
        e.dataURL = this.flipPhoto ?
            this.flipFrame_(this.previewImage_) : this.previewImage_.src;
        this.dispatchEvent(e);
      }
    },

    /**
     * Performs photo capture from the live camera stream. 'phototaken' event
     * will be fired as soon as captured photo is available, with 'dataURL'
     * property containing the photo encoded as a data URL.
     * @return {boolean} Whether photo capture was successful.
     */
    takePhoto: function() {
      if (!this.cameraOnline)
        return false;
      var canvas = document.createElement('canvas');
      canvas.width = CAPTURE_SIZE.width;
      canvas.height = CAPTURE_SIZE.height;
      this.captureFrame_(
          this.cameraVideo_, canvas.getContext('2d'), CAPTURE_SIZE);
      // Preload image before displaying it.
      var previewImg = new Image();
      previewImg.addEventListener('load', function(e) {
        this.cameraTitle_ = this.capturedImageTitle_;
        this.cameraImage = previewImg.src;
      }.bind(this));
      previewImg.src = canvas.toDataURL('image/png');
      var e = new Event('phototaken');
      e.dataURL = this.flipPhoto ? this.flipFrame_(canvas) : previewImg.src;
      this.dispatchEvent(e);
      return true;
    },

    /**
     * Performs video capture from the live camera stream.
     * @param {function=} opt_callback Callback that receives taken video as
     *     data URL of a vertically stacked PNG sprite.
     */
    takeVideo: function(opt_callback) {
      var canvas = document.createElement('canvas');
      canvas.width = CAPTURE_SIZE.width;
      canvas.height = CAPTURE_SIZE.height * RECORD_FRAMES;
      var ctx = canvas.getContext('2d');
      // Force canvas initialization to prevent FPS lag on the first frame.
      ctx.fillRect(0, 0, 1, 1);
      var captureData = {
        callback: opt_callback,
        canvas: canvas,
        ctx: ctx,
        frameNo: 0,
        lastTimestamp: new Date().getTime()
      };
      captureData.timer = window.setInterval(
          this.captureVideoFrame_.bind(this, captureData), 1000 / RECORD_FPS);
    },

    /**
     * Discard current photo and return to the live camera stream.
     */
    discardPhoto: function() {
      this.cameraTitle_ = this.placeholderTitle_;
      this.cameraImage = null;
    },

    /**
     * Capture a single still frame from a <video> element, placing it at the
     * current drawing origin of a canvas context.
     * @param {HTMLVideoElement} video Video element to capture from.
     * @param {CanvasRenderingContext2D} ctx Canvas context to draw onto.
     * @param {{width: number, height: number}} destSize Capture size.
     * @private
     */
    captureFrame_: function(video, ctx, destSize) {
      var width = video.videoWidth;
      var height = video.videoHeight;
      if (width < destSize.width || height < destSize.height) {
        console.error('Video capture size too small: ' +
                      width + 'x' + height + '!');
      }
      var src = {};
      if (width / destSize.width > height / destSize.height) {
        // Full height, crop left/right.
        src.height = height;
        src.width = height * destSize.width / destSize.height;
      } else {
        // Full width, crop top/bottom.
        src.width = width;
        src.height = width * destSize.height / destSize.width;
      }
      src.x = (width - src.width) / 2;
      src.y = (height - src.height) / 2;
      ctx.drawImage(video, src.x, src.y, src.width, src.height,
                    0, 0, destSize.width, destSize.height);
    },

    /**
     * Flips frame horizontally.
     * @param {HTMLImageElement|HTMLCanvasElement|HTMLVideoElement} source
     *     Frame to flip.
     * @return {string} Flipped frame as data URL.
     */
    flipFrame_: function(source) {
      var canvas = document.createElement('canvas');
      canvas.width = CAPTURE_SIZE.width;
      canvas.height = CAPTURE_SIZE.height;
      var ctx = canvas.getContext('2d');
      ctx.translate(CAPTURE_SIZE.width, 0);
      ctx.scale(-1.0, 1.0);
      ctx.drawImage(source, 0, 0);
      return canvas.toDataURL('image/png');
    },

    /**
     * Capture next frame of the video being recorded after a takeVideo() call.
     * @param {Object} data Property bag with the recorder details.
     * @private
     */
    captureVideoFrame_: function(data) {
      var lastTimestamp = new Date().getTime();
      var delayMs = lastTimestamp - data.lastTimestamp;
      console.error('Delay: ' + delayMs + ' (' + (1000 / delayMs + ' FPS)'));
      data.lastTimestamp = lastTimestamp;

      this.captureFrame_(this.cameraVideo_, data.ctx, CAPTURE_SIZE);
      data.ctx.translate(0, CAPTURE_SIZE.height);

      if (++data.frameNo == RECORD_FRAMES) {
        window.clearTimeout(data.timer);
        if (data.callback && typeof data.callback == 'function')
          data.callback(data.canvas.toDataURL('image/png'));
      }
    },

    /**
     * Adds new image to the user image grid.
     * @param {string} src Image URL.
     * @param {string=} opt_title Image tooltip.
     * @param {function=} opt_clickHandler Image click handler.
     * @param {number=} opt_position If given, inserts new image into
     *     that position (0-based) in image list.
     * @param {function=} opt_decorateFn Function called with the list element
     *     as argument to do any final decoration.
     * @return {!Object} Image data inserted into the data model.
     */
    // TODO(ivankr): this function needs some argument list refactoring.
    addItem: function(url, opt_title, opt_clickHandler, opt_position,
                      opt_decorateFn) {
      var imageInfo = {
        url: url,
        title: opt_title,
        clickHandler: opt_clickHandler,
        decorateFn: opt_decorateFn
      };
      this.inProgramSelection_ = true;
      if (opt_position !== undefined)
        this.dataModel.splice(opt_position, 0, imageInfo);
      else
        this.dataModel.push(imageInfo);
      this.inProgramSelection_ = false;
      return imageInfo;
    },

    /**
     * Returns index of an image in grid.
     * @param {Object} imageInfo Image data returned from addItem() call.
     * @return {number} Image index (0-based) or -1 if image was not found.
     */
    indexOf: function(imageInfo) {
      return this.dataModel.indexOf(imageInfo);
    },

    /**
     * Replaces an image in the grid.
     * @param {Object} imageInfo Image data returned from addItem() call.
     * @param {string} imageUrl New image URL.
     * @param {string=} opt_title New image tooltip (if undefined, tooltip
     *     is left unchanged).
     * @return {!Object} Image data of the added or updated image.
     */
    updateItem: function(imageInfo, imageUrl, opt_title) {
      var imageIndex = this.indexOf(imageInfo);
      var wasSelected = this.selectionModel.selectedIndex == imageIndex;
      this.removeItem(imageInfo);
      var newInfo = this.addItem(
          imageUrl,
          opt_title === undefined ? imageInfo.title : opt_title,
          imageInfo.clickHandler,
          imageIndex,
          imageInfo.decorateFn);
      // Update image data with the reset of the keys from the old data.
      for (k in imageInfo) {
        if (!(k in newInfo))
          newInfo[k] = imageInfo[k];
      }
      if (wasSelected)
        this.selectedItem = newInfo;
      return newInfo;
    },

    /**
     * Removes previously added image from the grid.
     * @param {Object} imageInfo Image data returned from the addItem() call.
     */
    removeItem: function(imageInfo) {
      var index = this.indexOf(imageInfo);
      if (index != -1) {
        var wasSelected = this.selectionModel.selectedIndex == index;
        this.inProgramSelection_ = true;
        this.dataModel.splice(index, 1);
        if (wasSelected) {
          // If item removed was selected, select the item next to it.
          this.selectedItem = this.dataModel.item(
              Math.min(this.dataModel.length - 1, index));
        }
        this.inProgramSelection_ = false;
      }
    },

    /**
     * Forces re-display, size re-calculation and focuses grid.
     */
    updateAndFocus: function() {
      // Recalculate the measured item size.
      this.measured_ = null;
      this.columns = 0;
      this.redraw();
      this.focus();
    }
  };

  /**
   * URLs of special button images.
   * @enum {string}
   */
  UserImagesGrid.ButtonImages = {
    TAKE_PHOTO: 'chrome://theme/IDR_BUTTON_USER_IMAGE_TAKE_PHOTO',
    CHOOSE_FILE: 'chrome://theme/IDR_BUTTON_USER_IMAGE_CHOOSE_FILE',
    PROFILE_PICTURE: 'chrome://theme/IDR_PROFILE_PICTURE_LOADING'
  };

  return {
    UserImagesGrid: UserImagesGrid
  };
});
