// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
if (!chrome)
  chrome = {};

if (!chrome.embeddedSearch) {
  chrome.embeddedSearch = new function() {
    this.searchBox = new function() {

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function Focus();
      native function GetDisplayInstantResults();
      native function GetMostVisitedItemData();
      native function GetQuery();
      native function GetSearchRequestParams();
      native function GetRightToLeft();
      native function GetSuggestionToPrefetch();
      native function IsFocused();
      native function IsKeyCaptureEnabled();
      native function Paste();
      native function StartCapturingKeyStrokes();
      native function StopCapturingKeyStrokes();

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      this.__defineGetter__('displayInstantResults', GetDisplayInstantResults);
      this.__defineGetter__('isFocused', IsFocused);
      this.__defineGetter__('isKeyCaptureEnabled', IsKeyCaptureEnabled);
      this.__defineGetter__('rtl', GetRightToLeft);
      this.__defineGetter__('suggestion', GetSuggestionToPrefetch);
      this.__defineGetter__('value', GetQuery);
      Object.defineProperty(this, 'requestParams',
                            { get: GetSearchRequestParams });

      this.focus = function() {
        Focus();
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.getMostVisitedItemData = function(restrictedId) {
        var item = GetMostVisitedItemData(restrictedId);
        if (item) {
          var sizeInPx = Math.floor(48 * window.devicePixelRatio + 0.5);
          // Populate large icon and fallback icon data, if they exist. We'll
          // render everything here, once these become available by default.
          if (item.largeIconUrl) {
            item.largeIconUrl +=
                sizeInPx + "/" + item.renderViewId + "/" + item.rid;
          }
          if (item.fallbackIconUrl) {
            item.fallbackIconUrl +=
                sizeInPx + ",,,,/" + item.renderViewId + "/" + item.rid;
          }
        }
        return item;
      };

      this.paste = function(value) {
        Paste(value);
      };

      this.startCapturingKeyStrokes = function() {
        StartCapturingKeyStrokes();
      };

      this.stopCapturingKeyStrokes = function() {
        StopCapturingKeyStrokes();
      };

      this.onfocuschange = null;
      this.onkeycapturechange = null;
      this.onsubmit = null;
      this.onsuggestionchange = null;

      //TODO(jered): Remove this empty method when google no longer requires it.
      this.setRestrictedValue = function() {};
    };

    this.newTabPage = new function() {

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function CheckIsUserSignedInToChromeAs();
      native function CheckIsUserSyncingHistory();
      native function DeleteMostVisitedItem();
      native function GetAppLauncherEnabled();
      native function GetMostVisitedItems();
      native function GetThemeBackgroundInfo();
      native function IsInputInProgress();
      native function LogEvent();
      native function LogMostVisitedImpression();
      native function LogMostVisitedNavigation();
      native function UndoAllMostVisitedDeletions();
      native function UndoMostVisitedDeletion();

      function GetMostVisitedItemsWrapper() {
        var mostVisitedItems = GetMostVisitedItems();
        for (var i = 0, item; item = mostVisitedItems[i]; ++i) {
          item.faviconUrl = GenerateFaviconURL(item.renderViewId, item.rid);

          // These properties are private data and should not be returned to
          // the page. They are only accessible via getMostVisitedItemData().
          delete item.url;
          delete item.title;
          delete item.domain;
          delete item.direction;
          delete item.renderViewId;
          delete item.largeIconUrl;
          delete item.fallbackIconUrl;
        }
        return mostVisitedItems;
      }

      function GenerateFaviconURL(renderViewId, rid) {
        return "chrome-search://favicon/size/16@" +
            window.devicePixelRatio + "x/" +
            renderViewId + "/" + rid;
      }

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      this.__defineGetter__('appLauncherEnabled', GetAppLauncherEnabled);
      this.__defineGetter__('isInputInProgress', IsInputInProgress);
      this.__defineGetter__('mostVisited', GetMostVisitedItemsWrapper);
      this.__defineGetter__('themeBackgroundInfo', GetThemeBackgroundInfo);

      this.deleteMostVisitedItem = function(restrictedId) {
        DeleteMostVisitedItem(restrictedId);
      };

      this.checkIsUserSignedIntoChromeAs = function(identity) {
        CheckIsUserSignedInToChromeAs(identity);
      };

      this.checkIsUserSyncingHistory = function() {
        CheckIsUserSyncingHistory();
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.logEvent = function(histogram_name) {
        LogEvent(histogram_name);
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.logMostVisitedImpression = function(position, provider) {
        LogMostVisitedImpression(position, provider);
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.logMostVisitedNavigation = function(position, provider) {
        LogMostVisitedNavigation(position, provider);
      };

      this.undoAllMostVisitedDeletions = function() {
        UndoAllMostVisitedDeletions();
      };

      this.undoMostVisitedDeletion = function(restrictedId) {
        UndoMostVisitedDeletion(restrictedId);
      };

      this.onsignedincheckdone = null;
      this.onhistorysynccheckdone = null;
      this.oninputcancel = null;
      this.oninputstart = null;
      this.onmostvisitedchange = null;
      this.onthemechange = null;
    };

    // TODO(jered): Remove when google no longer expects this object.
    chrome.searchBox = this.searchBox;
  };
}
