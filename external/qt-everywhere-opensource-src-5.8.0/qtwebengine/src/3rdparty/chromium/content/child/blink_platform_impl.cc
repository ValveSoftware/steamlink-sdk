// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <math.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "blink/public/resources/grit/blink_image_resources.h"
#include "blink/public/resources/grit/blink_resources.h"
#include "build/build_config.h"
#include "components/mime_util/mime_util.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/child/background_sync/background_sync_provider.h"
#include "content/child/child_thread_impl.h"
#include "content/child/content_child_helpers.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/notification_manager.h"
#include "content/child/permissions/permission_dispatcher.h"
#include "content/child/permissions/permission_dispatcher_thread_proxy.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/push_messaging/push_provider.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_url_loader_impl.h"
#include "content/child/web_url_request_util.h"
#include "content/child/websocket_bridge.h"
#include "content/child/worker_thread_registry.h"
#include "content/public/common/content_client.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "ui/base/layout.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using blink::WebData;
using blink::WebFallbackThemeEngine;
using blink::WebLocalizedString;
using blink::WebString;
using blink::WebThemeEngine;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using scheduler::WebThreadImplForWorkerScheduler;

namespace content {

namespace {


}  // namespace

static int ToMessageID(WebLocalizedString::Name name) {
  switch (name) {
    case WebLocalizedString::AXAMPMFieldText:
      return IDS_AX_AM_PM_FIELD_TEXT;
    case WebLocalizedString::AXButtonActionVerb:
      return IDS_AX_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXCalendarShowMonthSelector:
      return IDS_AX_CALENDAR_SHOW_MONTH_SELECTOR;
    case WebLocalizedString::AXCalendarShowNextMonth:
      return IDS_AX_CALENDAR_SHOW_NEXT_MONTH;
    case WebLocalizedString::AXCalendarShowPreviousMonth:
      return IDS_AX_CALENDAR_SHOW_PREVIOUS_MONTH;
    case WebLocalizedString::AXCalendarWeekDescription:
      return IDS_AX_CALENDAR_WEEK_DESCRIPTION;
    case WebLocalizedString::AXCheckedCheckBoxActionVerb:
      return IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXDateTimeFieldEmptyValueText:
      return IDS_AX_DATE_TIME_FIELD_EMPTY_VALUE_TEXT;
    case WebLocalizedString::AXDayOfMonthFieldText:
      return IDS_AX_DAY_OF_MONTH_FIELD_TEXT;
    case WebLocalizedString::AXDefaultActionVerb:
      return IDS_AX_DEFAULT_ACTION_VERB;
    case WebLocalizedString::AXHeadingText:
      return IDS_AX_ROLE_HEADING;
    case WebLocalizedString::AXHourFieldText:
      return IDS_AX_HOUR_FIELD_TEXT;
    case WebLocalizedString::AXImageMapText:
      return IDS_AX_ROLE_IMAGE_MAP;
    case WebLocalizedString::AXLinkActionVerb:
      return IDS_AX_LINK_ACTION_VERB;
    case WebLocalizedString::AXLinkText:
      return IDS_AX_ROLE_LINK;
    case WebLocalizedString::AXListMarkerText:
      return IDS_AX_ROLE_LIST_MARKER;
    case WebLocalizedString::AXMediaDefault:
      return IDS_AX_MEDIA_DEFAULT;
    case WebLocalizedString::AXMediaAudioElement:
      return IDS_AX_MEDIA_AUDIO_ELEMENT;
    case WebLocalizedString::AXMediaVideoElement:
      return IDS_AX_MEDIA_VIDEO_ELEMENT;
    case WebLocalizedString::AXMediaMuteButton:
      return IDS_AX_MEDIA_MUTE_BUTTON;
    case WebLocalizedString::AXMediaUnMuteButton:
      return IDS_AX_MEDIA_UNMUTE_BUTTON;
    case WebLocalizedString::AXMediaPlayButton:
      return IDS_AX_MEDIA_PLAY_BUTTON;
    case WebLocalizedString::AXMediaPauseButton:
      return IDS_AX_MEDIA_PAUSE_BUTTON;
    case WebLocalizedString::AXMediaSlider:
      return IDS_AX_MEDIA_SLIDER;
    case WebLocalizedString::AXMediaSliderThumb:
      return IDS_AX_MEDIA_SLIDER_THUMB;
    case WebLocalizedString::AXMediaCurrentTimeDisplay:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY;
    case WebLocalizedString::AXMediaTimeRemainingDisplay:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY;
    case WebLocalizedString::AXMediaStatusDisplay:
      return IDS_AX_MEDIA_STATUS_DISPLAY;
    case WebLocalizedString::AXMediaEnterFullscreenButton:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON;
    case WebLocalizedString::AXMediaExitFullscreenButton:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON;
    case WebLocalizedString::AXMediaShowClosedCaptionsButton:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_BUTTON;
    case WebLocalizedString::AXMediaHideClosedCaptionsButton:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_BUTTON;
    case WebLocalizedString::AxMediaCastOffButton:
      return IDS_AX_MEDIA_CAST_OFF_BUTTON;
    case WebLocalizedString::AxMediaCastOnButton:
      return IDS_AX_MEDIA_CAST_ON_BUTTON;
    case WebLocalizedString::AXMediaAudioElementHelp:
      return IDS_AX_MEDIA_AUDIO_ELEMENT_HELP;
    case WebLocalizedString::AXMediaVideoElementHelp:
      return IDS_AX_MEDIA_VIDEO_ELEMENT_HELP;
    case WebLocalizedString::AXMediaMuteButtonHelp:
      return IDS_AX_MEDIA_MUTE_BUTTON_HELP;
    case WebLocalizedString::AXMediaUnMuteButtonHelp:
      return IDS_AX_MEDIA_UNMUTE_BUTTON_HELP;
    case WebLocalizedString::AXMediaPlayButtonHelp:
      return IDS_AX_MEDIA_PLAY_BUTTON_HELP;
    case WebLocalizedString::AXMediaPauseButtonHelp:
      return IDS_AX_MEDIA_PAUSE_BUTTON_HELP;
    case WebLocalizedString::AXMediaAudioSliderHelp:
      return IDS_AX_MEDIA_AUDIO_SLIDER_HELP;
    case WebLocalizedString::AXMediaVideoSliderHelp:
      return IDS_AX_MEDIA_VIDEO_SLIDER_HELP;
    case WebLocalizedString::AXMediaSliderThumbHelp:
      return IDS_AX_MEDIA_SLIDER_THUMB_HELP;
    case WebLocalizedString::AXMediaCurrentTimeDisplayHelp:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY_HELP;
    case WebLocalizedString::AXMediaTimeRemainingDisplayHelp:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY_HELP;
    case WebLocalizedString::AXMediaStatusDisplayHelp:
      return IDS_AX_MEDIA_STATUS_DISPLAY_HELP;
    case WebLocalizedString::AXMediaEnterFullscreenButtonHelp:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON_HELP;
    case WebLocalizedString::AXMediaExitFullscreenButtonHelp:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON_HELP;
    case WebLocalizedString::AXMediaShowClosedCaptionsButtonHelp:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_BUTTON_HELP;
    case WebLocalizedString::AXMediaHideClosedCaptionsButtonHelp:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_BUTTON_HELP;
    case WebLocalizedString::AxMediaCastOffButtonHelp:
      return IDS_AX_MEDIA_CAST_OFF_BUTTON_HELP;
    case WebLocalizedString::AxMediaCastOnButtonHelp:
      return IDS_AX_MEDIA_CAST_ON_BUTTON_HELP;
    case WebLocalizedString::AXMillisecondFieldText:
      return IDS_AX_MILLISECOND_FIELD_TEXT;
    case WebLocalizedString::AXMinuteFieldText:
      return IDS_AX_MINUTE_FIELD_TEXT;
    case WebLocalizedString::AXMonthFieldText:
      return IDS_AX_MONTH_FIELD_TEXT;
    case WebLocalizedString::AXPopUpButtonActionVerb:
      return IDS_AX_POP_UP_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXRadioButtonActionVerb:
      return IDS_AX_RADIO_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXSecondFieldText:
      return IDS_AX_SECOND_FIELD_TEXT;
    case WebLocalizedString::AXTextFieldActionVerb:
      return IDS_AX_TEXT_FIELD_ACTION_VERB;
    case WebLocalizedString::AXUncheckedCheckBoxActionVerb:
      return IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXWebAreaText:
      return IDS_AX_ROLE_WEB_AREA;
    case WebLocalizedString::AXWeekOfYearFieldText:
      return IDS_AX_WEEK_OF_YEAR_FIELD_TEXT;
    case WebLocalizedString::AXYearFieldText:
      return IDS_AX_YEAR_FIELD_TEXT;
    case WebLocalizedString::CalendarClear:
      return IDS_FORM_CALENDAR_CLEAR;
    case WebLocalizedString::CalendarToday:
      return IDS_FORM_CALENDAR_TODAY;
    case WebLocalizedString::DateFormatDayInMonthLabel:
      return IDS_FORM_DATE_FORMAT_DAY_IN_MONTH;
    case WebLocalizedString::DateFormatMonthLabel:
      return IDS_FORM_DATE_FORMAT_MONTH;
    case WebLocalizedString::DateFormatYearLabel:
      return IDS_FORM_DATE_FORMAT_YEAR;
    case WebLocalizedString::DetailsLabel:
      return IDS_DETAILS_WITHOUT_SUMMARY_LABEL;
    case WebLocalizedString::DownloadButtonLabel:
      return IDS_DOWNLOAD_BUTTON_LABEL;
    case WebLocalizedString::FileButtonChooseFileLabel:
      return IDS_FORM_FILE_BUTTON_LABEL;
    case WebLocalizedString::FileButtonChooseMultipleFilesLabel:
      return IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL;
    case WebLocalizedString::FileButtonNoFileSelectedLabel:
      return IDS_FORM_FILE_NO_FILE_LABEL;
    case WebLocalizedString::InputElementAltText:
      return IDS_FORM_INPUT_ALT;
    case WebLocalizedString::KeygenMenuHighGradeKeySize:
      return IDS_KEYGEN_HIGH_GRADE_KEY;
    case WebLocalizedString::KeygenMenuMediumGradeKeySize:
      return IDS_KEYGEN_MED_GRADE_KEY;
    case WebLocalizedString::MissingPluginText:
      return IDS_PLUGIN_INITIALIZATION_ERROR;
    case WebLocalizedString::MultipleFileUploadText:
      return IDS_FORM_FILE_MULTIPLE_UPLOAD;
    case WebLocalizedString::OtherColorLabel:
      return IDS_FORM_OTHER_COLOR_LABEL;
    case WebLocalizedString::OtherDateLabel:
        return IDS_FORM_OTHER_DATE_LABEL;
    case WebLocalizedString::OtherMonthLabel:
      return IDS_FORM_OTHER_MONTH_LABEL;
    case WebLocalizedString::OtherTimeLabel:
      return IDS_FORM_OTHER_TIME_LABEL;
    case WebLocalizedString::OtherWeekLabel:
      return IDS_FORM_OTHER_WEEK_LABEL;
    case WebLocalizedString::PlaceholderForDayOfMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_DAY_OF_MONTH_FIELD;
    case WebLocalizedString::PlaceholderForMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_MONTH_FIELD;
    case WebLocalizedString::PlaceholderForYearField:
      return IDS_FORM_PLACEHOLDER_FOR_YEAR_FIELD;
    case WebLocalizedString::ResetButtonDefaultLabel:
      return IDS_FORM_RESET_LABEL;
    case WebLocalizedString::SearchableIndexIntroduction:
      return IDS_SEARCHABLE_INDEX_INTRO;
    case WebLocalizedString::SearchMenuClearRecentSearchesText:
      return IDS_RECENT_SEARCHES_CLEAR;
    case WebLocalizedString::SearchMenuNoRecentSearchesText:
      return IDS_RECENT_SEARCHES_NONE;
    case WebLocalizedString::SearchMenuRecentSearchesText:
      return IDS_RECENT_SEARCHES;
    case WebLocalizedString::SelectMenuListText:
      return IDS_FORM_SELECT_MENU_LIST_TEXT;
    case WebLocalizedString::SubmitButtonDefaultLabel:
      return IDS_FORM_SUBMIT_LABEL;
    case WebLocalizedString::ThisMonthButtonLabel:
      return IDS_FORM_THIS_MONTH_LABEL;
    case WebLocalizedString::ThisWeekButtonLabel:
      return IDS_FORM_THIS_WEEK_LABEL;
    case WebLocalizedString::ValidationBadInputForDateTime:
      return IDS_FORM_VALIDATION_BAD_INPUT_DATETIME;
    case WebLocalizedString::ValidationBadInputForNumber:
      return IDS_FORM_VALIDATION_BAD_INPUT_NUMBER;
    case WebLocalizedString::ValidationPatternMismatch:
      return IDS_FORM_VALIDATION_PATTERN_MISMATCH;
    case WebLocalizedString::ValidationRangeOverflow:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW;
    case WebLocalizedString::ValidationRangeOverflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW_DATETIME;
    case WebLocalizedString::ValidationRangeUnderflow:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW;
    case WebLocalizedString::ValidationRangeUnderflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW_DATETIME;
    case WebLocalizedString::ValidationStepMismatch:
      return IDS_FORM_VALIDATION_STEP_MISMATCH;
    case WebLocalizedString::ValidationStepMismatchCloseToLimit:
      return IDS_FORM_VALIDATION_STEP_MISMATCH_CLOSE_TO_LIMIT;
    case WebLocalizedString::ValidationTooLong:
      return IDS_FORM_VALIDATION_TOO_LONG;
    case WebLocalizedString::ValidationTooShort:
      return IDS_FORM_VALIDATION_TOO_SHORT;
    case WebLocalizedString::ValidationTypeMismatch:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH;
    case WebLocalizedString::ValidationTypeMismatchForEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL;
    case WebLocalizedString::ValidationTypeMismatchForEmailEmpty:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY;
    case WebLocalizedString::ValidationTypeMismatchForEmailEmptyDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_DOMAIN;
    case WebLocalizedString::ValidationTypeMismatchForEmailEmptyLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_LOCAL;
    case WebLocalizedString::ValidationTypeMismatchForEmailInvalidDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOMAIN;
    case WebLocalizedString::ValidationTypeMismatchForEmailInvalidDots:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOTS;
    case WebLocalizedString::ValidationTypeMismatchForEmailInvalidLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_LOCAL;
    case WebLocalizedString::ValidationTypeMismatchForEmailNoAtSign:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_NO_AT_SIGN;
    case WebLocalizedString::ValidationTypeMismatchForMultipleEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_MULTIPLE_EMAIL;
    case WebLocalizedString::ValidationTypeMismatchForURL:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_URL;
    case WebLocalizedString::ValidationValueMissing:
      return IDS_FORM_VALIDATION_VALUE_MISSING;
    case WebLocalizedString::ValidationValueMissingForCheckbox:
      return IDS_FORM_VALIDATION_VALUE_MISSING_CHECKBOX;
    case WebLocalizedString::ValidationValueMissingForFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_FILE;
    case WebLocalizedString::ValidationValueMissingForMultipleFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_MULTIPLE_FILE;
    case WebLocalizedString::ValidationValueMissingForRadio:
      return IDS_FORM_VALIDATION_VALUE_MISSING_RADIO;
    case WebLocalizedString::ValidationValueMissingForSelect:
      return IDS_FORM_VALIDATION_VALUE_MISSING_SELECT;
    case WebLocalizedString::WeekFormatTemplate:
      return IDS_FORM_INPUT_WEEK_TEMPLATE;
    case WebLocalizedString::WeekNumberLabel:
      return IDS_FORM_WEEK_NUMBER_LABEL;
    case WebLocalizedString::TextTracksNoLabel:
      return IDS_MEDIA_TRACKS_NO_LABEL;
    case WebLocalizedString::TextTracksOff:
      return IDS_MEDIA_TRACKS_OFF;
    // This "default:" line exists to avoid compile warnings about enum
    // coverage when we add a new symbol to WebLocalizedString.h in WebKit.
    // After a planned WebKit patch is landed, we need to add a case statement
    // for the added symbol here.
    default:
      break;
  }
  return -1;
}

class TraceLogObserverAdapter
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  TraceLogObserverAdapter(
      blink::Platform::TraceLogEnabledStateObserver* observer)
      : observer_(observer) {}

  void OnTraceLogEnabled() override { observer_->onTraceLogEnabled(); }

  void OnTraceLogDisabled() override { observer_->onTraceLogDisabled(); }

 private:
  blink::Platform::TraceLogEnabledStateObserver* observer_;
  DISALLOW_COPY_AND_ASSIGN(TraceLogObserverAdapter);
};

// TODO(skyostil): Ensure that we always have an active task runner when
// constructing the platform.
BlinkPlatformImpl::BlinkPlatformImpl()
    : BlinkPlatformImpl(base::ThreadTaskRunnerHandle::IsSet()
                            ? base::ThreadTaskRunnerHandle::Get()
                            : nullptr) {
}

BlinkPlatformImpl::BlinkPlatformImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(main_thread_task_runner),
      compositor_thread_(nullptr) {
  InternalInit();
}

void BlinkPlatformImpl::InternalInit() {
  // ChildThread may not exist in some tests.
  if (ChildThreadImpl::current()) {
    thread_safe_sender_ = ChildThreadImpl::current()->thread_safe_sender();
    notification_dispatcher_ =
        ChildThreadImpl::current()->notification_dispatcher();
    push_dispatcher_ = ChildThreadImpl::current()->push_dispatcher();
    permission_client_.reset(new PermissionDispatcher(
        ChildThreadImpl::current()->GetRemoteInterfaces()));
    main_thread_sync_provider_.reset(
        new BackgroundSyncProvider(main_thread_task_runner_.get()));
  }
}

void BlinkPlatformImpl::WaitUntilWebThreadTLSUpdate(
    scheduler::WebThreadBase* thread) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&BlinkPlatformImpl::UpdateWebThreadTLS, base::Unretained(this),
                 base::Unretained(thread), base::Unretained(&event)));
  event.Wait();
}

void BlinkPlatformImpl::UpdateWebThreadTLS(blink::WebThread* thread,
                                           base::WaitableEvent* event) {
  DCHECK(!current_thread_slot_.Get());
  current_thread_slot_.Set(thread);
  event->Signal();
}

BlinkPlatformImpl::~BlinkPlatformImpl() {
}

blink::WebSocketHandle* BlinkPlatformImpl::createWebSocketHandle() {
  return new WebSocketBridge;
}

WebString BlinkPlatformImpl::userAgent() {
  return blink::WebString::fromUTF8(GetContentClient()->GetUserAgent());
}

WebData BlinkPlatformImpl::parseDataURL(const WebURL& url,
                                        WebString& mimetype_out,
                                        WebString& charset_out) {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) &&
      mime_util::IsSupportedMimeType(mime_type)) {
    mimetype_out = WebString::fromUTF8(mime_type);
    charset_out = WebString::fromUTF8(char_set);
    return data;
  }
  return WebData();
}

WebURLError BlinkPlatformImpl::cancelledError(
    const WebURL& unreachableURL) const {
  return CreateWebURLError(unreachableURL, false, net::ERR_ABORTED);
}

bool BlinkPlatformImpl::parseMultipartHeadersFromBody(
    const char* bytes,
    size_t size,
    blink::WebURLResponse* response,
    size_t* end) const {
  return WebURLLoaderImpl::ParseMultipartHeadersFromBody(
      bytes, size, response, end);
}

blink::WebThread* BlinkPlatformImpl::createThread(const char* name) {
  std::unique_ptr<WebThreadImplForWorkerScheduler> thread(
      new WebThreadImplForWorkerScheduler(name));
  thread->Init();
  WaitUntilWebThreadTLSUpdate(thread.get());
  return thread.release();
}

void BlinkPlatformImpl::SetCompositorThread(
    scheduler::WebThreadBase* compositor_thread) {
  compositor_thread_ = compositor_thread;
  if (compositor_thread_)
    WaitUntilWebThreadTLSUpdate(compositor_thread_);
}

blink::WebThread* BlinkPlatformImpl::currentThread() {
  return static_cast<blink::WebThread*>(current_thread_slot_.Get());
}

void BlinkPlatformImpl::recordAction(const blink::UserMetricsAction& name) {
    if (ChildThread* child_thread = ChildThread::Get())
        child_thread->RecordComputedAction(name.action());
}

void BlinkPlatformImpl::addTraceLogEnabledStateObserver(
    TraceLogEnabledStateObserver* observer) {
  TraceLogObserverAdapter* adapter = new TraceLogObserverAdapter(observer);
  bool did_insert =
      trace_log_observers_.add(observer, base::WrapUnique(adapter)).second;
  DCHECK(did_insert);
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(adapter);
}

void BlinkPlatformImpl::removeTraceLogEnabledStateObserver(
    TraceLogEnabledStateObserver* observer) {
  std::unique_ptr<TraceLogObserverAdapter> adapter =
      trace_log_observers_.take_and_erase(observer);
  DCHECK(adapter);
  DCHECK(base::trace_event::TraceLog::GetInstance()->HasEnabledStateObserver(
      adapter.get()));
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(
      adapter.get());
}

namespace {

WebData loadAudioSpatializationResource(const char* name) {
#ifdef IDR_AUDIO_SPATIALIZATION_COMPOSITE
  if (!strcmp(name, "Composite")) {
    base::StringPiece resource = GetContentClient()->GetDataResource(
        IDR_AUDIO_SPATIALIZATION_COMPOSITE, ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif

#ifdef IDR_AUDIO_SPATIALIZATION_T000_P000
  const size_t kExpectedSpatializationNameLength = 31;
  if (strlen(name) != kExpectedSpatializationNameLength) {
    return WebData();
  }

  // Extract the azimuth and elevation from the resource name.
  int azimuth = 0;
  int elevation = 0;
  int values_parsed =
      sscanf(name, "IRC_Composite_C_R0195_T%3d_P%3d", &azimuth, &elevation);
  if (values_parsed != 2) {
    return WebData();
  }

  // The resource index values go through the elevations first, then azimuths.
  const int kAngleSpacing = 15;

  // 0 <= elevation <= 90 (or 315 <= elevation <= 345)
  // in increments of 15 degrees.
  int elevation_index =
      elevation <= 90 ? elevation / kAngleSpacing :
      7 + (elevation - 315) / kAngleSpacing;
  bool is_elevation_index_good = 0 <= elevation_index && elevation_index < 10;

  // 0 <= azimuth < 360 in increments of 15 degrees.
  int azimuth_index = azimuth / kAngleSpacing;
  bool is_azimuth_index_good = 0 <= azimuth_index && azimuth_index < 24;

  const int kNumberOfElevations = 10;
  const int kNumberOfAudioResources = 240;
  int resource_index = kNumberOfElevations * azimuth_index + elevation_index;
  bool is_resource_index_good = 0 <= resource_index &&
      resource_index < kNumberOfAudioResources;

  if (is_azimuth_index_good && is_elevation_index_good &&
      is_resource_index_good) {
    const int kFirstAudioResourceIndex = IDR_AUDIO_SPATIALIZATION_T000_P000;
    base::StringPiece resource = GetContentClient()->GetDataResource(
        kFirstAudioResourceIndex + resource_index, ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif  // IDR_AUDIO_SPATIALIZATION_T000_P000

  NOTREACHED();
  return WebData();
}

struct DataResource {
  const char* name;
  int id;
  ui::ScaleFactor scale_factor;
};

const DataResource kDataResources[] = {
    {"missingImage", IDR_BROKENIMAGE, ui::SCALE_FACTOR_100P},
    {"missingImage@2x", IDR_BROKENIMAGE, ui::SCALE_FACTOR_200P},
    {"mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON, ui::SCALE_FACTOR_100P},
    {"mediaplayerPauseNew",
     IDR_MEDIAPLAYER_PAUSE_BUTTON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON, ui::SCALE_FACTOR_100P},
    {"mediaplayerPlayNew",
     IDR_MEDIAPLAYER_PLAY_BUTTON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerPlayDisabled",
     IDR_MEDIAPLAYER_PLAY_BUTTON_DISABLED,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundLevel3",
     IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundLevel3New",
     IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundLevel2",
     IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundLevel1",
     IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundLevel0",
     IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundLevel0New",
     IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSoundDisabled",
     IDR_MEDIAPLAYER_SOUND_DISABLED,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSliderThumb",
     IDR_MEDIAPLAYER_SLIDER_THUMB,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSliderThumbNew",
     IDR_MEDIAPLAYER_SLIDER_THUMB_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerVolumeSliderThumb",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerVolumeSliderThumbNew",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerVolumeSliderThumbDisabled",
     IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DISABLED,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerClosedCaption",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerClosedCaptionNew",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerClosedCaptionDisabled",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerClosedCaptionDisabledNew",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerEnterFullscreen",
     IDR_MEDIAPLAYER_ENTER_FULLSCREEN_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerExitFullscreen",
     IDR_MEDIAPLAYER_EXIT_FULLSCREEN_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerFullscreen",
     IDR_MEDIAPLAYER_FULLSCREEN_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerCastOff",
     IDR_MEDIAPLAYER_CAST_BUTTON_OFF,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerCastOn",
     IDR_MEDIAPLAYER_CAST_BUTTON_ON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerCastOffNew",
     IDR_MEDIAPLAYER_CAST_BUTTON_OFF_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerCastOnNew",
     IDR_MEDIAPLAYER_CAST_BUTTON_ON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerFullscreenDisabled",
     IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DISABLED,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerOverlayCastOff",
     IDR_MEDIAPLAYER_OVERLAY_CAST_BUTTON_OFF,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerOverlayCastOffNew",
     IDR_MEDIAPLAYER_OVERLAY_CAST_BUTTON_OFF_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerOverlayPlay",
     IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerOverlayPlayNew",
     IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerTrackSelectionCheckmark",
     IDR_MEDIAPLAYER_TRACKSELECTION_CHECKMARK,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerTrackSelectionCheckmarkNew",
     IDR_MEDIAPLAYER_TRACKSELECTION_CHECKMARK_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerClosedCaptionsIcon",
     IDR_MEDIAPLAYER_CLOSEDCAPTIONS_ICON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerClosedCaptionsIconNew",
     IDR_MEDIAPLAYER_CLOSEDCAPTIONS_ICON_NEW,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSubtitlesIcon",
     IDR_MEDIAPLAYER_SUBTITLES_ICON,
     ui::SCALE_FACTOR_100P},
    {"mediaplayerSubtitlesIconNew",
     IDR_MEDIAPLAYER_SUBTITLES_ICON_NEW,
     ui::SCALE_FACTOR_100P},
    {"searchCancel", IDR_SEARCH_CANCEL, ui::SCALE_FACTOR_100P},
    {"searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED, ui::SCALE_FACTOR_100P},
    {"textAreaResizeCorner", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_100P},
    {"textAreaResizeCorner@2x", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_200P},
    {"generatePassword", IDR_PASSWORD_GENERATION_ICON, ui::SCALE_FACTOR_100P},
    {"generatePasswordHover",
     IDR_PASSWORD_GENERATION_ICON_HOVER,
     ui::SCALE_FACTOR_100P},
    {"html.css", IDR_UASTYLE_HTML_CSS, ui::SCALE_FACTOR_NONE},
    {"quirks.css", IDR_UASTYLE_QUIRKS_CSS, ui::SCALE_FACTOR_NONE},
    {"view-source.css", IDR_UASTYLE_VIEW_SOURCE_CSS, ui::SCALE_FACTOR_NONE},
#if defined(OS_ANDROID)
    {"themeChromiumAndroid.css",
     IDR_UASTYLE_THEME_CHROMIUM_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE},
    {"fullscreenAndroid.css",
      IDR_UASTYLE_FULLSCREEN_ANDROID_CSS,
      ui::SCALE_FACTOR_NONE},
    {"mediaControlsAndroid.css",
     IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE},
    {"mediaControlsAndroidNew.css",
     IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_NEW_CSS,
     ui::SCALE_FACTOR_NONE},
#endif
#if !defined(OS_WIN)
    {"themeChromiumLinux.css",
     IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS,
     ui::SCALE_FACTOR_NONE},
#endif
    {"themeInputMultipleFields.css",
     IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS,
     ui::SCALE_FACTOR_NONE},
#if defined(OS_MACOSX)
    {"themeMac.css", IDR_UASTYLE_THEME_MAC_CSS, ui::SCALE_FACTOR_NONE},
#endif
    {"themeWin.css", IDR_UASTYLE_THEME_WIN_CSS, ui::SCALE_FACTOR_NONE},
    {"themeWinQuirks.css",
     IDR_UASTYLE_THEME_WIN_QUIRKS_CSS,
     ui::SCALE_FACTOR_NONE},
    {"svg.css", IDR_UASTYLE_SVG_CSS, ui::SCALE_FACTOR_NONE},
    {"mathml.css", IDR_UASTYLE_MATHML_CSS, ui::SCALE_FACTOR_NONE},
    {"mediaControls.css",
     IDR_UASTYLE_MEDIA_CONTROLS_CSS,
     ui::SCALE_FACTOR_NONE},
    {"mediaControlsNew.css",
     IDR_UASTYLE_MEDIA_CONTROLS_NEW_CSS,
     ui::SCALE_FACTOR_NONE},
    {"fullscreen.css", IDR_UASTYLE_FULLSCREEN_CSS, ui::SCALE_FACTOR_NONE},
    {"xhtmlmp.css", IDR_UASTYLE_XHTMLMP_CSS, ui::SCALE_FACTOR_NONE},
    {"viewportAndroid.css",
     IDR_UASTYLE_VIEWPORT_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE},
    {"viewportTelevision.css",
     IDR_UASTYLE_VIEWPORT_TELEVISION_CSS,
     ui::SCALE_FACTOR_NONE},
    {"InspectorOverlayPage.html",
     IDR_INSPECTOR_OVERLAY_PAGE_HTML,
     ui::SCALE_FACTOR_NONE},
    {"DocumentExecCommand.js",
     IDR_PRIVATE_SCRIPT_DOCUMENTEXECCOMMAND_JS,
     ui::SCALE_FACTOR_NONE},
    {"DocumentXMLTreeViewer.css",
     IDR_PRIVATE_SCRIPT_DOCUMENTXMLTREEVIEWER_CSS,
     ui::SCALE_FACTOR_NONE},
    {"DocumentXMLTreeViewer.js",
     IDR_PRIVATE_SCRIPT_DOCUMENTXMLTREEVIEWER_JS,
     ui::SCALE_FACTOR_NONE},
    {"HTMLMarqueeElement.js",
     IDR_PRIVATE_SCRIPT_HTMLMARQUEEELEMENT_JS,
     ui::SCALE_FACTOR_NONE},
    {"PrivateScriptRunner.js",
     IDR_PRIVATE_SCRIPT_PRIVATESCRIPTRUNNER_JS,
     ui::SCALE_FACTOR_NONE},
#ifdef IDR_PICKER_COMMON_JS
    {"pickerCommon.js", IDR_PICKER_COMMON_JS, ui::SCALE_FACTOR_NONE},
    {"pickerCommon.css", IDR_PICKER_COMMON_CSS, ui::SCALE_FACTOR_NONE},
    {"calendarPicker.js", IDR_CALENDAR_PICKER_JS, ui::SCALE_FACTOR_NONE},
    {"calendarPicker.css", IDR_CALENDAR_PICKER_CSS, ui::SCALE_FACTOR_NONE},
    {"listPicker.js", IDR_LIST_PICKER_JS, ui::SCALE_FACTOR_NONE},
    {"listPicker.css", IDR_LIST_PICKER_CSS, ui::SCALE_FACTOR_NONE},
    {"pickerButton.css", IDR_PICKER_BUTTON_CSS, ui::SCALE_FACTOR_NONE},
    {"suggestionPicker.js", IDR_SUGGESTION_PICKER_JS, ui::SCALE_FACTOR_NONE},
    {"suggestionPicker.css", IDR_SUGGESTION_PICKER_CSS, ui::SCALE_FACTOR_NONE},
    {"colorSuggestionPicker.js",
     IDR_COLOR_SUGGESTION_PICKER_JS,
     ui::SCALE_FACTOR_NONE},
    {"colorSuggestionPicker.css",
     IDR_COLOR_SUGGESTION_PICKER_CSS,
     ui::SCALE_FACTOR_NONE},
#endif
};

}  // namespace

WebData BlinkPlatformImpl::loadResource(const char* name) {
  // Some clients will call into this method with an empty |name| when they have
  // optional resources.  For example, the PopupMenuChromium code can have icons
  // for some Autofill items but not for others.
  if (!strlen(name))
    return WebData();

  // Check the name prefix to see if it's an audio resource.
  if (base::StartsWith(name, "IRC_Composite", base::CompareCase::SENSITIVE) ||
      base::StartsWith(name, "Composite", base::CompareCase::SENSITIVE))
    return loadAudioSpatializationResource(name);

  // TODO(flackr): We should use a better than linear search here, a trie would
  // be ideal.
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (!strcmp(name, kDataResources[i].name)) {
      base::StringPiece resource = GetContentClient()->GetDataResource(
          kDataResources[i].id, kDataResources[i].scale_factor);
      return WebData(resource.data(), resource.size());
    }
  }

  NOTREACHED() << "Unknown image resource " << name;
  return WebData();
}

WebString BlinkPlatformImpl::queryLocalizedString(
    WebLocalizedString::Name name) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return GetContentClient()->GetLocalizedString(message_id);
}

WebString BlinkPlatformImpl::queryLocalizedString(
    WebLocalizedString::Name name, int numeric_value) {
  return queryLocalizedString(name, base::IntToString16(numeric_value));
}

WebString BlinkPlatformImpl::queryLocalizedString(
    WebLocalizedString::Name name, const WebString& value) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(message_id), value, NULL);
}

WebString BlinkPlatformImpl::queryLocalizedString(
    WebLocalizedString::Name name,
    const WebString& value1,
    const WebString& value2) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  std::vector<base::string16> values;
  values.reserve(2);
  values.push_back(value1);
  values.push_back(value2);
  return base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(message_id), values, NULL);
}

blink::WebThread* BlinkPlatformImpl::compositorThread() const {
  return compositor_thread_;
}

blink::WebGestureCurve* BlinkPlatformImpl::createFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  return ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
             gfx::Vector2dF(velocity.x, velocity.y),
             gfx::Vector2dF(cumulative_scroll.width, cumulative_scroll.height),
             IsMainThread()).release();
}

void BlinkPlatformImpl::didStartWorkerThread() {
  WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
}

void BlinkPlatformImpl::willStopWorkerThread() {
  WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
}

bool BlinkPlatformImpl::allowScriptExtensionForServiceWorker(
    const blink::WebURL& scriptUrl) {
  return GetContentClient()->AllowScriptExtensionForServiceWorker(scriptUrl);
}

blink::WebCrypto* BlinkPlatformImpl::crypto() {
  return &web_crypto_;
}

blink::WebNotificationManager*
BlinkPlatformImpl::notificationManager() {
  if (!thread_safe_sender_.get() || !notification_dispatcher_.get())
    return nullptr;

  return NotificationManager::ThreadSpecificInstance(
      thread_safe_sender_.get(),
      notification_dispatcher_.get());
}

blink::WebPushProvider* BlinkPlatformImpl::pushProvider() {
  if (!thread_safe_sender_.get() || !push_dispatcher_.get())
    return nullptr;

  return PushProvider::ThreadSpecificInstance(thread_safe_sender_.get(),
                                              push_dispatcher_.get());
}

blink::WebPermissionClient* BlinkPlatformImpl::permissionClient() {
  if (!permission_client_.get())
    return nullptr;

  if (IsMainThread())
    return permission_client_.get();

  return PermissionDispatcherThreadProxy::GetThreadInstance(
      main_thread_task_runner_.get(), permission_client_.get());
}

blink::WebSyncProvider* BlinkPlatformImpl::backgroundSyncProvider() {
  if (IsMainThread())
    return main_thread_sync_provider_.get();

  return BackgroundSyncProvider::GetOrCreateThreadSpecificInstance(
      main_thread_task_runner_.get());
}

WebThemeEngine* BlinkPlatformImpl::themeEngine() {
  return &native_theme_engine_;
}

WebFallbackThemeEngine* BlinkPlatformImpl::fallbackThemeEngine() {
  return &fallback_theme_engine_;
}

blink::Platform::FileHandle BlinkPlatformImpl::databaseOpenFile(
    const blink::WebString& vfs_file_name, int desired_flags) {
#if defined(OS_WIN)
  return INVALID_HANDLE_VALUE;
#elif defined(OS_POSIX)
  return -1;
#endif
}

int BlinkPlatformImpl::databaseDeleteFile(
    const blink::WebString& vfs_file_name, bool sync_dir) {
  return -1;
}

long BlinkPlatformImpl::databaseGetFileAttributes(
    const blink::WebString& vfs_file_name) {
  return 0;
}

long long BlinkPlatformImpl::databaseGetFileSize(
    const blink::WebString& vfs_file_name) {
  return 0;
}

long long BlinkPlatformImpl::databaseGetSpaceAvailableForOrigin(
    const blink::WebSecurityOrigin& origin) {
  return 0;
}

bool BlinkPlatformImpl::databaseSetFileSize(
    const blink::WebString& vfs_file_name, long long size) {
  return false;
}

blink::WebString BlinkPlatformImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const blink::WebString& challenge,
    const blink::WebURL& url,
    const blink::WebURL& top_origin) {
  return blink::WebString("");
}

size_t BlinkPlatformImpl::actualMemoryUsageMB() {
  return GetMemoryUsageKB() >> 10;
}

size_t BlinkPlatformImpl::numberOfProcessors() {
  return static_cast<size_t>(base::SysInfo::NumberOfProcessors());
}

size_t BlinkPlatformImpl::maxDecodedImageBytes() {
#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    // Limit image decoded size to 3M pixels on low end devices.
    // 4 is maximum number of bytes per pixel.
    return 3 * 1024 * 1024 * 4;
  }
  // For other devices, limit decoded image size based on the amount of physical
  // memory.
  // In some cases all physical memory is not accessible by Chromium, as it can
  // be reserved for direct use by certain hardware. Thus, we set the limit so
  // that 1.6GB of reported physical memory on a 2GB device is enough to set the
  // limit at 16M pixels, which is a desirable value since 4K*4K is a relatively
  // common texture size.
  return base::SysInfo::AmountOfPhysicalMemory() / 25;
#else
  return noDecodedImageByteLimit;
#endif
}

uint32_t BlinkPlatformImpl::getUniqueIdForProcess() {
  // TODO(rickyz): Replace this with base::GetUniqueIdForProcess when that's
  // ready.
  return base::trace_event::TraceLog::GetInstance()->process_id();
}

bool BlinkPlatformImpl::IsMainThread() const {
  return main_thread_task_runner_.get() &&
         main_thread_task_runner_->BelongsToCurrentThread();
}

WebString BlinkPlatformImpl::domCodeStringFromEnum(int dom_code) {
  return WebString::fromUTF8(ui::KeycodeConverter::DomCodeToCodeString(
      static_cast<ui::DomCode>(dom_code)));
}

int BlinkPlatformImpl::domEnumFromCodeString(const WebString& code) {
  return static_cast<int>(ui::KeycodeConverter::CodeStringToDomCode(
      code.utf8()));
}

WebString BlinkPlatformImpl::domKeyStringFromEnum(int dom_key) {
  return WebString::fromUTF8(ui::KeycodeConverter::DomKeyToKeyString(
      static_cast<ui::DomKey>(dom_key)));
}

int BlinkPlatformImpl::domKeyEnumFromString(const WebString& key_string) {
  return static_cast<int>(
      ui::KeycodeConverter::KeyStringToDomKey(key_string.utf8()));
}

}  // namespace content
