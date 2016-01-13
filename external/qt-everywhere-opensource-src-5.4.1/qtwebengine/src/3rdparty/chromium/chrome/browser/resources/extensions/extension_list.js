// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_error.js"></include>

cr.define('options', function() {
  'use strict';

  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionsList = cr.ui.define('div');

  /**
   * @type {Object.<string, boolean>} A map from extension id to a boolean
   *     indicating whether the incognito warning is showing. This persists
   *     between calls to decorate.
   */
  var butterBarVisibility = {};

  /**
   * @type {Object.<string, string>} A map from extension id to last reloaded
   *     timestamp. The timestamp is recorded when the user click the 'Reload'
   *     link. It is used to refresh the icon of an unpacked extension.
   *     This persists between calls to decorate.
   */
  var extensionReloadedTimestamp = {};

  ExtensionsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.textContent = '';

      this.showExtensionNodes_();
    },

    getIdQueryParam_: function() {
      return parseQueryParams(document.location)['id'];
    },

    /**
     * Creates all extension items from scratch.
     * @private
     */
    showExtensionNodes_: function() {
      // Iterate over the extension data and add each item to the list.
      this.data_.extensions.forEach(this.createNode_, this);

      var idToHighlight = this.getIdQueryParam_();
      if (idToHighlight && $(idToHighlight)) {
        // Scroll offset should be calculated slightly higher than the actual
        // offset of the element being scrolled to, so that it ends up not all
        // the way at the top. That way it is clear that there are more elements
        // above the element being scrolled to.
        var scrollFudge = 1.2;
        var scrollTop = $(idToHighlight).offsetTop - scrollFudge *
            $(idToHighlight).clientHeight;
        setScrollTopForDocument(document, scrollTop);
      }

      if (this.data_.extensions.length == 0)
        this.classList.add('empty-extension-list');
      else
        this.classList.remove('empty-extension-list');
    },

    /**
     * Synthesizes and initializes an HTML element for the extension metadata
     * given in |extension|.
     * @param {Object} extension A dictionary of extension metadata.
     * @private
     */
    createNode_: function(extension) {
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
      node.id = extension.id;

      if (!extension.enabled || extension.terminated)
        node.classList.add('inactive-extension');

      if (extension.managedInstall) {
        node.classList.add('may-not-modify');
        node.classList.add('may-not-remove');
      } else if (extension.suspiciousInstall || extension.corruptInstall) {
        node.classList.add('may-not-modify');
      }

      var idToHighlight = this.getIdQueryParam_();
      if (node.id == idToHighlight)
        node.classList.add('extension-highlight');

      var item = node.querySelector('.extension-list-item');
      // Prevent the image cache of extension icon by using the reloaded
      // timestamp as a query string. The timestamp is recorded when the user
      // clicks the 'Reload' link. http://crbug.com/159302.
      if (extensionReloadedTimestamp[extension.id]) {
        item.style.backgroundImage =
            'url(' + extension.icon + '?' +
            extensionReloadedTimestamp[extension.id] + ')';
      } else {
        item.style.backgroundImage = 'url(' + extension.icon + ')';
      }

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      var version = node.querySelector('.extension-version');
      version.textContent = extension.version;

      var locationText = node.querySelector('.location-text');
      locationText.textContent = extension.locationText;

      var blacklistText = node.querySelector('.blacklist-text');
      blacklistText.textContent = extension.blacklistText;

      var description = node.querySelector('.extension-description span');
      description.textContent = extension.description;

      // The 'Show Browser Action' button.
      if (extension.enable_show_button) {
        var showButton = node.querySelector('.show-button');
        showButton.addEventListener('click', function(e) {
          chrome.send('extensionSettingsShowButton', [extension.id]);
        });
        showButton.hidden = false;
      }

      // The 'allow in incognito' checkbox.
      var incognito = node.querySelector('.incognito-control input');
      incognito.disabled = !extension.incognitoCanBeEnabled;
      incognito.checked = extension.enabledIncognito;
      if (!incognito.disabled) {
        incognito.addEventListener('change', function(e) {
          var checked = e.target.checked;
          butterBarVisibility[extension.id] = checked;
          butterBar.hidden = !checked || extension.is_hosted_app;
          chrome.send('extensionSettingsEnableIncognito',
                      [extension.id, String(checked)]);
        });
      }
      var butterBar = node.querySelector('.butter-bar');
      butterBar.hidden = !butterBarVisibility[extension.id];

      // The 'collect errors' checkbox. This should only be visible if the
      // error console is enabled - we can detect this by the existence of the
      // |errorCollectionEnabled| property.
      if (extension.wantsErrorCollection) {
        node.querySelector('.error-collection-control').hidden = false;
        var errorCollection =
            node.querySelector('.error-collection-control input');
        errorCollection.checked = extension.errorCollectionEnabled;
        errorCollection.addEventListener('change', function(e) {
          chrome.send('extensionSettingsEnableErrorCollection',
                      [extension.id, String(e.target.checked)]);
        });
      }

      // The 'allow on all urls' checkbox. This should only be visible if
      // active script restrictions are enabled. If they are not enabled, no
      // extensions should want all urls.
      if (extension.wantsAllUrls) {
        var allUrls = node.querySelector('.all-urls-control');
        allUrls.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowOnAllUrls',
                      [extension.id, String(e.target.checked)]);
        });
        allUrls.querySelector('input').checked = extension.allowAllUrls;
        allUrls.hidden = false;
      }

      // The 'allow file:// access' checkbox.
      if (extension.wantsFileAccess) {
        var fileAccess = node.querySelector('.file-access-control');
        fileAccess.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowFileAccess',
                      [extension.id, String(e.target.checked)]);
        });
        fileAccess.querySelector('input').checked = extension.allowFileAccess;
        fileAccess.hidden = false;
      }

      // The 'Options' link.
      if (extension.enabled && extension.optionsUrl) {
        var options = node.querySelector('.options-link');
        options.addEventListener('click', function(e) {
          chrome.send('extensionSettingsOptions', [extension.id]);
          e.preventDefault();
        });
        options.hidden = false;
      }

      // The 'Permissions' link.
      var permissions = node.querySelector('.permissions-link');
      permissions.addEventListener('click', function(e) {
        chrome.send('extensionSettingsPermissions', [extension.id]);
        e.preventDefault();
      });

      // The 'View in Web Store/View Web Site' link.
      if (extension.homepageUrl) {
        var siteLink = node.querySelector('.site-link');
        siteLink.href = extension.homepageUrl;
        siteLink.textContent = loadTimeData.getString(
                extension.homepageProvided ? 'extensionSettingsVisitWebsite' :
                                             'extensionSettingsVisitWebStore');
        siteLink.hidden = false;
      }

      if (extension.allow_reload) {
        // The 'Reload' link.
        var reload = node.querySelector('.reload-link');
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
          extensionReloadedTimestamp[extension.id] = Date.now();
        });
        reload.hidden = false;

        if (extension.is_platform_app) {
          // The 'Launch' link.
          var launch = node.querySelector('.launch-link');
          launch.addEventListener('click', function(e) {
            chrome.send('extensionSettingsLaunch', [extension.id]);
          });
          launch.hidden = false;
        }
      }

      if (!extension.terminated) {
        // The 'Enabled' checkbox.
        var enable = node.querySelector('.enable-checkbox');
        enable.hidden = false;
        var managedOrHosedExtension = extension.managedInstall ||
                                      extension.suspiciousInstall ||
                                      extension.corruptInstall;
        enable.querySelector('input').disabled = managedOrHosedExtension;

        if (!managedOrHosedExtension) {
          enable.addEventListener('click', function(e) {
            // When e.target is the label instead of the checkbox, it doesn't
            // have the checked property and the state of the checkbox is
            // left unchanged.
            var checked = e.target.checked;
            if (checked == undefined)
              checked = !e.currentTarget.querySelector('input').checked;
            chrome.send('extensionSettingsEnable',
                        [extension.id, checked ? 'true' : 'false']);

            // This may seem counter-intuitive (to not set/clear the checkmark)
            // but this page will be updated asynchronously if the extension
            // becomes enabled/disabled. It also might not become enabled or
            // disabled, because the user might e.g. get prompted when enabling
            // and choose not to.
            e.preventDefault();
          });
        }

        enable.querySelector('input').checked = extension.enabled;
      } else {
        var terminatedReload = node.querySelector('.terminated-reload-link');
        terminatedReload.hidden = false;
        terminatedReload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
      }

      // 'Remove' button.
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.title = loadTimeData.getString('extensionUninstall');
      trash.addEventListener('click', function(e) {
        butterBarVisibility[extension.id] = false;
        chrome.send('extensionSettingsUninstall', [extension.id]);
      });
      node.querySelector('.enable-controls').appendChild(trash);

      // Developer mode ////////////////////////////////////////////////////////

      // First we have the id.
      var idLabel = node.querySelector('.extension-id');
      idLabel.textContent = ' ' + extension.id;

      // Then the path, if provided by unpacked extension.
      if (extension.isUnpacked) {
        var loadPath = node.querySelector('.load-path');
        loadPath.hidden = false;
        loadPath.querySelector('span:nth-of-type(2)').textContent =
            ' ' + extension.path;
      }

      // Then the 'managed, cannot uninstall/disable' message.
      if (extension.managedInstall) {
        node.querySelector('.managed-message').hidden = false;
      } else {
        if (extension.suspiciousInstall) {
          // Then the 'This isn't from the webstore, looks suspicious' message.
          node.querySelector('.suspicious-install-message').hidden = false;
        }
        if (extension.corruptInstall) {
          // Then the 'This is a corrupt extension' message.
          node.querySelector('.corrupt-install-message').hidden = false;
        }
      }

      // Then active views.
      if (extension.views.length > 0) {
        var activeViews = node.querySelector('.active-views');
        activeViews.hidden = false;
        var link = activeViews.querySelector('a');

        extension.views.forEach(function(view, i) {
          var displayName = view.generatedBackgroundPage ?
              loadTimeData.getString('backgroundPage') : view.path;
          var label = displayName +
              (view.incognito ?
                  ' ' + loadTimeData.getString('viewIncognito') : '') +
              (view.renderProcessId == -1 ?
                  ' ' + loadTimeData.getString('viewInactive') : '');
          link.textContent = label;
          link.addEventListener('click', function(e) {
            // TODO(estade): remove conversion to string?
            chrome.send('extensionSettingsInspect', [
              String(extension.id),
              String(view.renderProcessId),
              String(view.renderViewId),
              view.incognito
            ]);
          });

          if (i < extension.views.length - 1) {
            link = link.cloneNode(true);
            activeViews.appendChild(link);
          }
        });
      }

      // The extension warnings (describing runtime issues).
      if (extension.warnings) {
        var panel = node.querySelector('.extension-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.warnings.forEach(function(warning) {
          list.appendChild(document.createElement('li')).innerText = warning;
        });
      }

      // If the ErrorConsole is enabled, we should have manifest and/or runtime
      // errors. Otherwise, we may have install warnings. We should not have
      // both ErrorConsole errors and install warnings.
      if (extension.manifestErrors) {
        var panel = node.querySelector('.manifest-errors');
        panel.hidden = false;
        panel.appendChild(new extensions.ExtensionErrorList(
            extension.manifestErrors));
      }
      if (extension.runtimeErrors) {
        var panel = node.querySelector('.runtime-errors');
        panel.hidden = false;
        panel.appendChild(new extensions.ExtensionErrorList(
            extension.runtimeErrors));
      }
      if (extension.installWarnings) {
        var panel = node.querySelector('.install-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.installWarnings.forEach(function(warning) {
          var li = document.createElement('li');
          li.innerText = warning.message;
          list.appendChild(li);
        });
      }

      this.appendChild(node);
      if (location.hash.substr(1) == extension.id) {
        // Scroll beneath the fixed header so that the extension is not
        // obscured.
        var topScroll = node.offsetTop - $('page-header').offsetHeight;
        var pad = parseInt(getComputedStyle(node, null).marginTop, 10);
        if (!isNaN(pad))
          topScroll -= pad / 2;
        setScrollTopForDocument(document, topScroll);
      }
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
