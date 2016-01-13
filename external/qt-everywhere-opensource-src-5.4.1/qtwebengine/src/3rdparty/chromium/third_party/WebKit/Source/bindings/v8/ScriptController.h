/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef ScriptController_h
#define ScriptController_h

#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/SharedPersistent.h"

#include "core/fetch/CrossOriginAccessControl.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/TextPosition.h"
#include <v8.h>

struct NPObject;

namespace WebCore {

class DOMWrapperWorld;
class ExecutionContext;
class Event;
class HTMLDocument;
class HTMLPlugInElement;
class KURL;
class LocalFrame;
class ScriptState;
class ScriptSourceCode;
class SecurityOrigin;
class V8WindowShell;
class Widget;

typedef WTF::Vector<v8::Extension*> V8Extensions;

enum ReasonForCallingCanExecuteScripts {
    AboutToExecuteScript,
    NotAboutToExecuteScript
};

class ScriptController {
public:
    enum ExecuteScriptPolicy {
        ExecuteScriptWhenScriptsDisabled,
        DoNotExecuteScriptWhenScriptsDisabled
    };

    ScriptController(LocalFrame*);
    ~ScriptController();

    bool initializeMainWorld();
    V8WindowShell* windowShell(DOMWrapperWorld&);
    V8WindowShell* existingWindowShell(DOMWrapperWorld&);

    // Evaluate JavaScript in the main world.
    void executeScriptInMainWorld(const String&, ExecuteScriptPolicy = DoNotExecuteScriptWhenScriptsDisabled);
    void executeScriptInMainWorld(const ScriptSourceCode&, AccessControlStatus = NotSharableCrossOrigin);
    v8::Local<v8::Value> executeScriptInMainWorldAndReturnValue(const ScriptSourceCode&);
    v8::Local<v8::Value> executeScriptAndReturnValue(v8::Handle<v8::Context>, const ScriptSourceCode&, AccessControlStatus = NotSharableCrossOrigin);

    // Executes JavaScript in an isolated world. The script gets its own global scope,
    // its own prototypes for intrinsic JavaScript objects (String, Array, and so-on),
    // and its own wrappers for all DOM nodes and DOM constructors.
    //
    // If an isolated world with the specified ID already exists, it is reused.
    // Otherwise, a new world is created.
    //
    // FIXME: Get rid of extensionGroup here.
    void executeScriptInIsolatedWorld(int worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup, Vector<v8::Local<v8::Value> >* results);

    // Returns true if argument is a JavaScript URL.
    bool executeScriptIfJavaScriptURL(const KURL&);

    v8::Local<v8::Value> callFunction(v8::Handle<v8::Function>, v8::Handle<v8::Value>, int argc, v8::Handle<v8::Value> argv[]);
    static v8::Local<v8::Value> callFunction(ExecutionContext*, v8::Handle<v8::Function>, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> info[], v8::Isolate*);

    // Returns true if the current world is isolated, and has its own Content
    // Security Policy. In this case, the policy of the main world should be
    // ignored when evaluating resources injected into the DOM.
    bool shouldBypassMainWorldContentSecurityPolicy();

    // Creates a property of the global object of a frame.
    void bindToWindowObject(LocalFrame*, const String& key, NPObject*);

    PassRefPtr<SharedPersistent<v8::Object> > createPluginWrapper(Widget*);

    void enableEval();
    void disableEval(const String& errorMessage);

    static bool canAccessFromCurrentOrigin(LocalFrame*);

    static void setCaptureCallStackForUncaughtExceptions(bool);
    void collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*> >&);

    bool canExecuteScripts(ReasonForCallingCanExecuteScripts);

    TextPosition eventHandlerPosition() const;

    void clearWindowShell();
    void updateDocument();

    void namedItemAdded(HTMLDocument*, const AtomicString&);
    void namedItemRemoved(HTMLDocument*, const AtomicString&);

    void updateSecurityOrigin(SecurityOrigin*);
    void clearScriptObjects();
    void cleanupScriptObjectsForPlugin(Widget*);

    void clearForClose();

    NPObject* createScriptObjectForPluginElement(HTMLPlugInElement*);
    NPObject* windowScriptNPObject();

    // Registers a v8 extension to be available on webpages. Will only
    // affect v8 contexts initialized after this call. Takes ownership of
    // the v8::Extension object passed.
    static void registerExtensionIfNeeded(v8::Extension*);
    static V8Extensions& registeredExtensions();

    bool setContextDebugId(int);
    static int contextDebugId(v8::Handle<v8::Context>);

    v8::Isolate* isolate() const { return m_isolate; }

private:
    typedef HashMap<int, OwnPtr<V8WindowShell> > IsolatedWorldMap;
    typedef HashMap<Widget*, NPObject*> PluginObjectMap;

    v8::Local<v8::Value> evaluateScriptInMainWorld(const ScriptSourceCode&, AccessControlStatus, ExecuteScriptPolicy);

    LocalFrame* m_frame;
    const String* m_sourceURL;
    v8::Isolate* m_isolate;

    OwnPtr<V8WindowShell> m_windowShell;
    IsolatedWorldMap m_isolatedWorlds;

    // A mapping between Widgets and their corresponding script object.
    // This list is used so that when the plugin dies, we can immediately
    // invalidate all sub-objects which are associated with that plugin.
    // The frame keeps a NPObject reference for each item on the list.
    PluginObjectMap m_pluginObjects;

    NPObject* m_windowScriptNPObject;
};

} // namespace WebCore

#endif // ScriptController_h
