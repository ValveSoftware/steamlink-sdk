// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.dialogs', function() {

  function BaseDialog(parentNode) {
    this.parentNode_ = parentNode;
    this.document_ = parentNode.ownerDocument;

    // The DOM element from the dialog which should receive focus when the
    // dialog is first displayed.
    this.initialFocusElement_ = null;

    // The DOM element from the parent which had focus before we were displayed,
    // so we can restore it when we're hidden.
    this.previousActiveElement_ = null;

    this.initDom_();
  }

  /**
   * Default text for Ok and Cancel buttons.
   *
   * Clients should override these with localized labels.
   */
  BaseDialog.OK_LABEL = '[LOCALIZE ME] Ok';
  BaseDialog.CANCEL_LABEL = '[LOCALIZE ME] Cancel';

  /**
   * Number of miliseconds animation is expected to take, plus some margin for
   * error.
   */
  BaseDialog.ANIMATE_STABLE_DURATION = 500;

  BaseDialog.prototype.initDom_ = function() {
    var doc = this.document_;
    this.container_ = doc.createElement('div');
    this.container_.className = 'cr-dialog-container';
    this.container_.addEventListener('keydown',
                                     this.onContainerKeyDown_.bind(this));
    this.shield_ = doc.createElement('div');
    this.shield_.className = 'cr-dialog-shield';
    this.container_.appendChild(this.shield_);
    this.container_.addEventListener('mousedown',
                                     this.onContainerMouseDown_.bind(this));

    this.frame_ = doc.createElement('div');
    this.frame_.className = 'cr-dialog-frame';
    // Elements that have negative tabIndex can be focused but are not traversed
    // by Tab key.
    this.frame_.tabIndex = -1;
    this.container_.appendChild(this.frame_);

    this.title_ = doc.createElement('div');
    this.title_.className = 'cr-dialog-title';
    this.frame_.appendChild(this.title_);

    this.closeButton_ = doc.createElement('div');
    this.closeButton_.className = 'cr-dialog-close';
    this.closeButton_.addEventListener('click',
                                        this.onCancelClick_.bind(this));
    this.frame_.appendChild(this.closeButton_);

    this.text_ = doc.createElement('div');
    this.text_.className = 'cr-dialog-text';
    this.frame_.appendChild(this.text_);

    this.buttons = doc.createElement('div');
    this.buttons.className = 'cr-dialog-buttons';
    this.frame_.appendChild(this.buttons);

    this.okButton_ = doc.createElement('button');
    this.okButton_.className = 'cr-dialog-ok';
    this.okButton_.textContent = BaseDialog.OK_LABEL;
    this.okButton_.addEventListener('click', this.onOkClick_.bind(this));
    this.buttons.appendChild(this.okButton_);

    this.cancelButton_ = doc.createElement('button');
    this.cancelButton_.className = 'cr-dialog-cancel';
    this.cancelButton_.textContent = BaseDialog.CANCEL_LABEL;
    this.cancelButton_.addEventListener('click',
                                        this.onCancelClick_.bind(this));
    this.buttons.appendChild(this.cancelButton_);

    this.initialFocusElement_ = this.okButton_;
  };

  BaseDialog.prototype.onOk_ = null;
  BaseDialog.prototype.onCancel_ = null;

  BaseDialog.prototype.onContainerKeyDown_ = function(event) {
    // Handle Escape.
    if (event.keyCode == 27 && !this.cancelButton_.disabled) {
      this.onCancelClick_(event);
      event.stopPropagation();
      // Prevent the event from being handled by the container of the dialog.
      // e.g. Prevent the parent container from closing at the same time.
      event.preventDefault();
    }
  };

  BaseDialog.prototype.onContainerMouseDown_ = function(event) {
    if (event.target == this.container_) {
      var classList = this.frame_.classList;
      // Start 'pulse' animation.
      classList.remove('pulse');
      setTimeout(classList.add.bind(classList, 'pulse'), 0);
      event.preventDefault();
    }
  };

  BaseDialog.prototype.onOkClick_ = function(event) {
    this.hide();
    if (this.onOk_)
      this.onOk_();
  };

  BaseDialog.prototype.onCancelClick_ = function(event) {
    this.hide();
    if (this.onCancel_)
      this.onCancel_();
  };

  BaseDialog.prototype.setOkLabel = function(label) {
    this.okButton_.textContent = label;
  };

  BaseDialog.prototype.setCancelLabel = function(label) {
    this.cancelButton_.textContent = label;
  };

  BaseDialog.prototype.setInitialFocusOnCancel = function() {
    this.initialFocusElement_ = this.cancelButton_;
  };

  BaseDialog.prototype.show = function(message, onOk, onCancel, onShow) {
    this.showWithTitle(null, message, onOk, onCancel, onShow);
  };

  BaseDialog.prototype.showHtml = function(title, message,
      onOk, onCancel, onShow) {
    this.text_.innerHTML = message;
    this.show_(title, onOk, onCancel, onShow);
  };

  BaseDialog.prototype.findFocusableElements_ = function(doc) {
    var elements = Array.prototype.filter.call(
        doc.querySelectorAll('*'),
        function(n) { return n.tabIndex >= 0; });

    var iframes = doc.querySelectorAll('iframe');
    for (var i = 0; i < iframes.length; i++) {
      // Some iframes have an undefined contentDocument for security reasons,
      // such as chrome://terms (which is used in the chromeos OOBE screens).
      var iframe = iframes[i];
      var contentDoc;
      try {
        contentDoc = iframe.contentDocument;
      } catch(e) {} // ignore SecurityError
      if (contentDoc)
        elements = elements.concat(this.findFocusableElements_(contentDoc));
    }
    return elements;
  };

  BaseDialog.prototype.showWithTitle = function(title, message,
      onOk, onCancel, onShow) {
    this.text_.textContent = message;
    this.show_(title, onOk, onCancel, onShow);
  };

  BaseDialog.prototype.show_ = function(title, onOk, onCancel, onShow) {
    // Make all outside nodes unfocusable while the dialog is active.
    this.deactivatedNodes_ = this.findFocusableElements_(this.document_);
    this.tabIndexes_ = this.deactivatedNodes_.map(
        function(n) { return n.getAttribute('tabindex'); });
    this.deactivatedNodes_.forEach(
        function(n) { n.tabIndex = -1; });

    this.previousActiveElement_ = this.document_.activeElement;
    this.parentNode_.appendChild(this.container_);

    this.onOk_ = onOk;
    this.onCancel_ = onCancel;

    if (title) {
      this.title_.textContent = title;
      this.title_.hidden = false;
    } else {
      this.title_.textContent = '';
      this.title_.hidden = true;
    }

    var self = this;
    setTimeout(function() {
      // Note that we control the opacity of the *container*, but the top/left
      // of the *frame*.
      self.container_.classList.add('shown');
      self.initialFocusElement_.focus();
      setTimeout(function() {
        if (onShow)
          onShow();
      }, BaseDialog.ANIMATE_STABLE_DURATION);
    }, 0);
  };

  BaseDialog.prototype.hide = function(onHide) {
    // Restore focusability.
    for (var i = 0; i < this.deactivatedNodes_.length; i++) {
      var node = this.deactivatedNodes_[i];
      if (this.tabIndexes_[i] === null)
        node.removeAttribute('tabindex');
      else
        node.setAttribute('tabindex', this.tabIndexes_[i]);
    }
    this.deactivatedNodes_ = null;
    this.tabIndexes_ = null;

    // Note that we control the opacity of the *container*, but the top/left
    // of the *frame*.
    this.container_.classList.remove('shown');

    if (this.previousActiveElement_) {
      this.previousActiveElement_.focus();
    } else {
      this.document_.body.focus();
    }
    this.frame_.classList.remove('pulse');

    var self = this;
    setTimeout(function() {
      // Wait until the transition is done before removing the dialog.
      self.parentNode_.removeChild(self.container_);
      if (onHide)
        onHide();
    }, BaseDialog.ANIMATE_STABLE_DURATION);
  };

  /**
   * AlertDialog contains just a message and an ok button.
   */
  function AlertDialog(parentNode) {
    BaseDialog.apply(this, [parentNode]);
    this.cancelButton_.style.display = 'none';
  }

  AlertDialog.prototype = {__proto__: BaseDialog.prototype};

  AlertDialog.prototype.show = function(message, onOk, onShow) {
    return BaseDialog.prototype.show.apply(this, [message, onOk, onOk, onShow]);
  };

  /**
   * ConfirmDialog contains a message, an ok button, and a cancel button.
   */
  function ConfirmDialog(parentNode) {
    BaseDialog.apply(this, [parentNode]);
  }

  ConfirmDialog.prototype = {__proto__: BaseDialog.prototype};

  /**
   * PromptDialog contains a message, a text input, an ok button, and a
   * cancel button.
   */
  function PromptDialog(parentNode) {
    BaseDialog.apply(this, [parentNode]);
    this.input_ = this.document_.createElement('input');
    this.input_.setAttribute('type', 'text');
    this.input_.addEventListener('focus', this.onInputFocus.bind(this));
    this.input_.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.initialFocusElement_ = this.input_;
    this.frame_.insertBefore(this.input_, this.text_.nextSibling);
  }

  PromptDialog.prototype = {__proto__: BaseDialog.prototype};

  PromptDialog.prototype.onInputFocus = function(event) {
    this.input_.select();
  };

  PromptDialog.prototype.onKeyDown_ = function(event) {
    if (event.keyCode == 13) {  // Enter
      this.onOkClick_(event);
      event.preventDefault();
    }
  };

  PromptDialog.prototype.show = function(message, defaultValue, onOk, onCancel,
                                        onShow) {
    this.input_.value = defaultValue || '';
    return BaseDialog.prototype.show.apply(this, [message, onOk, onCancel,
                                                  onShow]);
  };

  PromptDialog.prototype.getValue = function() {
    return this.input_.value;
  };

  PromptDialog.prototype.onOkClick_ = function(event) {
    this.hide();
    if (this.onOk_)
      this.onOk_(this.getValue());
  };

  return {
    BaseDialog: BaseDialog,
    AlertDialog: AlertDialog,
    ConfirmDialog: ConfirmDialog,
    PromptDialog: PromptDialog
  };
});
