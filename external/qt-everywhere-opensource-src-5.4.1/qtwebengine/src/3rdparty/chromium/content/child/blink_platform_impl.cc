// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <math.h>

#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "content/child/content_child_helpers.h"
#include "content/child/fling_curve_configuration.h"
#include "content/child/web_discardable_memory_impl.h"
#include "content/child/web_socket_stream_handle_impl.h"
#include "content/child/web_url_loader_impl.h"
#include "content/child/websocket_bridge.h"
#include "content/child/webthread_impl.h"
#include "content/child/worker_task_runner.h"
#include "content/public/common/content_client.h"
#include "grit/blink_resources.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "net/base/data_url.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebConvertableToTraceFormat.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebWaitableEvent.h"
#include "ui/base/layout.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#include "content/child/fling_animator_impl_android.h"
#endif

#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

using blink::WebData;
using blink::WebFallbackThemeEngine;
using blink::WebLocalizedString;
using blink::WebString;
using blink::WebSocketStreamHandle;
using blink::WebThemeEngine;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;

namespace content {

namespace {

class WebWaitableEventImpl : public blink::WebWaitableEvent {
 public:
  WebWaitableEventImpl() : impl_(new base::WaitableEvent(false, false)) {}
  virtual ~WebWaitableEventImpl() {}

  virtual void wait() { impl_->Wait(); }
  virtual void signal() { impl_->Signal(); }

  base::WaitableEvent* impl() {
    return impl_.get();
  }

 private:
  scoped_ptr<base::WaitableEvent> impl_;
  DISALLOW_COPY_AND_ASSIGN(WebWaitableEventImpl);
};

// A simple class to cache the memory usage for a given amount of time.
class MemoryUsageCache {
 public:
  // Retrieves the Singleton.
  static MemoryUsageCache* GetInstance() {
    return Singleton<MemoryUsageCache>::get();
  }

  MemoryUsageCache() : memory_value_(0) { Init(); }
  ~MemoryUsageCache() {}

  void Init() {
    const unsigned int kCacheSeconds = 1;
    cache_valid_time_ = base::TimeDelta::FromSeconds(kCacheSeconds);
  }

  // Returns true if the cached value is fresh.
  // Returns false if the cached value is stale, or if |cached_value| is NULL.
  bool IsCachedValueValid(size_t* cached_value) {
    base::AutoLock scoped_lock(lock_);
    if (!cached_value)
      return false;
    if (base::Time::Now() - last_updated_time_ > cache_valid_time_)
      return false;
    *cached_value = memory_value_;
    return true;
  };

  // Setter for |memory_value_|, refreshes |last_updated_time_|.
  void SetMemoryValue(const size_t value) {
    base::AutoLock scoped_lock(lock_);
    memory_value_ = value;
    last_updated_time_ = base::Time::Now();
  }

 private:
  // The cached memory value.
  size_t memory_value_;

  // How long the cached value should remain valid.
  base::TimeDelta cache_valid_time_;

  // The last time the cached value was updated.
  base::Time last_updated_time_;

  base::Lock lock_;
};

class ConvertableToTraceFormatWrapper
    : public base::debug::ConvertableToTraceFormat {
 public:
  explicit ConvertableToTraceFormatWrapper(
      const blink::WebConvertableToTraceFormat& convertable)
      : convertable_(convertable) {}
  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE {
    *out += convertable_.asTraceFormat().utf8();
  }

 private:
  virtual ~ConvertableToTraceFormatWrapper() {}

  blink::WebConvertableToTraceFormat convertable_;
};

}  // namespace

static int ToMessageID(WebLocalizedString::Name name) {
  switch (name) {
    case WebLocalizedString::AXAMPMFieldText:
      return IDS_AX_AM_PM_FIELD_TEXT;
    case WebLocalizedString::AXButtonActionVerb:
      return IDS_AX_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXCheckedCheckBoxActionVerb:
      return IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXDateTimeFieldEmptyValueText:
      return IDS_AX_DATE_TIME_FIELD_EMPTY_VALUE_TEXT;
    case WebLocalizedString::AXDayOfMonthFieldText:
      return IDS_AX_DAY_OF_MONTH_FIELD_TEXT;
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
    case WebLocalizedString::AXMediaSliderHelp:
      return IDS_AX_MEDIA_SLIDER_HELP;
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
    case WebLocalizedString::AXMillisecondFieldText:
      return IDS_AX_MILLISECOND_FIELD_TEXT;
    case WebLocalizedString::AXMinuteFieldText:
      return IDS_AX_MINUTE_FIELD_TEXT;
    case WebLocalizedString::AXMonthFieldText:
      return IDS_AX_MONTH_FIELD_TEXT;
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
    // This "default:" line exists to avoid compile warnings about enum
    // coverage when we add a new symbol to WebLocalizedString.h in WebKit.
    // After a planned WebKit patch is landed, we need to add a case statement
    // for the added symbol here.
    default:
      break;
  }
  return -1;
}

BlinkPlatformImpl::BlinkPlatformImpl()
    : main_loop_(base::MessageLoop::current()),
      shared_timer_func_(NULL),
      shared_timer_fire_time_(0.0),
      shared_timer_fire_time_was_set_while_suspended_(false),
      shared_timer_suspended_(0),
      fling_curve_configuration_(new FlingCurveConfiguration),
      current_thread_slot_(&DestroyCurrentThread) {}

BlinkPlatformImpl::~BlinkPlatformImpl() {
}

WebURLLoader* BlinkPlatformImpl::createURLLoader() {
  return new WebURLLoaderImpl;
}

WebSocketStreamHandle* BlinkPlatformImpl::createSocketStreamHandle() {
  return new WebSocketStreamHandleImpl;
}

blink::WebSocketHandle* BlinkPlatformImpl::createWebSocketHandle() {
  return new WebSocketBridge;
}

WebString BlinkPlatformImpl::userAgent() {
  return WebString::fromUTF8(GetContentClient()->GetUserAgent());
}

WebData BlinkPlatformImpl::parseDataURL(const WebURL& url,
                                        WebString& mimetype_out,
                                        WebString& charset_out) {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data)
      && net::IsSupportedMimeType(mime_type)) {
    mimetype_out = WebString::fromUTF8(mime_type);
    charset_out = WebString::fromUTF8(char_set);
    return data;
  }
  return WebData();
}

WebURLError BlinkPlatformImpl::cancelledError(
    const WebURL& unreachableURL) const {
  return WebURLLoaderImpl::CreateError(unreachableURL, false, net::ERR_ABORTED);
}

blink::WebThread* BlinkPlatformImpl::createThread(const char* name) {
  return new WebThreadImpl(name);
}

blink::WebThread* BlinkPlatformImpl::currentThread() {
  WebThreadImplForMessageLoop* thread =
      static_cast<WebThreadImplForMessageLoop*>(current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop.get())
    return NULL;

  thread = new WebThreadImplForMessageLoop(message_loop.get());
  current_thread_slot_.Set(thread);
  return thread;
}

blink::WebWaitableEvent* BlinkPlatformImpl::createWaitableEvent() {
  return new WebWaitableEventImpl();
}

blink::WebWaitableEvent* BlinkPlatformImpl::waitMultipleEvents(
    const blink::WebVector<blink::WebWaitableEvent*>& web_events) {
  std::vector<base::WaitableEvent*> events;
  for (size_t i = 0; i < web_events.size(); ++i)
    events.push_back(static_cast<WebWaitableEventImpl*>(web_events[i])->impl());
  size_t idx = base::WaitableEvent::WaitMany(
      vector_as_array(&events), events.size());
  DCHECK_LT(idx, web_events.size());
  return web_events[idx];
}

void BlinkPlatformImpl::decrementStatsCounter(const char* name) {
  base::StatsCounter(name).Decrement();
}

void BlinkPlatformImpl::incrementStatsCounter(const char* name) {
  base::StatsCounter(name).Increment();
}

void BlinkPlatformImpl::histogramCustomCounts(
    const char* name, int sample, int min, int max, int bucket_count) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::HistogramBase* counter =
      base::Histogram::FactoryGet(name, min, max, bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  counter->Add(sample);
}

void BlinkPlatformImpl::histogramEnumeration(
    const char* name, int sample, int boundary_value) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(name, 1, boundary_value,
          boundary_value + 1, base::HistogramBase::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  counter->Add(sample);
}

void BlinkPlatformImpl::histogramSparse(const char* name, int sample) {
  // For sparse histograms, we can use the macro, as it does not incorporate a
  // static.
  UMA_HISTOGRAM_SPARSE_SLOWLY(name, sample);
}

const unsigned char* BlinkPlatformImpl::getTraceCategoryEnabledFlag(
    const char* category_group) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

long* BlinkPlatformImpl::getTraceSamplingState(
    const unsigned thread_bucket) {
  switch (thread_bucket) {
    case 0:
      return reinterpret_cast<long*>(&TRACE_EVENT_API_THREAD_BUCKET(0));
    case 1:
      return reinterpret_cast<long*>(&TRACE_EVENT_API_THREAD_BUCKET(1));
    case 2:
      return reinterpret_cast<long*>(&TRACE_EVENT_API_THREAD_BUCKET(2));
    default:
      NOTREACHED() << "Unknown thread bucket type.";
  }
  return NULL;
}

COMPILE_ASSERT(
    sizeof(blink::Platform::TraceEventHandle) ==
        sizeof(base::debug::TraceEventHandle),
    TraceEventHandle_types_must_be_same_size);

blink::Platform::TraceEventHandle BlinkPlatformImpl::addTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    unsigned char flags) {
  base::debug::TraceEventHandle handle = TRACE_EVENT_API_ADD_TRACE_EVENT(
      phase, category_group_enabled, name, id,
      num_args, arg_names, arg_types, arg_values, NULL, flags);
  blink::Platform::TraceEventHandle result;
  memcpy(&result, &handle, sizeof(result));
  return result;
}

blink::Platform::TraceEventHandle BlinkPlatformImpl::addTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    const blink::WebConvertableToTraceFormat* convertable_values,
    unsigned char flags) {
  scoped_refptr<base::debug::ConvertableToTraceFormat> convertable_wrappers[2];
  if (convertable_values) {
    size_t size = std::min(static_cast<size_t>(num_args),
                           arraysize(convertable_wrappers));
    for (size_t i = 0; i < size; ++i) {
      if (arg_types[i] == TRACE_VALUE_TYPE_CONVERTABLE) {
        convertable_wrappers[i] =
            new ConvertableToTraceFormatWrapper(convertable_values[i]);
      }
    }
  }
  base::debug::TraceEventHandle handle =
      TRACE_EVENT_API_ADD_TRACE_EVENT(phase,
                                      category_group_enabled,
                                      name,
                                      id,
                                      num_args,
                                      arg_names,
                                      arg_types,
                                      arg_values,
                                      convertable_wrappers,
                                      flags);
  blink::Platform::TraceEventHandle result;
  memcpy(&result, &handle, sizeof(result));
  return result;
}

void BlinkPlatformImpl::updateTraceEventDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    TraceEventHandle handle) {
  base::debug::TraceEventHandle traceEventHandle;
  memcpy(&traceEventHandle, &handle, sizeof(handle));
  TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
      category_group_enabled, name, traceEventHandle);
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
  { "missingImage", IDR_BROKENIMAGE, ui::SCALE_FACTOR_100P },
  { "missingImage@2x", IDR_BROKENIMAGE, ui::SCALE_FACTOR_200P },
  { "mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerPauseHover",
    IDR_MEDIAPLAYER_PAUSE_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerPauseDown",
    IDR_MEDIAPLAYER_PAUSE_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayHover",
    IDR_MEDIAPLAYER_PLAY_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayDown",
    IDR_MEDIAPLAYER_PLAY_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayDisabled",
    IDR_MEDIAPLAYER_PLAY_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundDisabled",
    IDR_MEDIAPLAYER_SOUND_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumb",
    IDR_MEDIAPLAYER_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumbHover",
    IDR_MEDIAPLAYER_SLIDER_THUMB_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumbDown",
    IDR_MEDIAPLAYER_SLIDER_THUMB_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumb",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbHover",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbDown",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbDisabled",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaption",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaptionHover",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaptionDown",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaptionDisabled",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreen",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenHover",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenDown",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenDisabled",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerOverlayPlay",
    IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON, ui::SCALE_FACTOR_100P },
#if defined(OS_MACOSX)
  { "overhangPattern", IDR_OVERHANG_PATTERN, ui::SCALE_FACTOR_100P },
  { "overhangShadow", IDR_OVERHANG_SHADOW, ui::SCALE_FACTOR_100P },
#endif
  { "panIcon", IDR_PAN_SCROLL_ICON, ui::SCALE_FACTOR_100P },
  { "searchCancel", IDR_SEARCH_CANCEL, ui::SCALE_FACTOR_100P },
  { "searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED, ui::SCALE_FACTOR_100P },
  { "searchMagnifier", IDR_SEARCH_MAGNIFIER, ui::SCALE_FACTOR_100P },
  { "searchMagnifierResults",
    IDR_SEARCH_MAGNIFIER_RESULTS, ui::SCALE_FACTOR_100P },
  { "textAreaResizeCorner", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_100P },
  { "textAreaResizeCorner@2x", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_200P },
  { "generatePassword", IDR_PASSWORD_GENERATION_ICON, ui::SCALE_FACTOR_100P },
  { "generatePasswordHover",
    IDR_PASSWORD_GENERATION_ICON_HOVER, ui::SCALE_FACTOR_100P },
};

}  // namespace

WebData BlinkPlatformImpl::loadResource(const char* name) {
  // Some clients will call into this method with an empty |name| when they have
  // optional resources.  For example, the PopupMenuChromium code can have icons
  // for some Autofill items but not for others.
  if (!strlen(name))
    return WebData();

  // Check the name prefix to see if it's an audio resource.
  if (StartsWithASCII(name, "IRC_Composite", true) ||
      StartsWithASCII(name, "Composite", true))
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
  return ReplaceStringPlaceholders(GetContentClient()->GetLocalizedString(
      message_id), value, NULL);
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
  return ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(message_id), values, NULL);
}

double BlinkPlatformImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

double BlinkPlatformImpl::monotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

void BlinkPlatformImpl::cryptographicallyRandomValues(
    unsigned char* buffer, size_t length) {
  base::RandBytes(buffer, length);
}

void BlinkPlatformImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void BlinkPlatformImpl::setSharedTimerFireInterval(
    double interval_seconds) {
  shared_timer_fire_time_ = interval_seconds + monotonicallyIncreasingTime();
  if (shared_timer_suspended_) {
    shared_timer_fire_time_was_set_while_suspended_ = true;
    return;
  }

  // By converting between double and int64 representation, we run the risk
  // of losing precision due to rounding errors. Performing computations in
  // microseconds reduces this risk somewhat. But there still is the potential
  // of us computing a fire time for the timer that is shorter than what we
  // need.
  // As the event loop will check event deadlines prior to actually firing
  // them, there is a risk of needlessly rescheduling events and of
  // needlessly looping if sleep times are too short even by small amounts.
  // This results in measurable performance degradation unless we use ceil() to
  // always round up the sleep times.
  int64 interval = static_cast<int64>(
      ceil(interval_seconds * base::Time::kMillisecondsPerSecond)
      * base::Time::kMicrosecondsPerMillisecond);

  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(FROM_HERE, base::TimeDelta::FromMicroseconds(interval),
                      this, &BlinkPlatformImpl::DoTimeout);
  OnStartSharedTimer(base::TimeDelta::FromMicroseconds(interval));
}

void BlinkPlatformImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void BlinkPlatformImpl::callOnMainThread(
    void (*func)(void*), void* context) {
  main_loop_->PostTask(FROM_HERE, base::Bind(func, context));
}

blink::WebGestureCurve* BlinkPlatformImpl::createFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
#if defined(OS_ANDROID)
  return FlingAnimatorImpl::CreateAndroidGestureCurve(
      velocity,
      cumulative_scroll);
#endif

  if (device_source == blink::WebGestureDeviceTouchscreen)
    return fling_curve_configuration_->CreateForTouchScreen(velocity,
                                                            cumulative_scroll);

  return fling_curve_configuration_->CreateForTouchPad(velocity,
                                                       cumulative_scroll);
}

void BlinkPlatformImpl::didStartWorkerRunLoop(
    const blink::WebWorkerRunLoop& runLoop) {
  WorkerTaskRunner* worker_task_runner = WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStarted(runLoop);
}

void BlinkPlatformImpl::didStopWorkerRunLoop(
    const blink::WebWorkerRunLoop& runLoop) {
  WorkerTaskRunner* worker_task_runner = WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStopped(runLoop);
}

blink::WebCrypto* BlinkPlatformImpl::crypto() {
  WebCryptoImpl::EnsureInit();
  return &web_crypto_;
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
    const blink::WebString& origin_identifier) {
  return 0;
}

blink::WebString BlinkPlatformImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const blink::WebString& challenge,
    const blink::WebURL& url) {
  return blink::WebString("");
}

static scoped_ptr<base::ProcessMetrics> CurrentProcessMetrics() {
  using base::ProcessMetrics;
#if defined(OS_MACOSX)
  return scoped_ptr<ProcessMetrics>(
      // The default port provider is sufficient to get data for the current
      // process.
      ProcessMetrics::CreateProcessMetrics(base::GetCurrentProcessHandle(),
                                           NULL));
#else
  return scoped_ptr<ProcessMetrics>(
      ProcessMetrics::CreateProcessMetrics(base::GetCurrentProcessHandle()));
#endif
}

static size_t getMemoryUsageMB(bool bypass_cache) {
  size_t current_mem_usage = 0;
  MemoryUsageCache* mem_usage_cache_singleton = MemoryUsageCache::GetInstance();
  if (!bypass_cache &&
      mem_usage_cache_singleton->IsCachedValueValid(&current_mem_usage))
    return current_mem_usage;

  current_mem_usage = GetMemoryUsageKB() >> 10;
  mem_usage_cache_singleton->SetMemoryValue(current_mem_usage);
  return current_mem_usage;
}

size_t BlinkPlatformImpl::memoryUsageMB() {
  return getMemoryUsageMB(false);
}

size_t BlinkPlatformImpl::actualMemoryUsageMB() {
  return getMemoryUsageMB(true);
}

size_t BlinkPlatformImpl::physicalMemoryMB() {
  return static_cast<size_t>(base::SysInfo::AmountOfPhysicalMemoryMB());
}

size_t BlinkPlatformImpl::virtualMemoryLimitMB() {
  return static_cast<size_t>(base::SysInfo::AmountOfVirtualMemoryMB());
}

size_t BlinkPlatformImpl::numberOfProcessors() {
  return static_cast<size_t>(base::SysInfo::NumberOfProcessors());
}

void BlinkPlatformImpl::startHeapProfiling(
  const blink::WebString& prefix) {
  // FIXME(morrita): Make this built on windows.
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  HeapProfilerStart(prefix.utf8().data());
#endif
}

void BlinkPlatformImpl::stopHeapProfiling() {
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  HeapProfilerStop();
#endif
}

void BlinkPlatformImpl::dumpHeapProfiling(
  const blink::WebString& reason) {
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  HeapProfilerDump(reason.utf8().data());
#endif
}

WebString BlinkPlatformImpl::getHeapProfile() {
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  char* data = GetHeapProfile();
  WebString result = WebString::fromUTF8(std::string(data));
  free(data);
  return result;
#else
  return WebString();
#endif
}

bool BlinkPlatformImpl::processMemorySizesInBytes(
    size_t* private_bytes,
    size_t* shared_bytes) {
  return CurrentProcessMetrics()->GetMemoryBytes(private_bytes, shared_bytes);
}

bool BlinkPlatformImpl::memoryAllocatorWasteInBytes(size_t* size) {
  return base::allocator::GetAllocatorWasteSize(size);
}

blink::WebDiscardableMemory*
BlinkPlatformImpl::allocateAndLockDiscardableMemory(size_t bytes) {
  base::DiscardableMemoryType type =
      base::DiscardableMemory::GetPreferredType();
  if (type == base::DISCARDABLE_MEMORY_TYPE_EMULATED)
    return NULL;
  return content::WebDiscardableMemoryImpl::CreateLockedMemory(bytes).release();
}

size_t BlinkPlatformImpl::maxDecodedImageBytes() {
#if defined(OS_ANDROID)
  if (base::android::SysUtils::IsLowEndDevice()) {
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

void BlinkPlatformImpl::SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen) {
  fling_curve_configuration_->SetCurveParameters(new_touchpad, new_touchscreen);
}

void BlinkPlatformImpl::SuspendSharedTimer() {
  ++shared_timer_suspended_;
}

void BlinkPlatformImpl::ResumeSharedTimer() {
  DCHECK_GT(shared_timer_suspended_, 0);

  // The shared timer may have fired or been adjusted while we were suspended.
  if (--shared_timer_suspended_ == 0 &&
      (!shared_timer_.IsRunning() ||
       shared_timer_fire_time_was_set_while_suspended_)) {
    shared_timer_fire_time_was_set_while_suspended_ = false;
    setSharedTimerFireInterval(
        shared_timer_fire_time_ - monotonicallyIncreasingTime());
  }
}

// static
void BlinkPlatformImpl::DestroyCurrentThread(void* thread) {
  WebThreadImplForMessageLoop* impl =
      static_cast<WebThreadImplForMessageLoop*>(thread);
  delete impl;
}

}  // namespace content
