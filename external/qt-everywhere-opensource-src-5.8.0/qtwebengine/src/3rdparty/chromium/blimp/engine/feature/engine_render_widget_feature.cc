// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/engine_render_widget_feature.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/net/input_message_converter.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {
namespace engine {

EngineRenderWidgetFeature::EngineRenderWidgetFeature(SettingsManager* settings)
    : settings_manager_(settings) {
  DCHECK(settings_manager_);
  settings_manager_->AddObserver(this);
}

EngineRenderWidgetFeature::~EngineRenderWidgetFeature() {
  DCHECK(settings_manager_);
  settings_manager_->RemoveObserver(this);
}

void EngineRenderWidgetFeature::set_render_widget_message_sender(
    std::unique_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  render_widget_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::set_input_message_sender(
    std::unique_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  input_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::set_ime_message_sender(
    std::unique_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  ime_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::set_compositor_message_sender(
    std::unique_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  compositor_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::OnRenderWidgetCreated(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(render_widget_host);

  int render_widget_id = AddRenderWidget(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  RenderWidgetMessage* render_widget_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&render_widget_message, tab_id);
  render_widget_message->set_type(RenderWidgetMessage::CREATED);
  render_widget_message->set_render_widget_id(render_widget_id);

  render_widget_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::OnRenderWidgetInitialized(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(render_widget_host);

  int render_widget_id = GetRenderWidgetId(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  RenderWidgetMessage* render_widget_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&render_widget_message, tab_id);
  render_widget_message->set_type(RenderWidgetMessage::INITIALIZE);
  render_widget_message->set_render_widget_id(render_widget_id);

  render_widget_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::OnRenderWidgetDeleted(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(render_widget_host);

  int render_widget_id = DeleteRenderWidget(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  RenderWidgetMessage* render_widget_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&render_widget_message, tab_id);
  render_widget_message->set_type(RenderWidgetMessage::DELETED);
  render_widget_message->set_render_widget_id(render_widget_id);

  render_widget_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::SendCompositorMessage(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host,
    const std::vector<uint8_t>& message) {
  CompositorMessage* compositor_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&compositor_message, tab_id);

  int render_widget_id = GetRenderWidgetId(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  compositor_message->set_render_widget_id(render_widget_id);
  // TODO(dtrainor): Move the transport medium to std::string* and use
  // set_allocated_payload.
  compositor_message->set_payload(message.data(),
                                  base::checked_cast<int>(message.size()));

  compositor_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::SendShowImeRequest(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host,
    const ui::TextInputClient* client) {
  DCHECK(client);

  ImeMessage* ime_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&ime_message, tab_id);

  int render_widget_id = GetRenderWidgetId(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);
  ime_message->set_render_widget_id(render_widget_id);
  ime_message->set_type(ImeMessage::SHOW_IME);
  ime_message->set_text_input_type(
      InputMessageConverter::TextInputTypeToProto(client->GetTextInputType()));

  gfx::Range text_range;
  base::string16 existing_text;
  client->GetTextRange(&text_range);
  client->GetTextFromRange(text_range, &existing_text);
  ime_message->set_ime_text(base::UTF16ToUTF8(existing_text));

  ime_message_sender_->ProcessMessage(std::move(blimp_message),
                                      net::CompletionCallback());
}

void EngineRenderWidgetFeature::SendHideImeRequest(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  ImeMessage* ime_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&ime_message, tab_id);

  int render_widget_id = GetRenderWidgetId(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);
  ime_message->set_render_widget_id(render_widget_id);
  ime_message->set_type(ImeMessage::HIDE_IME);

  ime_message_sender_->ProcessMessage(std::move(blimp_message),
                                      net::CompletionCallback());
}

void EngineRenderWidgetFeature::SetDelegate(
    const int tab_id,
    RenderWidgetMessageDelegate* delegate) {
  DCHECK(!FindDelegate(tab_id));
  delegates_[tab_id] = delegate;
}

void EngineRenderWidgetFeature::RemoveDelegate(const int tab_id) {
  DelegateMap::iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    delegates_.erase(it);
}

void EngineRenderWidgetFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(message->feature_case() == BlimpMessage::kRenderWidget ||
         message->feature_case() == BlimpMessage::kIme ||
         message->feature_case() == BlimpMessage::kInput ||
         message->feature_case() == BlimpMessage::kCompositor);

  int target_tab_id = message->target_tab_id();

  RenderWidgetMessageDelegate* delegate = FindDelegate(target_tab_id);
  DCHECK(delegate);

  content::RenderWidgetHost* render_widget_host = nullptr;

  switch (message->feature_case()) {
    case BlimpMessage::kInput:
      render_widget_host = GetRenderWidgetHost(target_tab_id,
                              message->input().render_widget_id());
      if (render_widget_host) {
        std::unique_ptr<blink::WebGestureEvent> event =
            input_message_converter_.ProcessMessage(message->input());
        if (event)
          delegate->OnWebGestureEvent(render_widget_host, std::move(event));
      }
      break;
    case BlimpMessage::kCompositor:
      render_widget_host = GetRenderWidgetHost(target_tab_id,
                                    message->compositor().render_widget_id());
      if (render_widget_host) {
        std::vector<uint8_t> payload(message->compositor().payload().size());
        memcpy(payload.data(),
               message->compositor().payload().data(),
               payload.size());
        delegate->OnCompositorMessageReceived(render_widget_host, payload);
      }
      break;
    case BlimpMessage::kIme:
      DCHECK(message->ime().type() == ImeMessage::SET_TEXT);
      render_widget_host =
          GetRenderWidgetHost(target_tab_id, message->ime().render_widget_id());
      if (render_widget_host && render_widget_host->GetView()) {
        SetTextFromIME(render_widget_host->GetView()->GetTextInputClient(),
                       message->ime().ime_text());
      }
      break;
    default:
      NOTREACHED();
  }

  callback.Run(net::OK);
}

void EngineRenderWidgetFeature::OnWebPreferencesChanged() {
  for (TabMap::iterator tab_it = tabs_.begin(); tab_it != tabs_.end();
       tab_it++) {
    RenderWidgetMaps render_widget_maps = tab_it->second;
    RenderWidgetToIdMap render_widget_to_id = render_widget_maps.first;
    for (RenderWidgetToIdMap::iterator it = render_widget_to_id.begin();
         it != render_widget_to_id.end(); it++) {
      content::RenderWidgetHost* render_widget_host = it->first;
      content::RenderViewHost* render_view_host =
          content::RenderViewHost::From(render_widget_host);
      if (render_view_host)
        render_view_host->OnWebkitPreferencesChanged();
    }
  }
}

void EngineRenderWidgetFeature::SetTextFromIME(ui::TextInputClient* client,
                                               std::string text) {
  if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
    // Clear out any existing text first and then insert new text entered
    // through IME.
    gfx::Range text_range;
    client->GetTextRange(&text_range);
    client->ExtendSelectionAndDelete(text_range.length(), text_range.length());

    client->InsertText(base::UTF8ToUTF16(text));
  }
}

EngineRenderWidgetFeature::RenderWidgetMessageDelegate*
EngineRenderWidgetFeature::FindDelegate(const int tab_id) {
  DelegateMap::const_iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    return it->second;
  return nullptr;
}

int EngineRenderWidgetFeature::AddRenderWidget(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  TabMap::iterator tab_it = tabs_.find(tab_id);
  if (tab_it == tabs_.end()) {
    tabs_[tab_id] = std::make_pair(RenderWidgetToIdMap(),
                                   IdToRenderWidgetMap());
    tab_it = tabs_.find(tab_id);
  }

  int render_widget_id = next_widget_id_.GetNext() + 1;

  RenderWidgetMaps* render_widget_maps = &tab_it->second;

  RenderWidgetToIdMap* render_widget_to_id = &render_widget_maps->first;
  IdToRenderWidgetMap* id_to_render_widget = &render_widget_maps->second;

  DCHECK(render_widget_to_id->find(render_widget_host) ==
      render_widget_to_id->end());
  DCHECK(id_to_render_widget->find(render_widget_id) ==
      id_to_render_widget->end());

  (*render_widget_to_id)[render_widget_host] = render_widget_id;
  (*id_to_render_widget)[render_widget_id] = render_widget_host;

  return render_widget_id;
}

int EngineRenderWidgetFeature::DeleteRenderWidget(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  TabMap::iterator tab_it = tabs_.find(tab_id);
  DCHECK(tab_it != tabs_.end());

  RenderWidgetMaps* render_widget_maps = &tab_it->second;

  RenderWidgetToIdMap* render_widget_to_id = &render_widget_maps->first;
  RenderWidgetToIdMap::iterator widget_to_id_it =
      render_widget_to_id->find(render_widget_host);
  DCHECK(widget_to_id_it != render_widget_to_id->end());

  int render_widget_id = widget_to_id_it->second;
  render_widget_to_id->erase(widget_to_id_it);

  IdToRenderWidgetMap* id_to_render_widget = &render_widget_maps->second;
  IdToRenderWidgetMap::iterator id_to_widget_it =
      id_to_render_widget->find(render_widget_id);
  DCHECK(id_to_widget_it->second == render_widget_host);
  id_to_render_widget->erase(id_to_widget_it);

  return render_widget_id;
}

int EngineRenderWidgetFeature::GetRenderWidgetId(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  TabMap::const_iterator tab_it = tabs_.find(tab_id);
  if (tab_it == tabs_.end())
    return 0;

  const RenderWidgetMaps* render_widget_maps = &tab_it->second;
  const RenderWidgetToIdMap* render_widget_to_id = &render_widget_maps->first;

  RenderWidgetToIdMap::const_iterator widget_it =
      render_widget_to_id->find(render_widget_host);
  if (widget_it == render_widget_to_id->end())
    return 0;

  return widget_it->second;
}

content::RenderWidgetHost* EngineRenderWidgetFeature::GetRenderWidgetHost(
    const int tab_id,
    const int render_widget_id) {
  TabMap::const_iterator tab_it = tabs_.find(tab_id);
  if (tab_it == tabs_.end())
    return nullptr;

  const RenderWidgetMaps* render_widget_maps = &tab_it->second;
  const IdToRenderWidgetMap* id_to_render_widget = &render_widget_maps->second;

  IdToRenderWidgetMap::const_iterator widget_id_it =
      id_to_render_widget->find(render_widget_id);
  if (widget_id_it == id_to_render_widget->end())
    return nullptr;

  return widget_id_it->second;
}

}  // namespace engine
}  // namespace blimp
