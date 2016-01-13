// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('wallpapers', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var Grid = cr.ui.Grid;
  /** @const */ var GridItem = cr.ui.GridItem;
  /** @const */ var GridSelectionController = cr.ui.GridSelectionController;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;
  /** @const */ var ThumbnailSuffix = '_thumbnail.png';
  /** @const */ var ShowSpinnerDelayMs = 500;

  /**
   * Creates a new wallpaper thumbnails grid item.
   * @param {{baseURL: string, layout: string, source: string,
   *          availableOffline: boolean, opt_dynamicURL: string,
   *          opt_author: string, opt_authorWebsite: string}}
   *     wallpaperInfo Wallpaper data item in WallpaperThumbnailsGrid's data
   *     model.
   * @param {number} dataModelId A unique ID that this item associated to.
   * @param {function} callback The callback function when decoration finished.
   * @constructor
   * @extends {cr.ui.GridItem}
   */
  function WallpaperThumbnailsGridItem(wallpaperInfo, dataModelId, callback) {
    var el = new GridItem(wallpaperInfo);
    el.__proto__ = WallpaperThumbnailsGridItem.prototype;
    el.dataModelId = dataModelId;
    el.callback = callback;
    return el;
  }

  WallpaperThumbnailsGridItem.prototype = {
    __proto__: GridItem.prototype,

    /**
     * The unique ID this thumbnail grid associated to.
     * @type {number}
     */
    dataModelId: null,

    /**
     * Called when the WallpaperThumbnailsGridItem is decorated or failed to
     * decorate. If the decoration contains image, the callback function should
     * be called after image loaded.
     * @type {function}
     */
    callback: null,

    /** @override */
    decorate: function() {
      GridItem.prototype.decorate.call(this);
      // Removes garbage created by GridItem.
      this.innerText = '';
      var imageEl = cr.doc.createElement('img');
      imageEl.classList.add('thumbnail');
      cr.defineProperty(imageEl, 'offline', cr.PropertyKind.BOOL_ATTR);
      imageEl.offline = this.dataItem.availableOffline;
      this.appendChild(imageEl);
      var self = this;

      switch (this.dataItem.source) {
        case Constants.WallpaperSourceEnum.AddNew:
          this.id = 'add-new';
          this.addEventListener('click', function(e) {
            var checkbox = $('surprise-me').querySelector('#checkbox');
            if (!checkbox.classList.contains('checked'))
              $('wallpaper-selection-container').hidden = false;
          });
          // Delay dispatching the completion callback until all items have
          // begun loading and are tracked.
          window.setTimeout(this.callback.bind(this, this.dataModelId), 0);
          break;
        case Constants.WallpaperSourceEnum.Custom:
          var errorHandler = function(e) {
            self.callback(self.dataModelId);
            console.error('Can not access file system.');
          };
          var wallpaperDirectories = WallpaperDirectories.getInstance();
          var getThumbnail = function(fileName) {
            var setURL = function(fileEntry) {
              imageEl.src = fileEntry.toURL();
              self.callback(self.dataModelId);
            };
            var fallback = function() {
              wallpaperDirectories.getDirectory(WallpaperDirNameEnum.ORIGINAL,
                                          function(dirEntry) {
                dirEntry.getFile(fileName, {create: false}, setURL,
                                 errorHandler);
              }, errorHandler);
            };
            var success = function(dirEntry) {
              dirEntry.getFile(fileName, {create: false}, setURL, fallback);
            };
            wallpaperDirectories.getDirectory(WallpaperDirNameEnum.THUMBNAIL,
                                              success,
                                              errorHandler);
          }
          getThumbnail(self.dataItem.baseURL);
          break;
        case Constants.WallpaperSourceEnum.OEM:
        case Constants.WallpaperSourceEnum.Online:
          chrome.wallpaperPrivate.getThumbnail(this.dataItem.baseURL,
                                               this.dataItem.source,
                                               function(data) {
            if (data) {
              var blob = new Blob([new Int8Array(data)],
                                  {'type': 'image\/png'});
              imageEl.src = window.URL.createObjectURL(blob);
              imageEl.addEventListener('load', function(e) {
                self.callback(self.dataModelId);
                window.URL.revokeObjectURL(this.src);
              });
            } else if (self.dataItem.source ==
                       Constants.WallpaperSourceEnum.Online) {
              var xhr = new XMLHttpRequest();
              xhr.open('GET', self.dataItem.baseURL + ThumbnailSuffix, true);
              xhr.responseType = 'arraybuffer';
              xhr.send(null);
              xhr.addEventListener('load', function(e) {
                if (xhr.status === 200) {
                  chrome.wallpaperPrivate.saveThumbnail(self.dataItem.baseURL,
                                                        xhr.response);
                  var blob = new Blob([new Int8Array(xhr.response)],
                                      {'type' : 'image\/png'});
                  imageEl.src = window.URL.createObjectURL(blob);
                  // TODO(bshe): We currently use empty div to reserve space for
                  // thumbnail. Use a placeholder like "loading" image may
                  // better.
                  imageEl.addEventListener('load', function(e) {
                    self.callback(self.dataModelId);
                    window.URL.revokeObjectURL(this.src);
                  });
                } else {
                  self.callback(self.dataModelId);
                }
              });
            }
          });
          break;
        default:
          console.error('Unsupported image source.');
          // Delay dispatching the completion callback until all items have
          // begun loading and are tracked.
          window.setTimeout(this.callback.bind(this, this.dataModelId), 0);
      }
    },
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
  function WallpaperThumbnailsGridSelectionController(selectionModel, grid) {
    GridSelectionController.call(this, selectionModel, grid);
  }

  WallpaperThumbnailsGridSelectionController.prototype = {
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
    },
  };

  /**
   * Creates a new user images grid element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Grid}
   */
  var WallpaperThumbnailsGrid = cr.ui.define('grid');

  WallpaperThumbnailsGrid.prototype = {
    __proto__: Grid.prototype,

    /**
     * The checkbox element.
     */
    checkmark_: undefined,

    /**
     * ID of spinner delay timer.
     * @private
     */
    spinnerTimeout_: 0,

    /**
     * The item in data model which should have a checkmark.
     * @type {{baseURL: string, dynamicURL: string, layout: string,
     *         author: string, authorWebsite: string,
     *         availableOffline: boolean}}
     *     wallpaperInfo The information of the wallpaper to be set active.
     */
    activeItem_: undefined,
    set activeItem(activeItem) {
      if (this.activeItem_ != activeItem) {
        this.activeItem_ = activeItem;
        this.updateActiveThumb_();
      }
    },

    /**
     * A unique ID that assigned to each set dataModel operation. Note that this
     * id wont increase if the new dataModel is null or empty.
     */
    dataModelId_: 0,

    /**
     * The number of items that need to be generated after a new dataModel is
     * set.
     */
    pendingItems_: 0,

    /** @override */
    set dataModel(dataModel) {
      if (this.dataModel_ == dataModel)
        return;

      if (dataModel && dataModel.length != 0) {
        this.dataModelId_++;
        // Clears old pending items. The new pending items will be counted when
        // item is constructed in function itemConstructor below.
        this.pendingItems_ = 0;

        this.style.visibility = 'hidden';
        // If spinner is hidden, schedule to show the spinner after
        // ShowSpinnerDelayMs delay. Otherwise, keep it spinning.
        if ($('spinner-container').hidden) {
          this.spinnerTimeout_ = window.setTimeout(function() {
            $('spinner-container').hidden = false;
          }, ShowSpinnerDelayMs);
        }
      } else {
        // Sets dataModel to null should hide spinner immedidately.
        $('spinner-container').hidden = true;
      }

      var parentSetter = cr.ui.Grid.prototype.__lookupSetter__('dataModel');
      parentSetter.call(this, dataModel);
    },

    get dataModel() {
      return this.dataModel_;
    },

    /** @override */
    createSelectionController: function(sm) {
      return new WallpaperThumbnailsGridSelectionController(sm, this);
    },

    /**
     * Check if new thumbnail grid finished loading. This reduces the count of
     * remaining items to be loaded and when 0, shows the thumbnail grid. Note
     * it does not reduce the count on a previous |dataModelId|.
     * @param {number} dataModelId A unique ID that a thumbnail item is
     *     associated to.
     */
    pendingItemComplete: function(dataModelId) {
      if (dataModelId != this.dataModelId_)
        return;
      this.pendingItems_--;
      if (this.pendingItems_ == 0) {
        this.style.visibility = 'visible';
        window.clearTimeout(this.spinnerTimeout_);
        this.spinnerTimeout_ = 0;
        $('spinner-container').hidden = true;
      }
    },

    /** @override */
    decorate: function() {
      Grid.prototype.decorate.call(this);
      // checkmark_ needs to be initialized before set data model. Otherwise, we
      // may try to access checkmark before initialization in
      // updateActiveThumb_().
      this.checkmark_ = cr.doc.createElement('div');
      this.checkmark_.classList.add('check');
      this.dataModel = new ArrayDataModel([]);
      var self = this;
      this.itemConstructor = function(value) {
        var dataModelId = self.dataModelId_;
        self.pendingItems_++;
        return WallpaperThumbnailsGridItem(value, dataModelId,
            self.pendingItemComplete.bind(self));
      };
      this.selectionModel = new ListSingleSelectionModel();
      this.inProgramSelection_ = false;
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
     * Set index to the image selected.
     * @type {number} index The index of selected image.
     */
    set selectedItemIndex(index) {
      this.inProgramSelection_ = true;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * The selected item.
     * @type {!Object} Wallpaper information inserted into the data model.
     */
    get selectedItem() {
      var index = this.selectionModel.selectedIndex;
      return index != -1 ? this.dataModel.item(index) : null;
    },
    set selectedItem(selectedItem) {
      var index = this.dataModel.indexOf(selectedItem);
      this.inProgramSelection_ = true;
      this.selectionModel.leadIndex = index;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
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
    },

    /**
     * Shows a checkmark on the active thumbnail and clears previous active one
     * if any. Note if wallpaper was not set successfully, checkmark should not
     * show on that thumbnail.
     */
    updateActiveThumb_: function() {
      var selectedGridItem = this.getListItem(this.activeItem_);
      if (this.checkmark_.parentNode &&
          this.checkmark_.parentNode == selectedGridItem) {
        return;
      }

      // Clears previous checkmark.
      if (this.checkmark_.parentNode)
        this.checkmark_.parentNode.removeChild(this.checkmark_);

      if (!selectedGridItem)
        return;
      selectedGridItem.appendChild(this.checkmark_);
    },

    /**
     * Redraws the viewport.
     */
    redraw: function() {
      Grid.prototype.redraw.call(this);
      // The active thumbnail maybe deleted in the above redraw(). Sets it again
      // to make sure checkmark shows correctly.
      this.updateActiveThumb_();
    }
  };

  return {
    WallpaperThumbnailsGrid: WallpaperThumbnailsGrid
  };
});
