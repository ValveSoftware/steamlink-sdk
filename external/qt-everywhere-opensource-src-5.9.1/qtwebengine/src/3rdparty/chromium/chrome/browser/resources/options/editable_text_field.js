// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * @constructor
   * @extends {HTMLDivElement}
   */
  var EditableTextField = cr.ui.define('div');

  EditableTextField.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The actual input element in this field.
     * @type {?HTMLElement}
     * @private
     */
    editField_: null,

    /**
     * The static text displayed when this field isn't editable.
     * @type {?HTMLElement}
     * @private
     */
    staticText_: null,

    /**
     * The data model for this field.
     * @type {?Object}
     * @private
     */
    model_: null,

    /**
     * Whether or not the current edit should be considered canceled, rather
     * than committed, when editing ends.
     * @type {boolean}
     * @private
     */
    editCanceled_: true,

    /** @protected */
    decorate: function() {
      this.classList.add('editable-text-field');

      this.createEditableTextCell('');

      if (this.hasAttribute('i18n-placeholder-text')) {
        var identifier = this.getAttribute('i18n-placeholder-text');
        var localizedText = loadTimeData.getString(identifier);
        if (localizedText)
          this.setAttribute('placeholder-text', localizedText);
      }

      this.addEventListener('keydown', this.handleKeyDown_);
      this.editField_.addEventListener('focus', this.handleFocus_.bind(this));
      this.editField_.addEventListener('blur', this.handleBlur_.bind(this));
      this.checkForEmpty_();
    },

    /**
     * Indicates that this field has no value in the model, and the placeholder
     * text (if any) should be shown.
     * @type {boolean}
     */
    get empty() {
      return this.hasAttribute('empty');
    },

    /**
     * The placeholder text to be used when the model or its value is empty.
     * @type {string}
     */
    get placeholderText() {
      return this.getAttribute('placeholder-text');
    },
    set placeholderText(text) {
      if (text)
        this.setAttribute('placeholder-text', text);
      else
        this.removeAttribute('placeholder-text');

      this.checkForEmpty_();
    },

    /**
     * Returns the input element in this text field.
     * @type {HTMLElement} The element that is the actual input field.
     */
    get editField() {
      return this.editField_;
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
        this.editCanceled_ = false;

        if (this.empty) {
          this.removeAttribute('empty');
          if (this.editField)
            this.editField.value = '';
        }
        if (this.editField) {
          this.editField.focus();
          this.editField.select();
        }
      } else {
        if (!this.editCanceled_ && this.hasBeenEdited &&
            this.currentInputIsValid) {
          this.updateStaticValues_();
          cr.dispatchSimpleEvent(this, 'commitedit', true);
        } else {
          this.resetEditableValues_();
          cr.dispatchSimpleEvent(this, 'canceledit', true);
        }
        this.checkForEmpty_();
      }
    },

    /**
     * Whether the item is editable.
     * @type {boolean}
     */
    get editable() {
      return this.hasAttribute('editable');
    },
    set editable(editable) {
      if (this.editable == editable)
        return;

      if (editable)
        this.setAttribute('editable', '');
      else
        this.removeAttribute('editable');
      this.editable_ = editable;
    },

    /**
     * The data model for this field.
     * @type {Object}
     */
    get model() {
      return this.model_;
    },
    set model(model) {
      this.model_ = model;
      this.checkForEmpty_();  // This also updates the editField value.
      this.updateStaticValues_();
    },

    /**
     * The HTML element that should have focus initially when editing starts,
     * if a specific element wasn't clicked. Defaults to the first <input>
     * element; can be overridden by subclasses if a different element should be
     * focused.
     * @type {?HTMLElement}
     */
    get initialFocusElement() {
      return this.querySelector('input');
    },

    /**
     * Whether the input in currently valid to submit. If this returns false
     * when editing would be submitted, either editing will not be ended,
     * or it will be cancelled, depending on the context. Can be overridden by
     * subclasses to perform input validation.
     * @type {boolean}
     */
    get currentInputIsValid() {
      return true;
    },

    /**
     * Returns true if the item has been changed by an edit. Can be overridden
     * by subclasses to return false when nothing has changed to avoid
     * unnecessary commits.
     * @type {boolean}
     */
    get hasBeenEdited() {
      return true;
    },

    /**
     * Mutates the input during a successful commit.  Can be overridden to
     * provide a way to "clean up" valid input so that it conforms to a
     * desired format.  Will only be called when commit succeeds for valid
     * input, or when the model is set.
     * @param {string} value Input text to be mutated.
     * @return {string} mutated text.
     */
    mutateInput: function(value) {
      return value;
    },

    /**
     * Creates a div containing an <input>, as well as static text, keeping
     * references to them so they can be manipulated.
     * @param {string} text The text of the cell.
     * @private
     */
    createEditableTextCell: function(text) {
      // This function should only be called once.
      if (this.editField_)
        return;

      var container = this.ownerDocument.createElement('div');

      var textEl = /** @type {HTMLElement} */(
          this.ownerDocument.createElement('div'));
      textEl.className = 'static-text';
      textEl.textContent = text;
      textEl.setAttribute('displaymode', 'static');
      this.appendChild(textEl);
      this.staticText_ = textEl;

      var inputEl = /** @type {HTMLElement} */(
          this.ownerDocument.createElement('input'));
      inputEl.className = 'editable-text';
      inputEl.type = 'text';
      inputEl.value = text;
      inputEl.setAttribute('displaymode', 'edit');
      inputEl.staticVersion = textEl;
      this.appendChild(inputEl);
      this.editField_ = inputEl;
    },

    /**
     * Resets the editable version of any controls created by
     * createEditableTextCell to match the static text.
     * @private
     */
    resetEditableValues_: function() {
      var editField = this.editField_;
      var staticLabel = editField.staticVersion;
      if (!staticLabel)
        return;

      if (editField instanceof HTMLInputElement)
        editField.value = staticLabel.textContent;

      editField.setCustomValidity('');
    },

    /**
     * Sets the static version of any controls created by createEditableTextCell
     * to match the current value of the editable version. Called on commit so
     * that there's no flicker of the old value before the model updates.  Also
     * updates the model's value with the mutated value of the edit field.
     * @private
     */
    updateStaticValues_: function() {
      var editField = this.editField_;
      var staticLabel = editField.staticVersion;
      if (!staticLabel)
        return;

      if (editField instanceof HTMLInputElement) {
        staticLabel.textContent = editField.value;
        this.model_.value = this.mutateInput(editField.value);
      }
    },

    /**
     * Checks to see if the model or its value are empty.  If they are, then set
     * the edit field to the placeholder text, if any, and if not, set it to the
     * model's value.
     * @private
     */
    checkForEmpty_: function() {
      var editField = this.editField_;
      if (!editField)
        return;

      if (!this.model_ || !this.model_.value) {
        this.setAttribute('empty', '');
        editField.value = this.placeholderText || '';
      } else {
        this.removeAttribute('empty');
        editField.value = this.model_.value;
      }
    },

    /**
     * Called when this widget receives focus.
     * @param {Event} e the focus event.
     * @private
     */
    handleFocus_: function(e) {
      if (this.editing)
        return;

      this.editing = true;
      if (this.editField_)
        this.editField_.focus();
    },

    /**
     * Called when this widget loses focus.
     * @param {Event} e the blur event.
     * @private
     */
    handleBlur_: function(e) {
      if (!this.editing)
        return;

      this.editing = false;
    },

    /**
     * Called when a key is pressed. Handles committing and canceling edits.
     * @param {Event} e The key down event.
     * @private
     */
    handleKeyDown_: function(e) {
      if (!this.editing)
        return;

      var endEdit;
      switch (e.key) {
        case 'Escape':
          this.editCanceled_ = true;
          endEdit = true;
          break;
        case 'Enter':
          if (this.currentInputIsValid)
            endEdit = true;
          break;
      }

      if (endEdit) {
        // Blurring will trigger the edit to end.
        this.ownerDocument.activeElement.blur();
        // Make sure that handled keys aren't passed on and double-handled.
        // (e.g., esc shouldn't both cancel an edit and close a subpage)
        e.stopPropagation();
      }
    },
  };

  /**
   * Takes care of committing changes to EditableTextField items when the
   * window loses focus.
   */
  window.addEventListener('blur', function(e) {
    var itemAncestor = findAncestor(document.activeElement, function(node) {
      return node instanceof EditableTextField;
    });
    if (itemAncestor)
      document.activeElement.blur();
  });

  return {
    EditableTextField: EditableTextField,
  };
});
