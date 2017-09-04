// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialContext_h
#define OriginTrialContext_h

#include "core/CoreExport.h"
#include "platform/Supplementable.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
enum class WebOriginTrialTokenStatus;
class WebTrialTokenValidator;

// The Origin Trials Framework provides limited access to experimental features,
// on a per-origin basis (origin trials). This class provides the implementation
// to check if the experimental feature should be enabled for the current
// context.  This class is not for direct use by feature implementers.
// Instead, the OriginTrials generated namespace provides a method for each
// trial to check if it is enabled. Experimental features must be defined in
// RuntimeEnabledFeatures.in, which is used to generate OriginTrials.h/cpp.
//
// Origin trials are defined by string names, provided by the implementers. The
// framework does not maintain an enum or constant list for trial names.
// Instead, the name provided by the feature implementation is validated against
// any provided tokens.
//
// For more information, see https://github.com/jpchase/OriginTrials.
class CORE_EXPORT OriginTrialContext final
    : public GarbageCollectedFinalized<OriginTrialContext>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(OriginTrialContext)
 public:
  enum CreateMode { CreateIfNotExists, DontCreateIfNotExists };

  OriginTrialContext(ExecutionContext*, WebTrialTokenValidator*);

  static const char* supplementName();

  // Returns the OriginTrialContext for a specific ExecutionContext. If
  // |create| is false, this returns null if no OriginTrialContext exists
  // yet for the ExecutionContext.
  static OriginTrialContext* from(ExecutionContext*,
                                  CreateMode = CreateIfNotExists);

  // Parses an Origin-Trial header as specified in
  // https://jpchase.github.io/OriginTrials/#header into individual tokens.
  // Returns null if the header value was malformed and could not be parsed.
  // If the header does not contain any tokens, this returns an empty vector.
  static std::unique_ptr<Vector<String>> parseHeaderValue(
      const String& headerValue);

  static void addTokensFromHeader(ExecutionContext*, const String& headerValue);
  static void addTokens(ExecutionContext*, const Vector<String>* tokens);

  // Returns the trial tokens that are active in a specific ExecutionContext.
  // Returns null if no tokens were added to the ExecutionContext.
  static std::unique_ptr<Vector<String>> getTokens(ExecutionContext*);

  void addToken(const String& token);
  void addTokens(const Vector<String>& tokens);

  // Returns true if the trial (and therefore the feature or features it
  // controls) should be considered enabled for the current execution context.
  bool isTrialEnabled(const String& trialName);

  // Installs JavaScript bindings on the Window object for any features which
  // should be enabled by the current set of trial tokens. This method is called
  // every time a token is added to the document, so that global interfaces will
  // be properly visible, even if the V8 context is being reused (i.e., after
  // navigation). If the V8 context is not initialized, this method will return
  // without doing anything.
  void initializePendingFeatures();

  DECLARE_VIRTUAL_TRACE();

 private:
  void validateToken(const String& token);

  Member<ExecutionContext> m_host;
  Vector<String> m_tokens;
  HashSet<String> m_enabledTrials;
  WebTrialTokenValidator* m_trialTokenValidator;
};

}  // namespace blink

#endif  // OriginTrialContext_h
