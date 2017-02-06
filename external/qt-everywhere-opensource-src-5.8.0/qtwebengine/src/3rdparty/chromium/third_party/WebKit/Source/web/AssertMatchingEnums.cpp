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

// Use this file to assert that various public API enum values continue
// matching blink defined enum values.

#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/IconURL.h"
#include "core/editing/SelectionType.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/markers/DocumentMarker.h"
#include "core/fileapi/FileError.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/layout/compositing/CompositedSelectionBound.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/PageVisibilityState.h"
#include "core/style/ComputedStyleConstants.h"
#include "modules/accessibility/AXObject.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "modules/quota/DeprecatedStorageQuota.h"
#include "modules/speech/SpeechRecognitionError.h"
#include "platform/Cursor.h"
#include "platform/FileMetadata.h"
#include "platform/FileSystemType.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceResponse.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TextChecking.h"
#include "platform/text/TextDecoration.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebCursorInfo.h"
#include "public/platform/WebFileError.h"
#include "public/platform/WebFileInfo.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFontDescription.h"
#include "public/platform/WebHistoryScrollRestorationType.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "public/platform/WebMediaSource.h"
#include "public/platform/WebMediaStreamSource.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebReferrerPolicy.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebScrollbarBehavior.h"
#include "public/platform/WebSelectionBound.h"
#include "public/platform/WebStorageQuotaError.h"
#include "public/platform/WebStorageQuotaType.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "public/platform/modules/indexeddb/WebIDBFactory.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "public/platform/modules/indexeddb/WebIDBMetadata.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "public/web/WebAXEnums.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebClientRedirectPolicy.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameLoadType.h"
#include "public/web/WebHistoryCommitType.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebIconURL.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebNavigatorContentUtilsClient.h"
#include "public/web/WebRemoteFrameClient.h"
#include "public/web/WebSandboxFlags.h"
#include "public/web/WebSecurityPolicy.h"
#include "public/web/WebSelection.h"
#include "public/web/WebSerializedScriptValueVersion.h"
#include "public/web/WebSettings.h"
#include "public/web/WebSpeechRecognizerClient.h"
#include "public/web/WebTextCheckingResult.h"
#include "public/web/WebTextCheckingType.h"
#include "public/web/WebTextDecorationType.h"
#include "public/web/WebTouchAction.h"
#include "public/web/WebView.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringImpl.h"

namespace blink {

#define STATIC_ASSERT_ENUM(a, b)                              \
    static_assert(static_cast<int>(a) == static_cast<int>(b), \
        "mismatching enum: " #a)

STATIC_ASSERT_ENUM(WebAXEventActiveDescendantChanged, AXObjectCache::AXActiveDescendantChanged);
STATIC_ASSERT_ENUM(WebAXEventAlert, AXObjectCache::AXAlert);
STATIC_ASSERT_ENUM(WebAXEventAriaAttributeChanged, AXObjectCache::AXAriaAttributeChanged);
STATIC_ASSERT_ENUM(WebAXEventAutocorrectionOccured, AXObjectCache::AXAutocorrectionOccured);
STATIC_ASSERT_ENUM(WebAXEventBlur, AXObjectCache::AXBlur);
STATIC_ASSERT_ENUM(WebAXEventCheckedStateChanged, AXObjectCache::AXCheckedStateChanged);
STATIC_ASSERT_ENUM(WebAXEventChildrenChanged, AXObjectCache::AXChildrenChanged);
STATIC_ASSERT_ENUM(WebAXEventClicked, AXObjectCache::AXClicked);
STATIC_ASSERT_ENUM(WebAXEventDocumentSelectionChanged, AXObjectCache::AXDocumentSelectionChanged);
STATIC_ASSERT_ENUM(WebAXEventExpandedChanged, AXObjectCache::AXExpandedChanged);
STATIC_ASSERT_ENUM(WebAXEventFocus, AXObjectCache::AXFocusedUIElementChanged);
STATIC_ASSERT_ENUM(WebAXEventHide, AXObjectCache::AXHide);
STATIC_ASSERT_ENUM(WebAXEventHover, AXObjectCache::AXHover);
STATIC_ASSERT_ENUM(WebAXEventInvalidStatusChanged, AXObjectCache::AXInvalidStatusChanged);
STATIC_ASSERT_ENUM(WebAXEventLayoutComplete, AXObjectCache::AXLayoutComplete);
STATIC_ASSERT_ENUM(WebAXEventLiveRegionChanged, AXObjectCache::AXLiveRegionChanged);
STATIC_ASSERT_ENUM(WebAXEventLoadComplete, AXObjectCache::AXLoadComplete);
STATIC_ASSERT_ENUM(WebAXEventLocationChanged, AXObjectCache::AXLocationChanged);
STATIC_ASSERT_ENUM(WebAXEventMenuListItemSelected, AXObjectCache::AXMenuListItemSelected);
STATIC_ASSERT_ENUM(WebAXEventMenuListItemUnselected, AXObjectCache::AXMenuListItemUnselected);
STATIC_ASSERT_ENUM(WebAXEventMenuListValueChanged, AXObjectCache::AXMenuListValueChanged);
STATIC_ASSERT_ENUM(WebAXEventRowCollapsed, AXObjectCache::AXRowCollapsed);
STATIC_ASSERT_ENUM(WebAXEventRowCountChanged, AXObjectCache::AXRowCountChanged);
STATIC_ASSERT_ENUM(WebAXEventRowExpanded, AXObjectCache::AXRowExpanded);
STATIC_ASSERT_ENUM(WebAXEventScrollPositionChanged, AXObjectCache::AXScrollPositionChanged);
STATIC_ASSERT_ENUM(WebAXEventScrolledToAnchor, AXObjectCache::AXScrolledToAnchor);
STATIC_ASSERT_ENUM(WebAXEventSelectedChildrenChanged, AXObjectCache::AXSelectedChildrenChanged);
STATIC_ASSERT_ENUM(WebAXEventSelectedTextChanged, AXObjectCache::AXSelectedTextChanged);
STATIC_ASSERT_ENUM(WebAXEventShow, AXObjectCache::AXShow);
STATIC_ASSERT_ENUM(WebAXEventTextChanged, AXObjectCache::AXTextChanged);
STATIC_ASSERT_ENUM(WebAXEventTextInserted, AXObjectCache::AXTextInserted);
STATIC_ASSERT_ENUM(WebAXEventTextRemoved, AXObjectCache::AXTextRemoved);
STATIC_ASSERT_ENUM(WebAXEventValueChanged, AXObjectCache::AXValueChanged);

STATIC_ASSERT_ENUM(WebAXRoleAbbr, AbbrRole);
STATIC_ASSERT_ENUM(WebAXRoleAlertDialog, AlertDialogRole);
STATIC_ASSERT_ENUM(WebAXRoleAlert, AlertRole);
STATIC_ASSERT_ENUM(WebAXRoleAnnotation, AnnotationRole);
STATIC_ASSERT_ENUM(WebAXRoleApplication, ApplicationRole);
STATIC_ASSERT_ENUM(WebAXRoleArticle, ArticleRole);
STATIC_ASSERT_ENUM(WebAXRoleBanner, BannerRole);
STATIC_ASSERT_ENUM(WebAXRoleBlockquote, BlockquoteRole);
STATIC_ASSERT_ENUM(WebAXRoleBusyIndicator, BusyIndicatorRole);
STATIC_ASSERT_ENUM(WebAXRoleButton, ButtonRole);
STATIC_ASSERT_ENUM(WebAXRoleCanvas, CanvasRole);
STATIC_ASSERT_ENUM(WebAXRoleCaption, CaptionRole);
STATIC_ASSERT_ENUM(WebAXRoleCell, CellRole);
STATIC_ASSERT_ENUM(WebAXRoleCheckBox, CheckBoxRole);
STATIC_ASSERT_ENUM(WebAXRoleColorWell, ColorWellRole);
STATIC_ASSERT_ENUM(WebAXRoleColumnHeader, ColumnHeaderRole);
STATIC_ASSERT_ENUM(WebAXRoleColumn, ColumnRole);
STATIC_ASSERT_ENUM(WebAXRoleComboBox, ComboBoxRole);
STATIC_ASSERT_ENUM(WebAXRoleComplementary, ComplementaryRole);
STATIC_ASSERT_ENUM(WebAXRoleContentInfo, ContentInfoRole);
STATIC_ASSERT_ENUM(WebAXRoleDate, DateRole);
STATIC_ASSERT_ENUM(WebAXRoleDateTime, DateTimeRole);
STATIC_ASSERT_ENUM(WebAXRoleDefinition, DefinitionRole);
STATIC_ASSERT_ENUM(WebAXRoleDescriptionListDetail, DescriptionListDetailRole);
STATIC_ASSERT_ENUM(WebAXRoleDescriptionList, DescriptionListRole);
STATIC_ASSERT_ENUM(WebAXRoleDescriptionListTerm, DescriptionListTermRole);
STATIC_ASSERT_ENUM(WebAXRoleDetails, DetailsRole);
STATIC_ASSERT_ENUM(WebAXRoleDialog, DialogRole);
STATIC_ASSERT_ENUM(WebAXRoleDirectory, DirectoryRole);
STATIC_ASSERT_ENUM(WebAXRoleDisclosureTriangle, DisclosureTriangleRole);
STATIC_ASSERT_ENUM(WebAXRoleDiv, DivRole);
STATIC_ASSERT_ENUM(WebAXRoleDocument, DocumentRole);
STATIC_ASSERT_ENUM(WebAXRoleEmbeddedObject, EmbeddedObjectRole);
STATIC_ASSERT_ENUM(WebAXRoleFigcaption, FigcaptionRole);
STATIC_ASSERT_ENUM(WebAXRoleFigure, FigureRole);
STATIC_ASSERT_ENUM(WebAXRoleFooter, FooterRole);
STATIC_ASSERT_ENUM(WebAXRoleForm, FormRole);
STATIC_ASSERT_ENUM(WebAXRoleGrid, GridRole);
STATIC_ASSERT_ENUM(WebAXRoleGroup, GroupRole);
STATIC_ASSERT_ENUM(WebAXRoleHeading, HeadingRole);
STATIC_ASSERT_ENUM(WebAXRoleIframe, IframeRole);
STATIC_ASSERT_ENUM(WebAXRoleIframePresentational, IframePresentationalRole);
STATIC_ASSERT_ENUM(WebAXRoleIgnored, IgnoredRole);
STATIC_ASSERT_ENUM(WebAXRoleImageMapLink, ImageMapLinkRole);
STATIC_ASSERT_ENUM(WebAXRoleImageMap, ImageMapRole);
STATIC_ASSERT_ENUM(WebAXRoleImage, ImageRole);
STATIC_ASSERT_ENUM(WebAXRoleInlineTextBox, InlineTextBoxRole);
STATIC_ASSERT_ENUM(WebAXRoleInputTime, InputTimeRole);
STATIC_ASSERT_ENUM(WebAXRoleLabel, LabelRole);
STATIC_ASSERT_ENUM(WebAXRoleLegend, LegendRole);
STATIC_ASSERT_ENUM(WebAXRoleLink, LinkRole);
STATIC_ASSERT_ENUM(WebAXRoleListBoxOption, ListBoxOptionRole);
STATIC_ASSERT_ENUM(WebAXRoleListBox, ListBoxRole);
STATIC_ASSERT_ENUM(WebAXRoleListItem, ListItemRole);
STATIC_ASSERT_ENUM(WebAXRoleListMarker, ListMarkerRole);
STATIC_ASSERT_ENUM(WebAXRoleList, ListRole);
STATIC_ASSERT_ENUM(WebAXRoleLog, LogRole);
STATIC_ASSERT_ENUM(WebAXRoleMain, MainRole);
STATIC_ASSERT_ENUM(WebAXRoleMark, MarkRole);
STATIC_ASSERT_ENUM(WebAXRoleMarquee, MarqueeRole);
STATIC_ASSERT_ENUM(WebAXRoleMath, MathRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuBar, MenuBarRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuButton, MenuButtonRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuItem, MenuItemRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuItemCheckBox, MenuItemCheckBoxRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuItemRadio, MenuItemRadioRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuListOption, MenuListOptionRole);
STATIC_ASSERT_ENUM(WebAXRoleMenuListPopup, MenuListPopupRole);
STATIC_ASSERT_ENUM(WebAXRoleMenu, MenuRole);
STATIC_ASSERT_ENUM(WebAXRoleMeter, MeterRole);
STATIC_ASSERT_ENUM(WebAXRoleNavigation, NavigationRole);
STATIC_ASSERT_ENUM(WebAXRoleNone, NoneRole);
STATIC_ASSERT_ENUM(WebAXRoleNote, NoteRole);
STATIC_ASSERT_ENUM(WebAXRoleOutline, OutlineRole);
STATIC_ASSERT_ENUM(WebAXRoleParagraph, ParagraphRole);
STATIC_ASSERT_ENUM(WebAXRolePopUpButton, PopUpButtonRole);
STATIC_ASSERT_ENUM(WebAXRolePre, PreRole);
STATIC_ASSERT_ENUM(WebAXRolePresentational, PresentationalRole);
STATIC_ASSERT_ENUM(WebAXRoleProgressIndicator, ProgressIndicatorRole);
STATIC_ASSERT_ENUM(WebAXRoleRadioButton, RadioButtonRole);
STATIC_ASSERT_ENUM(WebAXRoleRadioGroup, RadioGroupRole);
STATIC_ASSERT_ENUM(WebAXRoleRegion, RegionRole);
STATIC_ASSERT_ENUM(WebAXRoleRootWebArea, RootWebAreaRole);
STATIC_ASSERT_ENUM(WebAXRoleLineBreak, LineBreakRole);
STATIC_ASSERT_ENUM(WebAXRoleRowHeader, RowHeaderRole);
STATIC_ASSERT_ENUM(WebAXRoleRow, RowRole);
STATIC_ASSERT_ENUM(WebAXRoleRuby, RubyRole);
STATIC_ASSERT_ENUM(WebAXRoleRuler, RulerRole);
STATIC_ASSERT_ENUM(WebAXRoleSVGRoot, SVGRootRole);
STATIC_ASSERT_ENUM(WebAXRoleScrollArea, ScrollAreaRole);
STATIC_ASSERT_ENUM(WebAXRoleScrollBar, ScrollBarRole);
STATIC_ASSERT_ENUM(WebAXRoleSeamlessWebArea, SeamlessWebAreaRole);
STATIC_ASSERT_ENUM(WebAXRoleSearch, SearchRole);
STATIC_ASSERT_ENUM(WebAXRoleSearchBox, SearchBoxRole);
STATIC_ASSERT_ENUM(WebAXRoleSlider, SliderRole);
STATIC_ASSERT_ENUM(WebAXRoleSliderThumb, SliderThumbRole);
STATIC_ASSERT_ENUM(WebAXRoleSpinButtonPart, SpinButtonPartRole);
STATIC_ASSERT_ENUM(WebAXRoleSpinButton, SpinButtonRole);
STATIC_ASSERT_ENUM(WebAXRoleSplitter, SplitterRole);
STATIC_ASSERT_ENUM(WebAXRoleStaticText, StaticTextRole);
STATIC_ASSERT_ENUM(WebAXRoleStatus, StatusRole);
STATIC_ASSERT_ENUM(WebAXRoleSwitch, SwitchRole);
STATIC_ASSERT_ENUM(WebAXRoleTabGroup, TabGroupRole);
STATIC_ASSERT_ENUM(WebAXRoleTabList, TabListRole);
STATIC_ASSERT_ENUM(WebAXRoleTabPanel, TabPanelRole);
STATIC_ASSERT_ENUM(WebAXRoleTab, TabRole);
STATIC_ASSERT_ENUM(WebAXRoleTableHeaderContainer, TableHeaderContainerRole);
STATIC_ASSERT_ENUM(WebAXRoleTable, TableRole);
STATIC_ASSERT_ENUM(WebAXRoleTextField, TextFieldRole);
STATIC_ASSERT_ENUM(WebAXRoleTime, TimeRole);
STATIC_ASSERT_ENUM(WebAXRoleTimer, TimerRole);
STATIC_ASSERT_ENUM(WebAXRoleToggleButton, ToggleButtonRole);
STATIC_ASSERT_ENUM(WebAXRoleToolbar, ToolbarRole);
STATIC_ASSERT_ENUM(WebAXRoleTreeGrid, TreeGridRole);
STATIC_ASSERT_ENUM(WebAXRoleTreeItem, TreeItemRole);
STATIC_ASSERT_ENUM(WebAXRoleTree, TreeRole);
STATIC_ASSERT_ENUM(WebAXRoleUnknown, UnknownRole);
STATIC_ASSERT_ENUM(WebAXRoleUserInterfaceTooltip, UserInterfaceTooltipRole);
STATIC_ASSERT_ENUM(WebAXRoleWebArea, WebAreaRole);
STATIC_ASSERT_ENUM(WebAXRoleWindow, WindowRole);

STATIC_ASSERT_ENUM(WebAXStateBusy, AXBusyState);
STATIC_ASSERT_ENUM(WebAXStateChecked, AXCheckedState);
STATIC_ASSERT_ENUM(WebAXStateEnabled, AXEnabledState);
STATIC_ASSERT_ENUM(WebAXStateExpanded, AXExpandedState);
STATIC_ASSERT_ENUM(WebAXStateFocusable, AXFocusableState);
STATIC_ASSERT_ENUM(WebAXStateFocused, AXFocusedState);
STATIC_ASSERT_ENUM(WebAXStateHaspopup, AXHaspopupState);
STATIC_ASSERT_ENUM(WebAXStateHovered, AXHoveredState);
STATIC_ASSERT_ENUM(WebAXStateInvisible, AXInvisibleState);
STATIC_ASSERT_ENUM(WebAXStateLinked, AXLinkedState);
STATIC_ASSERT_ENUM(WebAXStateMultiline, AXMultilineState);
STATIC_ASSERT_ENUM(WebAXStateMultiselectable, AXMultiselectableState);
STATIC_ASSERT_ENUM(WebAXStateOffscreen, AXOffscreenState);
STATIC_ASSERT_ENUM(WebAXStatePressed, AXPressedState);
STATIC_ASSERT_ENUM(WebAXStateProtected, AXProtectedState);
STATIC_ASSERT_ENUM(WebAXStateReadonly, AXReadonlyState);
STATIC_ASSERT_ENUM(WebAXStateRequired, AXRequiredState);
STATIC_ASSERT_ENUM(WebAXStateSelectable, AXSelectableState);
STATIC_ASSERT_ENUM(WebAXStateSelected, AXSelectedState);
STATIC_ASSERT_ENUM(WebAXStateVertical, AXVerticalState);
STATIC_ASSERT_ENUM(WebAXStateVisited, AXVisitedState);

STATIC_ASSERT_ENUM(WebAXTextDirectionLR, AccessibilityTextDirectionLTR);
STATIC_ASSERT_ENUM(WebAXTextDirectionRL, AccessibilityTextDirectionRTL);
STATIC_ASSERT_ENUM(WebAXTextDirectionTB, AccessibilityTextDirectionTTB);
STATIC_ASSERT_ENUM(WebAXTextDirectionBT, AccessibilityTextDirectionBTT);

STATIC_ASSERT_ENUM(WebAXSortDirectionUndefined, SortDirectionUndefined);
STATIC_ASSERT_ENUM(WebAXSortDirectionNone, SortDirectionNone);
STATIC_ASSERT_ENUM(WebAXSortDirectionAscending, SortDirectionAscending);
STATIC_ASSERT_ENUM(WebAXSortDirectionDescending, SortDirectionDescending);
STATIC_ASSERT_ENUM(WebAXSortDirectionOther, SortDirectionOther);

STATIC_ASSERT_ENUM(WebAXExpandedUndefined, ExpandedUndefined);
STATIC_ASSERT_ENUM(WebAXExpandedCollapsed, ExpandedCollapsed);
STATIC_ASSERT_ENUM(WebAXExpandedExpanded, ExpandedExpanded);

STATIC_ASSERT_ENUM(WebAXOrientationUndefined, AccessibilityOrientationUndefined);
STATIC_ASSERT_ENUM(WebAXOrientationVertical, AccessibilityOrientationVertical);
STATIC_ASSERT_ENUM(WebAXOrientationHorizontal, AccessibilityOrientationHorizontal);

STATIC_ASSERT_ENUM(WebAXAriaCurrentStateUndefined, AriaCurrentStateUndefined);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStateFalse, AriaCurrentStateFalse);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStateTrue, AriaCurrentStateTrue);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStatePage, AriaCurrentStatePage);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStateStep, AriaCurrentStateStep);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStateLocation, AriaCurrentStateLocation);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStateDate, AriaCurrentStateDate);
STATIC_ASSERT_ENUM(WebAXAriaCurrentStateTime, AriaCurrentStateTime);

STATIC_ASSERT_ENUM(WebAXInvalidStateUndefined, InvalidStateUndefined);
STATIC_ASSERT_ENUM(WebAXInvalidStateFalse, InvalidStateFalse);
STATIC_ASSERT_ENUM(WebAXInvalidStateTrue, InvalidStateTrue);
STATIC_ASSERT_ENUM(WebAXInvalidStateSpelling, InvalidStateSpelling);
STATIC_ASSERT_ENUM(WebAXInvalidStateGrammar, InvalidStateGrammar);
STATIC_ASSERT_ENUM(WebAXInvalidStateOther, InvalidStateOther);

STATIC_ASSERT_ENUM(WebAXMarkerTypeSpelling, DocumentMarker::Spelling);
STATIC_ASSERT_ENUM(WebAXMarkerTypeGrammar, DocumentMarker::Grammar);
STATIC_ASSERT_ENUM(WebAXMarkerTypeTextMatch, DocumentMarker::TextMatch);

STATIC_ASSERT_ENUM(WebAXTextStyleNone, TextStyleNone);
STATIC_ASSERT_ENUM(WebAXTextStyleBold, TextStyleBold);
STATIC_ASSERT_ENUM(WebAXTextStyleItalic, TextStyleItalic);
STATIC_ASSERT_ENUM(WebAXTextStyleUnderline, TextStyleUnderline);
STATIC_ASSERT_ENUM(WebAXTextStyleLineThrough, TextStyleLineThrough);

STATIC_ASSERT_ENUM(WebAXNameFromUninitialized, AXNameFromUninitialized);
STATIC_ASSERT_ENUM(WebAXNameFromAttribute, AXNameFromAttribute);
STATIC_ASSERT_ENUM(WebAXNameFromCaption, AXNameFromCaption);
STATIC_ASSERT_ENUM(WebAXNameFromContents, AXNameFromContents);
STATIC_ASSERT_ENUM(WebAXNameFromPlaceholder, AXNameFromPlaceholder);
STATIC_ASSERT_ENUM(WebAXNameFromRelatedElement, AXNameFromRelatedElement);
STATIC_ASSERT_ENUM(WebAXNameFromValue, AXNameFromValue);
STATIC_ASSERT_ENUM(WebAXNameFromTitle, AXNameFromTitle);

STATIC_ASSERT_ENUM(WebAXDescriptionFromUninitialized, AXDescriptionFromUninitialized);
STATIC_ASSERT_ENUM(WebAXDescriptionFromAttribute, AXDescriptionFromAttribute);
STATIC_ASSERT_ENUM(WebAXDescriptionFromContents, AXDescriptionFromContents);
STATIC_ASSERT_ENUM(WebAXDescriptionFromPlaceholder, AXDescriptionFromPlaceholder);
STATIC_ASSERT_ENUM(WebAXDescriptionFromRelatedElement, AXDescriptionFromRelatedElement);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::Uncached, ApplicationCacheHost::UNCACHED);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Idle, ApplicationCacheHost::IDLE);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Checking, ApplicationCacheHost::CHECKING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Downloading, ApplicationCacheHost::DOWNLOADING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::UpdateReady, ApplicationCacheHost::UPDATEREADY);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Obsolete, ApplicationCacheHost::OBSOLETE);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::CheckingEvent, ApplicationCacheHost::CHECKING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ErrorEvent, ApplicationCacheHost::ERROR_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::NoUpdateEvent, ApplicationCacheHost::NOUPDATE_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::DownloadingEvent, ApplicationCacheHost::DOWNLOADING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ProgressEvent, ApplicationCacheHost::PROGRESS_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::UpdateReadyEvent, ApplicationCacheHost::UPDATEREADY_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::CachedEvent, ApplicationCacheHost::CACHED_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ObsoleteEvent, ApplicationCacheHost::OBSOLETE_EVENT);

STATIC_ASSERT_ENUM(WebClientRedirectPolicy::NotClientRedirect, ClientRedirectPolicy::NotClientRedirect);
STATIC_ASSERT_ENUM(WebClientRedirectPolicy::ClientRedirect, ClientRedirectPolicy::ClientRedirect);

STATIC_ASSERT_ENUM(WebCursorInfo::TypePointer, Cursor::Pointer);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeCross, Cursor::Cross);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeHand, Cursor::Hand);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeIBeam, Cursor::IBeam);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeWait, Cursor::Wait);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeHelp, Cursor::Help);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeEastResize, Cursor::EastResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthResize, Cursor::NorthResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthEastResize, Cursor::NorthEastResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthWestResize, Cursor::NorthWestResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeSouthResize, Cursor::SouthResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeSouthEastResize, Cursor::SouthEastResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeSouthWestResize, Cursor::SouthWestResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeWestResize, Cursor::WestResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthSouthResize, Cursor::NorthSouthResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeEastWestResize, Cursor::EastWestResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthEastSouthWestResize, Cursor::NorthEastSouthWestResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthWestSouthEastResize, Cursor::NorthWestSouthEastResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeColumnResize, Cursor::ColumnResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeRowResize, Cursor::RowResize);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeMiddlePanning, Cursor::MiddlePanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeEastPanning, Cursor::EastPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthPanning, Cursor::NorthPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthEastPanning, Cursor::NorthEastPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNorthWestPanning, Cursor::NorthWestPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeSouthPanning, Cursor::SouthPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeSouthEastPanning, Cursor::SouthEastPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeSouthWestPanning, Cursor::SouthWestPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeWestPanning, Cursor::WestPanning);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeMove, Cursor::Move);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeVerticalText, Cursor::VerticalText);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeCell, Cursor::Cell);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeContextMenu, Cursor::ContextMenu);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeAlias, Cursor::Alias);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeProgress, Cursor::Progress);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNoDrop, Cursor::NoDrop);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeCopy, Cursor::Copy);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNone, Cursor::None);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeNotAllowed, Cursor::NotAllowed);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeZoomIn, Cursor::ZoomIn);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeZoomOut, Cursor::ZoomOut);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeGrab, Cursor::Grab);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeGrabbing, Cursor::Grabbing);
STATIC_ASSERT_ENUM(WebCursorInfo::TypeCustom, Cursor::Custom);

STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilyNone, FontDescription::NoFamily);
STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilyStandard, FontDescription::StandardFamily);
STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilySerif, FontDescription::SerifFamily);
STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilySansSerif, FontDescription::SansSerifFamily);
STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilyMonospace, FontDescription::MonospaceFamily);
STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilyCursive, FontDescription::CursiveFamily);
STATIC_ASSERT_ENUM(WebFontDescription::GenericFamilyFantasy, FontDescription::FantasyFamily);

STATIC_ASSERT_ENUM(WebFontDescription::SmoothingAuto, AutoSmoothing);
STATIC_ASSERT_ENUM(WebFontDescription::SmoothingNone, NoSmoothing);
STATIC_ASSERT_ENUM(WebFontDescription::SmoothingGrayscale, Antialiased);
STATIC_ASSERT_ENUM(WebFontDescription::SmoothingSubpixel, SubpixelAntialiased);

STATIC_ASSERT_ENUM(WebFontDescription::Weight100, FontWeight100);
STATIC_ASSERT_ENUM(WebFontDescription::Weight200, FontWeight200);
STATIC_ASSERT_ENUM(WebFontDescription::Weight300, FontWeight300);
STATIC_ASSERT_ENUM(WebFontDescription::Weight400, FontWeight400);
STATIC_ASSERT_ENUM(WebFontDescription::Weight500, FontWeight500);
STATIC_ASSERT_ENUM(WebFontDescription::Weight600, FontWeight600);
STATIC_ASSERT_ENUM(WebFontDescription::Weight700, FontWeight700);
STATIC_ASSERT_ENUM(WebFontDescription::Weight800, FontWeight800);
STATIC_ASSERT_ENUM(WebFontDescription::Weight900, FontWeight900);
STATIC_ASSERT_ENUM(WebFontDescription::WeightNormal, FontWeightNormal);
STATIC_ASSERT_ENUM(WebFontDescription::WeightBold, FontWeightBold);

STATIC_ASSERT_ENUM(WebFrameOwnerProperties::ScrollingMode::Auto, ScrollbarAuto);
STATIC_ASSERT_ENUM(WebFrameOwnerProperties::ScrollingMode::AlwaysOff, ScrollbarAlwaysOff);
STATIC_ASSERT_ENUM(WebFrameOwnerProperties::ScrollingMode::AlwaysOn, ScrollbarAlwaysOn);

STATIC_ASSERT_ENUM(WebIconURL::TypeInvalid, InvalidIcon);
STATIC_ASSERT_ENUM(WebIconURL::TypeFavicon, Favicon);
STATIC_ASSERT_ENUM(WebIconURL::TypeTouch, TouchIcon);
STATIC_ASSERT_ENUM(WebIconURL::TypeTouchPrecomposed, TouchPrecomposedIcon);

STATIC_ASSERT_ENUM(WebInputEvent::ShiftKey, PlatformEvent::ShiftKey);
STATIC_ASSERT_ENUM(WebInputEvent::ControlKey, PlatformEvent::CtrlKey);
STATIC_ASSERT_ENUM(WebInputEvent::AltKey, PlatformEvent::AltKey);
STATIC_ASSERT_ENUM(WebInputEvent::MetaKey, PlatformEvent::MetaKey);
STATIC_ASSERT_ENUM(WebInputEvent::AltGrKey, PlatformEvent::AltGrKey);
STATIC_ASSERT_ENUM(WebInputEvent::FnKey, PlatformEvent::FnKey);
STATIC_ASSERT_ENUM(WebInputEvent::SymbolKey, PlatformEvent::SymbolKey);
STATIC_ASSERT_ENUM(WebInputEvent::IsKeyPad, PlatformEvent::IsKeyPad);
STATIC_ASSERT_ENUM(WebInputEvent::IsAutoRepeat, PlatformEvent::IsAutoRepeat);
STATIC_ASSERT_ENUM(WebInputEvent::IsLeft, PlatformEvent::IsLeft);
STATIC_ASSERT_ENUM(WebInputEvent::IsRight, PlatformEvent::IsRight);
STATIC_ASSERT_ENUM(WebInputEvent::IsTouchAccessibility, PlatformEvent::IsTouchAccessibility);
STATIC_ASSERT_ENUM(WebInputEvent::IsComposing, PlatformEvent::IsComposing);
STATIC_ASSERT_ENUM(WebInputEvent::LeftButtonDown, PlatformEvent::LeftButtonDown);
STATIC_ASSERT_ENUM(WebInputEvent::MiddleButtonDown, PlatformEvent::MiddleButtonDown);
STATIC_ASSERT_ENUM(WebInputEvent::RightButtonDown, PlatformEvent::RightButtonDown);
STATIC_ASSERT_ENUM(WebInputEvent::CapsLockOn, PlatformEvent::CapsLockOn);
STATIC_ASSERT_ENUM(WebInputEvent::NumLockOn, PlatformEvent::NumLockOn);
STATIC_ASSERT_ENUM(WebInputEvent::ScrollLockOn, PlatformEvent::ScrollLockOn);

STATIC_ASSERT_ENUM(WebMediaPlayer::ReadyStateHaveNothing, HTMLMediaElement::HAVE_NOTHING);
STATIC_ASSERT_ENUM(WebMediaPlayer::ReadyStateHaveMetadata, HTMLMediaElement::HAVE_METADATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::ReadyStateHaveCurrentData, HTMLMediaElement::HAVE_CURRENT_DATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::ReadyStateHaveFutureData, HTMLMediaElement::HAVE_FUTURE_DATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::ReadyStateHaveEnoughData, HTMLMediaElement::HAVE_ENOUGH_DATA);

STATIC_ASSERT_ENUM(WebMouseEvent::ButtonNone, NoButton);
STATIC_ASSERT_ENUM(WebMouseEvent::ButtonLeft, LeftButton);
STATIC_ASSERT_ENUM(WebMouseEvent::ButtonMiddle, MiddleButton);
STATIC_ASSERT_ENUM(WebMouseEvent::ButtonRight, RightButton);

STATIC_ASSERT_ENUM(WebScrollbar::Horizontal, HorizontalScrollbar);
STATIC_ASSERT_ENUM(WebScrollbar::Vertical, VerticalScrollbar);

STATIC_ASSERT_ENUM(WebScrollbar::ScrollByLine, ScrollByLine);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollByPage, ScrollByPage);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollByDocument, ScrollByDocument);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollByPixel, ScrollByPixel);

STATIC_ASSERT_ENUM(WebScrollbar::RegularScrollbar, RegularScrollbar);
STATIC_ASSERT_ENUM(WebScrollbar::SmallScrollbar, SmallScrollbar);
STATIC_ASSERT_ENUM(WebScrollbar::NoPart, NoPart);
STATIC_ASSERT_ENUM(WebScrollbar::BackButtonStartPart, BackButtonStartPart);
STATIC_ASSERT_ENUM(WebScrollbar::ForwardButtonStartPart, ForwardButtonStartPart);
STATIC_ASSERT_ENUM(WebScrollbar::BackTrackPart, BackTrackPart);
STATIC_ASSERT_ENUM(WebScrollbar::ThumbPart, ThumbPart);
STATIC_ASSERT_ENUM(WebScrollbar::ForwardTrackPart, ForwardTrackPart);
STATIC_ASSERT_ENUM(WebScrollbar::BackButtonEndPart, BackButtonEndPart);
STATIC_ASSERT_ENUM(WebScrollbar::ForwardButtonEndPart, ForwardButtonEndPart);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollbarBGPart, ScrollbarBGPart);
STATIC_ASSERT_ENUM(WebScrollbar::TrackBGPart, TrackBGPart);
STATIC_ASSERT_ENUM(WebScrollbar::AllParts, AllParts);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollbarOverlayStyleDefault, ScrollbarOverlayStyleDefault);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollbarOverlayStyleDark, ScrollbarOverlayStyleDark);
STATIC_ASSERT_ENUM(WebScrollbar::ScrollbarOverlayStyleLight, ScrollbarOverlayStyleLight);

STATIC_ASSERT_ENUM(WebScrollbarBehavior::ButtonNone, NoButton);
STATIC_ASSERT_ENUM(WebScrollbarBehavior::ButtonLeft, LeftButton);
STATIC_ASSERT_ENUM(WebScrollbarBehavior::ButtonMiddle, MiddleButton);
STATIC_ASSERT_ENUM(WebScrollbarBehavior::ButtonRight, RightButton);

STATIC_ASSERT_ENUM(WebSettings::EditingBehaviorMac, EditingMacBehavior);
STATIC_ASSERT_ENUM(WebSettings::EditingBehaviorWin, EditingWindowsBehavior);
STATIC_ASSERT_ENUM(WebSettings::EditingBehaviorUnix, EditingUnixBehavior);
STATIC_ASSERT_ENUM(WebSettings::EditingBehaviorAndroid, EditingAndroidBehavior);

STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::False, PassiveListenerDefault::False);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::True, PassiveListenerDefault::True);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::DocumentTrue, PassiveListenerDefault::DocumentTrue);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::ForceAllTrue, PassiveListenerDefault::ForceAllTrue);

STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionUnknownError, UnknownError);
STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionConstraintError, ConstraintError);
STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionDataError, DataError);
STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionVersionError, VersionError);
STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionAbortError, AbortError);
STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionQuotaError, QuotaExceededError);
STATIC_ASSERT_ENUM(WebIDBDatabaseExceptionTimeoutError, TimeoutError);

STATIC_ASSERT_ENUM(WebIDBKeyTypeInvalid, IDBKey::InvalidType);
STATIC_ASSERT_ENUM(WebIDBKeyTypeArray, IDBKey::ArrayType);
STATIC_ASSERT_ENUM(WebIDBKeyTypeBinary, IDBKey::BinaryType);
STATIC_ASSERT_ENUM(WebIDBKeyTypeString, IDBKey::StringType);
STATIC_ASSERT_ENUM(WebIDBKeyTypeDate, IDBKey::DateType);
STATIC_ASSERT_ENUM(WebIDBKeyTypeNumber, IDBKey::NumberType);

STATIC_ASSERT_ENUM(WebIDBKeyPathTypeNull, IDBKeyPath::NullType);
STATIC_ASSERT_ENUM(WebIDBKeyPathTypeString, IDBKeyPath::StringType);
STATIC_ASSERT_ENUM(WebIDBKeyPathTypeArray, IDBKeyPath::ArrayType);

STATIC_ASSERT_ENUM(WebIDBMetadata::NoVersion, IDBDatabaseMetadata::NoVersion);

STATIC_ASSERT_ENUM(WebFileSystem::TypeTemporary, FileSystemTypeTemporary);
STATIC_ASSERT_ENUM(WebFileSystem::TypePersistent, FileSystemTypePersistent);
STATIC_ASSERT_ENUM(WebFileSystem::TypeExternal, FileSystemTypeExternal);
STATIC_ASSERT_ENUM(WebFileSystem::TypeIsolated, FileSystemTypeIsolated);
STATIC_ASSERT_ENUM(WebFileInfo::TypeUnknown, FileMetadata::TypeUnknown);
STATIC_ASSERT_ENUM(WebFileInfo::TypeFile, FileMetadata::TypeFile);
STATIC_ASSERT_ENUM(WebFileInfo::TypeDirectory, FileMetadata::TypeDirectory);

STATIC_ASSERT_ENUM(WebFileErrorNotFound, FileError::NOT_FOUND_ERR);
STATIC_ASSERT_ENUM(WebFileErrorSecurity, FileError::SECURITY_ERR);
STATIC_ASSERT_ENUM(WebFileErrorAbort, FileError::ABORT_ERR);
STATIC_ASSERT_ENUM(WebFileErrorNotReadable, FileError::NOT_READABLE_ERR);
STATIC_ASSERT_ENUM(WebFileErrorEncoding, FileError::ENCODING_ERR);
STATIC_ASSERT_ENUM(WebFileErrorNoModificationAllowed, FileError::NO_MODIFICATION_ALLOWED_ERR);
STATIC_ASSERT_ENUM(WebFileErrorInvalidState, FileError::INVALID_STATE_ERR);
STATIC_ASSERT_ENUM(WebFileErrorSyntax, FileError::SYNTAX_ERR);
STATIC_ASSERT_ENUM(WebFileErrorInvalidModification, FileError::INVALID_MODIFICATION_ERR);
STATIC_ASSERT_ENUM(WebFileErrorQuotaExceeded, FileError::QUOTA_EXCEEDED_ERR);
STATIC_ASSERT_ENUM(WebFileErrorTypeMismatch, FileError::TYPE_MISMATCH_ERR);
STATIC_ASSERT_ENUM(WebFileErrorPathExists, FileError::PATH_EXISTS_ERR);

STATIC_ASSERT_ENUM(WebTextCheckingTypeSpelling, TextCheckingTypeSpelling);
STATIC_ASSERT_ENUM(WebTextCheckingTypeGrammar, TextCheckingTypeGrammar);

// TODO(rouslan): Remove these comparisons between text-checking and text-decoration enum values after removing the
// deprecated constructor WebTextCheckingResult(WebTextCheckingType).
STATIC_ASSERT_ENUM(WebTextCheckingTypeSpelling, TextDecorationTypeSpelling);
STATIC_ASSERT_ENUM(WebTextCheckingTypeGrammar, TextDecorationTypeGrammar);

STATIC_ASSERT_ENUM(WebTextDecorationTypeSpelling, TextDecorationTypeSpelling);
STATIC_ASSERT_ENUM(WebTextDecorationTypeGrammar, TextDecorationTypeGrammar);
STATIC_ASSERT_ENUM(WebTextDecorationTypeInvisibleSpellcheck, TextDecorationTypeInvisibleSpellcheck);

STATIC_ASSERT_ENUM(WebStorageQuotaErrorNotSupported, NotSupportedError);
STATIC_ASSERT_ENUM(WebStorageQuotaErrorInvalidModification, InvalidModificationError);
STATIC_ASSERT_ENUM(WebStorageQuotaErrorInvalidAccess, InvalidAccessError);
STATIC_ASSERT_ENUM(WebStorageQuotaErrorAbort, AbortError);

STATIC_ASSERT_ENUM(WebStorageQuotaTypeTemporary, DeprecatedStorageQuota::Temporary);
STATIC_ASSERT_ENUM(WebStorageQuotaTypePersistent, DeprecatedStorageQuota::Persistent);

STATIC_ASSERT_ENUM(WebPageVisibilityStateVisible, PageVisibilityStateVisible);
STATIC_ASSERT_ENUM(WebPageVisibilityStateHidden, PageVisibilityStateHidden);
STATIC_ASSERT_ENUM(WebPageVisibilityStatePrerender, PageVisibilityStatePrerender);

STATIC_ASSERT_ENUM(WebMediaStreamSource::TypeAudio, MediaStreamSource::TypeAudio);
STATIC_ASSERT_ENUM(WebMediaStreamSource::TypeVideo, MediaStreamSource::TypeVideo);
STATIC_ASSERT_ENUM(WebMediaStreamSource::ReadyStateLive, MediaStreamSource::ReadyStateLive);
STATIC_ASSERT_ENUM(WebMediaStreamSource::ReadyStateMuted, MediaStreamSource::ReadyStateMuted);
STATIC_ASSERT_ENUM(WebMediaStreamSource::ReadyStateEnded, MediaStreamSource::ReadyStateEnded);

STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::OtherError, SpeechRecognitionError::ErrorCodeOther);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::NoSpeechError, SpeechRecognitionError::ErrorCodeNoSpeech);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::AbortedError, SpeechRecognitionError::ErrorCodeAborted);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::AudioCaptureError, SpeechRecognitionError::ErrorCodeAudioCapture);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::NetworkError, SpeechRecognitionError::ErrorCodeNetwork);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::NotAllowedError, SpeechRecognitionError::ErrorCodeNotAllowed);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::ServiceNotAllowedError, SpeechRecognitionError::ErrorCodeServiceNotAllowed);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::BadGrammarError, SpeechRecognitionError::ErrorCodeBadGrammar);
STATIC_ASSERT_ENUM(WebSpeechRecognizerClient::LanguageNotSupportedError, SpeechRecognitionError::ErrorCodeLanguageNotSupported);

STATIC_ASSERT_ENUM(WebReferrerPolicyAlways, ReferrerPolicyAlways);
STATIC_ASSERT_ENUM(WebReferrerPolicyDefault, ReferrerPolicyDefault);
STATIC_ASSERT_ENUM(WebReferrerPolicyNoReferrerWhenDowngrade, ReferrerPolicyNoReferrerWhenDowngrade);
STATIC_ASSERT_ENUM(WebReferrerPolicyNever, ReferrerPolicyNever);
STATIC_ASSERT_ENUM(WebReferrerPolicyOrigin, ReferrerPolicyOrigin);
STATIC_ASSERT_ENUM(WebReferrerPolicyOriginWhenCrossOrigin, ReferrerPolicyOriginWhenCrossOrigin);
STATIC_ASSERT_ENUM(WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin, ReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin);

STATIC_ASSERT_ENUM(WebContentSecurityPolicyTypeReport, ContentSecurityPolicyHeaderTypeReport);
STATIC_ASSERT_ENUM(WebContentSecurityPolicyTypeEnforce, ContentSecurityPolicyHeaderTypeEnforce);

STATIC_ASSERT_ENUM(WebContentSecurityPolicySourceHTTP, ContentSecurityPolicyHeaderSourceHTTP);
STATIC_ASSERT_ENUM(WebContentSecurityPolicySourceMeta, ContentSecurityPolicyHeaderSourceMeta);

STATIC_ASSERT_ENUM(WebURLResponse::HTTPVersionUnknown, ResourceResponse::HTTPVersionUnknown);
STATIC_ASSERT_ENUM(WebURLResponse::HTTPVersion_0_9,
    ResourceResponse::HTTPVersion_0_9);
STATIC_ASSERT_ENUM(WebURLResponse::HTTPVersion_1_0,
    ResourceResponse::HTTPVersion_1_0);
STATIC_ASSERT_ENUM(WebURLResponse::HTTPVersion_1_1,
    ResourceResponse::HTTPVersion_1_1);
STATIC_ASSERT_ENUM(WebURLResponse::HTTPVersion_2_0,
    ResourceResponse::HTTPVersion_2_0);

STATIC_ASSERT_ENUM(WebURLRequest::PriorityUnresolved, ResourceLoadPriorityUnresolved);
STATIC_ASSERT_ENUM(WebURLRequest::PriorityVeryLow, ResourceLoadPriorityVeryLow);
STATIC_ASSERT_ENUM(WebURLRequest::PriorityLow, ResourceLoadPriorityLow);
STATIC_ASSERT_ENUM(WebURLRequest::PriorityMedium, ResourceLoadPriorityMedium);
STATIC_ASSERT_ENUM(WebURLRequest::PriorityHigh, ResourceLoadPriorityHigh);
STATIC_ASSERT_ENUM(WebURLRequest::PriorityVeryHigh, ResourceLoadPriorityVeryHigh);

STATIC_ASSERT_ENUM(WebNavigationPolicyIgnore, NavigationPolicyIgnore);
STATIC_ASSERT_ENUM(WebNavigationPolicyDownload, NavigationPolicyDownload);
STATIC_ASSERT_ENUM(WebNavigationPolicyCurrentTab, NavigationPolicyCurrentTab);
STATIC_ASSERT_ENUM(WebNavigationPolicyNewBackgroundTab, NavigationPolicyNewBackgroundTab);
STATIC_ASSERT_ENUM(WebNavigationPolicyNewForegroundTab, NavigationPolicyNewForegroundTab);
STATIC_ASSERT_ENUM(WebNavigationPolicyNewWindow, NavigationPolicyNewWindow);
STATIC_ASSERT_ENUM(WebNavigationPolicyNewPopup, NavigationPolicyNewPopup);

STATIC_ASSERT_ENUM(WebStandardCommit, StandardCommit);
STATIC_ASSERT_ENUM(WebBackForwardCommit, BackForwardCommit);
STATIC_ASSERT_ENUM(WebInitialCommitInChildFrame, InitialCommitInChildFrame);
STATIC_ASSERT_ENUM(WebHistoryInertCommit, HistoryInertCommit);

STATIC_ASSERT_ENUM(WebHistorySameDocumentLoad, HistorySameDocumentLoad);
STATIC_ASSERT_ENUM(WebHistoryDifferentDocumentLoad, HistoryDifferentDocumentLoad);

STATIC_ASSERT_ENUM(WebHistoryScrollRestorationManual, ScrollRestorationManual);
STATIC_ASSERT_ENUM(WebHistoryScrollRestorationAuto, ScrollRestorationAuto);

STATIC_ASSERT_ENUM(WebConsoleMessage::LevelDebug, DebugMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelLog, LogMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelWarning, WarningMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelError, ErrorMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelInfo, InfoMessageLevel);

STATIC_ASSERT_ENUM(WebCustomHandlersNew, NavigatorContentUtilsClient::CustomHandlersNew);
STATIC_ASSERT_ENUM(WebCustomHandlersRegistered, NavigatorContentUtilsClient::CustomHandlersRegistered);
STATIC_ASSERT_ENUM(WebCustomHandlersDeclined, NavigatorContentUtilsClient::CustomHandlersDeclined);

STATIC_ASSERT_ENUM(WebTouchActionNone, TouchActionNone);
STATIC_ASSERT_ENUM(WebTouchActionPanLeft, TouchActionPanLeft);
STATIC_ASSERT_ENUM(WebTouchActionPanRight, TouchActionPanRight);
STATIC_ASSERT_ENUM(WebTouchActionPanX, TouchActionPanX);
STATIC_ASSERT_ENUM(WebTouchActionPanUp, TouchActionPanUp);
STATIC_ASSERT_ENUM(WebTouchActionPanDown, TouchActionPanDown);
STATIC_ASSERT_ENUM(WebTouchActionPanY, TouchActionPanY);
STATIC_ASSERT_ENUM(WebTouchActionPan, TouchActionPan);
STATIC_ASSERT_ENUM(WebTouchActionPinchZoom, TouchActionPinchZoom);
STATIC_ASSERT_ENUM(WebTouchActionManipulation, TouchActionManipulation);
STATIC_ASSERT_ENUM(WebTouchActionDoubleTapZoom, TouchActionDoubleTapZoom);
STATIC_ASSERT_ENUM(WebTouchActionAuto, TouchActionAuto);

STATIC_ASSERT_ENUM(WebSelection::NoSelection, NoSelection);
STATIC_ASSERT_ENUM(WebSelection::CaretSelection, CaretSelection);
STATIC_ASSERT_ENUM(WebSelection::RangeSelection, RangeSelection);

STATIC_ASSERT_ENUM(WebSettings::ImageAnimationPolicyAllowed, ImageAnimationPolicyAllowed);
STATIC_ASSERT_ENUM(WebSettings::ImageAnimationPolicyAnimateOnce, ImageAnimationPolicyAnimateOnce);
STATIC_ASSERT_ENUM(WebSettings::ImageAnimationPolicyNoAnimation, ImageAnimationPolicyNoAnimation);

STATIC_ASSERT_ENUM(WebSettings::V8CacheOptionsDefault, V8CacheOptionsDefault);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptionsNone, V8CacheOptionsNone);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptionsParse, V8CacheOptionsParse);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptionsCode, V8CacheOptionsCode);

STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::Default, V8CacheStrategiesForCacheStorage::Default);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::None, V8CacheStrategiesForCacheStorage::None);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::Normal, V8CacheStrategiesForCacheStorage::Normal);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::Aggressive, V8CacheStrategiesForCacheStorage::Aggressive);

STATIC_ASSERT_ENUM(WebSecurityPolicy::PolicyAreaNone, SchemeRegistry::PolicyAreaNone);
STATIC_ASSERT_ENUM(WebSecurityPolicy::PolicyAreaImage, SchemeRegistry::PolicyAreaImage);
STATIC_ASSERT_ENUM(WebSecurityPolicy::PolicyAreaStyle, SchemeRegistry::PolicyAreaStyle);
STATIC_ASSERT_ENUM(WebSecurityPolicy::PolicyAreaAll, SchemeRegistry::PolicyAreaAll);

STATIC_ASSERT_ENUM(WebSandboxFlags::None, SandboxNone);
STATIC_ASSERT_ENUM(WebSandboxFlags::Navigation, SandboxNavigation);
STATIC_ASSERT_ENUM(WebSandboxFlags::Plugins, SandboxPlugins);
STATIC_ASSERT_ENUM(WebSandboxFlags::Origin, SandboxOrigin);
STATIC_ASSERT_ENUM(WebSandboxFlags::Forms, SandboxForms);
STATIC_ASSERT_ENUM(WebSandboxFlags::Scripts, SandboxScripts);
STATIC_ASSERT_ENUM(WebSandboxFlags::TopNavigation, SandboxTopNavigation);
STATIC_ASSERT_ENUM(WebSandboxFlags::Popups, SandboxPopups);
STATIC_ASSERT_ENUM(WebSandboxFlags::AutomaticFeatures, SandboxAutomaticFeatures);
STATIC_ASSERT_ENUM(WebSandboxFlags::PointerLock, SandboxPointerLock);
STATIC_ASSERT_ENUM(WebSandboxFlags::DocumentDomain, SandboxDocumentDomain);
STATIC_ASSERT_ENUM(WebSandboxFlags::OrientationLock, SandboxOrientationLock);
STATIC_ASSERT_ENUM(WebSandboxFlags::PropagatesToAuxiliaryBrowsingContexts, SandboxPropagatesToAuxiliaryBrowsingContexts);
STATIC_ASSERT_ENUM(WebSandboxFlags::Modals, SandboxModals);

STATIC_ASSERT_ENUM(FrameLoaderClient::BeforeUnloadHandler, WebFrameClient::BeforeUnloadHandler);
STATIC_ASSERT_ENUM(FrameLoaderClient::UnloadHandler, WebFrameClient::UnloadHandler);

STATIC_ASSERT_ENUM(WebFrameLoadType::Standard, FrameLoadTypeStandard);
STATIC_ASSERT_ENUM(WebFrameLoadType::BackForward, FrameLoadTypeBackForward);
STATIC_ASSERT_ENUM(WebFrameLoadType::Reload, FrameLoadTypeReload);
STATIC_ASSERT_ENUM(WebFrameLoadType::ReloadMainResource, FrameLoadTypeReloadMainResource);
STATIC_ASSERT_ENUM(WebFrameLoadType::ReplaceCurrentItem, FrameLoadTypeReplaceCurrentItem);
STATIC_ASSERT_ENUM(WebFrameLoadType::InitialInChildFrame, FrameLoadTypeInitialInChildFrame);
STATIC_ASSERT_ENUM(WebFrameLoadType::InitialHistoryLoad, FrameLoadTypeInitialHistoryLoad);
STATIC_ASSERT_ENUM(WebFrameLoadType::ReloadBypassingCache, FrameLoadTypeReloadBypassingCache);

STATIC_ASSERT_ENUM(FrameDetachType::Remove, WebFrameClient::DetachType::Remove);
STATIC_ASSERT_ENUM(FrameDetachType::Swap, WebFrameClient::DetachType::Swap);
STATIC_ASSERT_ENUM(FrameDetachType::Remove, WebRemoteFrameClient::DetachType::Remove);
STATIC_ASSERT_ENUM(FrameDetachType::Swap, WebRemoteFrameClient::DetachType::Swap);

STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::LoadEvent, ProgressBarCompletion::LoadEvent);
STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::ResourcesBeforeDCL, ProgressBarCompletion::ResourcesBeforeDCL);
STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::DOMContentLoaded, ProgressBarCompletion::DOMContentLoaded);
STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::ResourcesBeforeDCLAndSameOriginIFrames, ProgressBarCompletion::ResourcesBeforeDCLAndSameOriginIFrames);

static_assert(kSerializedScriptValueVersion == SerializedScriptValue::wireFormatVersion, "");

} // namespace blink
