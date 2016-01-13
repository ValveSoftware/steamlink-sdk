// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component used for searching for a print destination.
   * This is a modal dialog that allows the user to search and select a
   * destination to print to. When a destination is selected, it is written to
   * the destination store.
   * @param {!print_preview.DestinationStore} destinationStore Data store
   *     containing the destinations to search through.
   * @param {!print_preview.UserInfo} userInfo Event target that contains
   *     information about the logged in user.
   * @param {!print_preview.Metrics} metrics Used to record usage statistics.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationSearch(destinationStore, userInfo, metrics) {
    print_preview.Component.call(this);

    /**
     * Data store containing the destinations to search through.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Event target that contains information about the logged in user.
     * @type {!print_preview.UserInfo}
     * @private
     */
    this.userInfo_ = userInfo;

    /**
     * Used to record usage statistics.
     * @type {!print_preview.Metrics}
     * @private
     */
    this.metrics_ = metrics;

    /**
     * Whether or not a UMA histogram for the register promo being shown was
     * already recorded.
     * @type {bool}
     * @private
     */
    this.registerPromoShownMetricRecorded_ = false;

    /**
     * Search box used to search through the destination lists.
     * @type {!print_preview.SearchBox}
     * @private
     */
    this.searchBox_ = new print_preview.SearchBox();
    this.addChild(this.searchBox_);

    /**
     * Destination list containing recent destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.recentList_ = new print_preview.RecentDestinationList(this);
    this.addChild(this.recentList_);

    /**
     * Destination list containing local destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.localList_ = new print_preview.DestinationList(
        this,
        localStrings.getString('localDestinationsTitle'),
        cr.isChromeOS ? null : localStrings.getString('manage'));
    this.addChild(this.localList_);

    /**
     * Destination list containing cloud destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.cloudList_ = new print_preview.CloudDestinationList(this);
    this.addChild(this.cloudList_);
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSearch.EventType = {
    // Dispatched when user requests to sign-in into another Google account.
    ADD_ACCOUNT: 'print_preview.DestinationSearch.ADD_ACCOUNT',

    // Dispatched when the user requests to manage their cloud destinations.
    MANAGE_CLOUD_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_CLOUD_DESTINATIONS',

    // Dispatched when the user requests to manage their local destinations.
    MANAGE_LOCAL_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_LOCAL_DESTINATIONS',

    // Dispatched when the user requests to sign-in to their Google account.
    SIGN_IN: 'print_preview.DestinationSearch.SIGN_IN'
  };

  /**
   * Padding at the bottom of a destination list in pixels.
   * @type {number}
   * @const
   * @private
   */
  DestinationSearch.LIST_BOTTOM_PADDING_ = 18;

  /**
   * Number of unregistered destinations that may be promoted to the top.
   * @type {number}
   * @const
   * @private
   */
  DestinationSearch.MAX_PROMOTED_UNREGISTERED_PRINTERS_ = 2;

  DestinationSearch.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @return {boolean} Whether the component is visible. */
    getIsVisible: function() {
      return !this.getElement().classList.contains('transparent');
    },

    /** @param {boolean} isVisible Whether the component is visible. */
    setIsVisible: function(isVisible) {
      if (this.getIsVisible() == isVisible) {
        return;
      }
      if (isVisible) {
        setIsVisible(this.getElement(), true);
        setTimeout(function(element) {
          element.classList.remove('transparent');
        }.bind(this, this.getElement()), 0);
        this.searchBox_.focus();
        var promoEl = this.getChildElement('.cloudprint-promo');
        if (getIsVisible(promoEl)) {
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.
                  CLOUDPRINT_PROMO_SHOWN);
        }
        if (this.userInfo_.initialized) {
          this.onUsersChanged_();
        }
        this.reflowLists_();
      } else {
        this.getElement().classList.add('transparent');
        // Collapse all destination lists
        this.localList_.setIsShowAll(false);
        this.cloudList_.setIsShowAll(false);
        this.searchBox_.setQuery('');
        this.filterLists_(null);
      }
    },

    /** Shows the Google Cloud Print promotion banner. */
    showCloudPrintPromo: function() {
      setIsVisible(this.getChildElement('.cloudprint-promo'), true);
      if (this.getIsVisible()) {
        this.metrics_.incrementDestinationSearchBucket(
            print_preview.Metrics.DestinationSearchBucket.
                CLOUDPRINT_PROMO_SHOWN);
      }
      this.reflowLists_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      this.getElement().addEventListener('webkitTransitionEnd', function f(e) {
        if (e.target != e.currentTarget || e.propertyName != 'opacity')
          return;
        if (e.target.classList.contains('transparent')) {
          setIsVisible(e.target, false);
        }
      });

      this.tracker.add(
          this.getChildElement('.account-select'),
          'change',
          this.onAccountChange_.bind(this));

      this.tracker.add(
          this.getChildElement('.page > .close-button'),
          'click',
          this.onCloseClick_.bind(this));

      this.tracker.add(
          this.getChildElement('.sign-in'),
          'click',
          this.onSignInActivated_.bind(this));

      this.tracker.add(
          this.getChildElement('.cloudprint-promo > .close-button'),
          'click',
          this.onCloudprintPromoCloseButtonClick_.bind(this));
      this.tracker.add(
          this.searchBox_,
          print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));
      this.tracker.add(
          this,
          print_preview.DestinationListItem.EventType.SELECT,
          this.onDestinationSelect_.bind(this));

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
          this.onDestinationsInserted_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationStoreSelect_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_STARTED,
          this.updateThrobbers_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
          this.onDestinationSearchDone_.bind(this));

      this.tracker.add(
          this.localList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageLocalDestinationsActivated_.bind(this));
      this.tracker.add(
          this.cloudList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageCloudDestinationsActivated_.bind(this));

      this.tracker.add(
          this.getElement(), 'click', this.onClick_.bind(this));
      this.tracker.add(
          this.getChildElement('.page'),
          'webkitAnimationEnd',
          this.onAnimationEnd_.bind(this));

      this.tracker.add(
          this.userInfo_,
          print_preview.UserInfo.EventType.USERS_CHANGED,
          this.onUsersChanged_.bind(this));

      this.tracker.add(window, 'resize', this.onWindowResize_.bind(this));

      this.updateThrobbers_();

      // Render any destinations already in the store.
      this.renderDestinations_();
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.decorate($('search-box'));
      this.recentList_.render(this.getChildElement('.recent-list'));
      this.localList_.render(this.getChildElement('.local-list'));
      this.cloudList_.render(this.getChildElement('.cloud-list'));
      this.getChildElement('.promo-text').innerHTML = localStrings.getStringF(
          'cloudPrintPromotion',
          '<span class="sign-in link-button">',
          '</span>');
      this.getChildElement('.account-select-label').textContent =
          localStrings.getString('accountSelectTitle');
    },

    /**
     * @return {number} Height available for destination lists, in pixels.
     * @private
     */
    getAvailableListsHeight_: function() {
      var elStyle = window.getComputedStyle(this.getElement());
      return this.getElement().offsetHeight -
          parseInt(elStyle.getPropertyValue('padding-top')) -
          parseInt(elStyle.getPropertyValue('padding-bottom')) -
          this.getChildElement('.lists').offsetTop -
          this.getChildElement('.cloudprint-promo').offsetHeight;
    },

    /**
     * Filters all destination lists with the given query.
     * @param {?string} query Query to filter destination lists by.
     * @private
     */
    filterLists_: function(query) {
      this.recentList_.updateSearchQuery(query);
      this.localList_.updateSearchQuery(query);
      this.cloudList_.updateSearchQuery(query);
    },

    /**
     * Resets the search query.
     * @private
     */
    resetSearch_: function() {
      this.searchBox_.setQuery(null);
      this.filterLists_(null);
    },

    /**
     * Renders all of the destinations in the destination store.
     * @private
     */
    renderDestinations_: function() {
      var recentDestinations = [];
      var localDestinations = [];
      var cloudDestinations = [];
      var unregisteredCloudDestinations = [];

      var destinations =
          this.destinationStore_.destinations(this.userInfo_.activeUser);
      destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
        if (destination.isLocal ||
            destination.origin == print_preview.Destination.Origin.DEVICE) {
          localDestinations.push(destination);
        } else {
          if (destination.connectionStatus ==
                print_preview.Destination.ConnectionStatus.UNREGISTERED) {
            unregisteredCloudDestinations.push(destination);
          } else {
            cloudDestinations.push(destination);
          }
        }
      });

      if (unregisteredCloudDestinations.length != 0 &&
          !this.registerPromoShownMetricRecorded_) {
        this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.REGISTER_PROMO_SHOWN);
        this.registerPromoShownMetricRecorded_ = true;
      }

      var finalCloudDestinations = unregisteredCloudDestinations.slice(
        0, DestinationSearch.MAX_PROMOTED_UNREGISTERED_PRINTERS_).concat(
          cloudDestinations,
          unregisteredCloudDestinations.slice(
            DestinationSearch.MAX_PROMOTED_UNREGISTERED_PRINTERS_));

      this.recentList_.updateDestinations(recentDestinations);
      this.localList_.updateDestinations(localDestinations);
      this.cloudList_.updateDestinations(finalCloudDestinations);
    },

    /**
     * Reflows the destination lists according to the available height.
     * @private
     */
    reflowLists_: function() {
      if (!this.getIsVisible()) {
        return;
      }

      var hasCloudList = getIsVisible(this.getChildElement('.cloud-list'));
      var lists = [this.recentList_, this.localList_];
      if (hasCloudList) {
        lists.push(this.cloudList_);
      }

      var getListsTotalHeight = function(lists, counts) {
        return lists.reduce(function(sum, list, index) {
          return sum + list.getEstimatedHeightInPixels(counts[index]) +
              DestinationSearch.LIST_BOTTOM_PADDING_;
        }, 0);
      };
      var getCounts = function(lists, count) {
        return lists.map(function(list) { return count; });
      };

      var availableHeight = this.getAvailableListsHeight_();
      var listsEl = this.getChildElement('.lists');
      listsEl.style.maxHeight = availableHeight + 'px';

      var maxListLength = lists.reduce(function(prevCount, list) {
        return Math.max(prevCount, list.getDestinationsCount());
      }, 0);
      for (var i = 1; i <= maxListLength; i++) {
        if (getListsTotalHeight(lists, getCounts(lists, i)) > availableHeight) {
          i--;
          break;
        }
      }
      var counts = getCounts(lists, i);
      // Fill up the possible n-1 free slots left by the previous loop.
      if (getListsTotalHeight(lists, counts) < availableHeight) {
        for (var countIndex = 0; countIndex < counts.length; countIndex++) {
          counts[countIndex]++;
          if (getListsTotalHeight(lists, counts) > availableHeight) {
            counts[countIndex]--;
            break;
          }
        }
      }

      lists.forEach(function(list, index) {
        list.updateShortListSize(counts[index]);
      });

      // Set height of the list manually so that search filter doesn't change
      // lists height.
      var listsHeight = getListsTotalHeight(lists, counts) + 'px';
      if (listsHeight != listsEl.style.height) {
        // Try to close account select if there's a possibility it's open now.
        var accountSelectEl = this.getChildElement('.account-select');
        if (!accountSelectEl.disabled) {
          accountSelectEl.disabled = true;
          accountSelectEl.disabled = false;
        }
        listsEl.style.height = listsHeight;
      }
    },

    /**
     * Updates whether the throbbers for the various destination lists should be
     * shown or hidden.
     * @private
     */
    updateThrobbers_: function() {
      this.localList_.setIsThrobberVisible(
          this.destinationStore_.isLocalDestinationSearchInProgress);
      this.cloudList_.setIsThrobberVisible(
          this.destinationStore_.isCloudDestinationSearchInProgress);
      this.recentList_.setIsThrobberVisible(
          this.destinationStore_.isLocalDestinationSearchInProgress &&
          this.destinationStore_.isCloudDestinationSearchInProgress);
      this.reflowLists_();
    },

    /**
     * Called when user's logged in accounts change. Updates the UI.
     * @private
     */
    onUsersChanged_: function() {
      var loggedIn = this.userInfo_.loggedIn;
      if (loggedIn) {
        var accountSelectEl = this.getChildElement('.account-select');
        accountSelectEl.innerHTML = '';
        this.userInfo_.users.forEach(function(account) {
          var option = document.createElement('option');
          option.text = account;
          option.value = account;
          accountSelectEl.add(option);
        });
        var option = document.createElement('option');
        option.text = localStrings.getString('addAccountTitle');
        option.value = '';
        accountSelectEl.add(option);

        accountSelectEl.selectedIndex =
            this.userInfo_.users.indexOf(this.userInfo_.activeUser);
      }

      setIsVisible(this.getChildElement('.user-info'), loggedIn);
      setIsVisible(this.getChildElement('.cloud-list'), loggedIn);
      setIsVisible(this.getChildElement('.cloudprint-promo'), !loggedIn);
      this.reflowLists_();
    },

    /**
     * Called when a destination search should be executed. Filters the
     * destination lists with the given query.
     * @param {Event} evt Contains the search query.
     * @private
     */
    onSearch_: function(evt) {
      this.filterLists_(evt.query);
    },

    /**
     * Called when the close button is clicked. Hides the search widget.
     * @private
     */
    onCloseClick_: function() {
      this.setIsVisible(false);
      this.resetSearch_();
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.CANCELED);
    },

    /**
     * Called when a destination is selected. Clears the search and hides the
     * widget.
     * @param {Event} evt Contains the selected destination.
     * @private
     */
    onDestinationSelect_: function(evt) {
      this.setIsVisible(false);
      this.resetSearch_();
      this.destinationStore_.selectDestination(evt.destination);
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.DESTINATION_SELECTED);
    },

    /**
     * Called when a destination is selected. Selected destination are marked as
     * recent, so we have to update our recent destinations list.
     * @private
     */
    onDestinationStoreSelect_: function() {
      var destinations =
          this.destinationStore_.destinations(this.userInfo_.activeUser);
      var recentDestinations = [];
      destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
      });
      this.recentList_.updateDestinations(recentDestinations);
      this.reflowLists_();
    },

    /**
     * Called when destinations are inserted into the store. Rerenders
     * destinations.
     * @private
     */
    onDestinationsInserted_: function() {
      this.renderDestinations_();
      this.reflowLists_();
    },

    /**
     * Called when destinations are inserted into the store. Rerenders
     * destinations.
     * @private
     */
    onDestinationSearchDone_: function() {
      this.updateThrobbers_();
      this.renderDestinations_();
      this.reflowLists_();
    },

    /**
     * Called when the manage cloud printers action is activated.
     * @private
     */
    onManageCloudDestinationsActivated_: function() {
      cr.dispatchSimpleEvent(
          this,
          print_preview.DestinationSearch.EventType.MANAGE_CLOUD_DESTINATIONS);
    },

    /**
     * Called when the manage local printers action is activated.
     * @private
     */
    onManageLocalDestinationsActivated_: function() {
      cr.dispatchSimpleEvent(
          this,
          print_preview.DestinationSearch.EventType.MANAGE_LOCAL_DESTINATIONS);
    },

    /**
     * Called when the "Sign in" link on the Google Cloud Print promo is
     * activated.
     * @private
     */
    onSignInActivated_: function() {
      cr.dispatchSimpleEvent(this, DestinationSearch.EventType.SIGN_IN);
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.SIGNIN_TRIGGERED);
    },

    /**
     * Called when item in the Accounts list is selected. Initiates active user
     * switch or, for 'Add account...' item, opens Google sign-in page.
     * @private
     */
    onAccountChange_: function() {
      var accountSelectEl = this.getChildElement('.account-select');
      var account =
          accountSelectEl.options[accountSelectEl.selectedIndex].value;
      if (account) {
        this.userInfo_.activeUser = account;
        this.destinationStore_.reloadUserCookieBasedDestinations();
        this.metrics_.incrementDestinationSearchBucket(
            print_preview.Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
      } else {
        cr.dispatchSimpleEvent(this, DestinationSearch.EventType.ADD_ACCOUNT);
        // Set selection back to the active user.
        for (var i = 0; i < accountSelectEl.options.length; i++) {
          if (accountSelectEl.options[i].value == this.userInfo_.activeUser) {
            accountSelectEl.selectedIndex = i;
            break;
          }
        }
        this.metrics_.incrementDestinationSearchBucket(
            print_preview.Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
      }
    },

    /**
     * Called when the close button on the cloud print promo is clicked. Hides
     * the promo.
     * @private
     */
    onCloudprintPromoCloseButtonClick_: function() {
      setIsVisible(this.getChildElement('.cloudprint-promo'), false);
    },

    /**
     * Called when the overlay is clicked. Pulses the page.
     * @param {Event} event Contains the element that was clicked.
     * @private
     */
    onClick_: function(event) {
      if (event.target == this.getElement()) {
        this.getChildElement('.page').classList.add('pulse');
      }
    },

    /**
     * Called when an animation ends on the page.
     * @private
     */
    onAnimationEnd_: function() {
      this.getChildElement('.page').classList.remove('pulse');
    },

    /**
     * Called when the window is resized. Reflows layout of destination lists.
     * @private
     */
    onWindowResize_: function() {
      this.reflowLists_();
    }
  };

  // Export
  return {
    DestinationSearch: DestinationSearch
  };
});
