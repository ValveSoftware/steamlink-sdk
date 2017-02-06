// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_
#define EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_

#include <stddef.h>

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

class GURL;

namespace base {
class ListValue;
}

namespace blink {
class WebLocalFrame;
class WebSecurityOrigin;
}

namespace content {
class RenderFrame;
}

namespace extensions {
class ScriptContext;

// A container of ScriptContexts, responsible for both creating and managing
// them.
//
// Since calling JavaScript within a context can cause any number of contexts
// to be created or destroyed, this has additional smarts to help with the set
// changing underneath callers.
class ScriptContextSet {
 public:
  ScriptContextSet(
      // Set of the IDs of extensions that are active in this process.
      // Must outlive this. TODO(kalman): Combine this and |extensions|.
      ExtensionIdSet* active_extension_ids);

  ~ScriptContextSet();

  // Returns the number of contexts being tracked by this set.
  // This may also include invalid contexts. TODO(kalman): Useful?
  size_t size() const { return contexts_.size(); }

  // Creates and starts managing a new ScriptContext. Ownership is held.
  // Returns a weak reference to the new ScriptContext.
  ScriptContext* Register(blink::WebLocalFrame* frame,
                          const v8::Local<v8::Context>& v8_context,
                          int extension_group,
                          int world_id);

  // If the specified context is contained in this set, remove it, then delete
  // it asynchronously. After this call returns the context object will still
  // be valid, but its frame() pointer will be cleared.
  void Remove(ScriptContext* context);

  // Gets the ScriptContext corresponding to v8::Context::GetCurrent(), or
  // NULL if no such context exists.
  ScriptContext* GetCurrent() const;

  // Gets the ScriptContext corresponding to the specified
  // v8::Context or NULL if no such context exists.
  ScriptContext* GetByV8Context(const v8::Local<v8::Context>& context) const;
  // Static equivalent of the above.
  static ScriptContext* GetContextByV8Context(
      const v8::Local<v8::Context>& context);

  // Returns the ScriptContext corresponding to the V8 context that created the
  // given |object|.
  static ScriptContext* GetContextByObject(const v8::Local<v8::Object>& object);

  // Synchronously runs |callback| with each ScriptContext that belongs to
  // |extension_id| in |render_frame|.
  //
  // An empty |extension_id| will match all extensions, and a null
  // |render_frame| will match all render views, but try to use the inline
  // variants of these methods instead.
  void ForEach(const std::string& extension_id,
               content::RenderFrame* render_frame,
               const base::Callback<void(ScriptContext*)>& callback) const;
  // ForEach which matches all extensions.
  void ForEach(content::RenderFrame* render_frame,
               const base::Callback<void(ScriptContext*)>& callback) const {
    ForEach(std::string(), render_frame, callback);
  }
  // ForEach which matches all render views.
  void ForEach(const std::string& extension_id,
               const base::Callback<void(ScriptContext*)>& callback) const {
    ForEach(extension_id, nullptr, callback);
  }

  // Cleans up contexts belonging to an unloaded extension.
  //
  // Returns the set of ScriptContexts that were removed as a result. These
  // are safe to interact with until the end of the current event loop, since
  // they're deleted asynchronously.
  std::set<ScriptContext*> OnExtensionUnloaded(const std::string& extension_id);

 private:
  // Finds the extension for the JavaScript context associated with the
  // specified |frame| and isolated world. If |world_id| is zero, finds the
  // extension ID associated with the main world's JavaScript context. If the
  // JavaScript context isn't from an extension, returns empty string.
  const Extension* GetExtensionFromFrameAndWorld(
      const blink::WebLocalFrame* frame,
      int world_id,
      bool use_effective_url);

  // Returns the Feature::Context type of context for a JavaScript context.
  Feature::Context ClassifyJavaScriptContext(
      const Extension* extension,
      int extension_group,
      const GURL& url,
      const blink::WebSecurityOrigin& origin);

  // Helper for OnExtensionUnloaded().
  void RecordAndRemove(std::set<ScriptContext*>* removed,
                       ScriptContext* context);

  // Weak reference to all installed Extensions that are also active in this
  // process.
  ExtensionIdSet* active_extension_ids_;

  // The set of all ScriptContexts we own.
  std::set<ScriptContext*> contexts_;

  DISALLOW_COPY_AND_ASSIGN(ScriptContextSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_
