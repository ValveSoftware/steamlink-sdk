// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AddLanguageOverlay = options.AddLanguageOverlay;
var AlertOverlay = options.AlertOverlay;
var AutofillEditAddressOverlay = options.AutofillEditAddressOverlay;
var AutofillEditCreditCardOverlay = options.AutofillEditCreditCardOverlay;
var AutofillOptions = options.AutofillOptions;
var AutomaticSettingsResetBanner = options.AutomaticSettingsResetBanner;
var BrowserOptions = options.BrowserOptions;
var ClearBrowserDataOverlay = options.ClearBrowserDataOverlay;
var ClearBrowserDataHistoryNotice = options.ClearBrowserDataHistoryNotice;
var ConfirmDialog = options.ConfirmDialog;
var ContentSettingsExceptionsArea =
    options.contentSettings.ContentSettingsExceptionsArea;
var ContentSettings = options.ContentSettings;
var CookiesView = options.CookiesView;
var CreateProfileOverlay = options.CreateProfileOverlay;
var EditDictionaryOverlay = cr.IsMac ? null : options.EditDictionaryOverlay;
var EasyUnlockTurnOffOverlay = options.EasyUnlockTurnOffOverlay;
var FactoryResetOverlay = options.FactoryResetOverlay;
<if expr="enable_google_now">
var GeolocationOptions = options.GeolocationOptions;
</if>
var FontSettings = options.FontSettings;
var HandlerOptions = options.HandlerOptions;
var HomePageOverlay = options.HomePageOverlay;
var HotwordConfirmDialog = options.HotwordConfirmDialog;
var ImportDataOverlay = options.ImportDataOverlay;
var LanguageOptions = options.LanguageOptions;
var ManageProfileOverlay = options.ManageProfileOverlay;
var OptionsFocusManager = options.OptionsFocusManager;
var OptionsPage = options.OptionsPage;
var PageManager = cr.ui.pageManager.PageManager;
var PasswordManager = options.PasswordManager;
var Preferences = options.Preferences;
var PreferredNetworks = options.PreferredNetworks;
var ResetProfileSettingsOverlay = options.ResetProfileSettingsOverlay;
var SearchEngineManager = options.SearchEngineManager;
var SearchPage = options.SearchPage;
var StartupOverlay = options.StartupOverlay;
var SupervisedUserCreateConfirmOverlay =
    options.SupervisedUserCreateConfirmOverlay;
var SupervisedUserImportOverlay = options.SupervisedUserImportOverlay;
var SupervisedUserLearnMoreOverlay = options.SupervisedUserLearnMoreOverlay;
var SyncSetupOverlay = options.SyncSetupOverlay;
var ThirdPartyImeConfirmOverlay = options.ThirdPartyImeConfirmOverlay;
var TriggeredResetProfileSettingsOverlay =
    options.TriggeredResetProfileSettingsOverlay;

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  // Decorate the existing elements in the document.
  cr.ui.decorate('input[pref][type=checkbox]', options.PrefCheckbox);
  cr.ui.decorate('input[pref][type=number]', options.PrefNumber);
  cr.ui.decorate('input[pref][type=radio]', options.PrefRadio);
  cr.ui.decorate('input[pref][type=range]', options.PrefRange);
  cr.ui.decorate('select[pref]', options.PrefSelect);
  cr.ui.decorate('input[pref][type=text]', options.PrefTextField);
  cr.ui.decorate('input[pref][type=url]', options.PrefTextField);
  cr.ui.decorate('button[pref]', options.PrefButton);
  cr.ui.decorate('#content-settings-page input[type=radio]:not(.handler-radio)',
      options.ContentSettingsRadio);
  cr.ui.decorate('#content-settings-page input[type=radio].handler-radio',
      options.HandlersEnabledRadio);
  cr.ui.decorate('span.controlled-setting-indicator',
      options.ControlledSettingIndicator);

  // Top level pages.
  PageManager.register(SearchPage.getInstance());
  PageManager.register(BrowserOptions.getInstance());

  // Overlays.
  PageManager.registerOverlay(AddLanguageOverlay.getInstance(),
                              LanguageOptions.getInstance());
  PageManager.registerOverlay(AlertOverlay.getInstance());
  PageManager.registerOverlay(AutofillEditAddressOverlay.getInstance(),
                              AutofillOptions.getInstance());
  PageManager.registerOverlay(AutofillEditCreditCardOverlay.getInstance(),
                              AutofillOptions.getInstance());
  PageManager.registerOverlay(AutofillOptions.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('autofill-settings')]);
  PageManager.registerOverlay(ClearBrowserDataOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyClearDataButton')]);
  PageManager.registerOverlay(
      ClearBrowserDataHistoryNotice.getInstance(),
      ClearBrowserDataOverlay.getInstance());
  PageManager.registerOverlay(
      new ConfirmDialog(
          'doNotTrackConfirm',
          loadTimeData.getString('doNotTrackConfirmOverlayTabTitle'),
          'do-not-track-confirm-overlay',
          /** @type {HTMLButtonElement} */($('do-not-track-confirm-ok')),
          /** @type {HTMLButtonElement} */($('do-not-track-confirm-cancel')),
          $('do-not-track-enabled')['pref'],
          $('do-not-track-enabled')['metric']),
      BrowserOptions.getInstance());
  PageManager.registerOverlay(
      new ConfirmDialog(
          'spellingConfirm',
          loadTimeData.getString('spellingConfirmOverlayTabTitle'),
          'spelling-confirm-overlay',
          /** @type {HTMLButtonElement} */($('spelling-confirm-ok')),
          /** @type {HTMLButtonElement} */($('spelling-confirm-cancel')),
          $('spelling-enabled-control')['pref'],
          $('spelling-enabled-control')['metric']),
      BrowserOptions.getInstance());
  PageManager.registerOverlay(new HotwordConfirmDialog(),
                              BrowserOptions.getInstance());
  PageManager.registerOverlay(ContentSettings.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyContentSettingsButton')]);
  PageManager.registerOverlay(ContentSettingsExceptionsArea.getInstance(),
                              ContentSettings.getInstance());
  PageManager.registerOverlay(CookiesView.getInstance(),
                              ContentSettings.getInstance(),
                              [$('privacyContentSettingsButton'),
                               $('show-cookies-button')]);
  PageManager.registerOverlay(CreateProfileOverlay.getInstance(),
                              BrowserOptions.getInstance());
  PageManager.registerOverlay(EasyUnlockTurnOffOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('easy-unlock-turn-off-button')]);
  if (!cr.isMac) {
    PageManager.registerOverlay(EditDictionaryOverlay.getInstance(),
                                LanguageOptions.getInstance(),
                                [$('edit-custom-dictionary-button')]);
  }
  PageManager.registerOverlay(FontSettings.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('fontSettingsCustomizeFontsButton')]);
  if (HandlerOptions && $('manage-handlers-button')) {
    PageManager.registerOverlay(HandlerOptions.getInstance(),
                                ContentSettings.getInstance(),
                                [$('manage-handlers-button')]);
  }
  PageManager.registerOverlay(HomePageOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('change-home-page')]);
  PageManager.registerOverlay(ImportDataOverlay.getInstance(),
                              BrowserOptions.getInstance());
  PageManager.registerOverlay(LanguageOptions.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('language-button'),
                               $('manage-languages')]);
  PageManager.registerOverlay(ManageProfileOverlay.getInstance(),
                              BrowserOptions.getInstance());
  if (!cr.isChromeOS) {
    PageManager.registerOverlay(SupervisedUserCreateConfirmOverlay.
                                    getInstance(),
                                BrowserOptions.getInstance());
    PageManager.registerOverlay(SupervisedUserImportOverlay.getInstance(),
                                CreateProfileOverlay.getInstance());
    PageManager.registerOverlay(SupervisedUserLearnMoreOverlay.getInstance(),
                                CreateProfileOverlay.getInstance());
  }
  PageManager.registerOverlay(PasswordManager.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('manage-passwords')]);
  PageManager.registerOverlay(ResetProfileSettingsOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('reset-profile-settings')]);
  PageManager.registerOverlay(SearchEngineManager.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('manage-default-search-engines')]);
  PageManager.registerOverlay(StartupOverlay.getInstance(),
                              BrowserOptions.getInstance());
  PageManager.registerOverlay(SyncSetupOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('customize-sync')]);

<if expr="is_win">
  PageManager.registerOverlay(
      TriggeredResetProfileSettingsOverlay.getInstance(),
      BrowserOptions.getInstance());
</if>

  if (loadTimeData.getBoolean('showAbout')) {
    PageManager.registerOverlay(help.HelpPage.getInstance(),
                                BrowserOptions.getInstance());
    if (help.ChannelChangePage) {
      PageManager.registerOverlay(help.ChannelChangePage.getInstance(),
                                  help.HelpPage.getInstance());
    }
  }
  if (cr.isChromeOS) {
    PageManager.registerOverlay(AccountsOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('manage-accounts-button')]);
    PageManager.registerOverlay(BluetoothOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('bluetooth-add-device')]);
    PageManager.registerOverlay(BluetoothPairing.getInstance(),
                                BrowserOptions.getInstance());
    PageManager.registerOverlay(FactoryResetOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('factory-reset-restart')]);
    PageManager.registerOverlay(ChangePictureOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('account-picture')]);
    PageManager.registerOverlay(StorageClearDriveCacheOverlay.getInstance(),
                                StorageManager.getInstance());
    PageManager.registerOverlay(ConsumerManagementOverlay.getInstance(),
                                BrowserOptions.getInstance());
    PageManager.registerOverlay(DetailsInternetPage.getInstance(),
                                BrowserOptions.getInstance());
    PageManager.registerOverlay(DisplayOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('display-options')]);
    PageManager.registerOverlay(DisplayOverscan.getInstance(),
                                DisplayOptions.getInstance());
    PageManager.registerOverlay(KeyboardOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('keyboard-settings-button')]);
    PageManager.registerOverlay(PointerOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('pointer-settings-button')]);
    PageManager.registerOverlay(PreferredNetworks.getInstance(),
                                BrowserOptions.getInstance());
    PageManager.registerOverlay(PowerOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('power-settings-link')]);
    PageManager.registerOverlay(StorageManager.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('storage-manager-button')]);
    PageManager.registerOverlay(ThirdPartyImeConfirmOverlay.getInstance(),
                                LanguageOptions.getInstance());
    PageManager.registerOverlay(
        new ConfirmDialog(
            'arcOptOutConfirm',
            loadTimeData.getString('arcOptOutConfirmOverlayTabTitle'),
            'arc-opt-out-confirm-overlay',
            /** @type {HTMLButtonElement} */($('arc-opt-out-confirm-ok')),
            /** @type {HTMLButtonElement} */($('arc-opt-out-confirm-cancel')),
            $('android-apps-enabled')['pref'],
            $('android-apps-enabled')['metric'],
            undefined,
            false),
        BrowserOptions.getInstance());
  }

  if (!cr.isWindows && !cr.isMac) {
    PageManager.registerOverlay(CertificateBackupOverlay.getInstance(),
                                CertificateManager.getInstance());
    PageManager.registerOverlay(CertificateEditCaTrustOverlay.getInstance(),
                                CertificateManager.getInstance());
    PageManager.registerOverlay(CertificateImportErrorOverlay.getInstance(),
                                CertificateManager.getInstance());
    PageManager.registerOverlay(CertificateManager.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('certificatesManageButton')]);
    PageManager.registerOverlay(CertificateRestoreOverlay.getInstance(),
                                CertificateManager.getInstance());
  }

  OptionsFocusManager.getInstance().initialize();
  Preferences.getInstance().initialize();
  AutomaticSettingsResetBanner.getInstance().initialize();
  OptionsPage.initialize();
  PageManager.initialize(BrowserOptions.getInstance());
  PageManager.addObserver(new uber.PageManagerObserver());
  uber.onContentFrameLoaded();

  var pageName = PageManager.getPageNameFromPath();
  // Still update history so that chrome://settings/nonexistant redirects
  // appropriately to chrome://settings/. If the URL matches, updateHistory_
  // will avoid the extra replaceState.
  var updateHistory = true;
  PageManager.showPageByName(pageName, updateHistory,
                             {replaceState: true, hash: location.hash});

  var subpagesNavTabs = document.querySelectorAll('.subpages-nav-tabs');
  for (var i = 0; i < subpagesNavTabs.length; i++) {
    subpagesNavTabs[i].onclick = function(event) {
      OptionsPage.showTab(event.srcElement);
    };
  }

  window.setTimeout(function() {
    document.documentElement.classList.remove('loading');
    chrome.send('onFinishedLoadingOptions');
  }, 0);
}

document.documentElement.classList.add('loading');
document.addEventListener('DOMContentLoaded', load);

/**
 * Listener for the |beforeunload| event.
 */
window.onbeforeunload = function() {
  PageManager.willClose();
};

/**
 * Listener for the |popstate| event.
 * @param {Event} e The |popstate| event.
 */
window.onpopstate = function(e) {
  var pageName = PageManager.getPageNameFromPath();
  PageManager.setState(pageName, location.hash, e.state);
};
