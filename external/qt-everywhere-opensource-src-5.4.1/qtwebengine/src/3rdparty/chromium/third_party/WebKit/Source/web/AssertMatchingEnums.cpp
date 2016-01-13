/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Use this file to assert that various WebKit API enum values continue
// matching WebCore defined enum values.

#include "config.h"

#include "bindings/v8/SerializedScriptValue.h"
#include "core/accessibility/AXObject.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/DocumentMarker.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/IconURL.h"
#include "core/editing/TextAffinity.h"
#include "core/fileapi/FileError.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/InjectedStyleSheets.h"
#include "core/page/PageVisibilityState.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "modules/geolocation/GeolocationError.h"
#include "modules/geolocation/GeolocationPosition.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "modules/notifications/NotificationClient.h"
#include "modules/quota/DeprecatedStorageQuota.h"
#include "modules/speech/SpeechRecognitionError.h"
#include "platform/Cursor.h"
#include "platform/FileMetadata.h"
#include "platform/FileSystemType.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/graphics/filters/FilterOperation.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceResponse.h"
#include "platform/text/TextChecking.h"
#include "platform/text/TextDecoration.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebCursorInfo.h"
#include "public/platform/WebFileError.h"
#include "public/platform/WebFileInfo.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebIDBCursor.h"
#include "public/platform/WebIDBDatabase.h"
#include "public/platform/WebIDBDatabaseException.h"
#include "public/platform/WebIDBFactory.h"
#include "public/platform/WebIDBKey.h"
#include "public/platform/WebIDBKeyPath.h"
#include "public/platform/WebIDBMetadata.h"
#include "public/platform/WebIDBTypes.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "public/platform/WebMediaSource.h"
#include "public/platform/WebMediaStreamSource.h"
#include "public/platform/WebReferrerPolicy.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebScrollbarBehavior.h"
#include "public/platform/WebStorageQuotaError.h"
#include "public/platform/WebStorageQuotaType.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebAXEnums.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebFontDescription.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebGeolocationError.h"
#include "public/web/WebGeolocationPosition.h"
#include "public/web/WebHistoryCommitType.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebIconURL.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebNavigatorContentUtilsClient.h"
#include "public/web/WebNotificationPresenter.h"
#include "public/web/WebPageVisibilityState.h"
#include "public/web/WebSerializedScriptValueVersion.h"
#include "public/web/WebSettings.h"
#include "public/web/WebSpeechRecognizerClient.h"
#include "public/web/WebTextAffinity.h"
#include "public/web/WebTextCheckingResult.h"
#include "public/web/WebTextCheckingType.h"
#include "public/web/WebTextDecorationType.h"
#include "public/web/WebTouchAction.h"
#include "public/web/WebView.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringImpl.h"

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(blink::webkit_name) == int(WebCore::webcore_name), mismatching_enums)

#define COMPILE_ASSERT_MATCHING_UINT64(webkit_name, webcore_name) \
    COMPILE_ASSERT(blink::webkit_name == WebCore::webcore_name, mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebAXEventActiveDescendantChanged, AXObjectCache::AXActiveDescendantChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventAlert, AXObjectCache::AXAlert);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventAriaAttributeChanged, AXObjectCache::AXAriaAttributeChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventAutocorrectionOccured, AXObjectCache::AXAutocorrectionOccured);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventBlur, AXObjectCache::AXBlur);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventCheckedStateChanged, AXObjectCache::AXCheckedStateChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventChildrenChanged, AXObjectCache::AXChildrenChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventFocus, AXObjectCache::AXFocusedUIElementChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventHide, AXObjectCache::AXHide);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventInvalidStatusChanged, AXObjectCache::AXInvalidStatusChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventLayoutComplete, AXObjectCache::AXLayoutComplete);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventLiveRegionChanged, AXObjectCache::AXLiveRegionChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventLoadComplete, AXObjectCache::AXLoadComplete);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventLocationChanged, AXObjectCache::AXLocationChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventMenuListItemSelected, AXObjectCache::AXMenuListItemSelected);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventMenuListValueChanged, AXObjectCache::AXMenuListValueChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventRowCollapsed, AXObjectCache::AXRowCollapsed);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventRowCountChanged, AXObjectCache::AXRowCountChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventRowExpanded, AXObjectCache::AXRowExpanded);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventScrollPositionChanged, AXObjectCache::AXScrollPositionChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventScrolledToAnchor, AXObjectCache::AXScrolledToAnchor);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventSelectedChildrenChanged, AXObjectCache::AXSelectedChildrenChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventSelectedTextChanged, AXObjectCache::AXSelectedTextChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventShow, AXObjectCache::AXShow);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventTextChanged, AXObjectCache::AXTextChanged);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventTextInserted, AXObjectCache::AXTextInserted);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventTextRemoved, AXObjectCache::AXTextRemoved);
COMPILE_ASSERT_MATCHING_ENUM(WebAXEventValueChanged, AXObjectCache::AXValueChanged);

COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleAlertDialog, AlertDialogRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleAlert, AlertRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleAnnotation, AnnotationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleApplication, ApplicationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleArticle, ArticleRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleBanner, BannerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleBrowser, BrowserRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleBusyIndicator, BusyIndicatorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleButton, ButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleCanvas, CanvasRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleCell, CellRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleCheckBox, CheckBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleColorWell, ColorWellRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleColumnHeader, ColumnHeaderRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleColumn, ColumnRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleComboBox, ComboBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleComplementary, ComplementaryRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleContentInfo, ContentInfoRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDefinition, DefinitionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDescriptionListDetail, DescriptionListDetailRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDescriptionListTerm, DescriptionListTermRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDialog, DialogRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDirectory, DirectoryRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDisclosureTriangle, DisclosureTriangleRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDiv, DivRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDocument, DocumentRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleDrawer, DrawerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleEditableText, EditableTextRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleEmbeddedObject, EmbeddedObjectRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleFooter, FooterRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleForm, FormRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleGrid, GridRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleGroup, GroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleGrowArea, GrowAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleHeading, HeadingRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleHelpTag, HelpTagRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleIframe, IframeRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleIgnored, IgnoredRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleImageMapLink, ImageMapLinkRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleImageMap, ImageMapRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleImage, ImageRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleIncrementor, IncrementorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleInlineTextBox, InlineTextBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleLabel, LabelRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleLegend, LegendRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleLink, LinkRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleListBoxOption, ListBoxOptionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleListBox, ListBoxRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleListItem, ListItemRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleListMarker, ListMarkerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleList, ListRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleLog, LogRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMain, MainRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMarquee, MarqueeRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMathElement, MathElementRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMath, MathRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMatte, MatteRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMenuBar, MenuBarRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMenuButton, MenuButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMenuItem, MenuItemRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMenuListOption, MenuListOptionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMenuListPopup, MenuListPopupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleMenu, MenuRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleNavigation, NavigationRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleNote, NoteRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleOutline, OutlineRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleParagraph, ParagraphRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRolePopUpButton, PopUpButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRolePresentational, PresentationalRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleProgressIndicator, ProgressIndicatorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRadioButton, RadioButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRadioGroup, RadioGroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRegion, RegionRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRootWebArea, RootWebAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRowHeader, RowHeaderRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRow, RowRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRulerMarker, RulerMarkerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleRuler, RulerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSVGRoot, SVGRootRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleScrollArea, ScrollAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleScrollBar, ScrollBarRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSeamlessWebArea, SeamlessWebAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSearch, SearchRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSheet, SheetRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSlider, SliderRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSliderThumb, SliderThumbRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSpinButtonPart, SpinButtonPartRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSpinButton, SpinButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSplitGroup, SplitGroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSplitter, SplitterRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleStaticText, StaticTextRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleStatus, StatusRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleSystemWide, SystemWideRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTabGroup, TabGroupRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTabList, TabListRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTabPanel, TabPanelRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTab, TabRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTableHeaderContainer, TableHeaderContainerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTable, TableRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTextArea, TextAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTextField, TextFieldRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTimer, TimerRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleToggleButton, ToggleButtonRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleToolbar, ToolbarRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTreeGrid, TreeGridRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTreeItem, TreeItemRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleTree, TreeRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleUnknown, UnknownRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleUserInterfaceTooltip, UserInterfaceTooltipRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleValueIndicator, ValueIndicatorRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleWebArea, WebAreaRole);
COMPILE_ASSERT_MATCHING_ENUM(WebAXRoleWindow, WindowRole);

COMPILE_ASSERT_MATCHING_ENUM(WebAXStateBusy, AXBusyState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateChecked, AXCheckedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateCollapsed, AXCollapsedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateEnabled, AXEnabledState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateExpanded, AXExpandedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateFocusable, AXFocusableState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateFocused, AXFocusedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateHaspopup, AXHaspopupState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateHovered, AXHoveredState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateIndeterminate, AXIndeterminateState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateInvisible, AXInvisibleState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateLinked, AXLinkedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateMultiselectable, AXMultiselectableState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateOffscreen, AXOffscreenState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStatePressed, AXPressedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateProtected, AXProtectedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateReadonly, AXReadonlyState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateRequired, AXRequiredState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateSelectable, AXSelectableState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateSelected, AXSelectedState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateVertical, AXVerticalState);
COMPILE_ASSERT_MATCHING_ENUM(WebAXStateVisited, AXVisitedState);

COMPILE_ASSERT_MATCHING_ENUM(WebAXTextDirectionLR, AccessibilityTextDirectionLeftToRight);
COMPILE_ASSERT_MATCHING_ENUM(WebAXTextDirectionRL, AccessibilityTextDirectionRightToLeft);
COMPILE_ASSERT_MATCHING_ENUM(WebAXTextDirectionTB, AccessibilityTextDirectionTopToBottom);
COMPILE_ASSERT_MATCHING_ENUM(WebAXTextDirectionBT, AccessibilityTextDirectionBottomToTop);

COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Uncached, ApplicationCacheHost::UNCACHED);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Idle, ApplicationCacheHost::IDLE);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Checking, ApplicationCacheHost::CHECKING);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Downloading, ApplicationCacheHost::DOWNLOADING);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::UpdateReady, ApplicationCacheHost::UPDATEREADY);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::Obsolete, ApplicationCacheHost::OBSOLETE);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::CheckingEvent, ApplicationCacheHost::CHECKING_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::ErrorEvent, ApplicationCacheHost::ERROR_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::NoUpdateEvent, ApplicationCacheHost::NOUPDATE_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::DownloadingEvent, ApplicationCacheHost::DOWNLOADING_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::ProgressEvent, ApplicationCacheHost::PROGRESS_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::UpdateReadyEvent, ApplicationCacheHost::UPDATEREADY_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::CachedEvent, ApplicationCacheHost::CACHED_EVENT);
COMPILE_ASSERT_MATCHING_ENUM(WebApplicationCacheHost::ObsoleteEvent, ApplicationCacheHost::OBSOLETE_EVENT);

COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypePointer, Cursor::Pointer);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCross, Cursor::Cross);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeHand, Cursor::Hand);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeIBeam, Cursor::IBeam);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeWait, Cursor::Wait);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeHelp, Cursor::Help);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeEastResize, Cursor::EastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthResize, Cursor::NorthResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthEastResize, Cursor::NorthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthWestResize, Cursor::NorthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthResize, Cursor::SouthResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthEastResize, Cursor::SouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthWestResize, Cursor::SouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeWestResize, Cursor::WestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthSouthResize, Cursor::NorthSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeEastWestResize, Cursor::EastWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthEastSouthWestResize, Cursor::NorthEastSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthWestSouthEastResize, Cursor::NorthWestSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeColumnResize, Cursor::ColumnResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeRowResize, Cursor::RowResize);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeMiddlePanning, Cursor::MiddlePanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeEastPanning, Cursor::EastPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthPanning, Cursor::NorthPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthEastPanning, Cursor::NorthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNorthWestPanning, Cursor::NorthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthPanning, Cursor::SouthPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthEastPanning, Cursor::SouthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeSouthWestPanning, Cursor::SouthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeWestPanning, Cursor::WestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeMove, Cursor::Move);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeVerticalText, Cursor::VerticalText);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCell, Cursor::Cell);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeContextMenu, Cursor::ContextMenu);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeAlias, Cursor::Alias);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeProgress, Cursor::Progress);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNoDrop, Cursor::NoDrop);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCopy, Cursor::Copy);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNone, Cursor::None);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeNotAllowed, Cursor::NotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeZoomIn, Cursor::ZoomIn);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeZoomOut, Cursor::ZoomOut);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeGrab, Cursor::Grab);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeGrabbing, Cursor::Grabbing);
COMPILE_ASSERT_MATCHING_ENUM(WebCursorInfo::TypeCustom, Cursor::Custom);

COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilyNone, FontDescription::NoFamily);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilyStandard, FontDescription::StandardFamily);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilySerif, FontDescription::SerifFamily);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilySansSerif, FontDescription::SansSerifFamily);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilyMonospace, FontDescription::MonospaceFamily);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilyCursive, FontDescription::CursiveFamily);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::GenericFamilyFantasy, FontDescription::FantasyFamily);

COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::SmoothingAuto, AutoSmoothing);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::SmoothingNone, NoSmoothing);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::SmoothingGrayscale, Antialiased);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::SmoothingSubpixel, SubpixelAntialiased);

COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight100, FontWeight100);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight200, FontWeight200);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight300, FontWeight300);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight400, FontWeight400);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight500, FontWeight500);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight600, FontWeight600);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight700, FontWeight700);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight800, FontWeight800);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::Weight900, FontWeight900);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::WeightNormal, FontWeightNormal);
COMPILE_ASSERT_MATCHING_ENUM(WebFontDescription::WeightBold, FontWeightBold);

COMPILE_ASSERT_MATCHING_ENUM(WebIconURL::TypeInvalid, InvalidIcon);
COMPILE_ASSERT_MATCHING_ENUM(WebIconURL::TypeFavicon, Favicon);
COMPILE_ASSERT_MATCHING_ENUM(WebIconURL::TypeTouch, TouchIcon);
COMPILE_ASSERT_MATCHING_ENUM(WebIconURL::TypeTouchPrecomposed, TouchPrecomposedIcon);

COMPILE_ASSERT_MATCHING_ENUM(WebNode::ElementNode, Node::ELEMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::AttributeNode, Node::ATTRIBUTE_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::TextNode, Node::TEXT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::CDataSectionNode, Node::CDATA_SECTION_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::ProcessingInstructionsNode, Node::PROCESSING_INSTRUCTION_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::CommentNode, Node::COMMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::DocumentNode, Node::DOCUMENT_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::DocumentTypeNode, Node::DOCUMENT_TYPE_NODE);
COMPILE_ASSERT_MATCHING_ENUM(WebNode::DocumentFragmentNode, Node::DOCUMENT_FRAGMENT_NODE);

COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateEmpty, MediaPlayer::Empty);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateIdle, MediaPlayer::Idle);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateLoading, MediaPlayer::Loading);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateLoaded, MediaPlayer::Loaded);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateFormatError, MediaPlayer::FormatError);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateNetworkError, MediaPlayer::NetworkError);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::NetworkStateDecodeError, MediaPlayer::DecodeError);

COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::ReadyStateHaveNothing, HTMLMediaElement::HAVE_NOTHING);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::ReadyStateHaveMetadata, HTMLMediaElement::HAVE_METADATA);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::ReadyStateHaveCurrentData, HTMLMediaElement::HAVE_CURRENT_DATA);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::ReadyStateHaveFutureData, HTMLMediaElement::HAVE_FUTURE_DATA);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::ReadyStateHaveEnoughData, HTMLMediaElement::HAVE_ENOUGH_DATA);

COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::PreloadNone, MediaPlayer::None);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::PreloadMetaData, MediaPlayer::MetaData);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaPlayer::PreloadAuto, MediaPlayer::Auto);

COMPILE_ASSERT_MATCHING_ENUM(WebMouseEvent::ButtonNone, NoButton);
COMPILE_ASSERT_MATCHING_ENUM(WebMouseEvent::ButtonLeft, LeftButton);
COMPILE_ASSERT_MATCHING_ENUM(WebMouseEvent::ButtonMiddle, MiddleButton);
COMPILE_ASSERT_MATCHING_ENUM(WebMouseEvent::ButtonRight, RightButton);

COMPILE_ASSERT_MATCHING_ENUM(WebNotificationPresenter::PermissionAllowed, NotificationClient::PermissionAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebNotificationPresenter::PermissionNotAllowed, NotificationClient::PermissionNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebNotificationPresenter::PermissionDenied, NotificationClient::PermissionDenied);

COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::Horizontal, HorizontalScrollbar);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::Vertical, VerticalScrollbar);

COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollByLine, ScrollByLine);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollByPage, ScrollByPage);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollByDocument, ScrollByDocument);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollByPixel, ScrollByPixel);

COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::RegularScrollbar, RegularScrollbar);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::SmallScrollbar, SmallScrollbar);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::NoPart, NoPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::BackButtonStartPart, BackButtonStartPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ForwardButtonStartPart, ForwardButtonStartPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::BackTrackPart, BackTrackPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ThumbPart, ThumbPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ForwardTrackPart, ForwardTrackPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::BackButtonEndPart, BackButtonEndPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ForwardButtonEndPart, ForwardButtonEndPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollbarBGPart, ScrollbarBGPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::TrackBGPart, TrackBGPart);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::AllParts, AllParts);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollbarOverlayStyleDefault, ScrollbarOverlayStyleDefault);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollbarOverlayStyleDark, ScrollbarOverlayStyleDark);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbar::ScrollbarOverlayStyleLight, ScrollbarOverlayStyleLight);

COMPILE_ASSERT_MATCHING_ENUM(WebScrollbarBehavior::ButtonNone, NoButton);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbarBehavior::ButtonLeft, LeftButton);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbarBehavior::ButtonMiddle, MiddleButton);
COMPILE_ASSERT_MATCHING_ENUM(WebScrollbarBehavior::ButtonRight, RightButton);

COMPILE_ASSERT_MATCHING_ENUM(WebSettings::EditingBehaviorMac, EditingMacBehavior);
COMPILE_ASSERT_MATCHING_ENUM(WebSettings::EditingBehaviorWin, EditingWindowsBehavior);
COMPILE_ASSERT_MATCHING_ENUM(WebSettings::EditingBehaviorUnix, EditingUnixBehavior);
COMPILE_ASSERT_MATCHING_ENUM(WebSettings::EditingBehaviorAndroid, EditingAndroidBehavior);

COMPILE_ASSERT_MATCHING_ENUM(WebTextAffinityUpstream, UPSTREAM);
COMPILE_ASSERT_MATCHING_ENUM(WebTextAffinityDownstream, DOWNSTREAM);

COMPILE_ASSERT_MATCHING_ENUM(WebView::InjectStyleInAllFrames, InjectStyleInAllFrames);
COMPILE_ASSERT_MATCHING_ENUM(WebView::InjectStyleInTopFrameOnly, InjectStyleInTopFrameOnly);

COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionUnknownError, UnknownError);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionConstraintError, ConstraintError);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionDataError, DataError);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionVersionError, VersionError);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionAbortError, AbortError);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionQuotaError, QuotaExceededError);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBDatabaseExceptionTimeoutError, TimeoutError);

COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyTypeInvalid, IDBKey::InvalidType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyTypeArray, IDBKey::ArrayType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyTypeBinary, IDBKey::BinaryType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyTypeString, IDBKey::StringType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyTypeDate, IDBKey::DateType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyTypeNumber, IDBKey::NumberType);

COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyPathTypeNull, IDBKeyPath::NullType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyPathTypeString, IDBKeyPath::StringType);
COMPILE_ASSERT_MATCHING_ENUM(WebIDBKeyPathTypeArray, IDBKeyPath::ArrayType);

COMPILE_ASSERT_MATCHING_ENUM(WebIDBMetadata::NoIntVersion, IDBDatabaseMetadata::NoIntVersion);

COMPILE_ASSERT_MATCHING_ENUM(WebFileSystem::TypeTemporary, FileSystemTypeTemporary);
COMPILE_ASSERT_MATCHING_ENUM(WebFileSystem::TypePersistent, FileSystemTypePersistent);
COMPILE_ASSERT_MATCHING_ENUM(WebFileSystem::TypeExternal, FileSystemTypeExternal);
COMPILE_ASSERT_MATCHING_ENUM(WebFileSystem::TypeIsolated, FileSystemTypeIsolated);
COMPILE_ASSERT_MATCHING_ENUM(WebFileInfo::TypeUnknown, FileMetadata::TypeUnknown);
COMPILE_ASSERT_MATCHING_ENUM(WebFileInfo::TypeFile, FileMetadata::TypeFile);
COMPILE_ASSERT_MATCHING_ENUM(WebFileInfo::TypeDirectory, FileMetadata::TypeDirectory);

COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorNotFound, FileError::NOT_FOUND_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorSecurity, FileError::SECURITY_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorAbort, FileError::ABORT_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorNotReadable, FileError::NOT_READABLE_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorEncoding, FileError::ENCODING_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorNoModificationAllowed, FileError::NO_MODIFICATION_ALLOWED_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorInvalidState, FileError::INVALID_STATE_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorSyntax, FileError::SYNTAX_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorInvalidModification, FileError::INVALID_MODIFICATION_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorQuotaExceeded, FileError::QUOTA_EXCEEDED_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorTypeMismatch, FileError::TYPE_MISMATCH_ERR);
COMPILE_ASSERT_MATCHING_ENUM(WebFileErrorPathExists, FileError::PATH_EXISTS_ERR);

COMPILE_ASSERT_MATCHING_ENUM(WebGeolocationError::ErrorPermissionDenied, GeolocationError::PermissionDenied);
COMPILE_ASSERT_MATCHING_ENUM(WebGeolocationError::ErrorPositionUnavailable, GeolocationError::PositionUnavailable);

COMPILE_ASSERT_MATCHING_ENUM(WebTextCheckingTypeSpelling, TextCheckingTypeSpelling);
COMPILE_ASSERT_MATCHING_ENUM(WebTextCheckingTypeGrammar, TextCheckingTypeGrammar);

// TODO(rouslan): Remove these comparisons between text-checking and text-decoration enum values after removing the
// deprecated constructor WebTextCheckingResult(WebTextCheckingType).
COMPILE_ASSERT_MATCHING_ENUM(WebTextCheckingTypeSpelling, TextDecorationTypeSpelling);
COMPILE_ASSERT_MATCHING_ENUM(WebTextCheckingTypeGrammar, TextDecorationTypeGrammar);

COMPILE_ASSERT_MATCHING_ENUM(WebTextDecorationTypeSpelling, TextDecorationTypeSpelling);
COMPILE_ASSERT_MATCHING_ENUM(WebTextDecorationTypeGrammar, TextDecorationTypeGrammar);
COMPILE_ASSERT_MATCHING_ENUM(WebTextDecorationTypeInvisibleSpellcheck, TextDecorationTypeInvisibleSpellcheck);

COMPILE_ASSERT_MATCHING_ENUM(WebStorageQuotaErrorNotSupported, NotSupportedError);
COMPILE_ASSERT_MATCHING_ENUM(WebStorageQuotaErrorInvalidModification, InvalidModificationError);
COMPILE_ASSERT_MATCHING_ENUM(WebStorageQuotaErrorInvalidAccess, InvalidAccessError);
COMPILE_ASSERT_MATCHING_ENUM(WebStorageQuotaErrorAbort, AbortError);

COMPILE_ASSERT_MATCHING_ENUM(WebStorageQuotaTypeTemporary, DeprecatedStorageQuota::Temporary);
COMPILE_ASSERT_MATCHING_ENUM(WebStorageQuotaTypePersistent, DeprecatedStorageQuota::Persistent);

COMPILE_ASSERT_MATCHING_ENUM(WebPageVisibilityStateVisible, PageVisibilityStateVisible);
COMPILE_ASSERT_MATCHING_ENUM(WebPageVisibilityStateHidden, PageVisibilityStateHidden);
COMPILE_ASSERT_MATCHING_ENUM(WebPageVisibilityStatePrerender, PageVisibilityStatePrerender);

COMPILE_ASSERT_MATCHING_ENUM(WebMediaStreamSource::TypeAudio, MediaStreamSource::TypeAudio);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaStreamSource::TypeVideo, MediaStreamSource::TypeVideo);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaStreamSource::ReadyStateLive, MediaStreamSource::ReadyStateLive);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaStreamSource::ReadyStateMuted, MediaStreamSource::ReadyStateMuted);
COMPILE_ASSERT_MATCHING_ENUM(WebMediaStreamSource::ReadyStateEnded, MediaStreamSource::ReadyStateEnded);

COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::OtherError, SpeechRecognitionError::ErrorCodeOther);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::NoSpeechError, SpeechRecognitionError::ErrorCodeNoSpeech);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::AbortedError, SpeechRecognitionError::ErrorCodeAborted);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::AudioCaptureError, SpeechRecognitionError::ErrorCodeAudioCapture);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::NetworkError, SpeechRecognitionError::ErrorCodeNetwork);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::NotAllowedError, SpeechRecognitionError::ErrorCodeNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::ServiceNotAllowedError, SpeechRecognitionError::ErrorCodeServiceNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::BadGrammarError, SpeechRecognitionError::ErrorCodeBadGrammar);
COMPILE_ASSERT_MATCHING_ENUM(WebSpeechRecognizerClient::LanguageNotSupportedError, SpeechRecognitionError::ErrorCodeLanguageNotSupported);

COMPILE_ASSERT_MATCHING_ENUM(WebReferrerPolicyAlways, ReferrerPolicyAlways);
COMPILE_ASSERT_MATCHING_ENUM(WebReferrerPolicyDefault, ReferrerPolicyDefault);
COMPILE_ASSERT_MATCHING_ENUM(WebReferrerPolicyNever, ReferrerPolicyNever);
COMPILE_ASSERT_MATCHING_ENUM(WebReferrerPolicyOrigin, ReferrerPolicyOrigin);

COMPILE_ASSERT_MATCHING_ENUM(WebContentSecurityPolicyTypeReport, ContentSecurityPolicyHeaderTypeReport);
COMPILE_ASSERT_MATCHING_ENUM(WebContentSecurityPolicyTypeEnforce, ContentSecurityPolicyHeaderTypeEnforce);

COMPILE_ASSERT_MATCHING_ENUM(WebURLResponse::Unknown, ResourceResponse::Unknown);
COMPILE_ASSERT_MATCHING_ENUM(WebURLResponse::HTTP_0_9, ResourceResponse::HTTP_0_9);
COMPILE_ASSERT_MATCHING_ENUM(WebURLResponse::HTTP_1_0, ResourceResponse::HTTP_1_0);
COMPILE_ASSERT_MATCHING_ENUM(WebURLResponse::HTTP_1_1, ResourceResponse::HTTP_1_1);

COMPILE_ASSERT_MATCHING_ENUM(WebFormElement::AutocompleteResultSuccess, HTMLFormElement::AutocompleteResultSuccess);
COMPILE_ASSERT_MATCHING_ENUM(WebFormElement::AutocompleteResultErrorDisabled, HTMLFormElement::AutocompleteResultErrorDisabled);
COMPILE_ASSERT_MATCHING_ENUM(WebFormElement::AutocompleteResultErrorCancel, HTMLFormElement::AutocompleteResultErrorCancel);
COMPILE_ASSERT_MATCHING_ENUM(WebFormElement::AutocompleteResultErrorInvalid, HTMLFormElement::AutocompleteResultErrorInvalid);

COMPILE_ASSERT_MATCHING_ENUM(WebURLRequest::PriorityUnresolved, ResourceLoadPriorityUnresolved);
COMPILE_ASSERT_MATCHING_ENUM(WebURLRequest::PriorityVeryLow, ResourceLoadPriorityVeryLow);
COMPILE_ASSERT_MATCHING_ENUM(WebURLRequest::PriorityLow, ResourceLoadPriorityLow);
COMPILE_ASSERT_MATCHING_ENUM(WebURLRequest::PriorityMedium, ResourceLoadPriorityMedium);
COMPILE_ASSERT_MATCHING_ENUM(WebURLRequest::PriorityHigh, ResourceLoadPriorityHigh);
COMPILE_ASSERT_MATCHING_ENUM(WebURLRequest::PriorityVeryHigh, ResourceLoadPriorityVeryHigh);

COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyIgnore, NavigationPolicyIgnore);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyDownload, NavigationPolicyDownload);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyDownloadTo, NavigationPolicyDownloadTo);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyCurrentTab, NavigationPolicyCurrentTab);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyNewBackgroundTab, NavigationPolicyNewBackgroundTab);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyNewForegroundTab, NavigationPolicyNewForegroundTab);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyNewWindow, NavigationPolicyNewWindow);
COMPILE_ASSERT_MATCHING_ENUM(WebNavigationPolicyNewPopup, NavigationPolicyNewPopup);

COMPILE_ASSERT_MATCHING_ENUM(WebStandardCommit, StandardCommit);
COMPILE_ASSERT_MATCHING_ENUM(WebBackForwardCommit, BackForwardCommit);
COMPILE_ASSERT_MATCHING_ENUM(WebInitialCommitInChildFrame, InitialCommitInChildFrame);
COMPILE_ASSERT_MATCHING_ENUM(WebHistoryInertCommit, HistoryInertCommit);

COMPILE_ASSERT_MATCHING_ENUM(WebHistorySameDocumentLoad, HistorySameDocumentLoad);
COMPILE_ASSERT_MATCHING_ENUM(WebHistoryDifferentDocumentLoad, HistoryDifferentDocumentLoad);

COMPILE_ASSERT_MATCHING_ENUM(WebConsoleMessage::LevelDebug, DebugMessageLevel);
COMPILE_ASSERT_MATCHING_ENUM(WebConsoleMessage::LevelLog, LogMessageLevel);
COMPILE_ASSERT_MATCHING_ENUM(WebConsoleMessage::LevelWarning, WarningMessageLevel);
COMPILE_ASSERT_MATCHING_ENUM(WebConsoleMessage::LevelError, ErrorMessageLevel);
COMPILE_ASSERT_MATCHING_ENUM(WebConsoleMessage::LevelInfo, InfoMessageLevel);

COMPILE_ASSERT_MATCHING_ENUM(WebCustomHandlersNew, NavigatorContentUtilsClient::CustomHandlersNew);
COMPILE_ASSERT_MATCHING_ENUM(WebCustomHandlersRegistered, NavigatorContentUtilsClient::CustomHandlersRegistered);
COMPILE_ASSERT_MATCHING_ENUM(WebCustomHandlersDeclined, NavigatorContentUtilsClient::CustomHandlersDeclined);

COMPILE_ASSERT_MATCHING_ENUM(WebTouchActionNone, TouchActionNone);
COMPILE_ASSERT_MATCHING_ENUM(WebTouchActionAuto, TouchActionAuto);
COMPILE_ASSERT_MATCHING_ENUM(WebTouchActionPanX, TouchActionPanX);
COMPILE_ASSERT_MATCHING_ENUM(WebTouchActionPanY, TouchActionPanY);
COMPILE_ASSERT_MATCHING_ENUM(WebTouchActionPinchZoom, TouchActionPinchZoom);

COMPILE_ASSERT_MATCHING_UINT64(kSerializedScriptValueVersion, SerializedScriptValue::wireFormatVersion);
