// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders a destination item in a destination list.
   * @param {!cr.EventTarget} eventTarget Event target to dispatch selection
   *     events to.
   * @param {!print_preview.Destination} destination Destination data object to
   *     render.
   * @param {RegExp} query Active filter query.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationListItem(eventTarget, destination, query) {
    print_preview.Component.call(this);

    /**
     * Event target to dispatch selection events to.
     * @type {!cr.EventTarget}
     * @private
     */
    this.eventTarget_ = eventTarget;

    /**
     * Destination that the list item renders.
     * @type {!print_preview.Destination}
     * @private
     */
    this.destination_ = destination;

    /**
     * Active filter query text.
     * @type {RegExp}
     * @private
     */
    this.query_ = query;

    /**
     * FedEx terms-of-service widget or {@code null} if this list item does not
     * render the FedEx Office print destination.
     * @type {print_preview.FedexTos}
     * @private
     */
    this.fedexTos_ = null;
  };

  /**
   * Event types dispatched by the destination list item.
   * @enum {string}
   */
  DestinationListItem.EventType = {
    // Dispatched when the list item is activated.
    SELECT: 'print_preview.DestinationListItem.SELECT',
    REGISTER_PROMO_CLICKED:
        'print_preview.DestinationListItem.REGISTER_PROMO_CLICKED'
  };

  DestinationListItem.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-item-template'));
      this.updateUi_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(this.getElement(), 'click', this.onActivate_.bind(this));
      this.tracker.add(
          this.getElement(), 'keydown', this.onKeyDown_.bind(this));
      this.tracker.add(
          this.getChildElement('.register-promo-button'),
          'click',
          this.onRegisterPromoClicked_.bind(this));
    },

    /** @return {!print_preiew.Destination} */
    get destination() {
      return this.destination_;
    },

    /**
     * Updates the list item UI state.
     * @param {!print_preview.Destination} destination Destination data object
     *     to render.
     * @param {RegExp} query Active filter query.
     */
    update: function(destination, query) {
      this.destination_ = destination;
      this.query_ = query;
      this.updateUi_();
    },

    /**
     * Initializes the element with destination's info.
     * @private
     */
    updateUi_: function() {
      var iconImg = this.getChildElement('.destination-list-item-icon');
      iconImg.src = this.destination_.iconUrl;

      var nameEl = this.getChildElement('.destination-list-item-name');
      var textContent = this.destination_.displayName;
      if (this.query_) {
        nameEl.textContent = '';
        // When search query is specified, make it obvious why this particular
        // printer made it to the list. Display name is always visible, even if
        // it does not match the search query.
        this.addTextWithHighlight_(nameEl, textContent);
        // Show the first matching property.
        this.destination_.extraPropertiesToMatch.some(function(property) {
          if (property.match(this.query_)) {
            var hintSpan = document.createElement('span');
            hintSpan.className = 'search-hint';
            nameEl.appendChild(hintSpan);
            this.addTextWithHighlight_(hintSpan, property);
            // Add the same property to the element title.
            textContent += ' (' + property + ')';
            return true;
          }
        }, this);
      } else {
        // Show just the display name and nothing else to lessen visual clutter.
        nameEl.textContent = textContent;
      }
      nameEl.title = textContent;

      if (this.destination_.isExtension) {
        var extensionNameEl = this.getChildElement('.extension-name');
        var extensionName = this.destination_.extensionName;
        extensionNameEl.title = this.destination_.extensionName;
        if (this.query_) {
          extensionNameEl.textContent = '';
          this.addTextWithHighlight_(extensionNameEl, extensionName);
        } else {
          extensionNameEl.textContent = this.destination_.extensionName;
        }

        var extensionIconEl = this.getChildElement('.extension-icon');
        extensionIconEl.style.backgroundImage = '-webkit-image-set(' +
             'url(chrome://extension-icon/' +
                  this.destination_.extensionId + '/24/1) 1x,' +
             'url(chrome://extension-icon/' +
                  this.destination_.extensionId + '/48/1) 2x)';
        extensionIconEl.title = loadTimeData.getStringF(
            'extensionDestinationIconTooltip',
            this.destination_.extensionName);
        extensionIconEl.onclick = this.onExtensionIconClicked_.bind(this);
        extensionIconEl.onkeydown = this.onExtensionIconKeyDown_.bind(this);
      }

      var extensionIndicatorEl =
          this.getChildElement('.extension-controlled-indicator');
      setIsVisible(extensionIndicatorEl, this.destination_.isExtension);

      // Initialize the element which renders the destination's offline status.
      this.getElement().classList.toggle('stale', this.destination_.isOffline);
      var offlineStatusEl = this.getChildElement('.offline-status');
      offlineStatusEl.textContent = this.destination_.offlineStatusText;
      setIsVisible(offlineStatusEl, this.destination_.isOffline);

      // Initialize registration promo element for Privet unregistered printers.
      setIsVisible(
          this.getChildElement('.register-promo'),
          this.destination_.connectionStatus ==
              print_preview.Destination.ConnectionStatus.UNREGISTERED);
    },

    /**
     * Adds text to parent element wrapping search query matches in highlighted
     * spans.
     * @param {!Element} parent Element to build the text in.
     * @param {string} text The text string to highlight segments in.
     * @private
     */
    addTextWithHighlight_: function(parent, text) {
      var sections = text.split(this.query_);
      for (var i = 0; i < sections.length; ++i) {
        if (i % 2 == 0) {
          parent.appendChild(document.createTextNode(sections[i]));
        } else {
          var span = document.createElement('span');
          span.className = 'destination-list-item-query-highlight';
          span.textContent = sections[i];
          parent.appendChild(span);
        }
      }
    },

    /**
     * Called when the destination item is activated. Dispatches a SELECT event
     * on the given event target.
     * @private
     */
    onActivate_: function() {
      if (this.destination_.id ==
              print_preview.Destination.GooglePromotedId.FEDEX &&
          !this.destination_.isTosAccepted) {
        if (!this.fedexTos_) {
          this.fedexTos_ = new print_preview.FedexTos();
          this.fedexTos_.render(this.getElement());
          this.tracker.add(
              this.fedexTos_,
              print_preview.FedexTos.EventType.AGREE,
              this.onTosAgree_.bind(this));
        }
        this.fedexTos_.setIsVisible(true);
      } else if (this.destination_.connectionStatus !=
                     print_preview.Destination.ConnectionStatus.UNREGISTERED) {
        var selectEvt = new Event(DestinationListItem.EventType.SELECT);
        selectEvt.destination = this.destination_;
        this.eventTarget_.dispatchEvent(selectEvt);
      }
    },

    /**
     * Called when the key is pressed on the destination item. Dispatches a
     * SELECT event when Enter is pressed.
     * @param {KeyboardEvent} e Keyboard event to process.
     * @private
     */
    onKeyDown_: function(e) {
      if (!e.shiftKey && !e.ctrlKey && !e.altKey && !e.metaKey) {
        if (e.keyCode == 13) {
          var activeElementTag = document.activeElement ?
              document.activeElement.tagName.toUpperCase() : '';
          if (activeElementTag == 'LI') {
            e.stopPropagation();
            e.preventDefault();
            this.onActivate_();
          }
        }
      }
    },

    /**
     * Called when the user agrees to the print destination's terms-of-service.
     * Selects the print destination that was agreed to.
     * @private
     */
    onTosAgree_: function() {
      var selectEvt = new Event(DestinationListItem.EventType.SELECT);
      selectEvt.destination = this.destination_;
      this.eventTarget_.dispatchEvent(selectEvt);
    },

    /**
     * Called when the registration promo is clicked.
     * @private
     */
    onRegisterPromoClicked_: function() {
      var promoClickedEvent = new Event(
          DestinationListItem.EventType.REGISTER_PROMO_CLICKED);
      promoClickedEvent.destination = this.destination_;
      this.eventTarget_.dispatchEvent(promoClickedEvent);
    },

    /**
     * Handles click and 'Enter' key down events for the extension icon element.
     * It opens extensions page with the extension associated with the
     * destination highlighted.
     * @param {MouseEvent|KeyboardEvent} e The event to handle.
     * @private
     */
    onExtensionIconClicked_: function(e) {
      e.stopPropagation();
      window.open('chrome://extensions?id=' + this.destination_.extensionId);
    },

    /**
     * Handles key down event for the extensin icon element. Keys different than
     * 'Enter' are ignored.
     * @param {KeyboardEvent} e The event to handle.
     * @private
     */
    onExtensionIconKeyDown_: function(e) {
      if (e.shiftKey || e.ctrlKey || e.altKey || e.metaKey)
        return;
      if (e.keyCode != 13 /* Enter */)
        return;
      this.onExtensionIconClicked_(event);
    }
  };

  // Export
  return {
    DestinationListItem: DestinationListItem
  };
});
