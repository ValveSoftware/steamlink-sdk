// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/audio_modem/audio_modem_api.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "components/audio_modem/public/modem.h"
#include "components/audio_modem/test/stub_modem.h"
#include "components/audio_modem/test/stub_whispernet_client.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"

using audio_modem::AUDIBLE;
using audio_modem::AudioToken;
using audio_modem::INAUDIBLE;
using audio_modem::StubModem;
using audio_modem::StubWhispernetClient;

using base::BinaryValue;
using base::DictionaryValue;
using base::ListValue;
using base::StringValue;
using base::Value;

using content::BrowserContext;

namespace ext_test_utils = extension_function_test_utils;

namespace extensions {

namespace {

// The TestingFactoryFunction uses a BrowserContext as its context pointer.
// But each BrowserContext is still associated with a unit test.
// So we store the StubModem created in each test.
std::map<BrowserContext*, StubModem*> g_modems;

// Create a test AudioModemAPI and store the modem it uses.
std::unique_ptr<KeyedService> ApiFactoryFunction(BrowserContext* context) {
  StubModem* modem = new StubModem;
  g_modems[context] = modem;
  return base::WrapUnique(new AudioModemAPI(
      context,
      base::WrapUnique<audio_modem::WhispernetClient>(new StubWhispernetClient),
      base::WrapUnique<audio_modem::Modem>(modem)));
}

std::unique_ptr<DictionaryValue> CreateParams(const std::string& audio_band) {
  std::unique_ptr<DictionaryValue> params(new DictionaryValue);
  params->SetInteger("timeoutMillis", 60000);
  params->SetString("band", audio_band);
  params->SetInteger("encoding.tokenLength", 4);
  return params;
}

std::unique_ptr<BinaryValue> CreateToken(const std::string& token) {
  return BinaryValue::CreateWithCopiedBuffer(token.c_str(), token.size());
}

std::unique_ptr<ListValue> CreateList(std::unique_ptr<Value> single_elt) {
  std::unique_ptr<ListValue> list(new ListValue);
  list->Append(std::move(single_elt));
  return list;
}

std::unique_ptr<ListValue> CreateList(std::unique_ptr<Value> elt1,
                                      std::unique_ptr<Value> elt2) {
  std::unique_ptr<ListValue> list(new ListValue);
  list->Append(std::move(elt1));
  list->Append(std::move(elt2));
  return list;
}

DictionaryValue* CreateReceivedToken(const std::string& token,
                                     const std::string& audio_band) {
  DictionaryValue* out = new DictionaryValue;
  out->Set("token", CreateToken(token));
  out->SetString("band", audio_band);
  return out;
}

bool ReceivedSingleToken(const Event* event,
                         const DictionaryValue* expected_token) {
  ListValue* received_tokens;
  event->event_args->GetList(0, &received_tokens);
  if (received_tokens->GetSize() != 1)
    return false;

  DictionaryValue* received_token;
  received_tokens->GetDictionary(0, &received_token);
  return received_token->Equals(expected_token);
}

// TODO(ckehoe): Put this in //extensions/test.
// Then replace the other EventRouter mocks.
class StubEventRouter : public EventRouter {
 public:
  // Callback to receive events. First argument is
  // the extension id to receive the event.
  using EventCallback =
      base::Callback<void(const std::string&, std::unique_ptr<Event>)>;

  explicit StubEventRouter(BrowserContext* context)
      : EventRouter(context, nullptr) {}

  void DispatchEventToExtension(const std::string& extension_id,
                                std::unique_ptr<Event> event) override {
    event_callback_.Run(extension_id, std::move(event));
  }

  void SetEventCallBack(EventCallback event_callback) {
    event_callback_ = event_callback;
  }

  void ClearEventCallback() {
    event_callback_.Reset();
  }

 private:
  EventCallback event_callback_;
};

// StubEventRouter factory function
std::unique_ptr<KeyedService> StubEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return base::WrapUnique(new StubEventRouter(context));
}

}  // namespace

class AudioModemApiUnittest : public ExtensionApiUnittest {
 public:
  AudioModemApiUnittest() {}
  ~AudioModemApiUnittest() override {}

 protected:
  template <typename Function>
  const std::string RunFunction(std::unique_ptr<ListValue> args,
                                const Extension* extension) {
    scoped_refptr<UIThreadExtensionFunction> function(new Function);
    function->set_extension(extension);
    function->set_browser_context(profile());
    function->set_has_callback(true);
    ext_test_utils::RunFunction(function.get(), std::move(args), browser(),
                                ext_test_utils::NONE);

    std::string result_status;
    CHECK(function->GetResultList()->GetString(0, &result_status));
    return result_status;
  }

  template <typename Function>
  const std::string RunFunction(std::unique_ptr<ListValue> args) {
    return RunFunction<Function>(std::move(args), GetExtension(std::string()));
  }

  StubModem* GetModem() const {
    return g_modems[profile()];
  }

  const Extension* GetExtension(const std::string& name) {
    if (!extensions_by_name_[name].get()) {
      std::unique_ptr<DictionaryValue> extension_definition(
          new DictionaryValue);
      extension_definition->SetString("name", name);
      extension_definition->SetString("version", "1.0");
      extensions_by_name_[name] = api_test_utils::CreateExtension(
          Manifest::INTERNAL, extension_definition.get(), name);
      DVLOG(2) << "Created extension " << extensions_by_name_[name]->id();
    }
    return extensions_by_name_[name].get();
  }

  const std::vector<std::unique_ptr<const Event>>& GetEventsForExtension(
      const std::string& name) {
    const Extension* extension = extensions_by_name_[name].get();
    DCHECK(extension);
    return events_by_extension_id_[extension->id()];
  }

  const std::vector<std::unique_ptr<const Event>>& GetEvents() {
    return GetEventsForExtension(std::string());
  }

 private:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    AudioModemAPI::GetFactoryInstance()->SetTestingFactory(
        profile(), &ApiFactoryFunction);

    StubEventRouter* stub_event_router = static_cast<StubEventRouter*>(
        extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &StubEventRouterFactoryFunction));
    stub_event_router->SetEventCallBack(base::Bind(
        &AudioModemApiUnittest::CaptureEvent, base::Unretained(this)));
  }

  void CaptureEvent(const std::string& extension_id,
                    std::unique_ptr<Event> event) {
    events_by_extension_id_[extension_id].push_back(std::move(event));
  }

  std::map<std::string, scoped_refptr<Extension>> extensions_by_name_;

  std::map<std::string, std::vector<std::unique_ptr<const Event>>>
      events_by_extension_id_;
};

TEST_F(AudioModemApiUnittest, TransmitBasic) {
  // Start transmitting inaudibly.
  EXPECT_EQ("success", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("inaudible"), CreateToken("1234"))));
  EXPECT_TRUE(GetModem()->IsPlaying(INAUDIBLE));

  // Can't cancel audible transmit - we haven't started it yet.
  EXPECT_EQ("invalidRequest",
            RunFunction<AudioModemStopTransmitFunction>(
                CreateList(base::MakeUnique<StringValue>("audible"))));

  // Start transmitting audibly.
  EXPECT_EQ("success", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("audible"), CreateToken("ABCD"))));
  EXPECT_TRUE(GetModem()->IsPlaying(AUDIBLE));

  // Stop audible transmit. We're still transmitting inaudibly.
  EXPECT_EQ("success", RunFunction<AudioModemStopTransmitFunction>(CreateList(
                           base::MakeUnique<StringValue>("audible"))));
  EXPECT_FALSE(GetModem()->IsPlaying(AUDIBLE));
  EXPECT_TRUE(GetModem()->IsPlaying(INAUDIBLE));

  // Stop inaudible transmit.
  EXPECT_EQ("success", RunFunction<AudioModemStopTransmitFunction>(CreateList(
                           base::MakeUnique<StringValue>("inaudible"))));
  EXPECT_FALSE(GetModem()->IsPlaying(INAUDIBLE));
}

TEST_F(AudioModemApiUnittest, ReceiveBasic) {
  // Start listening for audible tokens.
  EXPECT_EQ("success", RunFunction<AudioModemReceiveFunction>(
      CreateList(CreateParams("audible"))));
  EXPECT_TRUE(GetModem()->IsRecording(AUDIBLE));

  // Can't cancel inaudible receive - we haven't started it yet.
  EXPECT_EQ("invalidRequest",
            RunFunction<AudioModemStopReceiveFunction>(
                CreateList(base::MakeUnique<StringValue>("inaudible"))));

  // Send some audible tokens.
  std::vector<AudioToken> tokens;
  tokens.push_back(AudioToken("1234", true));
  tokens.push_back(AudioToken("ABCD", true));
  tokens.push_back(AudioToken("abcd", false));
  GetModem()->DeliverTokens(tokens);

  // Check the tokens received.
  EXPECT_EQ(1u, GetEvents().size());
  std::unique_ptr<ListValue> expected_tokens(new ListValue);
  expected_tokens->Append(CreateReceivedToken("1234", "audible"));
  expected_tokens->Append(CreateReceivedToken("ABCD", "audible"));
  ListValue* received_tokens;
  GetEvents()[0]->event_args->GetList(0, &received_tokens);
  EXPECT_TRUE(received_tokens->Equals(expected_tokens.get()));

  // Start listening for inaudible tokens.
  EXPECT_EQ("success", RunFunction<AudioModemReceiveFunction>(
      CreateList(CreateParams("inaudible"))));
  EXPECT_TRUE(GetModem()->IsRecording(INAUDIBLE));

  // Send some more tokens.
  tokens.push_back(AudioToken("5678", false));
  GetModem()->DeliverTokens(tokens);

  // Check the tokens received.
  EXPECT_EQ(2u, GetEvents().size());
  expected_tokens->Append(CreateReceivedToken("abcd", "inaudible"));
  expected_tokens->Append(CreateReceivedToken("5678", "inaudible"));
  GetEvents()[1]->event_args->GetList(0, &received_tokens);
  EXPECT_TRUE(received_tokens->Equals(expected_tokens.get()));

  // Stop audible receive. We're still receiving inaudible.
  EXPECT_EQ("success", RunFunction<AudioModemStopReceiveFunction>(CreateList(
                           base::MakeUnique<StringValue>("audible"))));
  EXPECT_FALSE(GetModem()->IsRecording(AUDIBLE));
  EXPECT_TRUE(GetModem()->IsRecording(INAUDIBLE));

  // Stop inaudible receive.
  EXPECT_EQ("success", RunFunction<AudioModemStopReceiveFunction>(CreateList(
                           base::MakeUnique<StringValue>("inaudible"))));
  EXPECT_FALSE(GetModem()->IsRecording(INAUDIBLE));
}

TEST_F(AudioModemApiUnittest, TransmitMultiple) {
  // Start transmit.
  EXPECT_EQ("success", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("audible"), CreateToken("1234")),
      GetExtension("ext1")));
  EXPECT_TRUE(GetModem()->IsPlaying(AUDIBLE));

  // Another extension can't interfere with the first one.
  EXPECT_EQ("inUse", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("audible"), CreateToken("ABCD")),
      GetExtension("ext2")));
  EXPECT_EQ("invalidRequest",
            RunFunction<AudioModemStopTransmitFunction>(
                CreateList(base::MakeUnique<StringValue>("audible")),
                GetExtension("ext2")));
  EXPECT_TRUE(GetModem()->IsPlaying(AUDIBLE));

  // The other extension can use the other audio band, however.
  EXPECT_EQ("success", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("inaudible"), CreateToken("ABCD")),
      GetExtension("ext2")));
  EXPECT_TRUE(GetModem()->IsPlaying(INAUDIBLE));

  // The first extension can change its token.
  // But the other band is still in use.
  EXPECT_EQ("success", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("audible"), CreateToken("abcd")),
      GetExtension("ext1")));
  EXPECT_EQ("inUse", RunFunction<AudioModemTransmitFunction>(
      CreateList(CreateParams("inaudible"), CreateToken("1234")),
      GetExtension("ext1")));

  // Stop transmission.
  EXPECT_EQ("success", RunFunction<AudioModemStopTransmitFunction>(
                           CreateList(base::MakeUnique<StringValue>("audible")),
                           GetExtension("ext1")));
  EXPECT_FALSE(GetModem()->IsPlaying(AUDIBLE));
  EXPECT_EQ("success",
            RunFunction<AudioModemStopTransmitFunction>(
                CreateList(base::MakeUnique<StringValue>("inaudible")),
                GetExtension("ext2")));
  EXPECT_FALSE(GetModem()->IsPlaying(INAUDIBLE));
}

TEST_F(AudioModemApiUnittest, ReceiveMultiple) {
  // Start receive. Multiple extensions can receive on the same band.
  EXPECT_EQ("success", RunFunction<AudioModemReceiveFunction>(
      CreateList(CreateParams("inaudible")), GetExtension("ext1")));
  EXPECT_TRUE(GetModem()->IsRecording(INAUDIBLE));
  EXPECT_EQ("success", RunFunction<AudioModemReceiveFunction>(
      CreateList(CreateParams("inaudible")), GetExtension("ext2")));

  // Receive a token.
  GetModem()->DeliverTokens(std::vector<AudioToken>(
      1, AudioToken("abcd", false)));
  EXPECT_EQ(1u, GetEventsForExtension("ext1").size());
  EXPECT_EQ(1u, GetEventsForExtension("ext2").size());

  // Check the token received.
  std::unique_ptr<DictionaryValue> expected_token(
      CreateReceivedToken("abcd", "inaudible"));
  EXPECT_TRUE(ReceivedSingleToken(GetEventsForExtension("ext1")[0].get(),
                                  expected_token.get()));
  EXPECT_TRUE(ReceivedSingleToken(GetEventsForExtension("ext2")[0].get(),
                                  expected_token.get()));

  // If one extension stops, the modem is still receiving for the other.
  EXPECT_EQ("success",
            RunFunction<AudioModemStopReceiveFunction>(
                CreateList(base::MakeUnique<StringValue>("inaudible")),
                GetExtension("ext1")));
  EXPECT_TRUE(GetModem()->IsRecording(INAUDIBLE));

  // Receive another token. Should only go to ext2.
  GetModem()->DeliverTokens(std::vector<AudioToken>(
      1, AudioToken("1234", false)));
  EXPECT_EQ(1u, GetEventsForExtension("ext1").size());
  EXPECT_EQ(2u, GetEventsForExtension("ext2").size());
  expected_token.reset(CreateReceivedToken("1234", "inaudible"));
  EXPECT_TRUE(ReceivedSingleToken(GetEventsForExtension("ext2")[1].get(),
                                  expected_token.get()));

  EXPECT_EQ("success",
            RunFunction<AudioModemStopReceiveFunction>(
                CreateList(base::MakeUnique<StringValue>("inaudible")),
                GetExtension("ext2")));
  EXPECT_FALSE(GetModem()->IsRecording(INAUDIBLE));
}

}  // namespace extensions
