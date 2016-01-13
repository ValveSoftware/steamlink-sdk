// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column model
 */
cr.define('cr.ui.table', function() {
  /** @const */ var EventTarget = cr.EventTarget;

  /**
   * A table column model that wraps table columns array
   * This implementation supports widths in percents.
   * @param {!Array<cr.ui.table.TableColumn>} columnIds Array of table columns.
   * @constructor
   * @extends {EventTarget}
   */
  function TableColumnModel(tableColumns) {
    this.columns_ = [];
    for (var i = 0; i < tableColumns.length; i++) {
      this.columns_.push(tableColumns[i].clone());
    }
  }

  var MIMIMAL_WIDTH = 10;

  TableColumnModel.prototype = {
    __proto__: EventTarget.prototype,

    /**
     * The number of the columns.
     * @type {number}
     */
    get size() {
      return this.columns_.length;
    },

    /**
     * Returns id of column at the given index.
     * @param {number} index The index of the column.
     * @return {string} Column id.
     */
    getId: function(index) {
      return this.columns_[index].id;
    },

    /**
     * Returns name of column at the given index. Name is used as column header
     * label.
     * @param {number} index The index of the column.
     * @return {string} Column name.
     */
    getName: function(index) {
      return this.columns_[index].name;
    },

    /**
     * Sets name of column at the given index.
     * @param {number} index The index of the column.
     * @param {string} Column name.
     */
    setName: function(index, name) {
      if (index < 0 || index >= this.columns_.size - 1)
        return;
      if (name != this.columns_[index].name)
        return;

      this.columns_[index].name = name;
      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Returns width (in percent) of column at the given index.
     * @param {number} index The index of the column.
     * @return {string} Column width in pixels.
     */
    getWidth: function(index) {
      return this.columns_[index].width;
    },

    /**
     * Check if the column at the given index should align to the end.
     * @param {number} index The index of the column.
     * @return {boolean} True if the column is aligned to end.
     */
    isEndAlign: function(index) {
      return this.columns_[index].endAlign;
    },

    /**
     * Sets width of column at the given index.
     * @param {number} index The index of the column.
     * @param {number} Column width.
     */
    setWidth: function(index, width) {
      if (index < 0 || index >= this.columns_.size - 1)
        return;

      width = Math.max(width, MIMIMAL_WIDTH);
      if (width == this.columns_[index].width)
        return;

      this.columns_[index].width = width;
      cr.dispatchSimpleEvent(this, 'resize');
    },

    /**
     * Returns render function for the column at the given index.
     * @param {number} index The index of the column.
     * @return {Function(*, string, cr.ui.Table): HTMLElement} Render function.
     */
    getRenderFunction: function(index) {
      return this.columns_[index].renderFunction;
    },

    /**
     * Sets render function for the column at the given index.
     * @param {number} index The index of the column.
     * @param {Function(*, string, cr.ui.Table): HTMLElement} Render function.
     */
    setRenderFunction: function(index, renderFunction) {
      if (index < 0 || index >= this.columns_.size - 1)
        return;
      if (renderFunction !== this.columns_[index].renderFunction)
        return;

      this.columns_[index].renderFunction = renderFunction;
      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Render the column header.
     * @param {number} index The index of the column.
     * @param {cr.ui.Table} Owner table.
     */
    renderHeader: function(index, table) {
      var c = this.columns_[index];
      return c.headerRenderFunction.call(c, table);
    },

    /**
     * The total width of the columns.
     * @type {number}
     */
    get totalWidth() {
      var total = 0;
      for (var i = 0; i < this.size; i++) {
        total += this.columns_[i].width;
      }
      return total;
    },

    /**
     * Normalizes widths to make their sum 100%.
     */
    normalizeWidths: function(contentWidth) {
      if (this.size == 0)
        return;
      var c = this.columns_[0];
      c.width = Math.max(10, c.width - this.totalWidth + contentWidth);
    },

    /**
     * Returns default sorting order for the column at the given index.
     * @param {number} index The index of the column.
     * @return {string} 'asc' or 'desc'.
     */
    getDefaultOrder: function(index) {
      return this.columns_[index].defaultOrder;
    },

    /**
     * Returns index of the column with given id.
     * @param {string} id The id to find.
     * @return {number} The index of column with given id or -1 if not found.
     */
    indexOf: function(id) {
      for (var i = 0; i < this.size; i++) {
        if (this.getId(i) == id)
          return i;
      }
      return -1;
    },
  };

  return {
    TableColumnModel: TableColumnModel
  };
});
