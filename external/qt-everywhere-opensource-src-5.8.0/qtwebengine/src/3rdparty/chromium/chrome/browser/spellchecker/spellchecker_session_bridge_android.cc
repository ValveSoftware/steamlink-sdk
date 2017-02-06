// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellchecker_session_bridge_android.h"

#include <stddef.h>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "content/public/browser/render_process_host.h"
#include "jni/SpellCheckerSessionBridge_jni.h"

SpellCheckerSessionBridge::SpellCheckerSessionBridge(int render_process_id)
    : render_process_id_(render_process_id) {}

SpellCheckerSessionBridge::~SpellCheckerSessionBridge() {}

// static
bool SpellCheckerSessionBridge::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void SpellCheckerSessionBridge::RequestTextCheck(int route_id,
                                                 int identifier,
                                                 const base::string16& text) {
  // SpellCheckerSessionBridge#create() will return null if spell checker
  // service is unavailable.
  if (java_object_initialization_failed_)
    return;

  // RequestTextCheck IPC arrives at the message filter before
  // ToggleSpellCheck IPC when the user focuses an input field that already
  // contains completed text.  We need to initialize the spellchecker here
  // rather than in response to ToggleSpellCheck so that the existing text
  // will be spellchecked immediately.
  if (java_object_.is_null()) {
    java_object_.Reset(Java_SpellCheckerSessionBridge_create(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(this)));
    if (java_object_.is_null()) {
      java_object_initialization_failed_ = true;
      return;
    }
  }

  // Save incoming requests to run at the end of the currently active request.
  // If multiple requests arrive during one active request, only the most
  // recent request will run (the others get overwritten).
  if (active_request_) {
    pending_request_.reset(new SpellingRequest(route_id, identifier, text));
    return;
  }

  active_request_.reset(new SpellingRequest(route_id, identifier, text));

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SpellCheckerSessionBridge_requestTextCheck(
      env, java_object_.obj(),
      base::android::ConvertUTF16ToJavaString(env, text).obj());
}

void SpellCheckerSessionBridge::ProcessSpellCheckResults(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jintArray>& offset_array,
    const JavaParamRef<jintArray>& length_array) {
  std::vector<int> offsets;
  std::vector<int> lengths;

  base::android::JavaIntArrayToIntVector(env, offset_array, &offsets);
  base::android::JavaIntArrayToIntVector(env, length_array, &lengths);

  std::vector<SpellCheckResult> results;
  for (size_t i = 0; i < offsets.size(); i++) {
    results.push_back(
        SpellCheckResult(SpellCheckResult::SPELLING, offsets[i], lengths[i]));
  }

  content::RenderProcessHost* sender =
      content::RenderProcessHost::FromID(render_process_id_);

  if (sender != nullptr) {
    sender->Send(new SpellCheckMsg_RespondTextCheck(
        active_request_->route_id, active_request_->identifier,
        active_request_->text, results));
  }

  active_request_ = std::move(pending_request_);
  if (active_request_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SpellCheckerSessionBridge_requestTextCheck(
        env, java_object_.obj(),
        base::android::ConvertUTF16ToJavaString(env, active_request_->text)
            .obj());
  }
}

void SpellCheckerSessionBridge::DisconnectSession() {
  java_object_.Reset();
}

SpellCheckerSessionBridge::SpellingRequest::SpellingRequest(
    int route_id,
    int identifier,
    const base::string16& text)
    : route_id(route_id), identifier(identifier), text(text) {}

SpellCheckerSessionBridge::SpellingRequest::~SpellingRequest() {}
