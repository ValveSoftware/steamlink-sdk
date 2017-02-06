// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrame_h
#define WebRemoteFrame_h

#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSandboxFlags.h"

namespace blink {

enum class WebTreeScopeType;
class WebFrameClient;
class WebRemoteFrameClient;
class WebString;

class WebRemoteFrame : public WebFrame {
public:
    BLINK_EXPORT static WebRemoteFrame* create(WebTreeScopeType, WebRemoteFrameClient*, WebFrame* opener = nullptr);

    // Functions for the embedder replicate the frame tree between processes.
    // TODO(dcheng): The embedder currently does not replicate local frames in
    // insertion order, so the local child version takes a previous sibling to
    // ensure that it is inserted into the correct location in the list of
    // children.
    virtual WebLocalFrame* createLocalChild(WebTreeScopeType, const WebString& name, const WebString& uniqueName, WebSandboxFlags, WebFrameClient*, WebFrame* previousSibling, const WebFrameOwnerProperties&, WebFrame* opener) = 0;

    virtual WebRemoteFrame* createRemoteChild(WebTreeScopeType, const WebString& name, const WebString& uniqueName, WebSandboxFlags, WebRemoteFrameClient*, WebFrame* opener) = 0;

    // Transfer initial drawing parameters from a local frame.
    virtual void initializeFromFrame(WebLocalFrame*) const = 0;

    // Set security origin replicated from another process.
    virtual void setReplicatedOrigin(const WebSecurityOrigin&) const = 0;

    // Set sandbox flags replicated from another process.
    virtual void setReplicatedSandboxFlags(WebSandboxFlags) const = 0;

    // Set frame |name| and |uniqueName| replicated from another process.
    virtual void setReplicatedName(const WebString& name, const WebString& uniqueName) const = 0;

    // Adds |header| to the set of replicated CSP headers.
    virtual void addReplicatedContentSecurityPolicyHeader(const WebString& headerValue, WebContentSecurityPolicyType, WebContentSecurityPolicySource) const = 0;

    // Resets replicated CSP headers to an empty set.
    virtual void resetReplicatedContentSecurityPolicy() const = 0;

    // Set frame enforcement of insecure request policy replicated from another process.
    virtual void setReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy) const = 0;

    // Set the frame to a unique origin that is potentially trustworthy,
    // replicated from another process.
    virtual void setReplicatedPotentiallyTrustworthyUniqueOrigin(bool) const = 0;

    virtual void DispatchLoadEventForFrameOwner() const = 0;

    virtual void didStartLoading() = 0;
    virtual void didStopLoading() = 0;

    // Returns true if this frame should be ignored during hittesting.
    virtual bool isIgnoredForHitTest() const = 0;

    // This is called in OOPIF scenarios when an element contained in this
    // frame is about to enter fullscreen.  This frame's owner
    // corresponds to the HTMLFrameOwnerElement to be fullscreened. Calling
    // this prepares FullscreenController to enter fullscreen for that frame
    // owner.
    virtual void willEnterFullScreen() = 0;

    // Temporary method to allow embedders to get the script context of a
    // remote frame. This should only be used by legacy code that has not yet
    // migrated over to the new OOPI infrastructure.
    virtual v8::Local<v8::Context> deprecatedMainWorldScriptContext() const = 0;

protected:
    explicit WebRemoteFrame(WebTreeScopeType scope) : WebFrame(scope) { }

    // Inherited from WebFrame, but intentionally hidden: it never makes sense
    // to call these on a WebRemoteFrame.
    bool isWebLocalFrame() const override = 0;
    WebLocalFrame* toWebLocalFrame() override = 0;
    bool isWebRemoteFrame() const override = 0;
    WebRemoteFrame* toWebRemoteFrame() override = 0;
};

} // namespace blink

#endif // WebRemoteFrame_h
