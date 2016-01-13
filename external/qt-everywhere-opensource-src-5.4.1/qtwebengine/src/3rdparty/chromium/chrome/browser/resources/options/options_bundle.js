// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file exists to aggregate all of the javascript used by the
// settings page into a single file which will be flattened and served
// as a single resource.
<include src="preferences.js"></include>
<include src="controlled_setting.js"></include>
<include src="deletable_item_list.js"></include>
<include src="editable_text_field.js"></include>
<include src="hotword_search_setting_indicator.js"></include>
<include src="inline_editable_list.js"></include>
<include src="options_page.js"></include>
<include src="pref_ui.js"></include>
<include src="settings_dialog.js"></include>
<include src="settings_banner.js"></include>
<if expr="chromeos">
<include src="../chromeos/user_images_grid.js"></include>
// DO NOT BREAK THE FOLLOWING INCLUDE LINE INTO SEPARATE LINES!
// Even though the include line spans more than 80 characters,
// The grit html inlining parser will leave the end tag behind,
// causing a runtime JS break.
<include src="../../../../ui/webui/resources/js/chromeos/ui_account_tweaks.js"></include>
<include src="chromeos/change_picture_options.js"></include>
<include src="chromeos/internet_detail_ip_address_field.js"></include>
<include src="chromeos/internet_detail.js"></include>
<include src="chromeos/network_list.js"></include>
<include src="chromeos/preferred_networks.js"></include>
<include src="chromeos/bluetooth_device_list.js"></include>
<include src="chromeos/bluetooth_add_device_overlay.js"></include>
<include src="chromeos/bluetooth_pair_device_overlay.js"></include>
<include src="chromeos/accounts_options.js"></include>
<include src="chromeos/proxy_rules_list.js"></include>
<include src="chromeos/accounts_user_list.js"></include>
<include src="chromeos/accounts_user_name_edit.js"></include>
<include src="chromeos/display_options.js"></include>
<include src="chromeos/display_overscan.js"></include>
<include src="chromeos/keyboard_overlay.js"></include>
<include src="chromeos/pointer_overlay.js"></include>
<include src="chromeos/third_party_ime_confirm_overlay.js"></include>
var AccountsOptions = options.AccountsOptions;
var ChangePictureOptions = options.ChangePictureOptions;
var DetailsInternetPage = options.internet.DetailsInternetPage;
var DisplayOptions = options.DisplayOptions;
var DisplayOverscan = options.DisplayOverscan;
var BluetoothOptions = options.BluetoothOptions;
var BluetoothPairing = options.BluetoothPairing;
var KeyboardOverlay = options.KeyboardOverlay;
var PointerOverlay = options.PointerOverlay;
var UIAccountTweaks = uiAccountTweaks.UIAccountTweaks;
</if>
<if expr="use_nss">
<include src="certificate_tree.js"></include>
<include src="certificate_manager.js"></include>
<include src="certificate_restore_overlay.js"></include>
<include src="certificate_backup_overlay.js"></include>
<include src="certificate_edit_ca_trust_overlay.js"></include>
<include src="certificate_import_error_overlay.js"></include>
var CertificateManager = options.CertificateManager;
var CertificateRestoreOverlay = options.CertificateRestoreOverlay;
var CertificateBackupOverlay = options.CertificateBackupOverlay;
var CertificateEditCaTrustOverlay = options.CertificateEditCaTrustOverlay;
var CertificateImportErrorOverlay = options.CertificateImportErrorOverlay;
</if>
<include src="alert_overlay.js"></include>
<include src="autofill_edit_address_overlay.js"></include>
<include src="autofill_edit_creditcard_overlay.js"></include>
<include src="autofill_options_list.js"></include>
<include src="autofill_options.js"></include>
<include src="automatic_settings_reset_banner.js"></include>
<include src="browser_options.js"></include>
<include src="browser_options_profile_list.js"></include>
<include src="browser_options_startup_page_list.js"></include>
<include src="clear_browser_data_overlay.js"></include>
<include src="confirm_dialog.js"></include>
<include src="content_settings.js"></include>
<include src="content_settings_exceptions_area.js"></include>
<include src="content_settings_ui.js"></include>
<include src="cookies_list.js"></include>
<include src="cookies_view.js"></include>
<include src="factory_reset_overlay.js"></include>
<include src="font_settings.js"></include>
<if expr="enable_google_now">
<include src="geolocation_options.js"></include>
</if>
<include src="handler_options.js"></include>
<include src="handler_options_list.js"></include>
<include src="home_page_overlay.js"></include>
<include src="import_data_overlay.js"></include>
<include src="language_add_language_overlay.js"></include>
<if expr="not is_macosx">
<include src="language_dictionary_overlay_word_list.js"></include>
<include src="language_dictionary_overlay.js"></include>
</if>
<include src="language_list.js"></include>
<include src="language_options.js"></include>
<include src="manage_profile_overlay.js"></include>
<include src="managed_user_create_confirm.js"</include>
<include src="managed_user_import.js"></include>
<include src="managed_user_learn_more.js"</include>
<include src="managed_user_list.js"></include>
<include src="managed_user_list_data.js"></include>
<include src="options_focus_manager.js"></include>
<include src="password_manager.js"></include>
<include src="password_manager_list.js"></include>
<include src="profiles_icon_grid.js"></include>
<include src="reset_profile_settings_banner.js"></include>
<include src="reset_profile_settings_overlay.js"></include>
<include src="search_engine_manager.js"></include>
<include src="search_engine_manager_engine_list.js"></include>
<include src="search_page.js"></include>
<include src="startup_overlay.js"></include>
<include src="../sync_setup_overlay.js"></include>
<include src="../uber/uber_utils.js"></include>
<include src="options.js"></include>
<if expr="enable_settings_app">
<include src="options_settings_app.js"></include>
</if>
