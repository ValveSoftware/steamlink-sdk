// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WallpaperManager constructor.
 *
 * WallpaperManager objects encapsulate the functionality of the wallpaper
 * manager extension.
 *
 * @constructor
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     extension UI.
 */

function WallpaperManager(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.document_ = dialogDom.ownerDocument;
  this.enableOnlineWallpaper_ = loadTimeData.valueExists('manifestBaseURL');
  this.selectedCategory = null;
  this.selectedItem_ = null;
  this.progressManager_ = new ProgressManager();
  this.customWallpaperData_ = null;
  this.currentWallpaper_ = null;
  this.wallpaperRequest_ = null;
  this.wallpaperDirs_ = WallpaperDirectories.getInstance();
  this.preManifestDomInit_();
  this.fetchManifest_();
}

// Anonymous 'namespace'.
// TODO(bshe): Get rid of anonymous namespace.
(function() {

  /**
   * URL of the learn more page for wallpaper picker.
   */
  /** @const */ var LearnMoreURL =
      'https://support.google.com/chromeos/?p=wallpaper_fileerror&hl=' +
          navigator.language;

  /**
   * Index of the All category. It is the first category in wallpaper picker.
   */
  /** @const */ var AllCategoryIndex = 0;

  /**
   * Index offset of categories parsed from manifest. The All category is added
   * before them. So the offset is 1.
   */
  /** @const */ var OnlineCategoriesOffset = 1;

  /**
   * Returns a translated string.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to localStrings.getString(id).
   *
   * @param {string} id The id of the string to return.
   * @return {string} The translated string.
   */
  function str(id) {
    return loadTimeData.getString(id);
  }

  /**
   * Retruns the current selected layout.
   * @return {string} The selected layout.
   */
  function getSelectedLayout() {
    var setWallpaperLayout = $('set-wallpaper-layout');
    return setWallpaperLayout.options[setWallpaperLayout.selectedIndex].value;
  }

  /**
   * Loads translated strings.
   */
  WallpaperManager.initStrings = function(callback) {
    chrome.wallpaperPrivate.getStrings(function(strings) {
      loadTimeData.data = strings;
      if (callback)
        callback();
    });
  };

  /**
   * Requests wallpaper manifest file from server.
   */
  WallpaperManager.prototype.fetchManifest_ = function() {
    var locale = navigator.language;
    if (!this.enableOnlineWallpaper_) {
      this.postManifestDomInit_();
      return;
    }

    var urls = [
        str('manifestBaseURL') + locale + '.json',
        // Fallback url. Use 'en' locale by default.
        str('manifestBaseURL') + 'en.json'];

    var asyncFetchManifestFromUrls = function(urls, func, successCallback,
                                              failureCallback) {
      var index = 0;
      var loop = {
        next: function() {
          if (index < urls.length) {
            func(loop, urls[index]);
            index++;
          } else {
            failureCallback();
          }
        },

        success: function(response) {
          successCallback(response);
        },

        failure: function() {
          failureCallback();
        }
      };
      loop.next();
    };

    var fetchManifestAsync = function(loop, url) {
      var xhr = new XMLHttpRequest();
      try {
        xhr.addEventListener('loadend', function(e) {
          if (this.status == 200 && this.responseText != null) {
            try {
              var manifest = JSON.parse(this.responseText);
              loop.success(manifest);
            } catch (e) {
              loop.failure();
            }
          } else {
            loop.next();
          }
        });
        xhr.open('GET', url, true);
        xhr.send(null);
      } catch (e) {
        loop.failure();
      }
    };

    if (navigator.onLine) {
      asyncFetchManifestFromUrls(urls, fetchManifestAsync,
                                 this.onLoadManifestSuccess_.bind(this),
                                 this.onLoadManifestFailed_.bind(this));
    } else {
      // If device is offline, fetches manifest from local storage.
      // TODO(bshe): Always loading the offline manifest first and replacing
      // with the online one when available.
      this.onLoadManifestFailed_();
    }
  };

  /**
   * Shows error message in a centered dialog.
   * @private
   * @param {string} errroMessage The string to show in the error dialog.
   */
  WallpaperManager.prototype.showError_ = function(errorMessage) {
    document.querySelector('.error-message').textContent = errorMessage;
    $('error-container').hidden = false;
  };

  /**
   * Sets manifest loaded from server. Called after manifest is successfully
   * loaded.
   * @param {object} manifest The parsed manifest file.
   */
  WallpaperManager.prototype.onLoadManifestSuccess_ = function(manifest) {
    this.manifest_ = manifest;
    WallpaperUtil.saveToStorage(Constants.AccessManifestKey, manifest, false);
    this.postManifestDomInit_();
  };

  // Sets manifest to previously saved object if any and shows connection error.
  // Called after manifest failed to load.
  WallpaperManager.prototype.onLoadManifestFailed_ = function() {
    var accessManifestKey = Constants.AccessManifestKey;
    var self = this;
    Constants.WallpaperLocalStorage.get(accessManifestKey, function(items) {
      self.manifest_ = items[accessManifestKey] ? items[accessManifestKey] : {};
      self.showError_(str('connectionFailed'));
      self.postManifestDomInit_();
      $('wallpaper-grid').classList.add('image-picker-offline');
    });
  };

  /**
   * Toggle surprise me feature of wallpaper picker. It fires an storage
   * onChanged event. Event handler for that event is in event_page.js.
   * @private
   */
  WallpaperManager.prototype.toggleSurpriseMe_ = function() {
    var checkbox = $('surprise-me').querySelector('#checkbox');
    var shouldEnable = !checkbox.classList.contains('checked');
    WallpaperUtil.saveToStorage(Constants.AccessSurpriseMeEnabledKey,
                                shouldEnable, true, function() {
      if (chrome.runtime.lastError == null) {
          if (shouldEnable) {
            checkbox.classList.add('checked');
          } else {
            checkbox.classList.remove('checked');
          }
          $('categories-list').disabled = shouldEnable;
          $('wallpaper-grid').disabled = shouldEnable;
        } else {
          // TODO(bshe): show error message to user.
          console.error('Failed to save surprise me option to chrome storage.');
        }
    });
  };

  /**
   * One-time initialization of various DOM nodes. Fetching manifest may take a
   * long time due to slow connection. Dom nodes that do not depend on manifest
   * should be initialized here to unblock from manifest fetching.
   */
  WallpaperManager.prototype.preManifestDomInit_ = function() {
    $('window-close-button').addEventListener('click', function() {
      window.close();
    });
    this.document_.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));
    this.document_.defaultView.addEventListener(
        'keydown', this.onKeyDown_.bind(this));
    $('learn-more').href = LearnMoreURL;
    $('close-error').addEventListener('click', function() {
      $('error-container').hidden = true;
    });
    $('close-wallpaper-selection').addEventListener('click', function() {
      $('wallpaper-selection-container').hidden = true;
      $('set-wallpaper-layout').disabled = true;
    });
  };

  /**
   * One-time initialization of various DOM nodes. Dom nodes that do depend on
   * manifest should be initialized here.
   */
  WallpaperManager.prototype.postManifestDomInit_ = function() {
    i18nTemplate.process(this.document_, loadTimeData);
    this.initCategoriesList_();
    this.initThumbnailsGrid_();
    this.presetCategory_();

    $('file-selector').addEventListener(
        'change', this.onFileSelectorChanged_.bind(this));
    $('set-wallpaper-layout').addEventListener(
        'change', this.onWallpaperLayoutChanged_.bind(this));

    if (loadTimeData.valueExists('wallpaperAppName')) {
      $('wallpaper-set-by-message').textContent = loadTimeData.getStringF(
          'currentWallpaperSetByMessage', str('wallpaperAppName'));
    }

    if (this.enableOnlineWallpaper_) {
      var self = this;
      $('surprise-me').hidden = false;
      $('surprise-me').addEventListener('click',
                                        this.toggleSurpriseMe_.bind(this));
      Constants.WallpaperSyncStorage.get(Constants.AccessSurpriseMeEnabledKey,
                                          function(items) {
        // Surprise me has been moved from local to sync storage, prefer
        // values from sync, but if unset check local and update synced pref
        // if applicable.
        if (!items.hasOwnProperty(Constants.AccessSurpriseMeEnabledKey)) {
          Constants.WallpaperLocalStorage.get(
              Constants.AccessSurpriseMeEnabledKey, function(values) {
            if (values.hasOwnProperty(Constants.AccessSurpriseMeEnabledKey)) {
              WallpaperUtil.saveToStorage(Constants.AccessSurpriseMeEnabledKey,
                  values[Constants.AccessSurpriseMeEnabledKey], true);
            }
            if (values[Constants.AccessSurpriseMeEnabledKey]) {
                $('surprise-me').querySelector('#checkbox').classList.add(
                    'checked');
                $('categories-list').disabled = true;
                $('wallpaper-grid').disabled = true;
            }
          });
        } else if (items[Constants.AccessSurpriseMeEnabledKey]) {
          $('surprise-me').querySelector('#checkbox').classList.add('checked');
          $('categories-list').disabled = true;
          $('wallpaper-grid').disabled = true;
        }
      });

      window.addEventListener('offline', function() {
        chrome.wallpaperPrivate.getOfflineWallpaperList(function(lists) {
          if (!self.downloadedListMap_)
            self.downloadedListMap_ = {};
          for (var i = 0; i < lists.length; i++) {
            self.downloadedListMap_[lists[i]] = true;
          }
          var thumbnails = self.document_.querySelectorAll('.thumbnail');
          for (var i = 0; i < thumbnails.length; i++) {
            var thumbnail = thumbnails[i];
            var url = self.wallpaperGrid_.dataModel.item(i).baseURL;
            var fileName = url.substring(url.lastIndexOf('/') + 1) +
                Constants.HighResolutionSuffix;
            if (self.downloadedListMap_ &&
                self.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
              thumbnail.offline = true;
            }
          }
        });
        $('wallpaper-grid').classList.add('image-picker-offline');
      });
      window.addEventListener('online', function() {
        self.downloadedListMap_ = null;
        $('wallpaper-grid').classList.remove('image-picker-offline');
      });
    }

    this.onResize_();
    this.initContextMenuAndCommand_();
  };

  /**
   * One-time initialization of context menu and command.
   */
  WallpaperManager.prototype.initContextMenuAndCommand_ = function() {
    this.wallpaperContextMenu_ = $('wallpaper-context-menu');
    cr.ui.Menu.decorate(this.wallpaperContextMenu_);
    cr.ui.contextMenuHandler.setContextMenu(this.wallpaperGrid_,
                                            this.wallpaperContextMenu_);
    var commands = this.dialogDom_.querySelectorAll('command');
    for (var i = 0; i < commands.length; i++)
      cr.ui.Command.decorate(commands[i]);

    var doc = this.document_;
    doc.addEventListener('command', this.onCommand_.bind(this));
    doc.addEventListener('canExecute', this.onCommandCanExecute_.bind(this));
  };

  /**
   * Handles a command being executed.
   * @param {Event} event A command event.
   */
  WallpaperManager.prototype.onCommand_ = function(event) {
    if (event.command.id == 'delete') {
      var wallpaperGrid = this.wallpaperGrid_;
      var selectedIndex = wallpaperGrid.selectionModel.selectedIndex;
      var item = wallpaperGrid.dataModel.item(selectedIndex);
      if (!item || item.source != Constants.WallpaperSourceEnum.Custom)
        return;
      this.removeCustomWallpaper(item.baseURL);
      wallpaperGrid.dataModel.splice(selectedIndex, 1);
      // Calculate the number of remaining custom wallpapers. The add new button
      // in data model needs to be excluded.
      var customWallpaperCount = wallpaperGrid.dataModel.length - 1;
      if (customWallpaperCount == 0) {
        // Active custom wallpaper is also copied in chronos data dir. It needs
        // to be deleted.
        chrome.wallpaperPrivate.resetWallpaper();
        this.onWallpaperChanged_(null, null);
      } else {
        selectedIndex = Math.min(selectedIndex, customWallpaperCount - 1);
        wallpaperGrid.selectionModel.selectedIndex = selectedIndex;
      }
      event.cancelBubble = true;
    }
  };

  /**
   * Decides if a command can be executed on current target.
   * @param {Event} event A command event.
   */
  WallpaperManager.prototype.onCommandCanExecute_ = function(event) {
    switch (event.command.id) {
      case 'delete':
        var wallpaperGrid = this.wallpaperGrid_;
        var selectedIndex = wallpaperGrid.selectionModel.selectedIndex;
        var item = wallpaperGrid.dataModel.item(selectedIndex);
        if (selectedIndex != this.wallpaperGrid_.dataModel.length - 1 &&
          item && item.source == Constants.WallpaperSourceEnum.Custom) {
          event.canExecute = true;
          break;
        }
      default:
        event.canExecute = false;
    }
  };

  /**
   * Preset to the category which contains current wallpaper.
   */
  WallpaperManager.prototype.presetCategory_ = function() {
    this.currentWallpaper_ = str('currentWallpaper');
    // The currentWallpaper_ is either a url contains HightResolutionSuffix or a
    // custom wallpaper file name converted from an integer value represent
    // time (e.g., 13006377367586070).
    if (!this.enableOnlineWallpaper_ || (this.currentWallpaper_ &&
        this.currentWallpaper_.indexOf(Constants.HighResolutionSuffix) == -1)) {
      // Custom is the last one in the categories list.
      this.categoriesList_.selectionModel.selectedIndex =
          this.categoriesList_.dataModel.length - 1;
      return;
    }
    var self = this;
    var presetCategoryInner_ = function() {
      // Selects the first category in the categories list of current
      // wallpaper as the default selected category when showing wallpaper
      // picker UI.
      var presetCategory = AllCategoryIndex;
      if (self.currentWallpaper_) {
        for (var key in self.manifest_.wallpaper_list) {
          var url = self.manifest_.wallpaper_list[key].base_url +
              Constants.HighResolutionSuffix;
          if (url.indexOf(self.currentWallpaper_) != -1 &&
              self.manifest_.wallpaper_list[key].categories.length > 0) {
            presetCategory = self.manifest_.wallpaper_list[key].categories[0] +
                OnlineCategoriesOffset;
            break;
          }
        }
      }
      self.categoriesList_.selectionModel.selectedIndex = presetCategory;
    };
    if (navigator.onLine) {
      presetCategoryInner_();
    } else {
      // If device is offline, gets the available offline wallpaper list first.
      // Wallpapers which are not in the list will display a grayscaled
      // thumbnail.
      chrome.wallpaperPrivate.getOfflineWallpaperList(function(lists) {
        if (!self.downloadedListMap_)
          self.downloadedListMap_ = {};
        for (var i = 0; i < lists.length; i++)
          self.downloadedListMap_[lists[i]] = true;
        presetCategoryInner_();
      });
    }
  };

  /**
   * Constructs the thumbnails grid.
   */
  WallpaperManager.prototype.initThumbnailsGrid_ = function() {
    this.wallpaperGrid_ = $('wallpaper-grid');
    wallpapers.WallpaperThumbnailsGrid.decorate(this.wallpaperGrid_);
    this.wallpaperGrid_.autoExpands = true;

    this.wallpaperGrid_.addEventListener('change', this.onChange_.bind(this));
    this.wallpaperGrid_.addEventListener('dblclick', this.onClose_.bind(this));
  };

  /**
   * Handles change event dispatched by wallpaper grid.
   */
  WallpaperManager.prototype.onChange_ = function() {
    // splice may dispatch a change event because the position of selected
    // element changing. But the actual selected element may not change after
    // splice. Check if the new selected element equals to the previous selected
    // element before continuing. Otherwise, wallpaper may reset to previous one
    // as described in http://crbug.com/229036.
    if (this.selectedItem_ == this.wallpaperGrid_.selectedItem)
      return;
    this.selectedItem_ = this.wallpaperGrid_.selectedItem;
    this.onSelectedItemChanged_();
  };

  /**
   * Closes window if no pending wallpaper request.
   */
  WallpaperManager.prototype.onClose_ = function() {
    if (this.wallpaperRequest_) {
      this.wallpaperRequest_.addEventListener('loadend', function() {
        // Close window on wallpaper loading finished.
        window.close();
      });
    } else {
      window.close();
    }
  };

  /**
   * Moves the check mark to |activeItem| and hides the wallpaper set by third
   * party message if any. Called when wallpaper changed successfully.
   * @param {?Object} activeItem The active item in WallpaperThumbnailsGrid's
   *     data model.
   * @param {?string} currentWallpaperURL The URL or filename of current
   *     wallpaper.
   */
  WallpaperManager.prototype.onWallpaperChanged_ = function(
      activeItem, currentWallpaperURL) {
    this.wallpaperGrid_.activeItem = activeItem;
    this.currentWallpaper_ = currentWallpaperURL;
    // Hides the wallpaper set by message.
    $('wallpaper-set-by-message').textContent = '';
  };

  /**
    * Sets wallpaper to the corresponding wallpaper of selected thumbnail.
    * @param {{baseURL: string, layout: string, source: string,
    *          availableOffline: boolean, opt_dynamicURL: string,
    *          opt_author: string, opt_authorWebsite: string}}
    *     selectedItem the selected item in WallpaperThumbnailsGrid's data
    *     model.
    */
  WallpaperManager.prototype.setSelectedWallpaper_ = function(selectedItem) {
    var self = this;
    switch (selectedItem.source) {
      case Constants.WallpaperSourceEnum.Custom:
        var errorHandler = this.onFileSystemError_.bind(this);
        var success = function(dirEntry) {
          dirEntry.getFile(selectedItem.baseURL, {create: false},
                           function(fileEntry) {
            fileEntry.file(function(file) {
              var reader = new FileReader();
              reader.readAsArrayBuffer(file);
              reader.addEventListener('error', errorHandler);
              reader.addEventListener('load', function(e) {
                self.setCustomWallpaper(e.target.result,
                                        selectedItem.layout,
                                        false, selectedItem.baseURL,
                                        self.onWallpaperChanged_.bind(self,
                                            selectedItem, selectedItem.baseURL),
                                        errorHandler);
              });
            }, errorHandler);
          }, errorHandler);
        }
        this.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL,
                                         success, errorHandler);
        break;
      case Constants.WallpaperSourceEnum.OEM:
        // Resets back to default wallpaper.
        chrome.wallpaperPrivate.resetWallpaper();
        this.onWallpaperChanged_(selectedItem, selectedItem.baseURL);
        WallpaperUtil.saveWallpaperInfo(wallpaperURL, selectedItem.layout,
                                        selectedItem.source);
        break;
      case Constants.WallpaperSourceEnum.Online:
        var wallpaperURL = selectedItem.baseURL +
            Constants.HighResolutionSuffix;
        var selectedGridItem = this.wallpaperGrid_.getListItem(selectedItem);

        chrome.wallpaperPrivate.setWallpaperIfExists(wallpaperURL,
                                                     selectedItem.layout,
                                                     function(exists) {
          if (exists) {
            self.onWallpaperChanged_(selectedItem, wallpaperURL);
            WallpaperUtil.saveWallpaperInfo(wallpaperURL, selectedItem.layout,
                                            selectedItem.source);
            return;
          }

          // Falls back to request wallpaper from server.
          if (self.wallpaperRequest_)
            self.wallpaperRequest_.abort();

          self.wallpaperRequest_ = new XMLHttpRequest();
          self.progressManager_.reset(self.wallpaperRequest_, selectedGridItem);

          var onSuccess = function(xhr) {
            var image = xhr.response;
            chrome.wallpaperPrivate.setWallpaper(image, selectedItem.layout,
                wallpaperURL,
                function() {
                  self.progressManager_.hideProgressBar(selectedGridItem);

                  if (chrome.runtime.lastError != undefined &&
                      chrome.runtime.lastError.message !=
                          str('canceledWallpaper')) {
                    self.showError_(chrome.runtime.lastError.message);
                  } else {
                    self.onWallpaperChanged_(selectedItem, wallpaperURL);
                  }
                });
            WallpaperUtil.saveWallpaperInfo(wallpaperURL, selectedItem.layout,
                                            selectedItem.source);
            self.wallpaperRequest_ = null;
          };
          var onFailure = function() {
            self.progressManager_.hideProgressBar(selectedGridItem);
            self.showError_(str('downloadFailed'));
            self.wallpaperRequest_ = null;
          };
          WallpaperUtil.fetchURL(wallpaperURL, 'arraybuffer', onSuccess,
                                 onFailure, self.wallpaperRequest_);
        });
        break;
      default:
        console.error('Unsupported wallpaper source.');
    }
  };

  /*
   * Removes the oldest custom wallpaper. If the oldest one is set as current
   * wallpaper, removes the second oldest one to free some space. This should
   * only be called when exceeding wallpaper quota.
   */
  WallpaperManager.prototype.removeOldestWallpaper_ = function() {
    // Custom wallpapers should already sorted when put to the data model. The
    // last element is the add new button, need to exclude it as well.
    var oldestIndex = this.wallpaperGrid_.dataModel.length - 2;
    var item = this.wallpaperGrid_.dataModel.item(oldestIndex);
    if (!item || item.source != Constants.WallpaperSourceEnum.Custom)
      return;
    if (item.baseURL == this.currentWallpaper_)
      item = this.wallpaperGrid_.dataModel.item(--oldestIndex);
    if (item) {
      this.removeCustomWallpaper(item.baseURL);
      this.wallpaperGrid_.dataModel.splice(oldestIndex, 1);
    }
  };

  /*
   * Shows an error message to user and log the failed reason in console.
   */
  WallpaperManager.prototype.onFileSystemError_ = function(e) {
    var msg = '';
    switch (e.code) {
      case FileError.QUOTA_EXCEEDED_ERR:
        msg = 'QUOTA_EXCEEDED_ERR';
        // Instead of simply remove oldest wallpaper, we should consider a
        // better way to handle this situation. See crbug.com/180890.
        this.removeOldestWallpaper_();
        break;
      case FileError.NOT_FOUND_ERR:
        msg = 'NOT_FOUND_ERR';
        break;
      case FileError.SECURITY_ERR:
        msg = 'SECURITY_ERR';
        break;
      case FileError.INVALID_MODIFICATION_ERR:
        msg = 'INVALID_MODIFICATION_ERR';
        break;
      case FileError.INVALID_STATE_ERR:
        msg = 'INVALID_STATE_ERR';
        break;
      default:
        msg = 'Unknown Error';
        break;
    }
    console.error('Error: ' + msg);
    this.showError_(str('accessFileFailure'));
  };

  /**
   * Handles changing of selectedItem in wallpaper manager.
   */
  WallpaperManager.prototype.onSelectedItemChanged_ = function() {
    this.setWallpaperAttribution_(this.selectedItem_);

    if (!this.selectedItem_ || this.selectedItem_.source == 'ADDNEW')
      return;

    if (this.selectedItem_.baseURL && !this.wallpaperGrid_.inProgramSelection) {
      if (this.selectedItem_.source == Constants.WallpaperSourceEnum.Custom) {
        var items = {};
        var key = this.selectedItem_.baseURL;
        var self = this;
        Constants.WallpaperLocalStorage.get(key, function(items) {
          self.selectedItem_.layout =
              items[key] ? items[key] : 'CENTER_CROPPED';
          self.setSelectedWallpaper_(self.selectedItem_);
        });
      } else {
        this.setSelectedWallpaper_(this.selectedItem_);
      }
    }
  };

  /**
   * Set attributions of wallpaper with given URL. If URL is not valid, clear
   * the attributions.
   * @param {{baseURL: string, dynamicURL: string, layout: string,
   *          author: string, authorWebsite: string, availableOffline: boolean}}
   *     selectedItem selected wallpaper item in grid.
   * @private
   */
  WallpaperManager.prototype.setWallpaperAttribution_ = function(selectedItem) {
    // Only online wallpapers have author and website attributes. All other type
    // of wallpapers should not show attributions.
    if (selectedItem &&
        selectedItem.source == Constants.WallpaperSourceEnum.Online) {
      $('author-name').textContent = selectedItem.author;
      $('author-website').textContent = $('author-website').href =
          selectedItem.authorWebsite;
      chrome.wallpaperPrivate.getThumbnail(selectedItem.baseURL,
                                           selectedItem.source,
                                           function(data) {
        var img = $('attribute-image');
        if (data) {
          var blob = new Blob([new Int8Array(data)], {'type' : 'image\/png'});
          img.src = window.URL.createObjectURL(blob);
          img.addEventListener('load', function(e) {
            window.URL.revokeObjectURL(this.src);
          });
        } else {
          img.src = '';
        }
      });
      $('wallpaper-attribute').hidden = false;
      $('attribute-image').hidden = false;
      return;
    }
    $('wallpaper-attribute').hidden = true;
    $('attribute-image').hidden = true;
    $('author-name').textContent = '';
    $('author-website').textContent = $('author-website').href = '';
    $('attribute-image').src = '';
  };

  /**
   * Resize thumbnails grid and categories list to fit the new window size.
   */
  WallpaperManager.prototype.onResize_ = function() {
    this.wallpaperGrid_.redraw();
    this.categoriesList_.redraw();
  };

  /**
   * Close the last opened overlay on pressing the Escape key.
   * @param {Event} event A keydown event.
   */
  WallpaperManager.prototype.onKeyDown_ = function(event) {
    if (event.keyCode == 27) {
      // The last opened overlay coincides with the first match of querySelector
      // because the Error Container is declared in the DOM before the Wallpaper
      // Selection Container.
      // TODO(bshe): Make the overlay selection not dependent on the DOM.
      var closeButtonSelector = '.overlay-container:not([hidden]) .close';
      var closeButton = this.document_.querySelector(closeButtonSelector);
      if (closeButton) {
        closeButton.click();
        event.preventDefault();
      }
    }
  };

  /**
   * Constructs the categories list.
   */
  WallpaperManager.prototype.initCategoriesList_ = function() {
    this.categoriesList_ = $('categories-list');
    cr.ui.List.decorate(this.categoriesList_);
    // cr.ui.list calculates items in view port based on client height and item
    // height. However, categories list is displayed horizontally. So we should
    // not calculate visible items here. Sets autoExpands to true to show every
    // item in the list.
    // TODO(bshe): Use ul to replace cr.ui.list for category list.
    this.categoriesList_.autoExpands = true;

    var self = this;
    this.categoriesList_.itemConstructor = function(entry) {
      return self.renderCategory_(entry);
    };

    this.categoriesList_.selectionModel = new cr.ui.ListSingleSelectionModel();
    this.categoriesList_.selectionModel.addEventListener(
        'change', this.onCategoriesChange_.bind(this));

    var categoriesDataModel = new cr.ui.ArrayDataModel([]);
    if (this.enableOnlineWallpaper_) {
      // Adds all category as first category.
      categoriesDataModel.push(str('allCategoryLabel'));
      for (var key in this.manifest_.categories) {
        categoriesDataModel.push(this.manifest_.categories[key]);
      }
    }
    // Adds custom category as last category.
    categoriesDataModel.push(str('customCategoryLabel'));
    this.categoriesList_.dataModel = categoriesDataModel;
  };

  /**
   * Constructs the element in categories list.
   * @param {string} entry Text content of a category.
   */
  WallpaperManager.prototype.renderCategory_ = function(entry) {
    var li = this.document_.createElement('li');
    cr.defineProperty(li, 'custom', cr.PropertyKind.BOOL_ATTR);
    li.custom = (entry == str('customCategoryLabel'));
    cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);
    var div = this.document_.createElement('div');
    div.textContent = entry;
    li.appendChild(div);
    return li;
  };

  /**
   * Handles the custom wallpaper which user selected from file manager. Called
   * when users select a file.
   */
  WallpaperManager.prototype.onFileSelectorChanged_ = function() {
    var files = $('file-selector').files;
    if (files.length != 1)
      console.error('More than one files are selected or no file selected');
    if (!files[0].type.match('image/jpeg') &&
        !files[0].type.match('image/png')) {
      this.showError_(str('invalidWallpaper'));
      return;
    }
    var layout = getSelectedLayout();
    var self = this;
    var errorHandler = this.onFileSystemError_.bind(this);
    var setSelectedFile = function(file, layout, fileName) {
      var saveThumbnail = function(thumbnail) {
        var success = function(dirEntry) {
          dirEntry.getFile(fileName, {create: true}, function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.onwriteend = function(e) {
                $('set-wallpaper-layout').disabled = false;
                var wallpaperInfo = {
                  baseURL: fileName,
                  layout: layout,
                  source: Constants.WallpaperSourceEnum.Custom,
                  availableOffline: true
                };
                self.wallpaperGrid_.dataModel.splice(0, 0, wallpaperInfo);
                self.wallpaperGrid_.selectedItem = wallpaperInfo;
                self.onWallpaperChanged_(wallpaperInfo, fileName);
                WallpaperUtil.saveToStorage(self.currentWallpaper_, layout,
                                            false);
              };

              fileWriter.onerror = errorHandler;

              var blob = new Blob([new Int8Array(thumbnail)],
                                  {'type' : 'image\/jpeg'});
              fileWriter.write(blob);
            }, errorHandler);
          }, errorHandler);
        };
        self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.THUMBNAIL,
            success, errorHandler);
      };

      var success = function(dirEntry) {
        dirEntry.getFile(fileName, {create: true}, function(fileEntry) {
          fileEntry.createWriter(function(fileWriter) {
            fileWriter.addEventListener('writeend', function(e) {
              var reader = new FileReader();
              reader.readAsArrayBuffer(file);
              reader.addEventListener('error', errorHandler);
              reader.addEventListener('load', function(e) {
                self.setCustomWallpaper(e.target.result, layout, true, fileName,
                                        saveThumbnail, function() {
                  self.removeCustomWallpaper(fileName);
                  errorHandler();
                });
              });
            });

            fileWriter.addEventListener('error', errorHandler);
            fileWriter.write(file);
          }, errorHandler);
        }, errorHandler);
      };
      self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL, success,
                                       errorHandler);
    };
    setSelectedFile(files[0], layout, new Date().getTime().toString());
  };

  /**
   * Removes wallpaper and thumbnail with fileName from FileSystem.
   * @param {string} fileName The file name of wallpaper and thumbnail to be
   *     removed.
   */
  WallpaperManager.prototype.removeCustomWallpaper = function(fileName) {
    var errorHandler = this.onFileSystemError_.bind(this);
    var self = this;
    var removeFile = function(fileName) {
      var success = function(dirEntry) {
        dirEntry.getFile(fileName, {create: false}, function(fileEntry) {
          fileEntry.remove(function() {
          }, errorHandler);
        }, errorHandler);
      }

      // Removes copy of original.
      self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL, success,
                                       errorHandler);

      // Removes generated thumbnail.
      self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.THUMBNAIL, success,
                                       errorHandler);
    };
    removeFile(fileName);
  };

  /**
   * Sets current wallpaper and generate thumbnail if generateThumbnail is true.
   * @param {ArrayBuffer} wallpaper The binary representation of wallpaper.
   * @param {string} layout The user selected wallpaper layout.
   * @param {boolean} generateThumbnail True if need to generate thumbnail.
   * @param {string} fileName The unique file name of wallpaper.
   * @param {function(thumbnail):void} success Success callback. If
   *     generateThumbnail is true, the callback parameter should have the
   *     generated thumbnail.
   * @param {function(e):void} failure Failure callback. Called when there is an
   *     error from FileSystem.
   */
  WallpaperManager.prototype.setCustomWallpaper = function(wallpaper,
                                                           layout,
                                                           generateThumbnail,
                                                           fileName,
                                                           success,
                                                           failure) {
    var self = this;
    var onFinished = function(opt_thumbnail) {
      if (chrome.runtime.lastError != undefined &&
          chrome.runtime.lastError.message != str('canceledWallpaper')) {
        self.showError_(chrome.runtime.lastError.message);
        $('set-wallpaper-layout').disabled = true;
        failure();
      } else {
        success(opt_thumbnail);
        // Custom wallpapers are not synced yet. If login on a different
        // computer after set a custom wallpaper, wallpaper wont change by sync.
        WallpaperUtil.saveWallpaperInfo(fileName, layout,
                                        Constants.WallpaperSourceEnum.Custom);
      }
    };

    chrome.wallpaperPrivate.setCustomWallpaper(wallpaper, layout,
                                               generateThumbnail,
                                               fileName, onFinished);
  };

  /**
   * Handles the layout setting change of custom wallpaper.
   */
  WallpaperManager.prototype.onWallpaperLayoutChanged_ = function() {
    var layout = getSelectedLayout();
    var self = this;
    chrome.wallpaperPrivate.setCustomWallpaperLayout(layout, function() {
      if (chrome.runtime.lastError != undefined &&
          chrome.runtime.lastError.message != str('canceledWallpaper')) {
        self.showError_(chrome.runtime.lastError.message);
        self.removeCustomWallpaper(fileName);
        $('set-wallpaper-layout').disabled = true;
      } else {
        WallpaperUtil.saveToStorage(self.currentWallpaper_, layout, false);
        self.onWallpaperChanged_(self.wallpaperGrid_.activeItem,
                                 self.currentWallpaper_);
      }
    });
  };

  /**
   * Handles user clicking on a different category.
   */
  WallpaperManager.prototype.onCategoriesChange_ = function() {
    var categoriesList = this.categoriesList_;
    var selectedIndex = categoriesList.selectionModel.selectedIndex;
    if (selectedIndex == -1)
      return;
    var selectedListItem = categoriesList.getListItemByIndex(selectedIndex);
    var bar = $('bar');
    bar.style.left = selectedListItem.offsetLeft + 'px';
    bar.style.width = selectedListItem.offsetWidth + 'px';

    var wallpapersDataModel = new cr.ui.ArrayDataModel([]);
    var selectedItem;
    if (selectedListItem.custom) {
      this.document_.body.setAttribute('custom', '');
      var errorHandler = this.onFileSystemError_.bind(this);
      var toArray = function(list) {
        return Array.prototype.slice.call(list || [], 0);
      }

      var self = this;
      var processResults = function(entries) {
        for (var i = 0; i < entries.length; i++) {
          var entry = entries[i];
          var wallpaperInfo = {
                baseURL: entry.name,
                // The layout will be replaced by the actual value saved in
                // local storage when requested later. Layout is not important
                // for constructing thumbnails grid, we use CENTER_CROPPED here
                // to speed up the process of constructing. So we do not need to
                // wait for fetching correct layout.
                layout: 'CENTER_CROPPED',
                source: Constants.WallpaperSourceEnum.Custom,
                availableOffline: true
          };
          wallpapersDataModel.push(wallpaperInfo);
        }
        if (loadTimeData.getBoolean('isOEMDefaultWallpaper')) {
          var oemDefaultWallpaperElement = {
              baseURL: 'OemDefaultWallpaper',
              layout: 'CENTER_CROPPED',
              source: Constants.WallpaperSourceEnum.OEM,
              availableOffline: true
          };
          wallpapersDataModel.push(oemDefaultWallpaperElement);
        }
        for (var i = 0; i < wallpapersDataModel.length; i++) {
          if (self.currentWallpaper_ == wallpapersDataModel.item(i).baseURL)
            selectedItem = wallpapersDataModel.item(i);
        }
        var lastElement = {
            baseURL: '',
            layout: '',
            source: Constants.WallpaperSourceEnum.AddNew,
            availableOffline: true
        };
        wallpapersDataModel.push(lastElement);
        self.wallpaperGrid_.dataModel = wallpapersDataModel;
        self.wallpaperGrid_.selectedItem = selectedItem;
        self.wallpaperGrid_.activeItem = selectedItem;
      }

      var success = function(dirEntry) {
        var dirReader = dirEntry.createReader();
        var entries = [];
        // All of a directory's entries are not guaranteed to return in a single
        // call.
        var readEntries = function() {
          dirReader.readEntries(function(results) {
            if (!results.length) {
              processResults(entries.sort());
            } else {
              entries = entries.concat(toArray(results));
              readEntries();
            }
          }, errorHandler);
        };
        readEntries(); // Start reading dirs.
      }
      this.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL,
                                       success, errorHandler);
    } else {
      this.document_.body.removeAttribute('custom');
      for (var key in this.manifest_.wallpaper_list) {
        if (selectedIndex == AllCategoryIndex ||
            this.manifest_.wallpaper_list[key].categories.indexOf(
                selectedIndex - OnlineCategoriesOffset) != -1) {
          var wallpaperInfo = {
            baseURL: this.manifest_.wallpaper_list[key].base_url,
            layout: this.manifest_.wallpaper_list[key].default_layout,
            source: Constants.WallpaperSourceEnum.Online,
            availableOffline: false,
            author: this.manifest_.wallpaper_list[key].author,
            authorWebsite: this.manifest_.wallpaper_list[key].author_website,
            dynamicURL: this.manifest_.wallpaper_list[key].dynamic_url
          };
          var startIndex = wallpaperInfo.baseURL.lastIndexOf('/') + 1;
          var fileName = wallpaperInfo.baseURL.substring(startIndex) +
              Constants.HighResolutionSuffix;
          if (this.downloadedListMap_ &&
              this.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
            wallpaperInfo.availableOffline = true;
          }
          wallpapersDataModel.push(wallpaperInfo);
          var url = this.manifest_.wallpaper_list[key].base_url +
              Constants.HighResolutionSuffix;
          if (url == this.currentWallpaper_) {
            selectedItem = wallpaperInfo;
          }
        }
      }
      this.wallpaperGrid_.dataModel = wallpapersDataModel;
      this.wallpaperGrid_.selectedItem = selectedItem;
      this.wallpaperGrid_.activeItem = selectedItem;
    }
  };

})();
