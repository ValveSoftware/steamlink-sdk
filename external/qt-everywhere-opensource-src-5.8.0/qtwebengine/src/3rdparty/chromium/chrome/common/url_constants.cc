// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

#include <algorithm>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/features.h"
#include "content/public/common/url_constants.h"
#include "url/url_util.h"

namespace chrome {

#if defined(OS_CHROMEOS)
const char kCrosScheme[] = "cros";
#endif

#if defined(OS_ANDROID)
const char kAndroidAppScheme[] = "android-app";
#endif

// Add Chrome UI URLs as necessary, in alphabetical order.
// Be sure to add the corresponding kChromeUI*Host constant below.
// This is a WebUI page that lists other WebUI pages.
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIAppsURL[] = "chrome://apps/";
const char kChromeUIAppListStartPageURL[] = "chrome://app-list/";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUICertificateViewerDialogURL[] = "chrome://view-cert-dialog/";
const char kChromeUIChromeSigninURL[] = "chrome://chrome-signin/";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUIComponentsURL[] = "chrome://components/";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUICrashesURL[] = "chrome://crashes/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDevToolsURL[] =
    "chrome-devtools://devtools/bundled/inspector.html";
const char kChromeUIDomainReliabilityInternalsURL[] =
    "chrome://domain-reliability-internals/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionsFrameURL[] = "chrome://extensions-frame/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFallbackIconURL[] = "chrome://fallback-icon/";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFeedbackURL[] = "chrome://feedback/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIFlashURL[] = "chrome://flash/";
const char kChromeUIGCMInternalsURL[] = "chrome://gcm-internals/";
const char kChromeUIHelpFrameURL[] = "chrome://help-frame/";
const char kChromeUIHelpURL[] = "chrome://help/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIHistoryFrameURL[] = "chrome://history-frame/";
const char kChromeUIIdentityInternalsURL[] = "chrome://identity-internals/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInstantURL[] = "chrome://instant/";
const char kChromeUIInterstitialURL[] = "chrome://interstitials/";
const char kChromeUIInvalidationsURL[] = "chrome://invalidations/";
const char kChromeUILargeIconURL[] = "chrome://large-icon/";
const char kChromeUIMdPolicyURL[] = "chrome://md-policy/";
const char kChromeUIMdSettingsURL[] = "chrome://md-settings/";
const char kChromeUINaClURL[] = "chrome://nacl/";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINewProfileURL[] = "chrome://newprofile/";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUIOmniboxURL[] = "chrome://omnibox/";
const char kChromeUIPluginsURL[] = "chrome://plugins/";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUIProfileSigninConfirmationURL[] =
    "chrome://profile-signin-confirmation/";
const char kChromeUIUserManagerURL[] = "chrome://user-manager/";
const char kChromeUIMdUserManagerUrl[] = "chrome://md-user-manager/";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUIQuitURL[] = "chrome://quit/";
const char kChromeUIRestartURL[] = "chrome://restart/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUIContentSettingsURL[] = "chrome://settings/content";
const char kChromeUISettingsFrameURL[] = "chrome://settings-frame/";
const char kChromeUISuggestionsURL[] = "chrome://suggestions/";
const char kChromeUISupervisedUserPassphrasePageURL[] =
    "chrome://managed-user-passphrase/";
const char kChromeUISyncConfirmationURL[] = "chrome://sync-confirmation/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUIThumbnailListURL[] = "chrome://thumbnails/";
const char kChromeUIUberFrameURL[] = "chrome://uber-frame/";
const char kChromeUIUserActionsURL[] = "chrome://user-actions/";
const char kChromeUIVersionURL[] = "chrome://version/";

#if BUILDFLAG(ANDROID_JAVA_UI)
const char kChromeUIContextualSearchPromoURL[] =
    "chrome://contextual-search-promo";
const char kChromeUINativeScheme[] = "chrome-native";
const char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
const char kChromeUINativeBookmarksURL[] = "chrome-native://bookmarks/";
const char kChromeUINativePhysicalWebURL[] = "chrome-native://physical-web/";
const char kChromeUINativeRecentTabsURL[] = "chrome-native://recent-tabs/";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager/";
const char kChromeUIChooseMobileNetworkURL[] =
    "chrome://choose-mobile-network/";
const char kChromeUIDeviceEmulatorURL[] = "chrome://device-emulator/";
const char kChromeUIFirstRunURL[] = "chrome://first-run/";
const char kChromeUIKeyboardOverlayURL[] = "chrome://keyboardoverlay/";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUINfcDebugURL[] = "chrome://nfc-debug/";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIProxySettingsURL[] = "chrome://proxy-settings/";
const char kChromeUIScreenlockIconURL[] = "chrome://screenlock-icon/";
const char kChromeUISetTimeURL[] = "chrome://set-time/";
const char kChromeUISimUnlockURL[] = "chrome://sim-unlock/";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISystemInfoURL[] = "chrome://system/";
const char kChromeUITermsOemURL[] = "chrome://terms/oem";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const char kChromeUIMetroFlowURL[] = "chrome://make-metro/";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogURL[] =
    "chrome://tab-modal-confirm-dialog/";
#endif

#if !defined(OS_ANDROID)
const char kChromeUICopresenceURL[] = "chrome://copresence/";
#endif

#if defined(ENABLE_WEBRTC)
const char kChromeUIWebRtcLogsURL[] = "chrome://webrtc-logs/";
#endif

#if defined(ENABLE_MEDIA_ROUTER)
const char kChromeUIMediaRouterURL[] = "chrome://media-router/";
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUICastURL[] = "chrome://cast/";
#endif
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
#endif

// Add Chrome UI hosts here, in alphabetical order.
// Add hosts to kChromePaths in browser_about_handler.cc to be listed by
// chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.
const char kChromeUIAboutHost[] = "about";
const char kChromeUIBlankHost[] = "blank";
const char kChromeUIAppLauncherPageHost[] = "apps";
const char kChromeUIAppListStartPageHost[] = "app-list";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUICacheHost[] = "cache";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUICertificateViewerDialogHost[] = "view-cert-dialog";
const char kChromeUIChromeSigninHost[] = "chrome-signin";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConstrainedHTMLTestHost[] = "constrained-test";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDelayedHangUIHost[] = "delayeduithreadhang";
const char kChromeUIDeviceLogHost[] = "device-log";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDevToolsBundledPath[] = "bundled";
const char kChromeUIDevToolsRemotePath[] = "remote";
const char kChromeUIDNSHost[] = "dns";
const char kChromeUIDomainReliabilityInternalsHost[] =
    "domain-reliability-internals";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIDriveInternalsHost[] = "drive-internals";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionsFrameHost[] = "extensions-frame";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFallbackIconHost[] = "fallback-icon";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFeedbackHost[] = "feedback";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlashHost[] = "flash";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHangHost[] = "hang";
const char kChromeUIHangUIHost[] = "uithreadhang";
const char kChromeUIHelpFrameHost[] = "help-frame";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistoryFrameHost[] = "history-frame";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInstantHost[] = "instant";
const char kChromeUIInterstitialHost[] = "interstitials";
const char kChromeUIInvalidationsHost[] = "invalidations";
const char kChromeUIKillHost[] = "kill";
const char kChromeUILargeIconHost[] = "large-icon";
const char kChromeUILocalStateHost[] = "local-state";
const char kChromeUIMdPolicyHost[] = "md-policy";
const char kChromeUIMdSettingsHost[] = "md-settings";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPluginsHost[] = "plugins";
const char kChromeUIComponentsHost[] = "components";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIProfileSigninConfirmationHost[] =
    "profile-signin-confirmation";
const char kChromeUIUserManagerHost[] = "user-manager";
const char kChromeUIMdUserManagerHost[] = "md-user-manager";
const char kChromeUIPredictorsHost[] = "predictors";
const char kChromeUIProfilerHost[] = "profiler";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIQuitHost[] = "quit";
const char kChromeUIRestartHost[] = "restart";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISettingsFrameHost[] = "settings-frame";
const char kChromeUIShorthangHost[] = "shorthang";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISiteEngagementHost[] = "site-engagement";
const char kChromeUISuggestionsHost[] = "suggestions";
const char kChromeUISupervisedUserInternalsHost[] = "supervised-user-internals";
const char kChromeUISupervisedUserPassphrasePageHost[] =
    "managed-user-passphrase";
const char kChromeUISyncConfirmationHost[] = "sync-confirmation";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUITermsHost[] = "terms";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUIThumbnailHost2[] = "thumb2";
const char kChromeUIThumbnailListHost[] = "thumbnails";
const char kChromeUITouchIconHost[] = "touch-icon";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUberFrameHost[] = "uber-frame";
const char kChromeUIUberHost[] = "chrome";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIWorkersHost[] = "workers";

const char kChromeUIThemePath[] = "theme";

#if defined(ENABLE_PRINT_PREVIEW)
const char kChromeUIPrintHost[] = "print";
#endif  // ENABLE_PRINT_PREVIEW

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_ANDROID)
const char kChromeUIContextualSearchPromoHost[] = "contextual-search-promo";
const char kChromeUIOfflineInternalsHost[] = "offline-internals";
const char kChromeUIPhysicalWebHost[] = "physical-web";
const char kChromeUIPopularSitesInternalsHost[] = "popular-sites-internals";
const char kChromeUISnippetsInternalsHost[] = "snippets-internals";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIAppLaunchHost[] = "app-launch";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUICertificateManagerHost[] = "certificate-manager";
const char kChromeUIChooseMobileNetworkHost[] = "choose-mobile-network";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDeviceEmulatorHost[] = "device-emulator";
const char kChromeUIFirstRunHost[] = "first-run";
const char kChromeUIKeyboardOverlayHost[] = "keyboardoverlay";
const char kChromeUILoginContainerHost[] = "login-container";
const char kChromeUILoginHost[] = "login";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUINfcDebugHost[] = "nfc-debug";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIPowerHost[] = "power";
const char kChromeUIProxySettingsHost[] = "proxy-settings";
const char kChromeUIRotateHost[] = "rotate";
const char kChromeUIScreenlockIconHost[] = "screenlock-icon";
const char kChromeUISetTimeHost[] = "set-time";
const char kChromeUISimUnlockHost[] = "sim-unlock";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISlowTraceHost[] = "slow_trace";
const char kChromeUIUserImageHost[] = "userimage";
const char kChromeUIVoiceSearchHost[] = "voicesearch";

const char kEULAPathFormat[] = "/usr/share/chromeos-assets/eula/%s/eula.html";
const char kOemEulaURLPath[] = "oem";
const char kOnlineEulaURLPath[] =
    "https://www.google.com/intl/%s/chrome/eula_text.html";

const char kChromeOSCreditsPath[] =
    "/opt/google/chrome/resources/about_os_credits.html";

const char kChromeOSAssetHost[] = "chromeos-asset";
const char kChromeOSAssetPath[] = "/usr/share/chromeos-assets/";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const char kChromeUIMetroFlowHost[] = "make-metro";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogHost[] = "tab-modal-confirm-dialog";
#endif

#if !defined(OS_ANDROID)
const char kChromeUICopresenceHost[] = "copresence";
#endif

#if defined(ENABLE_WEBRTC)
const char kChromeUIWebRtcLogsHost[] = "webrtc-logs";
#endif

#if defined(ENABLE_MEDIA_ROUTER)
const char kChromeUIMediaRouterHost[] = "media-router";
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUICastHost[] = "cast";
#endif
#endif

// Option sub pages.
// Add sub page paths to kChromeSettingsSubPages in builtin_provider.cc to be
// listed by the built-in AutocompleteProvider.
const char kAutofillSubPage[] = "autofill";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kContentSettingsExceptionsSubPage[] = "contentExceptions";
const char kContentSettingsSubPage[] = "content";
const char kCreateProfileSubPage[] = "createProfile";
const char kExtensionsSubPage[] = "extensions";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kLanguageOptionsSubPage[] = "languages";
const char kManageProfileSubPage[] = "manageProfile";
const char kPasswordManagerSubPage[] = "passwords";
const char kPowerOptionsSubPage[] = "power-overlay";
const char kResetProfileSettingsSubPage[] = "resetProfileSettings";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSearchSubPage[] = "search";
const char kSearchUsersSubPage[] = "search#Users";
const char kSyncSetupSubPage[] = "syncSetup";
const char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";
#if defined(OS_CHROMEOS)
const char kInternetOptionsSubPage[] = "internet";
const char kChangeProfilePictureSubPage[] = "changePicture";
#endif

// Extension sub pages.
const char kExtensionConfigureCommandsSubPage[] = "configureCommands";

const char kExtensionInvalidRequestURL[] = "chrome-extension://invalid/";
const char kExtensionResourceInvalidRequestURL[] =
    "chrome-extension-resource://invalid/";

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kGoogleAccountActivityControlsURL[] =
    "https://myaccount.google.com/activitycontrols/search";

const char kPasswordManagerLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_password";
#else
    "https://support.google.com/chrome/?p=settings_password";
#endif

const char kUpgradeHelpCenterBaseURL[] =
    "https://support.google.com/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

const char kSmartLockHelpPage[] =
    "https://support.google.com/accounts/answer/6197437";

const char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_settings_api_extension";

const char kChromeHelpViaKeyboardURL[] =
#if defined(OS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=keyboard";
#endif  // defined(OFFICIAL_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=keyboard";
#endif  // defined(OS_CHROMEOS)

const char kChromeHelpViaMenuURL[] =
#if defined(OS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=menu";
#endif  // defined(OFFICIAL_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=menu";
#endif  // defined(OS_CHROMEOS)

const char kChromeHelpViaWebUIURL[] =
#if defined(OS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=settings";
#endif  // defined(OFFICIAL_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=settings";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
const char kChromeAccessibilityHelpURL[] =
    "https://support.google.com/chromebook/?p=accessibility_menu";
const char kChromeAccessibilitySettingsURL[] =
    "/chromevox/background/options.html";
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
const char kChromeSyncLearnMoreURL[] =
    "https://support.google.com/chrome/answer/165139";

const char kChromeSyncMergeTroubleshootingURL[] =
    "https://support.google.com/chrome/answer/1181420#merge";
#endif  // BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)

#if defined(OS_MACOSX)
const char kChromeEnterpriseSignInLearnMoreURL[] =
    "https://support.google.com/chromebook/answer/1331549";
#endif

const char kResetProfileSettingsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_reset_settings";

const char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_automatic_settings_reset";

const char kLegacySupervisedUserManagementURL[] =
    "https://www.chrome.com/manage";
const char kLegacySupervisedUserManagementDisplayURL[] =
    "www.chrome.com/manage";

const char kSettingsSearchHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_search_help";
#else
    "https://support.google.com/chrome/?p=settings_search_help";
#endif

const char kOmniboxLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_omnibox";
#else
    "https://support.google.com/chrome/?p=settings_omnibox";
#endif

const char kPageInfoHelpCenterURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=ui_security_indicator";
#else
    "https://support.google.com/chrome/?p=ui_security_indicator";
#endif

const char kCrashReasonURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=e_awsnap";
#else
    "https://support.google.com/chrome/?p=e_awsnap";
#endif

const char kCrashReasonFeedbackDisplayedURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=e_awsnap_rl";
#else
    "https://support.google.com/chrome/?p=e_awsnap_rl";
#endif

const char kPrivacyLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_privacy";
#else
    "https://support.google.com/chrome/?p=settings_privacy";
#endif

const char kDoNotTrackLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_do_not_track";
#else
    "https://support.google.com/chrome/?p=settings_do_not_track";
#endif

#if defined(OS_CHROMEOS)
const char kAttestationForContentProtectionLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=verified_access";
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
const char kEnhancedPlaybackNotificationLearnMoreURL[] =
#endif
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=enhanced_playback";
#elif defined(OS_ANDROID)
// Keep in sync with chrome/android/java/strings/android_chrome_strings.grd
    "https://support.google.com/chrome/?p=mobile_protected_content";
#endif

const char kChromiumProjectURL[] = "https://www.chromium.org/";

const char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome/?p=ui_usagestat";

#if defined(ENABLE_PLUGIN_INSTALLATION)
const char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_outdated_plugin";
#endif

const char kBlockedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_blocked_plugin";

const char kHotwordLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_hotword_search";

const char kManageAudioHistoryURL[] =
    "https://history.google.com/history/audio";

const char kLearnMoreRegisterProtocolHandlerURL[] =
    "https://support.google.com/chrome/?p=ib_protocol_handler";

const char kSyncLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sign_in";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_blocked";

const char kDownloadInterruptedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_download_errors";

const char kSyncEverythingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sync_all";

const char kCloudPrintLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_cloud_print";
#else
    "https://support.google.com/chrome/?p=settings_cloud_print";
#endif

const char kCloudPrintNoDestinationsLearnMoreURL[] =
    "https://www.google.com/cloudprint/learn/";

const char kAppLauncherHelpURL[] =
    "https://support.google.com/chrome_webstore/?p=cws_app_launcher";

const char kSyncEncryptionHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_encryption";
#else
    "https://support.google.com/chrome/?p=settings_encryption";
#endif

const char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome/?p=settings_sync_error";

#if defined(OS_CHROMEOS)
const char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromebook/?p=simple_scrolling";
#endif

#if defined(OS_CHROMEOS)
const char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromebook/answer/2535613";
#endif

const char kRemoveNonCWSExtensionURL[] =
    "https://support.google.com/chrome/?p=ui_remove_non_cws_extensions";

#if defined(OS_WIN)
const char kNotificationsHelpURL[] =
    "https://support.google.com/chrome/?p=ui_notifications";
#endif

const char kNotificationWelcomeLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_google_now_welcome";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
const char* const kChromeHostURLs[] = {
  kChromeUICacheHost,
  kChromeUIChromeURLsHost,
  kChromeUIComponentsHost,
  kChromeUICrashesHost,
  kChromeUICreditsHost,
  kChromeUIDeviceLogHost,
  kChromeUIDNSHost,
  kChromeUIFlagsHost,
  kChromeUIGCMInternalsHost,
  kChromeUIHistoryHost,
  kChromeUIInvalidationsHost,
  kChromeUILocalStateHost,
  kChromeUINetInternalsHost,
  kChromeUINewTabHost,
  kChromeUIOmniboxHost,
  kChromeUIPasswordManagerInternalsHost,
  kChromeUIPolicyHost,
  kChromeUIPredictorsHost,
  kChromeUIProfilerHost,
  kChromeUISignInInternalsHost,
  kChromeUISuggestionsHost,
  kChromeUISupervisedUserInternalsHost,
  kChromeUISyncInternalsHost,
  kChromeUITermsHost,
  kChromeUIThumbnailListHost,
  kChromeUITranslateInternalsHost,
  kChromeUIUserActionsHost,
  kChromeUIVersionHost,
  content::kChromeUIAccessibilityHost,
  content::kChromeUIAppCacheInternalsHost,
  content::kChromeUIBlobInternalsHost,
  content::kChromeUIGpuHost,
  content::kChromeUIHistogramHost,
  content::kChromeUIIndexedDBInternalsHost,
  content::kChromeUIMediaInternalsHost,
  content::kChromeUINetworkErrorHost,
  content::kChromeUINetworkErrorsListingHost,
  content::kChromeUINetworkViewCacheHost,
  content::kChromeUIServiceWorkerInternalsHost,
  content::kChromeUITracingHost,
  content::kChromeUIWebRTCInternalsHost,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
  kChromeUIAppLauncherPageHost,
#endif
  kChromeUIBookmarksHost,
  kChromeUIDownloadsHost,
  kChromeUIFlashHost,
  kChromeUIHelpHost,
  kChromeUIInspectHost,
  kChromeUIPluginsHost,
  kChromeUIQuotaInternalsHost,
  kChromeUISettingsHost,
  kChromeUISystemInfoHost,
  kChromeUIUberHost,
#endif
#if defined(OS_ANDROID)
  kChromeUINetExportHost,
  kChromeUIOfflineInternalsHost,
  kChromeUIPopularSitesInternalsHost,
  kChromeUISnippetsInternalsHost,
#else  // non-mobile
  kChromeUICopresenceHost,
#endif
#if defined(OS_CHROMEOS)
  kChromeUICertificateManagerHost,
  kChromeUIChooseMobileNetworkHost,
  kChromeUICryptohomeHost,
  kChromeUIDriveInternalsHost,
  kChromeUIFirstRunHost,
  kChromeUIKeyboardOverlayHost,
  kChromeUILoginHost,
  kChromeUINetworkHost,
  kChromeUIOobeHost,
  kChromeUIOSCreditsHost,
  kChromeUIPowerHost,
  kChromeUIProxySettingsHost,
  kChromeUIVoiceSearchHost,
#endif
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  kChromeUIDiscardsHost,
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  kChromeUILinuxProxyConfigHost,
  kChromeUISandboxHost,
#endif
#if defined(OS_WIN)
  kChromeUIConflictsHost,
#endif
#if !defined(DISABLE_NACL)
  kChromeUINaClHost,
#endif
#if defined(ENABLE_EXTENSIONS)
  kChromeUIExtensionsHost,
#endif
#if defined(ENABLE_PRINT_PREVIEW)
  kChromeUIPrintHost,
#endif
#if defined(ENABLE_SERVICE_DISCOVERY)
  kChromeUIDevicesHost,
#endif
#if defined(ENABLE_WEBRTC)
  kChromeUIWebRtcLogsHost,
#endif
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);

const char* const kChromeDebugURLs[] = {
  content::kChromeUIBadCastCrashURL,
  content::kChromeUICrashURL,
  content::kChromeUIDumpURL,
  content::kChromeUIKillURL,
  content::kChromeUIHangURL,
  content::kChromeUIShorthangURL,
  content::kChromeUIGpuCleanURL,
  content::kChromeUIGpuCrashURL,
  content::kChromeUIGpuHangURL,
  content::kChromeUIPpapiFlashCrashURL,
  content::kChromeUIPpapiFlashHangURL,
  chrome::kChromeUIQuitURL,
  chrome::kChromeUIRestartURL
};
const int kNumberOfChromeDebugURLs =
    static_cast<int>(arraysize(kChromeDebugURLs));

const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSearchScheme[] = "chrome-search";
const char kChromeSearchLocalNtpHost[] = "local-ntp";
const char kChromeSearchLocalNtpUrl[] =
    "chrome-search://local-ntp/local-ntp.html";
const char kChromeSearchRemoteNtpHost[] = "remote-ntp";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

const char kEasyUnlockLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=smart_lock";

// Google SafeSearch query parameters.
const char kSafeSearchSafeParameter[] = "safe=active";
const char kSafeSearchSsuiParameter[] = "ssui=on";

const char kMediaAccessLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ib_access_cam_mic";

const char kLanguageSettingsLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/answer/1059490";
#else
    "https://support.google.com/chrome/topic/1678461";
#endif

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
const char kLinuxWheezyPreciseDeprecationURL[] =
    "https://support.google.com/chrome/answer/95346";
#endif

#if defined(OS_MACOSX)
// TODO(mark): Change to a Help Center URL when one is available.
// https://crbug.com/555044
const char kMac10_678_DeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";
#endif

#if defined(OS_WIN)
const char kWindowsXPVistaDeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";
#endif

const char kChooserBluetoothOverviewURL[] =
    "https://support.google.com/chrome?p=bluetooth";

const char kChooserUsbOverviewURL[] =
    "https://support.google.com/chrome?p=webusb";

#if defined(OS_CHROMEOS)
const char kEolNotificationURL[] = "https://www.google.com/chromebook/older/";
#endif

}  // namespace chrome
