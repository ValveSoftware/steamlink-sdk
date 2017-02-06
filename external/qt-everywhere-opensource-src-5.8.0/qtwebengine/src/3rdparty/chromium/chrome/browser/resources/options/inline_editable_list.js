// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;

  /**
   * Creates a new list item with support for inline editing.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function InlineEditableItem() {
    var el = cr.doc.createElement('div');
    InlineEditableItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a inline-editable list item. Note that this is
   * a subclass of DeletableItem.
   * @param {!HTMLElement} el The element to decorate.
   */
  InlineEditableItem.decorate = function(el) {
    el.__proto__ = InlineEditableItem.prototype;
    el.decorate();
  };

  InlineEditableItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Index of currently focused column, or -1 for none.
     * @type {number}
     */
    focusedColumnIndex: -1,

    /**
     * Whether or not this item can be edited.
     * @type {boolean}
     * @private
     */
    editable_: true,

    /**
     * Whether or not this is a placeholder for adding a new item.
     * @type {boolean}
     * @private
     */
    isPlaceholder_: false,

    /**
     * Fields associated with edit mode.
     * @type {Array}
     * @private
     */
    editFields_: null,

    /**
     * Whether or not the current edit should be considered cancelled, rather
     * than committed, when editing ends.
     * @type {boolean}
     * @private
     */
    editCancelled_: true,

    /**
     * The editable item corresponding to the last click, if any. Used to decide
     * initial focus when entering edit mode.
     * @type {HTMLElement}
     * @private
     */
    editClickTarget_: null,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      this.editFields_ = [];
      this.addEventListener('mousedown', this.handleMouseDown_);
      this.addEventListener('keydown', this.handleKeyDown_);
      this.addEventListener('focusin', this.handleFocusIn_);
    },

    /** @override */
    selectionChanged: function() {
      if (!this.parentNode.ignoreChangeEvents_)
        this.updateEditState();
    },

    /**
     * Called when this element gains or loses 'lead' status. Updates editing
     * mode accordingly.
     */
    updateLeadState: function() {
      // Add focusability before call to updateEditState.
      if (this.lead) {
        this.setEditableValuesFocusable(true);
        this.setCloseButtonFocusable(true);
      }

      this.updateEditState();

      // Remove focusability after call to updateEditState.
      this.setStaticValuesFocusable(false);
      if (!this.lead) {
        this.setEditableValuesFocusable(false);
        this.setCloseButtonFocusable(false);
      }
    },

    /**
     * Updates the edit state based on the current selected and lead states.
     */
    updateEditState: function() {
      if (this.editable)
        this.editing = this.selected && this.lead;
    },

    /**
     * Whether the user is currently editing the list item.
     * @type {boolean}
     */
    get editing() {
      return this.hasAttribute('editing');
    },
    set editing(editing) {
      if (this.editing == editing)
        return;

      if (editing)
        this.setAttribute('editing', '');
      else
        this.removeAttribute('editing');

      if (editing) {
        this.editCancelled_ = false;

        cr.dispatchSimpleEvent(this, 'edit', true);

        var isMouseClick = this.editClickTarget_;
        var focusElement = this.getEditFocusElement_();
        if (focusElement) {
          if (isMouseClick) {
            // Delay focus to fix http://crbug.com/436789
            setTimeout(function() {
              this.focusAndMaybeSelect_(focusElement);
            }.bind(this), 0);
          } else {
            this.focusAndMaybeSelect_(focusElement);
          }
        }
      } else {
        if (!this.editCancelled_ && this.hasBeenEdited &&
            this.currentInputIsValid) {
          this.parentNode.needsToFocusPlaceholder_ = this.isPlaceholder &&
              this.parentNode.shouldFocusPlaceholderOnEditCommit();
          this.updateStaticValues_();
          cr.dispatchSimpleEvent(this, 'commitedit', true);
        } else {
          this.parentNode.needsToFocusPlaceholder_ = false;
          this.resetEditableValues_();
          cr.dispatchSimpleEvent(this, 'canceledit', true);
        }
      }
    },

    /**
     * Return editable element that should be focused, or null for none.
     * @private
     */
    getEditFocusElement_: function() {
      // If an edit field was clicked on then use the clicked element.
      if (this.editClickTarget_) {
        var result = this.editClickTarget_;
        this.editClickTarget_ = null;
        return result;
      }

      // If focusedColumnIndex is valid then use the element in that column.
      if (this.focusedColumnIndex != -1) {
        var nearestColumn =
            this.getNearestColumnByIndex_(this.focusedColumnIndex);
        if (nearestColumn)
          return nearestColumn;
      }

      // It's possible that focusedColumnIndex hasn't been updated yet.
      // Check getFocusedColumnIndex_ directly.
      // This can't completely replace the above focusedColumnIndex check
      // because InlineEditableItemList may have set focusedColumnIndex to a
      // different value.
      var columnIndex = this.getFocusedColumnIndex_();
      if (columnIndex != -1) {
        var nearestColumn = this.getNearestColumnByIndex_(columnIndex);
        if (nearestColumn)
          return nearestColumn;
      }

      // Everything else failed so return the default.
      return this.initialFocusElement;
    },

    /**
     * Focus on the specified element, and select the editable text in it
     * if possible.
     * @param {!Element} control An element to be focused.
     * @private
     */
    focusAndMaybeSelect_: function(control) {
      control.focus();
      if (control.tagName == 'INPUT')
        control.select();
    },

    /**
     * Whether the item is editable.
     * @type {boolean}
     */
    get editable() {
      return this.editable_;
    },
    set editable(editable) {
      this.editable_ = editable;
      if (!editable)
        this.editing = false;
    },

    /**
     * Whether the item is a new item placeholder.
     * @type {boolean}
     */
    get isPlaceholder() {
      return this.isPlaceholder_;
    },
    set isPlaceholder(isPlaceholder) {
      this.isPlaceholder_ = isPlaceholder;
      if (isPlaceholder)
        this.deletable = false;
    },

    /**
     * The HTML element that should have focus initially when editing starts,
     * if a specific element wasn't clicked.
     * Defaults to the first <input> element; can be overridden by subclasses if
     * a different element should be focused.
     * @type {HTMLElement}
     */
    get initialFocusElement() {
      return this.contentElement.querySelector('input');
    },

    /**
     * Whether the input in currently valid to submit. If this returns false
     * when editing would be submitted, either editing will not be ended,
     * or it will be cancelled, depending on the context.
     * Can be overridden by subclasses to perform input validation.
     * @type {boolean}
     */
    get currentInputIsValid() {
      return true;
    },

    /**
     * Returns true if the item has been changed by an edit.
     * Can be overridden by subclasses to return false when nothing has changed
     * to avoid unnecessary commits.
     * @type {boolean}
     */
    get hasBeenEdited() {
      return true;
    },

    /**
     * Sets whether the editable values can be given focus using the keyboard.
     * @param {boolean} focusable The desired focusable state.
     */
    setEditableValuesFocusable: function(focusable) {
      focusable = focusable && this.editable;
      var editFields = this.editFields_;
      for (var i = 0; i < editFields.length; i++) {
        editFields[i].tabIndex = focusable ? 0 : -1;
      }
    },

    /**
     * Sets whether the static values can be given focus using the keyboard.
     * @param {boolean} focusable The desired focusable state.
     */
    setStaticValuesFocusable: function(focusable) {
      var editFields = this.editFields_;
      for (var i = 0; i < editFields.length; i++) {
        var staticVersion = editFields[i].staticVersion;
        if (!staticVersion)
          continue;
        if (this.editable) {
          staticVersion.tabIndex = focusable ? 0 : -1;
        } else {
          // staticVersion remains visible when !this.editable. Remove
          // tabindex so that it will not become focused by clicking on it and
          // have selection box drawn around it.
          staticVersion.removeAttribute('tabindex');
        }
      }
    },

    /**
     * Sets whether the close button can be focused using the keyboard.
     * @param {boolean} focusable The desired focusable state.
     */
    setCloseButtonFocusable: function(focusable) {
      this.closeButtonElement.tabIndex =
          focusable && this.closeButtonFocusAllowed ? 0 : -1;
    },

    /**
     * Returns a div containing an <input>, as well as static text if
     * isPlaceholder is not true.
     * @param {string} text The text of the cell.
     * @return {HTMLElement} The HTML element for the cell.
     * @private
     */
    createEditableTextCell: function(text) {
      var container = /** @type {HTMLElement} */(
          this.ownerDocument.createElement('div'));
      var textEl = null;
      if (!this.isPlaceholder) {
        textEl = this.ownerDocument.createElement('div');
        textEl.className = 'static-text overruleable';
        textEl.textContent = text;
        textEl.setAttribute('displaymode', 'static');
        container.appendChild(textEl);
      }

      var inputEl = this.ownerDocument.createElement('input');
      inputEl.type = 'text';
      inputEl.value = text;
      if (!this.isPlaceholder)
        inputEl.setAttribute('displaymode', 'edit');

      // In some cases 'focus' event may arrive before 'input'.
      // To make sure revalidation is triggered we postpone 'focus' handling.
      var handler = this.handleFocus.bind(this);
      inputEl.addEventListener('focus', function() {
        window.setTimeout(function() {
          if (inputEl.ownerDocument.activeElement == inputEl)
            handler();
        }, 0);
      });
      container.appendChild(inputEl);
      this.addEditField(inputEl, textEl);

      return container;
    },

    /**
     * Register an edit field.
     * @param {!Element} control An editable element. It's a form control
     *     element typically.
     * @param {Element} staticElement An element representing non-editable
     *     state.
     */
    addEditField: function(control, staticElement) {
      control.staticVersion = staticElement;
      if (this.editable)
        control.tabIndex = -1;

      if (control.staticVersion) {
        if (this.editable)
          control.staticVersion.tabIndex = -1;
        control.staticVersion.editableVersion = control;
        control.staticVersion.addEventListener('focus',
                                               this.handleFocus.bind(this));
      }
      this.editFields_.push(control);
    },

    /**
     * Set the column index for a child element of this InlineEditableItem.
     * Only elements with a column index will be keyboard focusable, e.g. by
     * pressing the tab key.
     * @param {Element} element Element whose column index to set. Method does
     *     nothing if element is null.
     * @param {number} columnIndex The new column index to set on the element.
     *     -1 removes the column index.
     */
    setFocusableColumnIndex: function(element, columnIndex) {
      if (!element)
        return;

      if (columnIndex >= 0)
        element.setAttribute('inlineeditable-column', columnIndex);
      else
        element.removeAttribute('inlineeditable-column');
    },

    /**
     * Resets the editable version of any controls created by createEditable*
     * to match the static text.
     * @private
     */
    resetEditableValues_: function() {
      var editFields = this.editFields_;
      for (var i = 0; i < editFields.length; i++) {
        var staticLabel = editFields[i].staticVersion;
        if (!staticLabel && !this.isPlaceholder)
          continue;

        if (editFields[i].tagName == 'INPUT') {
          editFields[i].value =
            this.isPlaceholder ? '' : staticLabel.textContent;
        }
        // Add more tag types here as new createEditable* methods are added.

        editFields[i].setCustomValidity('');
      }
    },

    /**
     * Sets the static version of any controls created by createEditable*
     * to match the current value of the editable version. Called on commit so
     * that there's no flicker of the old value before the model updates.
     * @private
     */
    updateStaticValues_: function() {
      var editFields = this.editFields_;
      for (var i = 0; i < editFields.length; i++) {
        var staticLabel = editFields[i].staticVersion;
        if (!staticLabel)
          continue;

        if (editFields[i].tagName == 'INPUT')
          staticLabel.textContent = editFields[i].value;
        // Add more tag types here as new createEditable* methods are added.
      }
    },

    /**
     * Returns the index of the column that currently has focus, or -1 if no
     * column has focus.
     * @return {number}
     * @private
     */
    getFocusedColumnIndex_: function() {
      var element = document.activeElement.editableVersion ||
                    document.activeElement;

      if (element.hasAttribute('inlineeditable-column'))
        return parseInt(element.getAttribute('inlineeditable-column'), 10);
      return -1;
    },

    /**
     * Returns the element from the column that has the largest index where:
     * where:
     *   + index <= startIndex, and
     *   + the element exists, and
     *   + the element is not disabled
     * @return {Element}
     * @private
     */
    getNearestColumnByIndex_: function(startIndex) {
      for (var i = startIndex; i >= 0; --i) {
        var el = this.querySelector('[inlineeditable-column="' + i + '"]');
        if (el && !el.disabled)
          return el;
      }
      return null;
    },

    /**
     * Called when a key is pressed. Handles committing and canceling edits.
     * @param {Event} e The key down event.
     * @private
     */
    handleKeyDown_: function(e) {
      if (!this.editing)
        return;

      var endEdit = false;
      var handledKey = true;
      switch (e.key) {
        case 'Escape':
          this.editCancelled_ = true;
          endEdit = true;
          break;
        case 'Enter':
          if (this.currentInputIsValid)
            endEdit = true;
          break;
        default:
          handledKey = false;
      }
      if (handledKey) {
        // Make sure that handled keys aren't passed on and double-handled.
        // (e.g., esc shouldn't both cancel an edit and close a subpage)
        e.stopPropagation();
      }
      if (endEdit) {
        // Blurring will trigger the edit to end; see InlineEditableItemList.
        this.ownerDocument.activeElement.blur();
      }
    },

    /**
     * Called when the list item is clicked. If the click target corresponds to
     * an editable item, stores that item to focus when edit mode is started.
     * @param {Event} e The mouse down event.
     * @private
     */
    handleMouseDown_: function(e) {
      if (!this.editable)
        return;

      var clickTarget = e.target;
      var editFields = this.editFields_;
      var editClickTarget;
      for (var i = 0; i < editFields.length; i++) {
        if (editFields[i] == clickTarget ||
            editFields[i].staticVersion == clickTarget) {
          editClickTarget = editFields[i];
          break;
        }
      }

      if (this.editing) {
        if (!editClickTarget) {
          // Clicked on the list item outside of an edit field. Don't lose focus
          // from currently selected edit field.
          e.stopPropagation();
          e.preventDefault();
        }
        return;
      }

      if (editClickTarget && !editClickTarget.disabled)
        this.editClickTarget_ = editClickTarget;
    },

    /**
     * Called when this InlineEditableItem or any of its children are given
     * focus. Updates focusedColumnIndex with the index of the newly focused
     * column, or -1 if the focused element does not have a column index.
     * @param {Event} e The focusin event.
     * @private
     */
    handleFocusIn_: function(e) {
      var target = e.target.editableVersion || e.target;
      this.focusedColumnIndex = target.hasAttribute('inlineeditable-column') ?
          parseInt(target.getAttribute('inlineeditable-column'), 10) : -1;
    },
  };

  /**
   * Takes care of committing changes to inline editable list items when the
   * window loses focus.
   */
  function handleWindowBlurs() {
    window.addEventListener('blur', function(e) {
      var itemAncestor = findAncestor(document.activeElement, function(node) {
        return node instanceof InlineEditableItem;
      });
      if (itemAncestor)
        document.activeElement.blur();
    });
  }
  handleWindowBlurs();

  /**
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var InlineEditableItemList = cr.ui.define('list');

  InlineEditableItemList.prototype = {
    __proto__: DeletableItemList.prototype,

    /**
     * Whether to ignore list change events.
     * Used to modify the list without processing selection change and lead
     * change events.
     * @type {boolean}
     * @private
     */
    ignoreChangeEvents_: false,

    /**
     * Focuses the input element of the placeholder if true.
     * @type {boolean}
     * @private
     */
    needsToFocusPlaceholder_: false,

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.setAttribute('inlineeditable', '');
      this.addEventListener('hasElementFocusChange',
                            this.handleListFocusChange_);
      // <list> isn't focusable by default, but cr.ui.List defaults tabindex to
      // 0 if it's not set.
      this.tabIndex = -1;
    },

    /**
     * Called when the list hierarchy as a whole loses or gains focus; starts
     * or ends editing for the lead item if necessary.
     * @param {Event} e The change event.
     * @private
     */
    handleListFocusChange_: function(e) {
      var leadItem = this.getListItemByIndex(this.selectionModel.leadIndex);
      if (leadItem) {
        if (e.newValue) {
          // Add focusability before making other changes.
          leadItem.setEditableValuesFocusable(true);
          leadItem.setCloseButtonFocusable(true);
          leadItem.focusedColumnIndex = -1;
          leadItem.updateEditState();
          // Remove focusability after making other changes.
          leadItem.setStaticValuesFocusable(false);
        } else {
          // Add focusability before making other changes.
          leadItem.setStaticValuesFocusable(true);
          leadItem.setCloseButtonFocusable(true);
          leadItem.editing = false;
          // Remove focusability after making other changes.
          if (!leadItem.isPlaceholder)
            leadItem.setEditableValuesFocusable(false);
        }
      }
    },

    /** @override */
    handleLeadChange: function(e) {
      if (this.ignoreChangeEvents_)
        return;

      DeletableItemList.prototype.handleLeadChange.call(this, e);

      var focusedColumnIndex = -1;
      if (e.oldValue != -1) {
        var element = this.getListItemByIndex(e.oldValue);
        if (element) {
          focusedColumnIndex = element.focusedColumnIndex;
          element.updateLeadState();
        }
      }

      if (e.newValue != -1) {
        var element = this.getListItemByIndex(e.newValue);
        if (element) {
          element.focusedColumnIndex = focusedColumnIndex;
          element.updateLeadState();
        }
      }
    },

    /** @override */
    onSetDataModelComplete: function() {
      DeletableItemList.prototype.onSetDataModelComplete.call(this);

      if (this.needsToFocusPlaceholder_) {
        this.focusPlaceholder();
      } else {
        var item = this.getInitialFocusableItem();
        if (item) {
          item.setStaticValuesFocusable(true);
          item.setCloseButtonFocusable(true);
          if (item.isPlaceholder)
            item.setEditableValuesFocusable(true);
        }
      }
    },

    /**
     * Execute |callback| with list change events disabled. Selection change and
     * lead change events will not be processed.
     * @param {!Function} callback The function to execute.
     * @protected
     */
    ignoreChangeEvents: function(callback) {
      assert(!this.ignoreChangeEvents_);
      this.ignoreChangeEvents_ = true;
      callback();
      this.ignoreChangeEvents_ = false;
    },

    /**
     * Set the selected index without changing the focused element on the page.
     * Used to change the selected index when the list doesn't have focus (and
     * doesn't want to take focus).
     * @param {number} index The index to select.
     */
    selectIndexWithoutFocusing: function(index) {
      // Remove focusability from old item.
      var oldItem = this.getListItemByIndex(this.selectionModel.leadIndex) ||
                    this.getInitialFocusableItem();
      if (oldItem) {
        oldItem.setEditableValuesFocusable(false);
        oldItem.setStaticValuesFocusable(false);
        oldItem.setCloseButtonFocusable(false);
        oldItem.lead = false;
      }

      // Select the new item.
      this.ignoreChangeEvents(function() {
        this.selectionModel.selectedIndex = index;
      }.bind(this));

      // Add focusability to new item.
      var newItem = this.getListItemByIndex(index);
      if (newItem) {
        if (newItem.isPlaceholder)
          newItem.setEditableValuesFocusable(true);
        else
          newItem.setStaticValuesFocusable(true);

        newItem.setCloseButtonFocusable(true);
        newItem.lead = true;
      }
    },

    /**
     * Focus the placeholder's first input field.
     * Should only be called immediately after the list has been repopulated.
     */
    focusPlaceholder: function() {
      // Remove focusability from initial item.
      var item = this.getInitialFocusableItem();
      if (item) {
        item.setStaticValuesFocusable(false);
        item.setCloseButtonFocusable(false);
      }
      // Find placeholder and focus it.
      for (var i = 0; i < this.dataModel.length; i++) {
        var item = this.getListItemByIndex(i);
        if (item.isPlaceholder) {
          item.setEditableValuesFocusable(true);
          item.setCloseButtonFocusable(true);
          item.querySelector('input').focus();
          return;
        }
      }
    },

    /**
     * May be overridden by subclasses to disable focusing the placeholder.
     * @return {boolean} True if the placeholder element should be focused on
     *     edit commit.
     * @protected
     */
    shouldFocusPlaceholderOnEditCommit: function() {
      return true;
    },

    /**
    * Override to change which item is initially focusable.
    * @return {options.InlineEditableItem} Initially focusable item or null.
    * @protected
    */
    getInitialFocusableItem: function() {
      return /** @type {options.InlineEditableItem} */(
          this.getListItemByIndex(0));
    },
  };

  // Export
  return {
    InlineEditableItem: InlineEditableItem,
    InlineEditableItemList: InlineEditableItemList,
  };
});
