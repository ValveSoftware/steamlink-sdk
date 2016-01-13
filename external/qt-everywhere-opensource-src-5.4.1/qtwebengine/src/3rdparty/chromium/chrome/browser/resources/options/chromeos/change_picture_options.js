// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  var UserImagesGrid = options.UserImagesGrid;
  var ButtonImages = UserImagesGrid.ButtonImages;

  /**
   * Array of button URLs used on this page.
   * @type {Array.<string>}
   * @const
   */
  var ButtonImageUrls = [
    ButtonImages.TAKE_PHOTO,
    ButtonImages.CHOOSE_FILE
  ];

  /////////////////////////////////////////////////////////////////////////////
  // ChangePictureOptions class:

  /**
   * Encapsulated handling of ChromeOS change picture options page.
   * @constructor
   */
  function ChangePictureOptions() {
    OptionsPage.call(
        this,
        'changePicture',
        loadTimeData.getString('changePicturePage'),
        'change-picture-page');
  }

  cr.addSingletonGetter(ChangePictureOptions);

  ChangePictureOptions.prototype = {
    // Inherit ChangePictureOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes ChangePictureOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preferences initialization.
      OptionsPage.prototype.initializePage.call(this);

      var imageGrid = $('user-image-grid');
      UserImagesGrid.decorate(imageGrid);

      // Preview image will track the selected item's URL.
      var previewElement = $('user-image-preview');
      previewElement.oncontextmenu = function(e) { e.preventDefault(); };

      imageGrid.previewElement = previewElement;
      imageGrid.selectionType = 'default';
      imageGrid.flipPhotoElement = $('flip-photo');

      imageGrid.addEventListener('select',
                                 this.handleImageSelected_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));
      imageGrid.addEventListener('phototaken',
                                 this.handlePhotoTaken_.bind(this));
      imageGrid.addEventListener('photoupdated',
                                 this.handlePhotoTaken_.bind(this));

      // Add the "Choose file" button.
      imageGrid.addItem(ButtonImages.CHOOSE_FILE,
                        loadTimeData.getString('chooseFile'),
                        this.handleChooseFile_.bind(this)).type = 'file';

      // Profile image data.
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,
          loadTimeData.getString('profilePhotoLoading'));
      this.profileImage_.type = 'profile';

      // Set the title for camera item in the grid.
      imageGrid.setCameraTitles(
          loadTimeData.getString('takePhoto'),
          loadTimeData.getString('photoFromCamera'));

      $('take-photo').addEventListener(
          'click', this.handleTakePhoto_.bind(this));
      $('discard-photo').addEventListener(
          'click', this.handleDiscardPhoto_.bind(this));

      // Toggle 'animation' class for the duration of WebKit transition.
      $('flip-photo').addEventListener(
          'click', this.handleFlipPhoto_.bind(this));
      $('user-image-stream-crop').addEventListener(
          'webkitTransitionEnd', function(e) {
            previewElement.classList.remove('animation');
          });
      $('user-image-preview-img').addEventListener(
          'webkitTransitionEnd', function(e) {
            previewElement.classList.remove('animation');
          });

      // Old user image data (if present).
      this.oldImage_ = null;

      $('change-picture-overlay-confirm').addEventListener(
          'click', this.closeOverlay_.bind(this));

      chrome.send('onChangePicturePageInitialized');
    },

    /**
     * Called right after the page has been shown to user.
     */
    didShowPage: function() {
      var imageGrid = $('user-image-grid');
      // Reset camera element.
      imageGrid.cameraImage = null;
      imageGrid.updateAndFocus();
      chrome.send('onChangePicturePageShown');
    },

    /**
     * Called right before the page is hidden.
     */
    willHidePage: function() {
      var imageGrid = $('user-image-grid');
      imageGrid.blur();  // Make sure the image grid is not active.
      imageGrid.stopCamera();
      if (this.oldImage_) {
        imageGrid.removeItem(this.oldImage_);
        this.oldImage_ = null;
      }
      chrome.send('onChangePicturePageHidden');
    },

    /**
     * Called right after the page has been hidden.
     */
    // TODO(ivankr): both callbacks are required as only one of them is called
    // depending on the way the page was closed, see http://crbug.com/118923.
    didClosePage: function() {
      this.willHidePage();
    },

    /**
     * Closes the overlay, returning to the main settings page.
     * @private
     */
    closeOverlay_: function() {
      if (!$('change-picture-page').hidden)
        OptionsPage.closeOverlay();
    },

    /**
     * Handle camera-photo flip.
     */
    handleFlipPhoto_: function() {
      var imageGrid = $('user-image-grid');
      imageGrid.previewElement.classList.add('animation');
      imageGrid.flipPhoto = !imageGrid.flipPhoto;
      var flipMessageId = imageGrid.flipPhoto ?
         'photoFlippedAccessibleText' : 'photoFlippedBackAccessibleText';
      announceAccessibleMessage(loadTimeData.getString(flipMessageId));
    },

    /**
     * Handles "Take photo" button click.
     * @private
     */
    handleTakePhoto_: function() {
      $('user-image-grid').takePhoto();
      chrome.send('takePhoto');
    },

    /**
     * Handle photo captured event.
     * @param {Event} e Event with 'dataURL' property containing a data URL.
     */
    handlePhotoTaken_: function(e) {
      chrome.send('photoTaken', [e.dataURL]);
      announceAccessibleMessage(
          loadTimeData.getString('photoCaptureAccessibleText'));
    },

    /**
     * Handles "Discard photo" button click.
     * @private
     */
    handleDiscardPhoto_: function() {
      $('user-image-grid').discardPhoto();
      chrome.send('discardPhoto');
      announceAccessibleMessage(
          loadTimeData.getString('photoDiscardAccessibleText'));
    },

    /**
     * Handles "Choose a file" button activation.
     * @private
     */
    handleChooseFile_: function() {
      chrome.send('chooseFile');
      this.closeOverlay_();
    },

    /**
     * Handles image selection change.
     * @param {Event} e Selection change Event.
     * @private
     */
    handleImageSelected_: function(e) {
      var imageGrid = $('user-image-grid');
      var url = imageGrid.selectedItemUrl;

      // Flip button available only for camera picture.
      imageGrid.flipPhotoElement.tabIndex =
          imageGrid.selectionType == 'camera' ? 1 : -1;
      // Ignore selection change caused by program itself and selection of one
      // of the action buttons.
      if (!imageGrid.inProgramSelection &&
          url != ButtonImages.TAKE_PHOTO && url != ButtonImages.CHOOSE_FILE) {
        chrome.send('selectImage', [url, imageGrid.selectionType]);
      }
      // Start/stop camera on (de)selection.
      if (!imageGrid.inProgramSelection &&
          imageGrid.selectionType != e.oldSelectionType) {
        if (imageGrid.selectionType == 'camera') {
          imageGrid.startCamera(
              function() {
                // Start capture if camera is still the selected item.
                return imageGrid.selectedItem == imageGrid.cameraImage;
              });
        } else {
          imageGrid.stopCamera();
        }
      }
      // Update image attribution text.
      var image = imageGrid.selectedItem;
      $('user-image-author-name').textContent = image.author;
      $('user-image-author-website').textContent = image.website;
      $('user-image-author-website').href = image.website;
      $('user-image-attribution').style.visibility =
          (image.author || image.website) ? 'visible' : 'hidden';
    },

    /**
     * Handles image activation (by pressing Enter).
     * @private
     */
    handleImageActivated_: function() {
      switch ($('user-image-grid').selectedItemUrl) {
        case ButtonImages.TAKE_PHOTO:
          this.handleTakePhoto_();
          break;
        case ButtonImages.CHOOSE_FILE:
          this.handleChooseFile_();
          break;
        default:
          this.closeOverlay_();
          break;
      }
    },

    /**
     * Adds or updates old user image taken from file/camera (neither a profile
     * image nor a default one).
     * @param {string} imageUrl Old user image, as data or internal URL.
     * @private
     */
    setOldImage_: function(imageUrl) {
      var imageGrid = $('user-image-grid');
      if (this.oldImage_) {
        this.oldImage_ = imageGrid.updateItem(this.oldImage_, imageUrl);
      } else {
        // Insert next to the profile image.
        var pos = imageGrid.indexOf(this.profileImage_) + 1;
        this.oldImage_ = imageGrid.addItem(imageUrl, undefined, undefined, pos);
        this.oldImage_.type = 'old';
        imageGrid.selectedItem = this.oldImage_;
      }
    },

    /**
     * Updates user's profile image.
     * @param {string} imageUrl Profile image, encoded as data URL.
     * @param {boolean} select If true, profile image should be selected.
     * @private
     */
    setProfileImage_: function(imageUrl, select) {
      var imageGrid = $('user-image-grid');
      this.profileImage_ = imageGrid.updateItem(
          this.profileImage_, imageUrl, loadTimeData.getString('profilePhoto'));
      if (select)
        imageGrid.selectedItem = this.profileImage_;
    },

    /**
     * Selects user image with the given URL.
     * @param {string} url URL of the image to select.
     * @private
     */
    setSelectedImage_: function(url) {
      $('user-image-grid').selectedItemUrl = url;
    },

    /**
     * @param {boolean} present Whether camera is detected.
     */
    setCameraPresent_: function(present) {
      $('user-image-grid').cameraPresent = present;
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array.<{url: string, author: string, website: string}>}
     *   imagesData An array of default images data, including URL, author and
     *   website.
     * @private
     */
    setDefaultImages_: function(imagesData) {
      var imageGrid = $('user-image-grid');
      for (var i = 0, data; data = imagesData[i]; i++) {
        var item = imageGrid.addItem(data.url, data.title);
        item.type = 'default';
        item.author = data.author || '';
        item.website = data.website || '';
      }
    },
  };

  // Forward public APIs to private implementations.
  [
    'closeOverlay',
    'setCameraPresent',
    'setDefaultImages',
    'setOldImage',
    'setProfileImage',
    'setSelectedImage',
  ].forEach(function(name) {
    ChangePictureOptions[name] = function() {
      var instance = ChangePictureOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ChangePictureOptions: ChangePictureOptions
  };

});
