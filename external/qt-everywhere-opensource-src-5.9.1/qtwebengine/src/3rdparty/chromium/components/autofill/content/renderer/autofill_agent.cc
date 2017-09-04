// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>

#include <tuple>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/cert/cert_status_flags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebOptionElement.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebAutofillClient;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebString;
using blink::WebUserGestureIndicator;
using blink::WebVector;

namespace autofill {

namespace {

// Whether the "single click" autofill feature is enabled, through command-line
// or field trial.
bool IsSingleClickEnabled() {
// On Android, default to showing the dropdown on field focus.
// On desktop, require an extra click after field focus by default, unless the
// experiment is active.
#if defined(OS_ANDROID)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSingleClickAutofill);
#endif
  const std::string group_name =
      base::FieldTrialList::FindFullName("AutofillSingleClick");

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSingleClickAutofill))
    return true;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSingleClickAutofill))
    return false;

  return base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE);
}

// Gets all the data list values (with corresponding label) for the given
// element.
void GetDataListSuggestions(const WebInputElement& element,
                            std::vector<base::string16>* values,
                            std::vector<base::string16>* labels) {
  for (const auto& option : element.filteredDataListOptions()) {
    values->push_back(option.value());
    if (option.value() != option.label())
      labels->push_back(option.label());
    else
      labels->push_back(base::string16());
  }
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<base::string16>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaxListSize)
    strings->resize(kMaxListSize);

  // Limit the size of the strings in the vector.
  for (size_t i = 0; i < strings->size(); ++i) {
    if ((*strings)[i].length() > kMaxDataLength)
      (*strings)[i].resize(kMaxDataLength);
  }
}

}  // namespace

AutofillAgent::ShowSuggestionsOptions::ShowSuggestionsOptions()
    : autofill_on_empty_values(false),
      requires_caret_at_end(false),
      show_full_suggestion_list(false),
      show_password_suggestions_only(false) {
}

AutofillAgent::AutofillAgent(content::RenderFrame* render_frame,
                             PasswordAutofillAgent* password_autofill_agent,
                             PasswordGenerationAgent* password_generation_agent)
    : content::RenderFrameObserver(render_frame),
      form_cache_(*render_frame->GetWebFrame()),
      password_autofill_agent_(password_autofill_agent),
      password_generation_agent_(password_generation_agent),
      legacy_(render_frame->GetRenderView(), this),
      autofill_query_id_(0),
      was_query_node_autofilled_(false),
      ignore_text_changes_(false),
      is_popup_possibly_visible_(false),
      is_generation_popup_possibly_visible_(false),
      binding_(this),
      weak_ptr_factory_(this) {
  render_frame->GetWebFrame()->setAutofillClient(this);
  password_autofill_agent->SetAutofillAgent(this);

  // AutofillAgent is guaranteed to outlive |render_frame|.
  render_frame->GetInterfaceRegistry()->AddInterface(
      base::Bind(&AutofillAgent::BindRequest, base::Unretained(this)));

  // This owns itself, and will delete itself when |render_frame| is destructed
  // (same as AutofillAgent). This object must be constructed after
  // AutofillAgent so that password generation UI is shown before password
  // manager UI (see https://crbug.com/498545).
  new PageClickTracker(render_frame, this);
}

AutofillAgent::~AutofillAgent() {}

void AutofillAgent::BindRequest(mojom::AutofillAgentRequest request) {
  binding_.Bind(std::move(request));
}

bool AutofillAgent::FormDataCompare::operator()(const FormData& lhs,
                                                const FormData& rhs) const {
  return std::tie(lhs.name, lhs.origin, lhs.action, lhs.is_form_tag) <
         std::tie(rhs.name, rhs.origin, rhs.action, rhs.is_form_tag);
}

void AutofillAgent::DidCommitProvisionalLoad(bool is_new_navigation,
                                             bool is_same_page_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // TODO(dvadym): check if we need to check if it is main frame navigation
  // http://crbug.com/443155
  if (frame->parent())
    return;  // Not a top-level navigation.

  if (is_same_page_navigation) {
    OnSamePageNavigationCompleted();
  } else {
    // Navigation to a new page or a page refresh.
    form_cache_.Reset();
    submitted_forms_.clear();
    last_interacted_form_.reset();
    formless_elements_user_edited_.clear();
  }
}

void AutofillAgent::DidFinishDocumentLoad() {
  ProcessForms();
}

void AutofillAgent::WillSendSubmitEvent(const WebFormElement& form) {
  FireHostSubmitEvents(form, /*form_submitted=*/false);
}

void AutofillAgent::WillSubmitForm(const WebFormElement& form) {
  FireHostSubmitEvents(form, /*form_submitted=*/true);
}

void AutofillAgent::DidChangeScrollOffset() {
  if (IsKeyboardAccessoryEnabled())
    return;

  HidePopup();
}

void AutofillAgent::FocusedNodeChanged(const WebNode& node) {
  HidePopup();

  if (node.isNull() || !node.isElementNode()) {
    if (!last_interacted_form_.isNull()) {
      // Focus moved away from the last interacted form to somewhere else on
      // the page.
      GetAutofillDriver()->FocusNoLongerOnForm();
    }
    return;
  }

  WebElement web_element = node.toConst<WebElement>();
  const WebInputElement* element = toWebInputElement(&web_element);

  if (!last_interacted_form_.isNull() &&
      (!element || last_interacted_form_ != element->form())) {
    // The focused element is not part of the last interacted form (could be
    // in a different form).
    GetAutofillDriver()->FocusNoLongerOnForm();
    return;
  }

  if (!element || !element->isEnabled() || element->isReadOnly() ||
      !element->isTextField())
    return;

  element_ = *element;
}

void AutofillAgent::OnDestruct() {
  Shutdown();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void AutofillAgent::FireHostSubmitEvents(const WebFormElement& form,
                                         bool form_submitted) {
  FormData form_data;
  if (!form_util::ExtractFormData(form, &form_data))
    return;

  FireHostSubmitEvents(form_data, form_submitted);
}

void AutofillAgent::FireHostSubmitEvents(const FormData& form_data,
                                         bool form_submitted) {
  // We remember when we have fired this IPC for this form in this frame load,
  // because forms with a submit handler may fire both WillSendSubmitEvent
  // and WillSubmitForm, and we don't want duplicate messages.
  if (!submitted_forms_.count(form_data)) {
    GetAutofillDriver()->WillSubmitForm(form_data, base::TimeTicks::Now());
    submitted_forms_.insert(form_data);
  }

  if (form_submitted) {
    GetAutofillDriver()->FormSubmitted(form_data);
  }
}

void AutofillAgent::Shutdown() {
  binding_.Close();
  legacy_.Shutdown();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AutofillAgent::FocusChangeComplete() {
  WebDocument doc = render_frame()->GetWebFrame()->document();
  WebElement focused_element;
  if (!doc.isNull())
    focused_element = doc.focusedElement();
  if (!focused_element.isNull()) {
    if (password_generation_agent_ &&
        password_generation_agent_->FocusedNodeHasChanged(focused_element)) {
      is_generation_popup_possibly_visible_ = true;
      is_popup_possibly_visible_ = true;
    }
    if (password_autofill_agent_)
      password_autofill_agent_->FocusedNodeHasChanged(focused_element);
  }
}

void AutofillAgent::setIgnoreTextChanges(bool ignore) {
  ignore_text_changes_ = ignore;
}

void AutofillAgent::FormControlElementClicked(
    const WebFormControlElement& element,
    bool was_focused) {
  // TODO(estade): Remove this check when PageClickTracker is per-frame.
  if (element.document().frame() != render_frame()->GetWebFrame())
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (!input_element && !form_util::IsTextAreaElement(element))
    return;

  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  options.show_full_suggestion_list = element.isAutofilled();

  if (!IsSingleClickEnabled()) {
    // Show full suggestions when clicking on an already-focused form field. On
    // the initial click (not focused yet), only show password suggestions.
    options.show_full_suggestion_list =
        options.show_full_suggestion_list || was_focused;
    options.show_password_suggestions_only = !was_focused;
  }
  ShowSuggestions(element, options);
}

void AutofillAgent::textFieldDidEndEditing(const WebInputElement& element) {
  GetAutofillDriver()->DidEndTextFieldEditing();
}

void AutofillAgent::textFieldDidChange(const WebFormControlElement& element) {
  DCHECK(toWebInputElement(&element) || form_util::IsTextAreaElement(element));

  if (ignore_text_changes_)
    return;

  // Disregard text changes that aren't caused by user gestures or pastes. Note
  // that pastes aren't necessarily user gestures because Blink's conception of
  // user gestures is centered around creating new windows/tabs.
  if (!IsUserGesture() && !render_frame()->IsPasting())
    return;

  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AutofillAgent::TextFieldDidChangeImpl,
                            weak_ptr_factory_.GetWeakPtr(), element));
}

void AutofillAgent::TextFieldDidChangeImpl(
    const WebFormControlElement& element) {
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.focused())
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (input_element) {
    // Remember the last form the user interacted with.
    if (element.form().isNull()) {
      formless_elements_user_edited_.insert(element);
    } else {
      last_interacted_form_ = element.form();
    }

    // |password_autofill_agent_| keeps track of all text changes even if
    // it isn't displaying UI.
    password_autofill_agent_->UpdateStateForTextChange(*input_element);

    if (password_generation_agent_ &&
        password_generation_agent_->TextDidChangeInTextField(*input_element)) {
      is_popup_possibly_visible_ = true;
      return;
    }

    if (password_autofill_agent_->TextDidChangeInTextField(*input_element)) {
      is_popup_possibly_visible_ = true;
      element_ = element;
      return;
    }
  }

  ShowSuggestionsOptions options;
  options.requires_caret_at_end = true;
  ShowSuggestions(element, options);

  FormData form;
  FormFieldData field;
  if (form_util::FindFormAndFieldForFormControlElement(element, &form,
                                                       &field)) {
    GetAutofillDriver()->TextFieldDidChange(form, field,
                                            base::TimeTicks::Now());
  }
}

void AutofillAgent::textFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (event.windowsKeyCode == ui::VKEY_DOWN ||
      event.windowsKeyCode == ui::VKEY_UP) {
    ShowSuggestionsOptions options;
    options.autofill_on_empty_values = true;
    options.requires_caret_at_end = true;
    ShowSuggestions(element, options);
  }
}

void AutofillAgent::openTextDataListChooser(const WebInputElement& element) {
  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  ShowSuggestions(element, options);
}

void AutofillAgent::dataListOptionsChanged(const WebInputElement& element) {
  if (!is_popup_possibly_visible_ || !element.focused())
    return;

  TextFieldDidChangeImpl(element);
}

void AutofillAgent::firstUserGestureObserved() {
  password_autofill_agent_->FirstUserGestureObserved();

  GetAutofillDriver()->FirstUserGestureObserved();
}

void AutofillAgent::DoAcceptDataListSuggestion(
    const base::string16& suggested_value) {
  WebInputElement* input_element = toWebInputElement(&element_);
  DCHECK(input_element);
  base::string16 new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (input_element->isMultiple() && input_element->isEmailField()) {
    std::vector<base::string16> parts = base::SplitString(
        base::StringPiece16(input_element->editingValue()),
        base::ASCIIToUTF16(","), base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() == 0)
      parts.push_back(base::string16());

    base::string16 last_part = parts.back();
    // We want to keep just the leading whitespace.
    for (size_t i = 0; i < last_part.size(); ++i) {
      if (!base::IsUnicodeWhitespace(last_part[i])) {
        last_part = last_part.substr(0, i);
        break;
      }
    }
    last_part.append(suggested_value);
    parts.back() = last_part;

    new_value = base::JoinString(parts, base::ASCIIToUTF16(","));
  }
  DoFillFieldWithValue(new_value, input_element);
}

// mojom::AutofillAgent:
void AutofillAgent::FirstUserGestureObservedInTab() {
  password_autofill_agent_->FirstUserGestureObserved();
}

void AutofillAgent::FillForm(int32_t id, const FormData& form) {
  if (id != autofill_query_id_ && id != kNoQueryId)
    return;

  was_query_node_autofilled_ = element_.isAutofilled();
  form_util::FillForm(form, element_);
  if (!element_.form().isNull())
    last_interacted_form_ = element_.form();

  GetAutofillDriver()->DidFillAutofillFormData(form, base::TimeTicks::Now());
}

void AutofillAgent::PreviewForm(int32_t id, const FormData& form) {
  if (id != autofill_query_id_)
    return;

  was_query_node_autofilled_ = element_.isAutofilled();
  form_util::PreviewForm(form, element_);

  GetAutofillDriver()->DidPreviewAutofillFormData();
}

void AutofillAgent::OnPing() {
  GetAutofillDriver()->PingAck();
}

void AutofillAgent::FieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  for (const auto& form : forms) {
    form_cache_.ShowPredictions(form);
  }
}

void AutofillAgent::ClearForm() {
  form_cache_.ClearFormWithElement(element_);
}

void AutofillAgent::ClearPreviewedForm() {
  if (!element_.isNull()) {
    if (password_autofill_agent_->DidClearAutofillSelection(element_))
      return;

    form_util::ClearPreviewedFormWithElement(element_,
                                             was_query_node_autofilled_);
  } else {
    // TODO(isherman): There seem to be rare cases where this code *is*
    // reachable: see [ http://crbug.com/96321#c6 ].  Ideally we would
    // understand those cases and fix the code to avoid them.  However, so far I
    // have been unable to reproduce such a case locally.  If you hit this
    // NOTREACHED(), please file a bug against me.
    NOTREACHED();
  }
}

void AutofillAgent::FillFieldWithValue(const base::string16& value) {
  WebInputElement* input_element = toWebInputElement(&element_);
  if (input_element) {
    DoFillFieldWithValue(value, input_element);
    input_element->setAutofilled(true);
  }
}

void AutofillAgent::PreviewFieldWithValue(const base::string16& value) {
  WebInputElement* input_element = toWebInputElement(&element_);
  if (input_element)
    DoPreviewFieldWithValue(value, input_element);
}

void AutofillAgent::AcceptDataListSuggestion(const base::string16& value) {
  DoAcceptDataListSuggestion(value);
}

void AutofillAgent::FillPasswordSuggestion(const base::string16& username,
                                           const base::string16& password) {
  bool handled =
      password_autofill_agent_->FillSuggestion(element_, username, password);
  DCHECK(handled);
}

void AutofillAgent::PreviewPasswordSuggestion(const base::string16& username,
                                              const base::string16& password) {
  bool handled =
      password_autofill_agent_->PreviewSuggestion(element_, username, password);
  DCHECK(handled);
}

void AutofillAgent::ShowInitialPasswordAccountSuggestions(
    int32_t key,
    const PasswordFormFillData& form_data) {
  std::vector<blink::WebInputElement> elements;
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (password_autofill_agent_->logging_state_active()) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(SavePasswordProgressLogger::
                           STRING_ON_SHOW_INITIAL_PASSWORD_ACCOUNT_SUGGESTIONS);
  }
  password_autofill_agent_->GetFillableElementFromFormData(
      key, form_data, logger.get(), &elements);

  // If wait_for_username is true, we don't want to initially show form options
  // until the user types in a valid username.
  if (form_data.wait_for_username)
    return;

  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  options.show_full_suggestion_list = true;
  for (auto element : elements)
    ShowSuggestions(element, options);
}

void AutofillAgent::OnSamePageNavigationCompleted() {
  if (last_interacted_form_.isNull()) {
    // If no last interacted form is available (i.e., there is no form tag),
    // we check if all the elements the user has interacted with are gone,
    // to decide if submission has occurred.
    if (formless_elements_user_edited_.size() == 0 ||
        form_util::IsSomeControlElementVisible(formless_elements_user_edited_))
      return;

    FormData constructed_form;
    if (CollectFormlessElements(&constructed_form))
      FireHostSubmitEvents(constructed_form, /*form_submitted=*/true);
  } else {
    // Otherwise, assume form submission only if the form is now gone, either
    // invisible or removed from the DOM.
    if (form_util::AreFormContentsVisible(last_interacted_form_))
      return;

    FireHostSubmitEvents(last_interacted_form_, /*form_submitted=*/true);
  }

  last_interacted_form_.reset();
  formless_elements_user_edited_.clear();
}

bool AutofillAgent::CollectFormlessElements(FormData* output) {
  WebDocument document = render_frame()->GetWebFrame()->document();

  // Build up the FormData from the unowned elements. This logic mostly
  // mirrors the construction of the synthetic form in form_cache.cc, but
  // happens at submit-time so we can capture the modifications the user
  // has made, and doesn't depend on form_cache's internal state.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document.all(),
                                                         &fieldsets);

  if (control_elements.size() > form_util::kMaxParseableFields)
    return false;

  const form_util::ExtractMask extract_mask =
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTIONS);

  return form_util::UnownedCheckoutFormElementsAndFieldSetsToFormData(
      fieldsets, control_elements, nullptr, document, extract_mask, output,
      nullptr);
}

void AutofillAgent::ShowSuggestions(const WebFormControlElement& element,
                                    const ShowSuggestionsOptions& options) {
  if (!element.isEnabled() || element.isReadOnly())
    return;
  if (!element.suggestedValue().isEmpty())
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (input_element) {
    if (!input_element->isTextField())
      return;
    if (!input_element->suggestedValue().isEmpty())
      return;
  } else {
    DCHECK(form_util::IsTextAreaElement(element));
    if (!element.toConst<WebFormControlElement>().suggestedValue().isEmpty())
      return;
  }

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met.
  WebString value = element.editingValue();
  if (value.length() > kMaxDataLength ||
      (!options.autofill_on_empty_values && value.isEmpty()) ||
      (options.requires_caret_at_end &&
       (element.selectionStart() != element.selectionEnd() ||
        element.selectionEnd() != static_cast<int>(value.length())))) {
    // Any popup currently showing is obsolete.
    HidePopup();
    return;
  }

  element_ = element;
  if (form_util::IsAutofillableInputElement(input_element) &&
      password_autofill_agent_->ShowSuggestions(
          *input_element, options.show_full_suggestion_list,
          is_generation_popup_possibly_visible_)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (is_generation_popup_possibly_visible_)
    return;

  if (options.show_password_suggestions_only)
    return;

  // Password field elements should only have suggestions shown by the password
  // autofill agent.
  if (input_element && input_element->isPasswordField())
    return;

  QueryAutofillSuggestions(element);
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element) {
  if (!element.document().frame())
    return;

  DCHECK(toWebInputElement(&element) || form_util::IsTextAreaElement(element));

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  FormData form;
  FormFieldData field;
  if (!form_util::FindFormAndFieldForFormControlElement(element, &form,
                                                        &field)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, nullptr, form_util::EXTRACT_VALUE,
                                     &field);
  }

  std::vector<base::string16> data_list_values;
  std::vector<base::string16> data_list_labels;
  const WebInputElement* input_element = toWebInputElement(&element);
  if (input_element) {
    // Find the datalist values and send them to the browser process.
    GetDataListSuggestions(*input_element,
                           &data_list_values,
                           &data_list_labels);
    TrimStringVectorForIPC(&data_list_values);
    TrimStringVectorForIPC(&data_list_labels);
  }

  is_popup_possibly_visible_ = true;

  GetAutofillDriver()->SetDataList(data_list_values, data_list_labels);
  GetAutofillDriver()->QueryFormFieldAutofill(
      autofill_query_id_, form, field,
      render_frame()->GetRenderView()->ElementBoundsInWindow(element_));
}

void AutofillAgent::DoFillFieldWithValue(const base::string16& value,
                                         WebInputElement* node) {
  base::AutoReset<bool> auto_reset(&ignore_text_changes_, true);
  node->setEditingValue(value.substr(0, node->maxLength()));
}

void AutofillAgent::DoPreviewFieldWithValue(const base::string16& value,
                                            WebInputElement* node) {
  was_query_node_autofilled_ = element_.isAutofilled();
  node->setSuggestedValue(value.substr(0, node->maxLength()));
  node->setAutofilled(true);
  form_util::PreviewSuggestion(node->suggestedValue(), node->value(), node);
}

void AutofillAgent::ProcessForms() {
  // Record timestamp of when the forms are first seen. This is used to
  // measure the overhead of the Autofill feature.
  base::TimeTicks forms_seen_timestamp = base::TimeTicks::Now();

  WebLocalFrame* frame = render_frame()->GetWebFrame();
  std::vector<FormData> forms = form_cache_.ExtractNewForms();

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() || !frame->parent()) {
    GetAutofillDriver()->FormsSeen(forms, forms_seen_timestamp);
  }
}

void AutofillAgent::HidePopup() {
  if (!is_popup_possibly_visible_)
    return;
  is_popup_possibly_visible_ = false;
  is_generation_popup_possibly_visible_ = false;

  GetAutofillDriver()->HidePopup();
}

bool AutofillAgent::IsUserGesture() const {
  return WebUserGestureIndicator::isProcessingUserGesture();
}

void AutofillAgent::didAssociateFormControlsDynamically() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Frame is only processed if it has finished loading, otherwise you can end
  // up with a partially parsed form.
  if (frame && !frame->isLoading()) {
    ProcessForms();
    password_autofill_agent_->OnDynamicFormsSeen();
    if (password_generation_agent_)
      password_generation_agent_->OnDynamicFormsSeen();
  }
}

void AutofillAgent::ajaxSucceeded() {
  OnSamePageNavigationCompleted();
  password_autofill_agent_->AJAXSucceeded();
}

const mojom::AutofillDriverPtr& AutofillAgent::GetAutofillDriver() {
  if (!autofill_driver_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&autofill_driver_));
  }

  return autofill_driver_;
}

const mojom::PasswordManagerDriverPtr&
AutofillAgent::GetPasswordManagerDriver() {
  DCHECK(password_autofill_agent_);
  return password_autofill_agent_->GetPasswordManagerDriver();
}

// LegacyAutofillAgent ---------------------------------------------------------

AutofillAgent::LegacyAutofillAgent::LegacyAutofillAgent(
    content::RenderView* render_view,
    AutofillAgent* agent)
    : content::RenderViewObserver(render_view), agent_(agent) {
}

AutofillAgent::LegacyAutofillAgent::~LegacyAutofillAgent() {
}

void AutofillAgent::LegacyAutofillAgent::Shutdown() {
  agent_ = nullptr;
}

void AutofillAgent::LegacyAutofillAgent::OnDestruct() {
  // No-op. Don't delete |this|.
}

void AutofillAgent::LegacyAutofillAgent::FocusChangeComplete() {
  if (agent_)
    agent_->FocusChangeComplete();
}

}  // namespace autofill
