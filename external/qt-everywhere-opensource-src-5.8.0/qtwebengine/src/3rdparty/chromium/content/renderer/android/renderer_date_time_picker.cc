// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/renderer_date_time_picker.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "content/common/date_time_suggestion.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/WebKit/public/web/WebDateTimeInputType.h"
#include "third_party/WebKit/public/web/WebDateTimeSuggestion.h"
#include "ui/base/ime/text_input_type.h"

using blink::WebString;

namespace content {

namespace {

// Converts a |blink::WebDateTimeSuggestion| structure to |DateTimeSuggestion|.
DateTimeSuggestion ToDateTimeSuggestion(
    const blink::WebDateTimeSuggestion& suggestion) {
  DateTimeSuggestion result;
  result.value = suggestion.value;
  result.localized_value = suggestion.localizedValue;
  result.label = suggestion.label;
  return result;
}

}  // namespace

static ui::TextInputType ToTextInputType(int type) {
  switch (type) {
    case blink::WebDateTimeInputTypeDate:
      return ui::TEXT_INPUT_TYPE_DATE;
      break;
    case blink::WebDateTimeInputTypeDateTime:
      return ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case blink::WebDateTimeInputTypeDateTimeLocal:
      return ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
      break;
    case blink::WebDateTimeInputTypeMonth:
      return ui::TEXT_INPUT_TYPE_MONTH;
      break;
    case blink::WebDateTimeInputTypeTime:
      return ui::TEXT_INPUT_TYPE_TIME;
      break;
    case blink::WebDateTimeInputTypeWeek:
      return ui::TEXT_INPUT_TYPE_WEEK;
      break;
    case blink::WebDateTimeInputTypeNone:
    default:
      return ui::TEXT_INPUT_TYPE_NONE;
  }
}

RendererDateTimePicker::RendererDateTimePicker(
    RenderViewImpl* sender,
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion)
    : RenderViewObserver(sender),
      chooser_params_(params),
      chooser_completion_(completion) {
}

RendererDateTimePicker::~RendererDateTimePicker() {
}

bool RendererDateTimePicker::Open() {
  ViewHostMsg_DateTimeDialogValue_Params message;
  message.dialog_type = ToTextInputType(chooser_params_.type);
  message.dialog_value = chooser_params_.doubleValue;
  message.minimum = chooser_params_.minimum;
  message.maximum = chooser_params_.maximum;
  message.step = chooser_params_.step;
  for (size_t i = 0; i < chooser_params_.suggestions.size(); i++) {
    message.suggestions.push_back(
        ToDateTimeSuggestion(chooser_params_.suggestions[i]));
  }
  Send(new ViewHostMsg_OpenDateTimeDialog(routing_id(), message));
  return true;
}

bool RendererDateTimePicker::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererDateTimePicker, message)
    IPC_MESSAGE_HANDLER(ViewMsg_ReplaceDateTime, OnReplaceDateTime)
    IPC_MESSAGE_HANDLER(ViewMsg_CancelDateTimeDialog, OnCancel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererDateTimePicker::OnReplaceDateTime(double value) {
  if (chooser_completion_)
    chooser_completion_->didChooseValue(value);
  static_cast<RenderViewImpl*>(render_view())->DismissDateTimeDialog();
}

void RendererDateTimePicker::OnCancel() {
  if (chooser_completion_)
    chooser_completion_->didCancelChooser();
  static_cast<RenderViewImpl*>(render_view())->DismissDateTimeDialog();
}

void RendererDateTimePicker::OnDestruct() {
  delete this;
}

}  // namespace content
