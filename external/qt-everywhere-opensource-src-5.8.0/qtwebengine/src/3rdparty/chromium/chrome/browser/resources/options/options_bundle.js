// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file exists to aggregate all of the javascript used by the
// settings page into a single file which will be flattened and served
// as a single resource.
<include src="preferences.js">
<include src="controlled_setting.js">
<include src="deletable_item_list.js">
<include src="editable_text_field.js">
<include src="hotword_search_setting_indicator.js">
<include src="inline_editable_list.js">
<include src="options_page.js">
<include src="pref_ui.js">
<include src="settings_dialog.js">
<if expr="chromeos">
<include src="../chromeos/user_images_grid.js">
<include src="../help/channel_change_page.js">
<include src="../../../../ui/webui/resources/js/chromeos/ui_account_tweaks.js">
<include src="chromeos/onc_data.js">
<include src="chromeos/change_picture_options.js">
<include src="chromeos/internet_detail_ip_address_field.js">
<include src="chromeos/internet_detail.js">
<include src="chromeos/network_list.js">
<include src="chromeos/preferred_networks.js">
<include src="chromeos/bluetooth_device_list.js">
<include src="chromeos/bluetooth_add_device_overlay.js">
<include src="chromeos/bluetooth_pair_device_overlay.js">
<include src="chromeos/accounts_options.js">
<include src="chromeos/proxy_rules_list.js">
<include src="chromeos/accounts_user_list.js">
<include src="chromeos/accounts_user_name_edit.js">
<include src="chromeos/consumer_management_overlay.js">
<include src="chromeos/display_layout.js">
<include src="chromeos/display_layout_manager.js">
<include src="chromeos/display_layout_manager_multi.js">
<include src="chromeos/display_options.js">
<include src="chromeos/display_overscan.js">
<include src="chromeos/keyboard_overlay.js">
<include src="chromeos/pointer_overlay.js">
<include src="chromeos/storage_clear_drive_cache_overlay.js">
<include src="chromeos/storage_manager.js">
<include src="chromeos/third_party_ime_confirm_overlay.js">
<include src="chromeos/power_overlay.js">
var AccountsOptions = options.AccountsOptions;
var ChangePictureOptions = options.ChangePictureOptions;
var ConsumerManagementOverlay = options.ConsumerManagementOverlay;
var DetailsInternetPage = options.internet.DetailsInternetPage;
var DisplayOptions = options.DisplayOptions;
var DisplayOverscan = options.DisplayOverscan;
var BluetoothOptions = options.BluetoothOptions;
var BluetoothPairing = options.BluetoothPairing;
var KeyboardOverlay = options.KeyboardOverlay;
var PointerOverlay = options.PointerOverlay;
var PowerOverlay = options.PowerOverlay;
var StorageClearDriveCacheOverlay = options.StorageClearDriveCacheOverlay;
var StorageManager = options.StorageManager;
var UIAccountTweaks = uiAccountTweaks.UIAccountTweaks;
</if>
<if expr="use_nss_certs">
<include src="certificate_tree.js">
<include src="certificate_manager.js">
<include src="certificate_restore_overlay.js">
<include src="certificate_backup_overlay.js">
<include src="certificate_edit_ca_trust_overlay.js">
<include src="certificate_import_error_overlay.js">
var CertificateManager = options.CertificateManager;
var CertificateRestoreOverlay = options.CertificateRestoreOverlay;
var CertificateBackupOverlay = options.CertificateBackupOverlay;
var CertificateEditCaTrustOverlay = options.CertificateEditCaTrustOverlay;
var CertificateImportErrorOverlay = options.CertificateImportErrorOverlay;
</if>
<include src="alert_overlay.js">
<include src="autofill_edit_address_overlay.js">
<include src="autofill_edit_creditcard_overlay.js">
<include src="autofill_options.js">
<include src="autofill_options_list.js">
<include src="automatic_settings_reset_banner.js">
<include src="browser_options.js">
<include src="browser_options_profile_list.js">
<include src="browser_options_startup_page_list.js">
<include src="clear_browser_data_overlay.js">
<include src="clear_browser_data_history_notice_overlay.js">
<include src="confirm_dialog.js">
<include src="content_settings.js">
<include src="content_settings_exceptions_area.js">
<include src="content_settings_ui.js">
<include src="cookies_list.js">
<include src="cookies_view.js">
<include src="easy_unlock_turn_off_overlay.js">
<include src="factory_reset_overlay.js">
<include src="font_settings.js">
<if expr="enable_google_now">
<include src="geolocation_options.js">
</if>
<include src="handler_options.js">
<include src="handler_options_list.js">
<include src="home_page_overlay.js">
<include src="hotword_confirm_dialog.js">
<include src="import_data_overlay.js">
<include src="language_add_language_overlay.js">
<if expr="not is_macosx">
<include src="language_dictionary_overlay_word_list.js">
<include src="language_dictionary_overlay.js">
</if>
<include src="language_list.js">
<include src="language_options.js">
<include src="manage_profile_overlay.js">
<include src="options_focus_manager.js">
<include src="password_manager.js">
<include src="password_manager_list.js">
<include src="profiles_icon_grid.js">
<include src="reset_profile_settings_overlay.js">
<include src="search_engine_manager.js">
<include src="search_engine_manager_engine_list.js">
<include src="search_page.js">
<include src="startup_overlay.js">
<include src="supervised_user_create_confirm.js">
<include src="supervised_user_import.js">
<include src="supervised_user_learn_more.js">
<include src="supervised_user_list.js">
<include src="supervised_user_list_data.js">
<include src="../help/help_page.js">
<include src="sync_setup_overlay.js">
<if expr="is_win">
<include src="triggered_reset_profile_settings_overlay.js">
</if>
<include src="../uber/uber_page_manager_observer.js">
<include src="../uber/uber_utils.js">
<include src="options.js">
<if expr="enable_settings_app">
<include src="options_settings_app.js">
</if>
