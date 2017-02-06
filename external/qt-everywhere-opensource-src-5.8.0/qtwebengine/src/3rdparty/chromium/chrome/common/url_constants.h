// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/common/features.h"
#include "content/public/common/url_constants.h"

namespace chrome {

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIAboutURL[];
extern const char kChromeUIAppsURL[];
extern const char kChromeUIAppListStartPageURL[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUICertificateViewerURL[];
extern const char kChromeUICertificateViewerDialogURL[];
extern const char kChromeUIChromeSigninURL[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUIComponentsURL[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUICrashesURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIDevicesURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDomainReliabilityInternalsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionsFrameURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFallbackIconURL[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFeedbackURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFlashURL[];
extern const char kChromeUIGCMInternalsURL[];
extern const char kChromeUIHelpFrameURL[];
extern const char kChromeUIHelpURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIHistoryFrameURL[];
extern const char kChromeUIIdentityInternalsURL[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIInstantURL[];
extern const char kChromeUIInterstitialURL[];
extern const char kChromeUIInvalidationsURL[];
extern const char kChromeUILargeIconURL[];
extern const char kChromeUIMdPolicyURL[];
extern const char kChromeUINaClURL[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINewProfileURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIOmniboxURL[];
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIProfileSigninConfirmationURL[];
extern const char kChromeUIMdUserManagerUrl[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUIQuitURL[];
extern const char kChromeUIRestartURL[];
extern const char kChromeUIMdSettingsURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUIContentSettingsURL[];
extern const char kChromeUISettingsFrameURL[];
extern const char kChromeUISiteEngagementHost[];
extern const char kChromeUISuggestionsURL[];
extern const char kChromeUISupervisedUserPassphrasePageURL[];
extern const char kChromeUISyncConfirmationURL[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThemeURL[];
extern const char kChromeUIThumbnailURL[];
extern const char kChromeUIThumbnailListURL[];
extern const char kChromeUIUberFrameURL[];
extern const char kChromeUIUserActionsURL[];
extern const char kChromeUIUserManagerURL[];
extern const char kChromeUIVersionURL[];

#if BUILDFLAG(ANDROID_JAVA_UI)
extern const char kChromeUIContextualSearchPromoURL[];
extern const char kChromeUINativeScheme[];
extern const char kChromeUINativeNewTabURL[];
extern const char kChromeUINativeBookmarksURL[];
extern const char kChromeUINativePhysicalWebURL[];
extern const char kChromeUINativeRecentTabsURL[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIBluetoothPairingURL[];
extern const char kChromeUICertificateManagerDialogURL[];
extern const char kChromeUIChooseMobileNetworkURL[];
extern const char kChromeUIDeviceEmulatorURL[];
extern const char kChromeUIFirstRunURL[];
extern const char kChromeUIKeyboardOverlayURL[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUINfcDebugURL[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIProxySettingsURL[];
extern const char kChromeUIScreenlockIconURL[];
extern const char kChromeUISetTimeURL[];
extern const char kChromeUISimUnlockURL[];
extern const char kChromeUISlowURL[];
extern const char kChromeUISystemInfoURL[];
extern const char kChromeUITermsOemURL[];
extern const char kChromeUIUserImageURL[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
extern const char kChromeUIMetroFlowURL[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogURL[];
#endif

#if !defined(OS_ANDROID)
extern const char kChromeUICopresenceURL[];
extern const char kChromeUICopresenceHost[];
#endif

#if defined(ENABLE_WEBRTC)
extern const char kChromeUIWebRtcLogsURL[];
#endif

#if defined(ENABLE_MEDIA_ROUTER)
extern const char kChromeUIMediaRouterURL[];
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
extern const char kChromeUICastURL[];
#endif
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
extern const char kChromeUIDiscardsHost[];
extern const char kChromeUIDiscardsURL[];
#endif

// chrome components of URLs. Should be kept in sync with the full URLs above.
extern const char kChromeUIAboutHost[];
extern const char kChromeUIAboutPageFrameHost[];
extern const char kChromeUIBlankHost[];
extern const char kChromeUIAppLauncherPageHost[];
extern const char kChromeUIAppListStartPageHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUICacheHost[];
extern const char kChromeUICertificateViewerHost[];
extern const char kChromeUICertificateViewerDialogHost[];
extern const char kChromeUIChromeSigninHost[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUIConstrainedHTMLTestHost[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICrashHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUIDefaultHost[];
extern const char kChromeUIDelayedHangUIHost[];
extern const char kChromeUIDeviceLogHost[];
extern const char kChromeUIDevicesHost[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDevToolsBundledPath[];
extern const char kChromeUIDevToolsRemotePath[];
extern const char kChromeUIDNSHost[];
extern const char kChromeUIDomainReliabilityInternalsHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIDriveInternalsHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionsFrameHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFallbackIconHost[];
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFeedbackHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlashHost[];
extern const char kChromeUIGCMInternalsHost[];
extern const char kChromeUIHelpFrameHost[];
extern const char kChromeUIHelpHost[];
extern const char kChromeUIHangHost[];
extern const char kChromeUIHangUIHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIHistoryFrameHost[];
extern const char kChromeUIIdentityInternalsHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIInstantHost[];
extern const char kChromeUIInterstitialHost[];
extern const char kChromeUIInvalidationsHost[];
extern const char kChromeUIKillHost[];
extern const char kChromeUILargeIconHost[];
extern const char kChromeUILocalStateHost[];
extern const char kChromeUIMdPolicyHost[];
extern const char kChromeUIMdSettingsHost[];
extern const char kChromeUINaClHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIOfflineInternalsHost[];
extern const char kChromeUIOmniboxHost[];
extern const char kChromeUIPluginsHost[];
extern const char kChromeUIComponentsHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIProfileSigninConfirmationHost[];
extern const char kChromeUIUserManagerHost[];
extern const char kChromeUIMdUserManagerHost[];
extern const char kChromeUIPredictorsHost[];
extern const char kChromeUIProfilerHost[];
extern const char kChromeUIQuotaInternalsHost[];
extern const char kChromeUIQuitHost[];
extern const char kChromeUIRestartHost[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUISettingsFrameHost[];
extern const char kChromeUIShorthangHost[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISuggestionsHost[];
extern const char kChromeUISupervisedUserInternalsHost[];
extern const char kChromeUISupervisedUserPassphrasePageHost[];
extern const char kChromeUISyncConfirmationHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncFileSystemInternalsHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUIThemeHost[];
extern const char kChromeUIThumbnailHost[];
extern const char kChromeUIThumbnailHost2[];
extern const char kChromeUIThumbnailListHost[];
extern const char kChromeUITouchIconHost[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIUberFrameHost[];
extern const char kChromeUIUberHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIWorkersHost[];

extern const char kChromeUIThemePath[];

#if defined(ENABLE_PRINT_PREVIEW)
extern const char kChromeUIPrintHost[];
#endif  // ENABLE_PRINT_PREVIEW

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
extern const char kChromeUILinuxProxyConfigHost[];
extern const char kChromeUISandboxHost[];
#endif

#if defined(OS_ANDROID)
extern const char kChromeUIContextualSearchPromoHost[];
extern const char kChromeUIOfflineInternalsURL[];
extern const char kChromeUIPhysicalWebHost[];
extern const char kChromeUIPopularSitesInternalsHost[];
extern const char kChromeUISnippetsInternalsHost[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIAppLaunchHost[];
extern const char kChromeUIBluetoothPairingHost[];
extern const char kChromeUICertificateManagerHost[];
extern const char kChromeUIChooseMobileNetworkHost[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUIDeviceEmulatorHost[];
extern const char kChromeUIFirstRunHost[];
extern const char kChromeUIKeyboardOverlayHost[];
extern const char kChromeUILoginContainerHost[];
extern const char kChromeUILoginHost[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUINetworkHost[];
extern const char kChromeUINfcDebugHost[];
extern const char kChromeUIOobeHost[];
extern const char kChromeUIOSCreditsHost[];
extern const char kChromeUIPowerHost[];
extern const char kChromeUIProxySettingsHost[];
extern const char kChromeUIRotateHost[];
extern const char kChromeUIScreenlockIconHost[];
extern const char kChromeUISetTimeHost[];
extern const char kChromeUISimUnlockHost[];
extern const char kChromeUISlowHost[];
extern const char kChromeUISlowTraceHost[];
extern const char kChromeUIUserImageHost[];
extern const char kChromeUIVoiceSearchHost[];

extern const char kEULAPathFormat[];
extern const char kOemEulaURLPath[];
extern const char kOnlineEulaURLPath[];

extern const char kChromeOSCreditsPath[];

extern const char kChromeOSAssetHost[];
extern const char kChromeOSAssetPath[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
extern const char kChromeUIMetroFlowHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogHost[];
#endif

#if defined(ENABLE_WEBRTC)
extern const char kChromeUIWebRtcLogsHost[];
#endif

#if defined(ENABLE_MEDIA_ROUTER)
extern const char kChromeUIMediaRouterHost[];
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
extern const char kChromeUICastHost[];
#endif
#endif

// Options sub-pages.
extern const char kAutofillSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsExceptionsSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kCreateProfileSubPage[];
extern const char kExtensionsSubPage[];
extern const char kHandlerSettingsSubPage[];
extern const char kImportDataSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kPowerOptionsSubPage[];
extern const char kResetProfileSettingsSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSearchSubPage[];
extern const char kSearchUsersSubPage[];
extern const char kSyncSetupSubPage[];
extern const char kTriggeredResetProfileSettingsSubPage[];
#if defined(OS_CHROMEOS)
extern const char kInternetOptionsSubPage[];
extern const char kChangeProfilePictureSubPage[];
#endif

// Extensions sub pages.
extern const char kExtensionConfigureCommandsSubPage[];

// URLs used to indicate that an extension resource load request
// was invalid.
extern const char kExtensionInvalidRequestURL[];
extern const char kExtensionResourceInvalidRequestURL[];

extern const char kSyncGoogleDashboardURL[];

// URL of the 'Activity controls' section of the privacy settings page.
extern const char kGoogleAccountActivityControlsURL[];

extern const char kPasswordManagerLearnMoreURL[];
extern const char kUpgradeHelpCenterBaseURL[];
extern const char kSmartLockHelpPage[];

// "Learn more" URL for the Settings API, NTP bubble and other settings bubbles
// showing which extension is controlling them.
extern const char kExtensionControlledSettingLearnMoreURL[];

// General help links for Chrome, opened using various actions.
extern const char kChromeHelpViaKeyboardURL[];
extern const char kChromeHelpViaMenuURL[];
extern const char kChromeHelpViaWebUIURL[];

#if defined(OS_CHROMEOS)
// Accessibility help link for Chrome.
extern const char kChromeAccessibilityHelpURL[];
// Accessibility settings link for Chrome.
extern const char kChromeAccessibilitySettingsURL[];
#endif

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
// "Learn more" URL for the one click signin infobar.
extern const char kChromeSyncLearnMoreURL[];

// "Learn more" URL for the "Sign in with a different account" confirmation
// dialog.
extern const char kChromeSyncMergeTroubleshootingURL[];
#endif

#if defined(OS_MACOSX)
// "Learn more" URL for the enterprise sign-in confirmation dialog.
extern const char kChromeEnterpriseSignInLearnMoreURL[];
#endif

// "Learn more" URL for resetting profile preferences.
extern const char kResetProfileSettingsLearnMoreURL[];

// "Learn more" URL for when profile settings are automatically reset.
extern const char kAutomaticSettingsResetLearnMoreURL[];

// Management URL for Chrome Supervised Users.
extern const char kLegacySupervisedUserManagementURL[];

// Management URL for Chrome Supervised Users - version without scheme, used
// for display.
extern const char kLegacySupervisedUserManagementDisplayURL[];

// Help URL for the settings page's search feature.
extern const char kSettingsSearchHelpURL[];

// Help URL for the Omnibox setting.
extern const char kOmniboxLearnMoreURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
extern const char kCrashReasonURL[];

// "Learn more" URL for "Aw snap" page when showing "Send feedback" button.
extern const char kCrashReasonFeedbackDisplayedURL[];

// "Learn more" URL for killed tab page.
extern const char kKillReasonURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// "Learn more" URL for the "Do not track" setting in the privacy section.
extern const char kDoNotTrackLearnMoreURL[];

#if defined(OS_CHROMEOS)
// "Learn more" URL for the attestation of content protection setting.
extern const char kAttestationForContentProtectionLearnMoreURL[];
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
// "Learn more" URL for the enhanced playback notification dialog.
extern const char kEnhancedPlaybackNotificationLearnMoreURL[];
#endif

// The URL for the Chromium project used in the About dialog.
extern const char kChromiumProjectURL[];

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
extern const char kLearnMoreReportingURL[];

#if defined(ENABLE_PLUGIN_INSTALLATION)
// The URL for the "Learn more" page for the outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];
#endif

// The URL for the "Learn more" page for the blocked plugin infobar.
extern const char kBlockedPluginLearnMoreURL[];

// The URL for the "Learn more" page for hotword search voice trigger.
extern const char kHotwordLearnMoreURL[];

// The URL for managing a user's audio history.
extern const char kManageAudioHistoryURL[];

// The URL for the "Learn more" page for register protocol handler infobars.
extern const char kLearnMoreRegisterProtocolHandlerURL[];

// The URL for the "Learn more" page for sync setup on the personal stuff page.
extern const char kSyncLearnMoreURL[];

// The URL for the "Learn more" page for download scanning.
extern const char kDownloadScanningLearnMoreURL[];

// The URL for the "Learn more" page for interrupted downloads.
extern const char kDownloadInterruptedLearnMoreURL[];

// The URL for the "Learn more" page on the sync setup dialog, when syncing
// everything.
extern const char kSyncEverythingLearnMoreURL[];

// The URL for information on how to use the app launcher.
extern const char kAppLauncherHelpURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// The URL for the "Learn more" link when there is a sync error.
extern const char kSyncErrorsHelpURL[];

#if defined(OS_CHROMEOS)
// The URL for the "Learn more" link for natural scrolling on ChromeOS.
extern const char kNaturalScrollHelpURL[];

// The URL for the Learn More page about enterprise enrolled devices.
extern const char kLearnMoreEnterpriseURL[];
#endif

// The URL for the Learn More link of the non-CWS bubble.
extern const char kRemoveNonCWSExtensionURL[];

#if defined(OS_WIN)
extern const char kNotificationsHelpURL[];
#endif

// The Welcome Notification More Info URL.
extern const char kNotificationWelcomeLearnMoreURL[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern const int kNumberOfChromeDebugURLs;

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
extern const char kChromeNativeScheme[];

// The chrome-search: scheme is served by the same backend as chrome:.  However,
// only specific URLDataSources are enabled to serve requests via the
// chrome-search: scheme.  See |InstantIOContext::ShouldServiceRequest| and its
// callers for details.  Note that WebUIBindings should never be granted to
// chrome-search: pages.  chrome-search: pages are displayable but not readable
// by external search providers (that are rendered by Instant renderer
// processes), and neither displayable nor readable by normal (non-Instant) web
// pages.  To summarize, a non-Instant process, when trying to access
// 'chrome-search://something', will bump up against the following:
//
//  1. Renderer: The display-isolated check in WebKit will deny the request,
//  2. Browser: Assuming they got by #1, the scheme checks in
//     URLDataSource::ShouldServiceRequest will deny the request,
//  3. Browser: for specific sub-classes of URLDataSource, like ThemeSource
//     there are additional Instant-PID checks that make sure the request is
//     coming from a blessed Instant process, and deny the request.
extern const char kChromeSearchScheme[];

// Pages under chrome-search.
extern const char kChromeSearchLocalNtpHost[];
extern const char kChromeSearchLocalNtpUrl[];
extern const char kChromeSearchRemoteNtpHost[];

// Host and URL for most visited iframes used on the Instant Extended NTP.
extern const char kChromeSearchMostVisitedHost[];
extern const char kChromeSearchMostVisitedUrl[];

#if defined(OS_WIN) || defined(OS_CHROMEOS)
extern const char kChromeUIDiscardsHost[];
extern const char kChromeUIDiscardsURL[];
#endif

#if defined(OS_CHROMEOS)
extern const char kCrosScheme[];
#endif

#if defined(OS_ANDROID)
extern const char kAndroidAppScheme[];
#endif

// "Learn more" URL for the Cloud Print section under Options.
extern const char kCloudPrintLearnMoreURL[];

// "Learn more" URL for the Cloud Print Preview No Destinations Promotion.
extern const char kCloudPrintNoDestinationsLearnMoreURL[];

// The URL for the "Learn more" link the the Easy Unlock settings.
extern const char kEasyUnlockLearnMoreUrl[];

// Parameters that get appended to force SafeSearch.
extern const char kSafeSearchSafeParameter[];
extern const char kSafeSearchSsuiParameter[];

// The URL for the "Learn more" link in the media access infobar.
extern const char kMediaAccessLearnMoreUrl[];

// The URL for the "Learn more" link in the language settings.
extern const char kLanguageSettingsLearnMoreUrl[];

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kLinuxWheezyPreciseDeprecationURL[];
#endif

#if defined(OS_MACOSX)
// The URL for the Mac OS X 10.6/10.7/10.8 deprecation help center article.
extern const char kMac10_678_DeprecationURL[];
#endif

#if defined(OS_WIN)
// The URL for the Windows XP/Vista deprecation help center article.
extern const char kWindowsXPVistaDeprecationURL[];
#endif

// The URL for the Bluetooth Overview help center article in the Web Bluetooth
// Chooser.
extern const char kChooserBluetoothOverviewURL[];

// The URL for the WebUsb help center article.
extern const char kChooserUsbOverviewURL[];

#if defined(OS_CHROMEOS)
// The URL for EOL notification
extern const char kEolNotificationURL[];
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
