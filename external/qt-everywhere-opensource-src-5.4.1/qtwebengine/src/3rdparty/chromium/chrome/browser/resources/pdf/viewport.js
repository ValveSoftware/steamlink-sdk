// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns the area of the intersection of two rectangles.
 * @param {Object} rect1 the first rect
 * @param {Object} rect2 the second rect
 * @return {number} the area of the intersection of the rects
 */
function getIntersectionArea(rect1, rect2) {
  var xOverlap = Math.max(0,
      Math.min(rect1.x + rect1.width, rect2.x + rect2.width) -
      Math.max(rect1.x, rect2.x));
  var yOverlap = Math.max(0,
      Math.min(rect1.y + rect1.height, rect2.y + rect2.height) -
      Math.max(rect1.y, rect2.y));
  return xOverlap * yOverlap;
}

/**
 * Create a new viewport.
 * @param {Window} window the window
 * @param {Object} sizer is the element which represents the size of the
 *     document in the viewport
 * @param {Function} viewportChangedCallback is run when the viewport changes
 * @param {number} scrollbarWidth the width of scrollbars on the page
 */
function Viewport(window,
                  sizer,
                  viewportChangedCallback,
                  scrollbarWidth) {
  this.window_ = window;
  this.sizer_ = sizer;
  this.viewportChangedCallback_ = viewportChangedCallback;
  this.zoom_ = 1;
  this.documentDimensions_ = null;
  this.pageDimensions_ = [];
  this.scrollbarWidth_ = scrollbarWidth;
  this.fittingType_ = Viewport.FittingType.NONE;

  window.addEventListener('scroll', this.updateViewport_.bind(this));
  window.addEventListener('resize', this.resize_.bind(this));
}

/**
 * Enumeration of page fitting types.
 * @enum {string}
 */
Viewport.FittingType = {
  NONE: 'none',
  FIT_TO_PAGE: 'fit-to-page',
  FIT_TO_WIDTH: 'fit-to-width'
};

/**
 * The increment to scroll a page by in pixels when up/down/left/right arrow
 * keys are pressed. Usually we just let the browser handle scrolling on the
 * window when these keys are pressed but in certain cases we need to simulate
 * these events.
 */
Viewport.SCROLL_INCREMENT = 40;

/**
 * Predefined zoom factors to be used when zooming in/out. These are in
 * ascending order.
 */
Viewport.ZOOM_FACTORS = [0.25, 0.333, 0.5, 0.666, 0.75, 0.9, 1,
                         1.1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 5];

/**
 * The width of the page shadow around pages in pixels.
 */
Viewport.PAGE_SHADOW = {top: 3, bottom: 7, left: 5, right: 5};

Viewport.prototype = {
  /**
   * @private
   * Returns true if the document needs scrollbars at the given zoom level.
   * @param {number} zoom compute whether scrollbars are needed at this zoom
   * @return {Object} with 'horizontal' and 'vertical' keys which map to bool
   *     values indicating if the horizontal and vertical scrollbars are needed
   *     respectively.
   */
  documentNeedsScrollbars_: function(zoom) {
    var documentWidth = this.documentDimensions_.width * zoom;
    var documentHeight = this.documentDimensions_.height * zoom;
    return {
      horizontal: documentWidth > this.window_.innerWidth,
      vertical: documentHeight > this.window_.innerHeight
    };
  },

  /**
   * Returns true if the document needs scrollbars at the current zoom level.
   * @return {Object} with 'x' and 'y' keys which map to bool values
   *     indicating if the horizontal and vertical scrollbars are needed
   *     respectively.
   */
  documentHasScrollbars: function() {
    return this.documentNeedsScrollbars_(this.zoom_);
  },

  /**
   * @private
   * Helper function called when the zoomed document size changes.
   */
  contentSizeChanged_: function() {
    if (this.documentDimensions_) {
      this.sizer_.style.width =
          this.documentDimensions_.width * this.zoom_ + 'px';
      this.sizer_.style.height =
          this.documentDimensions_.height * this.zoom_ + 'px';
    }
  },

  /**
   * @private
   * Called when the viewport should be updated.
   */
  updateViewport_: function() {
    this.viewportChangedCallback_();
  },

  /**
   * @private
   * Called when the viewport size changes.
   */
  resize_: function() {
    if (this.fittingType_ == Viewport.FittingType.FIT_TO_PAGE)
      this.fitToPage();
    else if (this.fittingType_ == Viewport.FittingType.FIT_TO_WIDTH)
      this.fitToWidth();
    else
      this.updateViewport_();
  },

  /**
   * @type {Object} the scroll position of the viewport.
   */
  get position() {
    return {
      x: this.window_.pageXOffset,
      y: this.window_.pageYOffset
    };
  },

  /**
   * Scroll the viewport to the specified position.
   * @type {Object} position the position to scroll to.
   */
  set position(position) {
    this.window_.scrollTo(position.x, position.y);
  },

  /**
   * @type {Object} the size of the viewport excluding scrollbars.
   */
  get size() {
    var needsScrollbars = this.documentNeedsScrollbars_(this.zoom_);
    var scrollbarWidth = needsScrollbars.vertical ? this.scrollbarWidth_ : 0;
    var scrollbarHeight = needsScrollbars.horizontal ? this.scrollbarWidth_ : 0;
    return {
      width: this.window_.innerWidth - scrollbarWidth,
      height: this.window_.innerHeight - scrollbarHeight
    };
  },

  /**
   * @type {number} the zoom level of the viewport.
   */
  get zoom() {
    return this.zoom_;
  },

  /**
   * @private
   * Sets the zoom of the viewport.
   * @param {number} newZoom the zoom level to zoom to.
   */
  setZoom_: function(newZoom) {
    var oldZoom = this.zoom_;
    this.zoom_ = newZoom;
    // Record the scroll position (relative to the middle of the window).
    var currentScrollPos = [
      (this.window_.pageXOffset + this.window_.innerWidth / 2) / oldZoom,
      (this.window_.pageYOffset + this.window_.innerHeight / 2) / oldZoom
    ];
    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.window_.scrollTo(
        currentScrollPos[0] * newZoom - this.window_.innerWidth / 2,
        currentScrollPos[1] * newZoom - this.window_.innerHeight / 2);
  },

  /**
   * @type {number} the width of scrollbars in the viewport in pixels.
   */
  get scrollbarWidth() {
    return this.scrollbarWidth_;
  },

  /**
   * @type {Viewport.FittingType} the fitting type the viewport is currently in.
   */
  get fittingType() {
    return this.fittingType_;
  },

  /**
   * @private
   * @param {integer} y the y-coordinate to get the page at.
   * @return {integer} the index of a page overlapping the given y-coordinate.
   */
  getPageAtY_: function(y) {
    var min = 0;
    var max = this.pageDimensions_.length - 1;
    while (max >= min) {
      var page = Math.floor(min + ((max - min) / 2));
      // There might be a gap between the pages, in which case use the bottom
      // of the previous page as the top for finding the page.
      var top = 0;
      if (page > 0) {
        top = this.pageDimensions_[page - 1].y +
            this.pageDimensions_[page - 1].height;
      }
      var bottom = this.pageDimensions_[page].y +
          this.pageDimensions_[page].height;

      if (top <= y && bottom > y)
        return page;
      else if (top > y)
        max = page - 1;
      else
        min = page + 1;
    }
    return 0;
  },

  /**
   * Returns the page with the most pixels in the current viewport.
   * @return {int} the index of the most visible page.
   */
  getMostVisiblePage: function() {
    var firstVisiblePage = this.getPageAtY_(this.position.y / this.zoom_);
    var mostVisiblePage = {number: 0, area: 0};
    var viewportRect = {
      x: this.position.x / this.zoom_,
      y: this.position.y / this.zoom_,
      width: this.size.width / this.zoom_,
      height: this.size.height / this.zoom_
    };
    for (var i = firstVisiblePage; i < this.pageDimensions_.length; i++) {
      var area = getIntersectionArea(this.pageDimensions_[i],
                                     viewportRect);
      // If we hit a page with 0 area overlap, we must have gone past the
      // pages visible in the viewport so we can break.
      if (area == 0)
        break;
      if (area > mostVisiblePage.area) {
        mostVisiblePage.area = area;
        mostVisiblePage.number = i;
      }
    }
    return mostVisiblePage.number;
  },

  /**
   * @private
   * Compute the zoom level for fit-to-page or fit-to-width. |pageDimensions| is
   * the dimensions for a given page and if |widthOnly| is true, it indicates
   * that fit-to-page zoom should be computed rather than fit-to-page.
   * @param {Object} pageDimensions the dimensions of a given page
   * @param {boolean} widthOnly a bool indicating whether fit-to-page or
   *     fit-to-width should be computed.
   * @return {number} the zoom to use
   */
  computeFittingZoom_: function(pageDimensions, widthOnly) {
    // First compute the zoom without scrollbars.
    var zoomWidth = this.window_.innerWidth / pageDimensions.width;
    var zoom;
    if (widthOnly) {
      zoom = zoomWidth;
    } else {
      var zoomHeight = this.window_.innerHeight / pageDimensions.height;
      zoom = Math.min(zoomWidth, zoomHeight);
    }
    // Check if there needs to be any scrollbars.
    var needsScrollbars = this.documentNeedsScrollbars_(zoom);

    // If the document fits, just return the zoom.
    if (!needsScrollbars.horizontal && !needsScrollbars.vertical)
      return zoom;

    var zoomedDimensions = {
      width: this.documentDimensions_.width * zoom,
      height: this.documentDimensions_.height * zoom
    };

    // Check if adding a scrollbar will result in needing the other scrollbar.
    var scrollbarWidth = this.scrollbarWidth_;
    if (needsScrollbars.horizontal &&
        zoomedDimensions.height > this.window_.innerHeight - scrollbarWidth) {
      needsScrollbars.vertical = true;
    }
    if (needsScrollbars.vertical &&
        zoomedDimensions.width > this.window_.innerWidth - scrollbarWidth) {
      needsScrollbars.horizontal = true;
    }

    // Compute available window space.
    var windowWithScrollbars = {
      width: this.window_.innerWidth,
      height: this.window_.innerHeight
    };
    if (needsScrollbars.horizontal)
      windowWithScrollbars.height -= scrollbarWidth;
    if (needsScrollbars.vertical)
      windowWithScrollbars.width -= scrollbarWidth;

    // Recompute the zoom.
    zoomWidth = windowWithScrollbars.width / pageDimensions.width;
    if (widthOnly) {
      zoom = zoomWidth;
    } else {
      var zoomHeight = windowWithScrollbars.height / pageDimensions.height;
      zoom = Math.min(zoomWidth, zoomHeight);
    }
    return zoom;
  },

  /**
   * Zoom the viewport so that the page-width consumes the entire viewport.
   */
  fitToWidth: function() {
    this.fittingType_ = Viewport.FittingType.FIT_TO_WIDTH;
    if (!this.documentDimensions_)
      return;
    // Track the last y-position so we stay at the same position after zooming.
    var oldY = this.window_.pageYOffset / this.zoom_;
    // When computing fit-to-width, the maximum width of a page in the document
    // is used, which is equal to the size of the document width.
    this.setZoom_(this.computeFittingZoom_(this.documentDimensions_, true));
    var page = this.getMostVisiblePage();
    this.window_.scrollTo(0, oldY * this.zoom_);
    this.updateViewport_();
  },

  /**
   * Zoom the viewport so that a page consumes the entire viewport. Also scrolls
   * to the top of the most visible page.
   */
  fitToPage: function() {
    this.fittingType_ = Viewport.FittingType.FIT_TO_PAGE;
    if (!this.documentDimensions_)
      return;
    var page = this.getMostVisiblePage();
    this.setZoom_(this.computeFittingZoom_(this.pageDimensions_[page], false));
    // Center the document in the page by scrolling by the amount of empty
    // space to the left of the document.
    var xOffset =
        (this.documentDimensions_.width - this.pageDimensions_[page].width) *
        this.zoom_ / 2;
    this.window_.scrollTo(xOffset,
                          this.pageDimensions_[page].y * this.zoom_);
    this.updateViewport_();
  },

  /**
   * Zoom out to the next predefined zoom level.
   */
  zoomOut: function() {
    this.fittingType_ = Viewport.FittingType.NONE;
    var nextZoom = Viewport.ZOOM_FACTORS[0];
    for (var i = 0; i < Viewport.ZOOM_FACTORS.length; i++) {
      if (Viewport.ZOOM_FACTORS[i] < this.zoom_)
        nextZoom = Viewport.ZOOM_FACTORS[i];
    }
    this.setZoom_(nextZoom);
    this.updateViewport_();
  },

  /**
   * Zoom in to the next predefined zoom level.
   */
  zoomIn: function() {
    this.fittingType_ = Viewport.FittingType.NONE;
    var nextZoom = Viewport.ZOOM_FACTORS[Viewport.ZOOM_FACTORS.length - 1];
    for (var i = Viewport.ZOOM_FACTORS.length - 1; i >= 0; i--) {
      if (Viewport.ZOOM_FACTORS[i] > this.zoom_)
        nextZoom = Viewport.ZOOM_FACTORS[i];
    }
    this.setZoom_(nextZoom);
    this.updateViewport_();
  },

  /**
   * Go to the given page index.
   * @param {number} page the index of the page to go to.
   */
  goToPage: function(page) {
    if (this.pageDimensions_.length == 0)
      return;
    if (page < 0)
      page = 0;
    if (page >= this.pageDimensions_.length)
      page = this.pageDimensions_.length - 1;
    var dimensions = this.pageDimensions_[page];
    this.window_.scrollTo(dimensions.x * this.zoom_, dimensions.y * this.zoom_);
  },

  /**
   * Set the dimensions of the document.
   * @param {Object} documentDimensions the dimensions of the document
   */
  setDocumentDimensions: function(documentDimensions) {
    var initialDimensions = !this.documentDimensions_;
    this.documentDimensions_ = documentDimensions;
    this.pageDimensions_ = this.documentDimensions_.pageDimensions;
    if (initialDimensions) {
      this.setZoom_(this.computeFittingZoom_(this.documentDimensions_, true));
      if (this.zoom_ > 1)
        this.setZoom_(1);
      this.window_.scrollTo(0, 0);
    }
    this.contentSizeChanged_();
    this.resize_();
  },

  /**
   * Get the coordinates of the page contents (excluding the page shadow)
   * relative to the screen.
   * @param {number} page the index of the page to get the rect for.
   * @return {Object} a rect representing the page in screen coordinates.
   */
  getPageScreenRect: function(page) {
    if (page >= this.pageDimensions_.length)
      page = this.pageDimensions_.length - 1;

    var pageDimensions = this.pageDimensions_[page];

    // Compute the page dimensions minus the shadows.
    var insetDimensions = {
      x: pageDimensions.x + Viewport.PAGE_SHADOW.left,
      y: pageDimensions.y + Viewport.PAGE_SHADOW.top,
      width: pageDimensions.width - Viewport.PAGE_SHADOW.left -
          Viewport.PAGE_SHADOW.right,
      height: pageDimensions.height - Viewport.PAGE_SHADOW.top -
          Viewport.PAGE_SHADOW.bottom
    };

    // Compute the x-coordinate of the page within the document.
    // TODO(raymes): This should really be set when the PDF plugin passes the
    // page coordinates, but it isn't yet.
    var x = (this.documentDimensions_.width - pageDimensions.width) / 2 +
        Viewport.PAGE_SHADOW.left;
    // Compute the space on the left of the document if the document fits
    // completely in the screen.
    var spaceOnLeft = (this.size.width -
        this.documentDimensions_.width * this.zoom_) / 2;
    spaceOnLeft = Math.max(spaceOnLeft, 0);

    return {
      x: x * this.zoom_ + spaceOnLeft - this.window_.pageXOffset,
      y: insetDimensions.y * this.zoom_ - this.window_.pageYOffset,
      width: insetDimensions.width * this.zoom_,
      height: insetDimensions.height * this.zoom_
    };
  }
};
