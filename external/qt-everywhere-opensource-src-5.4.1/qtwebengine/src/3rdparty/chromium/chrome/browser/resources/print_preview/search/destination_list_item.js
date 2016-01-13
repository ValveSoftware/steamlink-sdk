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

  /**
   * CSS classes used by the destination list item.
   * @enum {string}
   * @private
   */
  DestinationListItem.Classes_ = {
    ICON: 'destination-list-item-icon',
    NAME: 'destination-list-item-name',
    STALE: 'stale'
  };

  DestinationListItem.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-item-template'));

      var iconImg = this.getElement().getElementsByClassName(
          print_preview.DestinationListItem.Classes_.ICON)[0];
      iconImg.src = this.destination_.iconUrl;

      var nameEl = this.getElement().getElementsByClassName(
          DestinationListItem.Classes_.NAME)[0];
      var textContent = this.destination_.displayName;
      if (this.query_) {
        // When search query is specified, make it obvious why the particular
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

      this.initializeOfflineStatusElement_();
      this.initializeRegistrationPromoElement_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(this.getElement(), 'click', this.onActivate_.bind(this));
    },

    /**
     * Initializes the element which renders the print destination's
     * offline status.
     * @private
     */
    initializeOfflineStatusElement_: function() {
      if (this.destination_.isOffline) {
        this.getElement().classList.add(DestinationListItem.Classes_.STALE);
        var offlineStatusEl = this.getChildElement('.offline-status');
        offlineStatusEl.textContent = this.destination_.offlineStatusText;
        setIsVisible(offlineStatusEl, true);
      }
    },

    /**
     * Initialize registration promo element for Privet unregistered printers.
     */
    initializeRegistrationPromoElement_: function() {
      if (this.destination_.connectionStatus ==
          print_preview.Destination.ConnectionStatus.UNREGISTERED) {
        var registerBtnEl = this.getChildElement('.register-promo-button');
        registerBtnEl.addEventListener('click',
                                       this.onRegisterPromoClicked_.bind(this));

        var registerPromoEl = this.getChildElement('.register-promo');
        setIsVisible(registerPromoEl, true);
      }
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
    }
  };

  // Export
  return {
    DestinationListItem: DestinationListItem
  };
});
