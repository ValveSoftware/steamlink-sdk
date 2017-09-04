// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */
Polymer({
  is: 'settings-about-page',

  behaviors: [WebUIListenerBehavior, MainPageBehavior, I18nBehavior],

  properties: {
    /** @private {?UpdateStatusChangedEvent} */
    currentUpdateStatusEvent_: {
      type: Object,
      value: {message: '', progress: 0, status: UpdateStatus.DISABLED},
    },

<if expr="chromeos">
    /** @private */
    hasCheckedForUpdates_: Boolean,

    /** @private {!BrowserChannel} */
    currentChannel_: String,

    /** @private {!BrowserChannel} */
    targetChannel_: String,

    /** @private {?RegulatoryInfo} */
    regulatoryInfo_: Object,
</if>

    /** @private {!{obsolete: boolean, endOfLine: boolean}} */
    obsoleteSystemInfo_: {
      type: Object,
      value: function() {
        return {
          obsolete: loadTimeData.getBoolean('aboutObsoleteNowOrSoon'),
          endOfLine: loadTimeData.getBoolean('aboutObsoleteEndOfTheLine'),
        };
      },
    },
  },

  /** @private {?settings.AboutPageBrowserProxy} */
  aboutBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.aboutBrowserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
    this.aboutBrowserProxy_.pageReady();

    this.lifetimeBrowserProxy_ =
        settings.LifetimeBrowserProxyImpl.getInstance();

<if expr="chromeos">
    this.addEventListener('target-channel-changed', function(e) {
      this.targetChannel_ = e.detail;
    }.bind(this));

    Promise.all([
      this.aboutBrowserProxy_.getCurrentChannel(),
      this.aboutBrowserProxy_.getTargetChannel(),
    ]).then(function(channels) {
      this.currentChannel_ = channels[0];
      this.targetChannel_ = channels[1];

      this.startListening_();
    }.bind(this));

    this.aboutBrowserProxy_.getRegulatoryInfo().then(function(info) {
      this.regulatoryInfo_ = info;
    }.bind(this));
</if>
<if expr="not chromeos">
    this.startListening_();
</if>
  },

  /** @private */
  startListening_: function() {
    this.addWebUIListener(
        'update-status-changed',
        this.onUpdateStatusChanged_.bind(this));
    this.aboutBrowserProxy_.refreshUpdateStatus();
  },

  /**
   * @param {!UpdateStatusChangedEvent} event
   * @private
   */
  onUpdateStatusChanged_: function(event) {
<if expr="chromeos">
    if (event.status == UpdateStatus.CHECKING)
      this.hasCheckedForUpdates_ = true;
</if>
    this.currentUpdateStatusEvent_ = event;
  },

  /** @private */
  onHelpTap_: function() {
    this.aboutBrowserProxy_.openHelpPage();
  },

  /** @private */
  onRelaunchTap_: function() {
    this.lifetimeBrowserProxy_.relaunch();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowUpdateStatusMessage_: function() {
    return this.currentUpdateStatusEvent_.status != UpdateStatus.DISABLED &&
        !this.obsoleteSystemInfo_.endOfLine;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowUpdateStatusIcon_: function() {
    return this.currentUpdateStatusEvent_.status != UpdateStatus.DISABLED ||
        this.obsoleteSystemInfo_.endOfLine;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRelaunch_: function() {
    var shouldShow = false;
<if expr="not chromeos">
    shouldShow = this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
</if>
<if expr="chromeos">
    shouldShow = this.checkStatus_(UpdateStatus.NEARLY_UPDATED) &&
        !this.isTargetChannelMoreStable_();
</if>
    return shouldShow;
  },

  /**
   * @return {string}
   * @private
   */
  getUpdateStatusMessage_: function() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
        return this.i18n('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
<if expr="chromeos">
        if (this.currentChannel_ != this.targetChannel_)
          return this.i18n('aboutUpgradeSuccessChannelSwitch');
</if>
        return this.i18n('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18n('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
        assert(typeof this.currentUpdateStatusEvent_.progress == 'number');
        var progressPercent = this.currentUpdateStatusEvent_.progress + '%';

<if expr="chromeos">
        if (this.currentChannel_ != this.targetChannel_) {
          return this.i18n('aboutUpgradeUpdatingChannelSwitch',
              this.i18n(settings.browserChannelToI18nId(this.targetChannel_)),
              progressPercent);
        }
</if>
        if (this.currentUpdateStatusEvent_.progress > 0) {
          // NOTE(dbeam): some platforms (i.e. Mac) always send 0% while
          // updating (they don't support incremental upgrade progress). Though
          // it's certainly quite possible to validly end up here with 0% on
          // platforms that support incremental progress, nobody really likes
          // seeing that they're 0% done with something.
          return this.i18n('aboutUpgradeUpdatingPercent', progressPercent);
        }
        return this.i18n('aboutUpgradeUpdating');
      default:
        var message = this.currentUpdateStatusEvent_.message;
        return message ?
            parseHtmlSubset('<b>' + message + '</b>').firstChild.innerHTML :
            '';
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getIcon_: function() {
    // If this platform has reached the end of the line, display an error icon
    // and ignore UpdateStatus.
    if (this.obsoleteSystemInfo_.endOfLine)
      return 'settings:error';

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr:domain';
      case UpdateStatus.FAILED:
        return 'settings:error';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
          return 'settings:check-circle';
      default:
          return null;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getIconSrc_: function() {
    if (this.obsoleteSystemInfo_.endOfLine)
      return null;

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @param {!UpdateStatus} status
   * @return {boolean}
   * @private
   */
  checkStatus_: function(status) {
    return this.currentUpdateStatusEvent_.status == status;
  },

<if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  isTargetChannelMoreStable_: function() {
    assert(this.currentChannel_.length > 0);
    assert(this.targetChannel_.length > 0);
    return settings.isTargetChannelMoreStable(
        this.currentChannel_, this.targetChannel_);
  },

  /** @private */
  onDetailedBuildInfoTap_: function() {
    settings.navigateTo(settings.Route.DETAILED_BUILD_INFO);
  },

  /** @private */
  onRelaunchAndPowerwashTap_: function() {
    this.lifetimeBrowserProxy_.factoryReset();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRelaunchAndPowerwash_: function() {
    return this.checkStatus_(UpdateStatus.NEARLY_UPDATED) &&
        this.isTargetChannelMoreStable_();
  },

  /** @private */
  onCheckUpdatesTap_: function() {
    this.onUpdateStatusChanged_({status: UpdateStatus.CHECKING});
    this.aboutBrowserProxy_.requestUpdate();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCheckUpdates_: function() {
    return !this.hasCheckedForUpdates_ ||
        this.checkStatus_(UpdateStatus.FAILED);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRegulatoryInfo_: function() {
    return this.regulatoryInfo_ !== null;
  },
</if>

  /** @private */
  onProductLogoTap_: function() {
    this.$['product-logo'].animate({
      transform: ['none', 'rotate(-10turn)'],
    }, {
      duration: 500,
      easing: 'cubic-bezier(1, 0, 0, 1)',
    });
  },

<if expr="_google_chrome">
  /** @private */
  onReportIssueTap_: function() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  },
</if>
});
