// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.proxyexceptions', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new exception list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var ProxyExceptions = cr.ui.define('list');

  ProxyExceptions.prototype = {
    __proto__: List.prototype,

    pref: 'cros.session.proxy.ignorelist',

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.autoExpands = true;

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', this.redraw.bind(this));

      this.addEventListener('click', this.handleClick_);

      var self = this;

      // Listens to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.load_(event.value.value);
          });
    },

    createItem: function(exception) {
      return new ProxyExceptionsItem(exception);
    },

    /**
     * Adds given exception to model and update backend.
     * @param {Object} exception A exception to be added to exception list.
     */
    addException: function(exception) {
      this.dataModel.push(exception);
      this.updateBackend_();
    },

    /**
     * Removes given exception from model and update backend.
     */
    removeException: function(exception) {
      var dataModel = this.dataModel;

      var index = dataModel.indexOf(exception);
      if (index >= 0) {
        dataModel.splice(index, 1);
        this.updateBackend_();
      }
    },

    /**
     * Handles the clicks on the list and triggers exception removal if the
     * click is on the remove exception button.
     * @private
     * @param {!Event} e The click event object.
     */
    handleClick_: function(e) {
      // Handle left button click
      if (e.button == 0) {
        var el = e.target;
        if (el.className == 'remove-exception-button') {
          this.removeException(el.parentNode.exception);
        }
      }
    },

    /**
     * Loads given exception list.
     * @param {Array} exceptions An array of exception object.
     */
    load_: function(exceptions) {
      this.dataModel = new ArrayDataModel(exceptions);
    },

    /**
     * Updates backend.
     */
    updateBackend_: function() {
      Preferences.setListPref(this.pref, this.dataModel.slice(), true);
    }
  };

  /**
   * Creates a new exception list item.
   * @param {Object} exception The exception account this represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function ProxyExceptionsItem(exception) {
    var el = cr.doc.createElement('div');
    el.exception = exception;
    ProxyExceptionsItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a exception account item.
   * @param {!HTMLElement} el The element to decorate.
   */
  ProxyExceptionsItem.decorate = function(el) {
    el.__proto__ = ProxyExceptionsItem.prototype;
    el.decorate();
  };

  ProxyExceptionsItem.prototype = {
    __proto__: ListItem.prototype,

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      this.className = 'exception-list-item';

      var labelException = this.ownerDocument.createElement('span');
      labelException.className = '';
      labelException.textContent = this.exception;
      this.appendChild(labelException);
    }
  };

  return {
    ProxyExceptions: ProxyExceptions
  };
});
