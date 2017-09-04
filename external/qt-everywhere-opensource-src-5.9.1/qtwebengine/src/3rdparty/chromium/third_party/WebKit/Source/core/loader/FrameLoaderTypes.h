/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoaderTypes_h
#define FrameLoaderTypes_h

namespace blink {

// See WebFrameLoadType in public/web/WebFrameLoadType.h for details.
enum FrameLoadType {
  FrameLoadTypeStandard,
  FrameLoadTypeBackForward,
  FrameLoadTypeReload,
  FrameLoadTypeReloadMainResource,
  FrameLoadTypeReplaceCurrentItem,
  FrameLoadTypeInitialInChildFrame,
  FrameLoadTypeInitialHistoryLoad,
  FrameLoadTypeReloadBypassingCache,
};

enum NavigationType {
  NavigationTypeLinkClicked,
  NavigationTypeFormSubmitted,
  NavigationTypeBackForward,
  NavigationTypeReload,
  NavigationTypeFormResubmitted,
  NavigationTypeOther
};

enum ObjectContentType {
  ObjectContentNone,
  ObjectContentImage,
  ObjectContentFrame,
  ObjectContentNetscapePlugin,
};

enum ShouldSendReferrer { MaybeSendReferrer, NeverSendReferrer };

enum ShouldSetOpener { MaybeSetOpener, NeverSetOpener };

enum ReasonForCallingAllowPlugins {
  AboutToInstantiatePlugin,
  NotAboutToInstantiatePlugin
};

enum LoadStartType {
  NavigationToDifferentDocument,
  NavigationWithinSameDocument
};

enum SameDocumentNavigationSource {
  SameDocumentNavigationDefault,
  SameDocumentNavigationHistoryApi,
};

enum HistoryLoadType { HistorySameDocumentLoad, HistoryDifferentDocumentLoad };

enum HistoryCommitType {
  StandardCommit,
  BackForwardCommit,
  InitialCommitInChildFrame,
  HistoryInertCommit
};

enum HistoryScrollRestorationType {
  ScrollRestorationAuto,
  ScrollRestorationManual
};

enum class ProgressBarCompletion {
  LoadEvent,
  ResourcesBeforeDCL,
  DOMContentLoaded,
  ResourcesBeforeDCLAndSameOriginIFrames
};

}  // namespace blink

#endif
