// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_switches.h"

namespace switches {

// The number of MSAA samples for canvas2D. Requires MSAA support by GPU to
// have an effect. 0 disables MSAA.
const char kAcceleratedCanvas2dMSAASampleCount[] = "canvas-msaa-sample-count";

// Enables a new tuning of the WebRTC Acoustic Echo Canceler (AEC). The new
// tuning aims at resolving two issues with the AEC:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5777
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5778
// TODO(hlundin): Remove this switch when experimentation is over;
// crbug.com/603821.
const char kAecRefinedAdaptiveFilter[] = "aec-refined-adaptive-filter";

// Override the default minimum starting volume of the Automatic Gain Control
// algorithm in WebRTC used with audio tracks from getUserMedia.
// The valid range is 12-255. Values outside that range will be clamped
// to the lowest or highest valid value inside WebRTC.
// TODO(tommi): Remove this switch when crbug.com/555577 is fixed.
const char kAgcStartupMinVolume[] = "agc-startup-min-volume";

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Allows loopback interface to be added in network list for peer connection.
const char kAllowLoopbackInPeerConnection[] =
    "allow-loopback-in-peer-connection";

// Enables the sandboxed processes to run without a job object assigned to them.
// This flag is required to allow Chrome to run in RemoteApps or Citrix. This
// flag can reduce the security of the sandboxed processes and allow them to do
// certain API calls like shut down Windows or access the clipboard. Also we
// lose the chance to kill some processes until the outer job that owns them
// finishes.
const char kAllowNoSandboxJob[]             = "allow-no-sandbox-job";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Uses the android SkFontManager on linux. The specified directory should
// include the configuration xml file with the name "fonts.xml".
// This is used in blimp to emulate android fonts on linux.
const char kAndroidFontsPath[]          = "android-fonts-path";

// Set blink settings. Format is <name>[=<value],<name>[=<value>],...
// The names are declared in Settings.in. For boolean type, use "true", "false",
// or omit '=<value>' part to set to true. For enum type, use the int value of
// the enum value. Applied after other command line flags and prefs.
const char kBlinkSettings[]                 = "blink-settings";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[]              = "default-tile-width";
const char kDefaultTileHeight[]             = "default-tile-height";

// Disable antialiasing on 2d canvas.
const char kDisable2dCanvasAntialiasing[]   = "disable-canvas-aa";

// Disables Canvas2D rendering into a scanout buffer for overlay support.
const char kDisable2dCanvasImageChromium[] = "disable-2d-canvas-image-chromium";

// Disables client-visible 3D APIs, in particular WebGL and Pepper 3D.
// This is controlled by policy and is kept separate from the other
// enable/disable switches to avoid accidentally regressing the policy
// support for controlling access to these APIs.
const char kDisable3DAPIs[]                 = "disable-3d-apis";

// Disable gpu-accelerated 2d canvas.
const char kDisableAccelerated2dCanvas[]    = "disable-accelerated-2d-canvas";

// Disable hardware acceleration of mjpeg decode for captured frame, where
// available.
const char kDisableAcceleratedMjpegDecode[] =
    "disable-accelerated-mjpeg-decode";

// Disables hardware acceleration of video decode, where available.
const char kDisableAcceleratedVideoDecode[] =
    "disable-accelerated-video-decode";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disable backgrounding renders for occluded windows. Done for tests to avoid
// nondeterministic behavior.
extern const char kDisableBackgroundingOccludedWindowsForTesting[] =
    "disable-backgrounding-occluded-windows";

// Disable task throttling of timer tasks from background pages.
const char kDisableBackgroundTimerThrottling[] =
    "disable-background-timer-throttling";

// Disable one or more Blink runtime-enabled features.
// Use names from RuntimeEnabledFeatures.in, separated by commas.
// Applied after kEnableBlinkFeatures, and after other flags that change these
// features.
const char kDisableBlinkFeatures[]          = "disable-blink-features";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disable the per-domain blocking for 3D APIs after GPU reset.
// This switch is intended only for tests.
const char kDisableDomainBlockingFor3DAPIs[] =
    "disable-domain-blocking-for-3d-apis";

// Disable experimental WebGL support.
const char kDisableExperimentalWebGL[]      = "disable-webgl";

// Comma-separated list of feature names to disable. See also kEnableFeatures.
const char kDisableFeatures[]               = "disable-features";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disable 3D inside of flapper.
const char kDisableFlash3d[]                = "disable-flash-3d";

// Disable Stage3D inside of flapper.
const char kDisableFlashStage3d[]           = "disable-flash-stage3d";

// Disable user gesture requirement for media playback.
const char kDisableGestureRequirementForMediaPlayback[] =
    "disable-gesture-requirement-for-media-playback";

// Disable user gesture requirement for presentation.
const char kDisableGestureRequirementForPresentation[] =
    "disable-gesture-requirement-for-presentation";

// Disables GPU hardware acceleration.  If software renderer is not in place,
// then the GPU process won't launch.
const char kDisableGpu[]                    = "disable-gpu";

// Prevent the compositor from using its GPU implementation.
const char kDisableGpuCompositing[]         = "disable-gpu-compositing";

// Disable proactive early init of GPU process.
const char kDisableGpuEarlyInit[]           = "disable-gpu-early-init";

// Do not force that all compositor resources be backed by GPU memory buffers.
const char kDisableGpuMemoryBufferCompositorResources[] =
    "disable-gpu-memory-buffer-compositor-resources";

// Disable GpuMemoryBuffer backed VideoFrames.
const char kDisableGpuMemoryBufferVideoFrames[] =
    "disable-gpu-memory-buffer-video-frames";

// Disable the limit on the number of times the GPU process may be restarted.
// For tests and platforms where software fallback is disabled.
const char kDisableGpuProcessCrashLimit[] = "disable-gpu-process-crash-limit";

// Disable async GL worker context. Overrides kEnableGpuAsyncWorkerContext.
const char kDisableGpuAsyncWorkerContext[] = "disable-gpu-async-worker-context";

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization and kForceGpuRasterization flags.
const char kDisableGpuRasterization[]       = "disable-gpu-rasterization";

// When using CPU rasterizing disable low resolution tiling. This uses
// less power, particularly during animations, but more white may be seen
// during fast scrolling especially on slower devices.
const char kDisableLowResTiling[] = "disable-low-res-tiling";

// Disable the GPU process sandbox.
const char kDisableGpuSandbox[]             = "disable-gpu-sandbox";

// Disable in-process stack traces.
const char kDisableInProcessStackTraces[]   = "disable-in-process-stack-traces";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable hiding the close buttons of inactive tabs when the tabstrip is in
// stacked mode.
const char kDisableHideInactiveStackedTabCloseButtons[] =
    "disable-hide-inactive-stacked-tab-close-buttons";

// Disable the RenderThread's HistogramCustomizer.
const char kDisableHistogramCustomizer[]    = "disable-histogram-customizer";

// Don't kill a child process when it sends a bad IPC message.  Apart
// from testing, it is a bad idea from a security perspective to enable
// this switch.
const char kDisableKillAfterBadIPC[]        = "disable-kill-after-bad-ipc";

// Disables LCD text.
const char kDisableLCDText[]                = "disable-lcd-text";

// Disables distance field text.
const char kDisableDistanceFieldText[]      = "disable-distance-field-text";

// Disable LocalStorage.
const char kDisableLocalStorage[]           = "disable-local-storage";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Disables using CODECAPI_AVLowLatencyMode when creating DXVA decoders.
const char kDisableLowLatencyDxva[]         = "disable-low-latency-dxva";

// Disables usage of the namespace sandbox.
const char kDisableNamespaceSandbox[]       = "disable-namespace-sandbox";

// Disables native GPU memory buffer support.
const char kDisableNativeGpuMemoryBuffers[] =
    "disable-native-gpu-memory-buffers";

// Disables the Web Notification and the Push APIs.
const char kDisableNotifications[]          = "disable-notifications";

// Disable partial raster in the renderer. Disabling this switch also disables
// the use of persistent gpu memory buffers.
const char kDisablePartialRaster[] = "disable-partial-raster";

// Enable partial raster in the renderer.
const char kEnablePartialRaster[] = "enable-partial-raster";

// Disable Pepper3D.
const char kDisablePepper3d[]               = "disable-pepper-3d";

// Disables the Permissions API.
const char kDisablePermissionsAPI[]         = "disable-permissions-api";

// Disable Image Chromium for Pepper 3d.
const char kDisablePepper3DImageChromium[] = "disable-pepper-3d-image-chromium";

// Disables compositor-accelerated touch-screen pinch gestures.
const char kDisablePinch[]                  = "disable-pinch";

// Disable the creation of compositing layers when it would prevent LCD text.
const char kDisablePreferCompositingToLCDText[] =
    "disable-prefer-compositing-to-lcd-text";

// Disables the Presentation API.
const char kDisablePresentationAPI[]        = "disable-presentation-api";

// Disables RGBA_4444 textures.
const char kDisableRGBA4444Textures[]       = "disable-rgba-4444-textures";

// Taints all <canvas> elements, regardless of origin.
const char kDisableReadingFromCanvas[]      = "disable-reading-from-canvas";

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Disables the RemotePlayback API.
const char kDisableRemotePlaybackAPI[]      = "disable-remote-playback-api";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Prevent renderer process backgrounding when set.
const char kDisableRendererBackgrounding[]  = "disable-renderer-backgrounding";

// Whether the resize lock is disabled. Default is false. This is generally only
// useful for tests that want to force disabling.
const char kDisableResizeLock[] = "disable-resize-lock";

// Disable the seccomp filter sandbox (seccomp-bpf) (Linux only).
const char kDisableSeccompFilterSandbox[]   = "disable-seccomp-filter-sandbox";

// Disable the setuid sandbox (Linux only).
const char kDisableSetuidSandbox[]          = "disable-setuid-sandbox";

// Disable shared workers.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

// Disables the Web Speech API.
const char kDisableSpeechAPI[]              = "disable-speech-api";

// Disable multithreaded GPU compositing of web content.
const char kDisableThreadedCompositing[]     = "disable-threaded-compositing";

// Disable multithreaded, compositor scrolling of web content.
const char kDisableThreadedScrolling[]      = "disable-threaded-scrolling";

// Disable V8 idle tasks.
const char kDisableV8IdleTasks[]            = "disable-v8-idle-tasks";

// Disables WebGL rendering into a scanout buffer for overlay support.
const char kDisableWebGLImageChromium[]     = "disable-webgl-image-chromium";

// Don't enforce the same-origin policy. (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disables Blink's XSSAuditor. The XSSAuditor mitigates reflective XSS.
const char kDisableXSSAuditor[]             = "disable-xss-auditor";

// Disable rasterizer that writes directly to GPU memory associated with tiles.
const char kDisableZeroCopy[]                = "disable-zero-copy";

// Disable the video decoder from drawing directly to a texture.
const char kDisableZeroCopyDxgiVideo[]      = "disable-zero-copy-dxgi-video";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating dom
// based tests.
const char kDomAutomationController[]       = "dom-automation";

// Disable antialiasing on 2d canvas clips
const char kDisable2dCanvasClipAntialiasing[] = "disable-2d-canvas-clip-aa";

// Disable partially decoding jpeg images using the GPU.
// At least YUV decoding will be accelerated when not using this flag.
// Has no effect unless GPU rasterization is enabled.
const char kDisableAcceleratedJpegDecoding[] =
    "disable-accelerated-jpeg-decoding";

// Enables LCD text.
const char kEnableLCDText[]                 = "enable-lcd-text";

// Enables using signed distance fields when rendering text.
// Only valid if GPU rasterization is enabled as well.
const char kEnableDistanceFieldText[]       = "enable-distance-field-text";

// Enable the creation of compositing layers when it would prevent LCD text.
const char kEnablePreferCompositingToLCDText[] =
    "enable-prefer-compositing-to-lcd-text";

// Enable one or more Blink runtime-enabled features.
// Use names from RuntimeEnabledFeatures.in, separated by commas.
// Applied before kDisableBlinkFeatures, and after other flags that change these
// features.
const char kEnableBlinkFeatures[]           = "enable-blink-features";

// PlzNavigate: Use the experimental browser-side navigation path.
const char kEnableBrowserSideNavigation[]   = "enable-browser-side-navigation";

// Enables display list based 2d canvas implementation. Options:
//  1. Enable: allow browser to use display list for 2d canvas (browser makes
//     decision).
//  2. Force: browser always uses display list for 2d canvas.
const char kEnableDisplayList2dCanvas[]     = "enable-display-list-2d-canvas";
const char kForceDisplayList2dCanvas[]      = "force-display-list-2d-canvas";
const char kDisableDisplayList2dCanvas[]    = "disable-display-list-2d-canvas";

// Enables dynamic rendering pipeline switching to optimize the
// performance of 2d canvas
const char kEnableCanvas2dDynamicRenderingModeSwitching[] =
    "enable-canvas-2d-dynamic-rendering-mode-switching";

// Enable experimental canvas features, e.g. canvas 2D context attributes
const char kEnableExperimentalCanvasFeatures[] =
    "enable-experimental-canvas-features";

// Enables Web Platform features that are in development.
const char kEnableExperimentalWebPlatformFeatures[] =
    "enable-experimental-web-platform-features";

// Comma-separated list of feature names to enable. See also kDisableFeatures.
const char kEnableFeatures[] = "enable-features";

// WebFonts intervention v2 flag and values.
const char kEnableWebFontsInterventionV2[] = "enable-webfonts-intervention-v2";
const char kEnableWebFontsInterventionV2SwitchValueEnabledWith2G[] =
    "enabled-2g";
const char kEnableWebFontsInterventionV2SwitchValueEnabledWith3G[] =
    "enabled-3g";
const char kEnableWebFontsInterventionV2SwitchValueEnabledWithSlow2G[] =
    "enabled-slow2g";
const char kEnableWebFontsInterventionV2SwitchValueDisabled[] = "disabled";

// Makes the GL worker context run asynchronously by using a separate stream.
const char kEnableGpuAsyncWorkerContext[] = "enable-gpu-async-worker-context";

// Specify that all compositor resources should be backed by GPU memory buffers.
const char kEnableGpuMemoryBufferCompositorResources[] =
    "enable-gpu-memory-buffer-compositor-resources";

// Enable GpuMemoryBuffer backed VideoFrames.
const char kEnableGpuMemoryBufferVideoFrames[] =
    "enable-gpu-memory-buffer-video-frames";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing +
// impl-side painting.
const char kEnableGpuRasterization[]        = "enable-gpu-rasterization";

// When using CPU rasterizing generate low resolution tiling. Low res
// tiles may be displayed during fast scrolls especially on slower devices.
const char kEnableLowResTiling[] = "enable-low-res-tiling";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// Enables the memory benchmarking extension
const char kEnableMemoryBenchmarking[]      = "enable-memory-benchmarking";

// Enables the network information API.
const char kEnableNetworkInformation[]      = "enable-network-information";

// Disables the video decoder from drawing to an NV12 textures instead of ARGB.
const char kDisableNv12DxgiVideo[] = "disable-nv12-dxgi-video";

// Enables compositor-accelerated touch-screen pinch gestures.
const char kEnablePinch[]                   = "enable-pinch";

// Enables testing features of the Plugin Placeholder. For internal use only.
const char kEnablePluginPlaceholderTesting[] =
    "enable-plugin-placeholder-testing";

// Make the values returned to window.performance.memory more granular and more
// up to date in shared worker. Without this flag, the memory information is
// still available, but it is bucketized and updated less frequently. This flag
// also applys to workers.
const char kEnablePreciseMemoryInfo[] = "enable-precise-memory-info";

// Enables RGBA_4444 textures.
const char kEnableRGBA4444Textures[] = "enable-rgba-4444-textures";

// Set options to cache V8 data. (off, preparse data, or code)
const char kV8CacheOptions[] = "v8-cache-options";

// Signals that the V8 natives file has been transfered to the child process
// by a file descriptor.
const char kV8NativesPassedByFD[] = "v8-natives-passed-by-fd";

// Signals that the V8 startup snapshot file has been transfered to the child
// process by a file descriptor.
const char kV8SnapshotPassedByFD[] = "v8-snapshot-passed-by-fd";

// Set strategies to cache V8 data in CacheStorage. (off, normal, or aggressive)
const char kV8CacheStrategiesForCacheStorage[] =
    "v8-cache-strategies-for-cache-storage";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Enables the Skia benchmarking extension
const char kEnableSkiaBenchmarking[]        = "enable-skia-benchmarking";

// Enables slimming paint phase 2: http://www.chromium.org/blink/slimming-paint
const char kEnableSlimmingPaintV2[]         = "enable-slimming-paint-v2";

// On platforms that support it, enables smooth scroll animation.
const char kEnableSmoothScrolling[]         = "enable-smooth-scrolling";

// Enable spatial navigation
const char kEnableSpatialNavigation[]       = "enable-spatial-navigation";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Blocks all insecure requests from secure contexts, and prevents the user
// from overriding that decision.
const char kEnableStrictMixedContentChecking[] =
    "enable-strict-mixed-content-checking";

// Blocks insecure usage of a number of powerful features (device orientation,
// for example) that we haven't yet deprecated for the web at large.
const char kEnableStrictPowerfulFeatureRestrictions[] =
    "enable-strict-powerful-feature-restrictions";

// Enable use of experimental TCP sockets API for sending data in the
// SYN packet.
const char kEnableTcpFastOpen[]             = "enable-tcp-fastopen";

// Enabled threaded compositing for layout tests.
const char kEnableThreadedCompositing[]     = "enable-threaded-compositing";

// Enable tracing during the execution of browser tests.
const char kEnableTracing[]                 = "enable-tracing";

// The filename to write the output of the test tracing to.
const char kEnableTracingOutput[]           = "enable-tracing-output";

// Enable screen capturing support for MediaStream API.
const char kEnableUserMediaScreenCapturing[] =
    "enable-usermedia-screen-capturing";

// Enable the mode that uses zooming to implment device scale factor behavior.
const char kEnableUseZoomForDSF[]            = "enable-use-zoom-for-dsf";

// Enables the use of the @viewport CSS rule, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enable the Vtune profiler support.
const char kEnableVtune[]                   = "enable-vtune-support";

// Enable Vulkan support, must also have ENABLE_VULKAN defined.
const char kEnableVulkan[] = "enable-vulkan";

// Enable WebFonts intervention and trigger the signal always.
const char kEnableWebFontsInterventionTrigger[] =
    "enable-webfonts-intervention-trigger";

// Enables WebGL extensions not yet approved by the community.
const char kEnableWebGLDraftExtensions[] = "enable-webgl-draft-extensions";

// Enables WebGL rendering into a scanout buffer for overlay support.
const char kEnableWebGLImageChromium[] = "enable-webgl-image-chromium";

// Enables interaction with virtual reality devices.
const char kEnableWebVR[] = "enable-webvr";

// Enable rasterizer that writes directly to GPU memory associated with tiles.
const char kEnableZeroCopy[]                = "enable-zero-copy";

// Explicitly allows additional ports using a comma-separated list of port
// numbers.
const char kExplicitlyAllowedPorts[]        = "explicitly-allowed-ports";

// Handle to the shared memory segment containing field trial state that is to
// be shared between processes. The argument to this switch is the handle id
// (pointer on Windows) as a string, followed by a comma, then the size of the
// shared memory segment as a string.
const char kFieldTrialHandle[] = "field-trial-handle";

// Always use the Skia GPU backend for drawing layer tiles. Only valid with GPU
// accelerated compositing + impl-side painting. Overrides the
// kEnableGpuRasterization flag.
const char kForceGpuRasterization[]        = "force-gpu-rasterization";

// The number of multisample antialiasing samples for GPU rasterization.
// Requires MSAA support on GPU to have an effect. 0 disables MSAA.
const char kGpuRasterizationMSAASampleCount[] =
    "gpu-rasterization-msaa-sample-count";

// Forces use of hardware overlay for fullscreen video playback. Useful for
// testing the Android overlay fullscreen functionality on other platforms.
const char kForceOverlayFullscreenVideo[]   = "force-overlay-fullscreen-video";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// For development / testing only. When running content_browsertests,
// saves output of failing accessibility tests to their expectations files in
// content/test/data/accessibility/, overwriting existing file content.
const char kGenerateAccessibilityTestExpectations[] =
    "generate-accessibility-test-expectations";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Allows shmat() system call in the GPU sandbox.
const char kGpuSandboxAllowSysVShm[]        = "gpu-sandbox-allow-sysv-shm";

// Makes GPU sandbox failures fatal.
const char kGpuSandboxFailuresFatal[]       = "gpu-sandbox-failures-fatal";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Ignores certificate-related errors.
const char kIgnoreCertificateErrors[]       = "ignore-certificate-errors";

// Don't allow content to arbitrarily append to the back/forward list.
// The page must prcoess a user gesture before an entry can be added.
const char kHistoryEntryRequiresUserGesture[] =
    "history-entry-requires-user-gesture";

// These mappings only apply to the host resolver.
const char kHostResolverRules[]             = "host-resolver-rules";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[]            = "ignore-gpu-blacklist";

// Makes all APIs reflect the layout viewport.
const char kInertVisualViewport[]           = "inert-visual-viewport";

// Run the GPU process as a thread in the browser process.
const char kInProcessGPU[]                  = "in-process-gpu";

// Overrides the timeout, in seconds, that a child process waits for a
// connection from the browser before killing itself.
const char kIPCConnectionTimeout[]          = "ipc-connection-timeout";

// Chrome is running in Mash.
const char kIsRunningInMash[] = "is-running-in-mash";

// Disable latest shipping ECMAScript 6 features.
const char kDisableJavaScriptHarmonyShipping[] =
    "disable-javascript-harmony-shipping";

// Enables experimental Harmony (ECMAScript 6) features.
const char kJavaScriptHarmony[]             = "javascript-harmony";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Logs GPU control list decisions when enforcing blacklist rules.
const char kLogGpuControlListDecisions[]    = "log-gpu-control-list-decisions";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Enables saving net log events to a file and sets the file name to use.
const char kLogNetLog[]                     = "log-net-log";

// Resizes of the main frame are caused by changing between landscape and
// portrait mode (i.e. Android) so the page should be rescaled to fit.
const char kMainFrameResizesAreOrientationChanges[] =
    "main-frame-resizes-are-orientation-changes";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerHeight[]         = "max-untiled-layer-height";
const char kMaxUntiledLayerWidth[]          = "max-untiled-layer-width";

// Sample memory usage with high frequency and store the results to the
// Renderer.Memory histogram. Used in memory tests.
const char kMemoryMetrics[]                 = "memory-metrics";

// Sets options for MHTML generator to skip no-store resources:
//   "skip-nostore-main" - fails to save a page if main frame is 'no-store'
//   "skip-nostore-all" - also skips no-store subresources.
const char kMHTMLGeneratorOption[]          = "mhtml-generator-option";
const char kMHTMLSkipNostoreMain[]          = "skip-nostore-main";
const char kMHTMLSkipNostoreAll[]           = "skip-nostore-all";

// Use a Mojo-based LocalStorage implementation.
const char kMojoLocalStorage[]              = "mojo-local-storage";

// Disables a Mojo-based ServiceWorker implementation.
const char kDisableMojoServiceWorker[]      = "disable-mojo-service-worker";

// Mutes audio sent to the audio device so it is not audible during
// automated testing.
const char kMuteAudio[]                     = "mute-audio";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

// Disables the sandbox for all process types that are normally sandboxed.
const char kNoSandbox[]                     = "no-sandbox";

// Disables the use of a zygote process for forking child processes. Instead,
// child processes will be forked and exec'd directly. Note that --no-sandbox
// should also be used together with this flag because the sandbox needs the
// zygote to work.
const char kNoZygote[] = "no-zygote";

// Enable or disable appcontainer/lowbox for renderer on Win8+ platforms.
const char kEnableAppContainer[]           = "enable-appcontainer";
const char kDisableAppContainer[]          = "disable-appcontainer";

// Number of worker threads used to rasterize content.
const char kNumRasterThreads[]              = "num-raster-threads";

// Override the behavior of plugin throttling for testing.
// By default the throttler is only enabled for a hard-coded list of plugins.
// Set the value to 'always' to always throttle every plugin instance.
const char kOverridePluginPowerSaverForTesting[] =
    "override-plugin-power-saver-for-testing";

// Controls the behavior of history navigation in response to horizontal
// overscroll.
// Set the value to '0' to disable.
// Set the value to '1' to enable the behavior where pages slide in and out in
// response to the horizontal overscroll gesture and a screenshot of the target
// page is shown.
// Set the value to '2' to enable the simplified overscroll UI where a
// navigation arrow slides in from the side of the screen in response to the
// horizontal overscroll gesture.
// Defaults to '1'.
const char kOverscrollHistoryNavigation[] =
    "overscroll-history-navigation";

// Override the default value for the 'passive' field in javascript
// addEventListener calls. Values are defined as:
//  'documentonlytrue' to set the default be true only for document level nodes.
//  'true' to set the default to be true on all nodes (when not specified).
//  'forcealltrue' to force the value on all nodes.
const char kPassiveListenersDefault[] = "passive-listeners-default";

// Argument to the process type that indicates a PPAPI broker process type.
const char kPpapiBrokerProcess[]            = "ppapi-broker";

// "Command-line" arguments for the PPAPI Flash; used for debugging options.
const char kPpapiFlashArgs[]                = "ppapi-flash-args";

// Runs PPAPI (Pepper) plugins in-process.
const char kPpapiInProcess[]                = "ppapi-in-process";

// Specifies a command that should be used to launch the ppapi plugin process.
// Useful for running the plugin process through purify or quantify.  Ex:
//   --ppapi-plugin-launcher="path\to\purify /Run=yes"
const char kPpapiPluginLauncher[]           = "ppapi-plugin-launcher";

// Argument to the process type that indicates a PPAPI plugin process type.
const char kPpapiPluginProcess[]            = "ppapi";

// Causes the PPAPI sub process to display a dialog on launch. Be sure to use
// --no-sandbox as well or the sandbox won't allow the dialog to display.
const char kPpapiStartupDialog[]            = "ppapi-startup-dialog";

// Enable the "Process Per Site" process model for all domains. This mode
// consolidates same-site pages so that they share a single process.
//
// More details here:
// - http://www.chromium.org/developers/design-documents/process-models
// - The class comment in site_instance.h, listing the supported process models.
//
// IMPORTANT: This isn't to be confused with --site-per-process (which is about
// isolation, not consolidation). You probably want the other one.
const char kProcessPerSite[]                = "process-per-site";

// Runs each set of script-connected tabs (i.e., a BrowsingInstance) in its own
// renderer process.  We default to using a renderer process for each
// site instance (i.e., group of pages from the same registered domain with
// script connections to each other).
const char kProcessPerTab[]                 = "process-per-tab";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const char kProcessType[]                   = "type";

// Enables more web features over insecure connections. Designed to be used
// for testing purposes only.
const char kReduceSecurityForTesting[]      = "reduce-security-for-testing";

// Register Pepper plugins (see pepper_plugin_list.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// Enables remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

const char kRendererClientId[] = "renderer-client-id";

// The contents of this flag are prepended to the renderer command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Overrides the default/calculated limit to the number of renderer processes.
// Very high values for this setting can lead to high memory/resource usage
// or instability.
const char kRendererProcessLimit[]          = "renderer-process-limit";

// Causes the renderer process to display a dialog on launch. Passing this flag
// also adds kNoSandbox on Windows non-official builds, since that's needed to
// show a dialog.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Reduce the default `referer` header's granularity.
const char kReducedReferrerGranularity[] =
  "reduced-referrer-granularity";

// Handles frame scrolls via the root RenderLayer instead of the FrameView.
const char kRootLayerScrolls[]              = "root-layer-scrolls";

// Causes the process to run as a sandbox IPC subprocess.
const char kSandboxIPCProcess[]             = "sandbox-ipc";

// Enables or disables scroll end effect in response to vertical overscroll.
// Set the value to '1' to enable the feature, and set to '0' to disable.
// Defaults to disabled.
const char kScrollEndEffect[] = "scroll-end-effect";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Enforces a one-site-per-process security policy:
//  * Each renderer process, for its whole lifetime, is dedicated to rendering
//    pages for just one site.
//  * Thus, pages from different sites are never in the same process.
//  * A renderer process's access rights are restricted based on its site.
//  * All cross-site navigations force process swaps.
//  * <iframe>s are rendered out-of-process whenever the src= is cross-site.
//
// More details here:
// - http://www.chromium.org/developers/design-documents/site-isolation
// - http://www.chromium.org/developers/design-documents/process-models
// - The class comment in site_instance.h, listing the supported process models.
//
// IMPORTANT: this isn't to be confused with --process-per-site (which is about
// process consolidation, not isolation). You probably want this one.
const char kSitePerProcess[]                = "site-per-process";

// Skip gpu info collection, blacklist loading, and blacklist auto-update
// scheduling at browser startup time.
// Therefore, all GPU features are available, and about:gpu page shows empty
// content. The switch is intended only for layout tests.
// TODO(gab): Get rid of this switch entirely.
const char kSkipGpuDataLoading[]            = "skip-gpu-data-loading";

// Skips reencoding bitmaps as PNGs when the encoded data is unavailable
// during SKP capture.  This allows for obtaining an accurate sample of
// the types of images on the web, rather than being weighted towards PNGs
// that we have encoded ourselves.
const char kSkipReencodingOnSKPCapture[]    = "skip-reencoding-on-skp-capture";

// Specifies if the browser should start in fullscreen mode, like if the user
// had pressed F11 right after startup.
const char kStartFullscreen[] = "start-fullscreen";

// Specifies if the |StatsCollectionController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when running a test
// that needs to access the provided statistics.
const char kStatsCollectionController[] =
    "enable-stats-collection-bindings";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kTestingFixedHttpPort[]          = "testing-fixed-http-port";
const char kTestingFixedHttpsPort[]         = "testing-fixed-https-port";

// Type of the current test harness ("browser" or "ui").
const char kTestType[]                      = "test-type";

// Groups all out-of-process iframes to a different process from the process
// of the top document. This is a performance isolation mode.
const char kTopDocumentIsolation[] = "top-document-isolation";

// Controls how text selection granularity changes when touch text selection
// handles are dragged. Should be "character" or "direction". If not specified,
// the platform default is used.
const char kTouchTextSelectionStrategy[]    = "touch-selection-strategy";

// Prioritizes the UI's command stream in the GPU process
const char kUIPrioritizeInGpuProcess[] = "ui-prioritize-in-gpu-process";

// Bypass the media stream infobar by selecting the default device for media
// streams (e.g. WebRTC). Works with --use-fake-device-for-media-stream.
const char kUseFakeUIForMediaStream[]     = "use-fake-ui-for-media-stream";

// Use the Mandoline UI Service in the Chrome render process.
const char kUseMusInRenderer[] = "use-mus-in-renderer";

// Enable native GPU memory buffer support when available.
const char kEnableNativeGpuMemoryBuffers[] = "enable-native-gpu-memory-buffers";

// Texture target for CHROMIUM_image backed content textures.
const char kContentImageTextureTarget[] = "content-image-texture-target";

// Texture target for CHROMIUM_image backed video frame textures.
const char kVideoImageTextureTarget[] = "video-image-texture-target";

// Set when Chromium should use a mobile user agent.
const char kUseMobileUserAgent[] = "use-mobile-user-agent";

// Use remote compositor for the renderer.
const char kUseRemoteCompositing[] = "use-remote-compositing";

// The contents of this flag are prepended to the utility process command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// The utility process is sandboxed, with access to one directory. This flag
// specifies the directory that can be accessed.
const char kUtilityProcessAllowedDir[]      = "utility-allowed-dir";

const char kUtilityProcessRunningElevated[] = "utility-run-elevated";

// In debug builds, asserts that the stream of input events is valid.
const char kValidateInputEventStream[] = "validate-input-event-stream";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

#if defined(ENABLE_WEBRTC)
// Disables HW decode acceleration for WebRTC.
const char kDisableWebRtcHWDecoding[]       = "disable-webrtc-hw-decoding";

// Disables encryption of RTP Media for WebRTC. When Chrome embeds Content, it
// ignores this switch on its stable and beta channels.
const char kDisableWebRtcEncryption[]      = "disable-webrtc-encryption";

// Disables HW encode acceleration for WebRTC.
const char kDisableWebRtcHWEncoding[]       = "disable-webrtc-hw-encoding";
const char kDisableWebRtcHWEncodingVPx[] = "vpx";
const char kDisableWebRtcHWEncodingH264[] = "h264";
const char kDisableWebRtcHWEncodingNone[] = "none";

// Enables Origin header in Stun messages for WebRTC.
const char kEnableWebRtcStunOrigin[]        = "enable-webrtc-stun-origin";

// Enforce IP Permission check. TODO(guoweis): Remove this once the feature is
// not under finch and becomes the default.
const char kEnforceWebRtcIPPermissionCheck[] =
    "enforce-webrtc-ip-permission-check";

// Override WebRTC IP handling policy to mimic the behavior when WebRTC IP
// handling policy is specified in Preferences.
const char kForceWebRtcIPHandlingPolicy[] = "force-webrtc-ip-handling-policy";

// Renderer process parameter for WebRTC Stun probe trial to determine the
// interval. Please see SetupStunProbeTrial in
// chrome_browser_field_trials_desktop.cc for more detail.
const char kWebRtcStunProbeTrialParameter[] = "webrtc-stun-probe-trial";

// Override the maximum framerate as can be specified in calls to getUserMedia.
// This flag expects a value.  Example: --max-gum-fps=17.5
const char kWebRtcMaxCaptureFramerate[]     = "max-gum-fps";
#endif

#if defined(OS_ANDROID)
// Disable overscroll edge effects like those found in Android views.
const char kDisableOverscrollEdgeEffect[]   = "disable-overscroll-edge-effect";

// Disable the pull-to-refresh effect when vertically overscrolling content.
const char kDisablePullToRefreshEffect[]   = "disable-pull-to-refresh-effect";

// Disable the locking feature of the screen orientation API.
const char kDisableScreenOrientationLock[]  = "disable-screen-orientation-lock";

// Enable inverting of selection handles so that they are not clipped by the
// viewport boundaries.
const char kEnableAdaptiveSelectionHandleOrientation[] =
    "enable-adaptive-selection-handle-orientation";

// Enable content intent detection in the renderer.
const char kEnableContentIntentDetection[] =
    "enable-content-intent-detection";

// Enable drag manipulation of longpress-triggered text selections.
const char kEnableLongpressDragSelection[]  = "enable-longpress-drag-selection";

// The telephony region (ISO country code) to use in phone number detection.
const char kNetworkCountryIso[] = "network-country-iso";

// When blink should declare a load "done" for the purpose of the
// progress bar.
const char kProgressBarCompletion[] = "progress-bar-completion";

// Enables remote debug over HTTP on the specified socket name.
const char kRemoteDebuggingSocketName[]     = "remote-debugging-socket-name";

// Block ChildProcessMain thread of the renderer's ChildProcessService until a
// Java debugger is attached.
const char kRendererWaitForJavaDebugger[] = "renderer-wait-for-java-debugger";

// Enables overscrolling for the OSK on Android.
const char kEnableOSKOverscroll[]               = "enable-osk-overscroll";
#endif

// Enable the aggressive flushing of DOM Storage to minimize data loss.
const char kEnableAggressiveDOMStorageFlushing[] =
    "enable-aggressive-domstorage-flushing";

// Enable audio for desktop share.
const char kDisableAudioSupportForDesktopShare[] =
    "disable-audio-support-for-desktop-share";

#if defined(OS_CHROMEOS)
// Disables panel fitting (used for mirror mode).
const char kDisablePanelFitting[]           = "disable-panel-fitting";

// Disables VA-API accelerated video encode.
const char kDisableVaapiAcceleratedVideoEncode[] =
    "disable-vaapi-accelerated-video-encode";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Allows sending text-to-speech requests to speech-dispatcher, a common
// Linux speech service. Because it's buggy, the user must explicitly
// enable it so that visiting a random webpage can't cause instability.
const char kEnableSpeechDispatcher[] = "enable-speech-dispatcher";
#endif

#if defined(OS_WIN)
// /prefetch:# arguments to use when launching various process types. It has
// been observed that when file reads are consistent for 3 process launches with
// the same /prefetch:# argument, the Windows prefetcher starts issuing reads in
// batch at process launch. Because reads depend on the process type, the
// prefetcher wouldn't be able to observe consistent reads if no /prefetch:#
// arguments were used. Note that the browser process has no /prefetch:#
// argument; as such all other processes must have one in order to avoid
// polluting its profile. Note: # must always be in [1, 8]; otherwise it is
// ignored by the Windows prefetcher.
const char kPrefetchArgumentRenderer[] = "/prefetch:1";
const char kPrefetchArgumentGpu[] = "/prefetch:2";
const char kPrefetchArgumentPpapi[] = "/prefetch:3";
const char kPrefetchArgumentPpapiBroker[] = "/prefetch:4";
// /prefetch:5, /prefetch:6 and /prefetch:7 are reserved for content embedders
// and are not to be used by content itself.

// /prefetch:# argument shared by all process types that don't have their own.
// It is likely that the prefetcher won't work for these process types as it
// won't be able to observe consistent file reads across launches. However,
// having a valid prefetch argument for these process types is required to
// prevent them from interfering with the prefetch profile of the browser
// process.
const char kPrefetchArgumentOther[] = "/prefetch:8";

// Device scale factor passed to certain processes like renderers, etc.
const char kDeviceScaleFactor[]     = "device-scale-factor";

// Disable the Legacy Window which corresponds to the size of the WebContents.
const char kDisableLegacyIntermediateWindow[] = "disable-legacy-window";

// Disables the Win32K process mitigation policy for child processes.
const char kDisableWin32kLockDown[] = "disable-win32k-lockdown";

// Enables experimental hardware acceleration for VP8/VP9 video decoding.
// Bitmask - 0x1=Microsoft, 0x2=AMD, 0x03=Try all.
const char kEnableAcceleratedVpxDecode[] = "enable-accelerated-vpx-decode";

// Enables H264 HW decode acceleration for WebRtc on Win 7.
const char kEnableWin7WebRtcHWH264Decoding[] =
    "enable-win7-webrtc-hw-h264-decoding";

// DirectWrite FontCache is shared by browser to renderers using shared memory.
// This switch allows us to pass the shared memory handle to the renderer.
const char kFontCacheSharedHandle[] = "font-cache-shared-handle";

// Sets the free memory thresholds below which the system is considered to be
// under moderate and critical memory pressure. Used in the browser process,
// and ignored if invalid. Specified as a pair of comma separated integers.
// See base/win/memory_pressure_monitor.cc for defaults.
const char kMemoryPressureThresholdsMb[] = "memory-pressure-thresholds-mb";

// The boolean value (0/1) of FontRenderParams::antialiasing to be passed to
// Ppapi processes.
const char kPpapiAntialiasedTextEnabled[] = "ppapi-antialiased-text-enabled";

// The enum value of FontRenderParams::subpixel_rendering to be passed to Ppapi
// processes.
const char kPpapiSubpixelRenderingSetting[] =
    "ppapi-subpixel-rendering-setting";

// Enables the exporting of the tracing events to ETW. This is only supported on
// Windows Vista and later.
const char kTraceExportEventsToETW[] = "trace-export-events-to-etw";
#endif

#if defined(ENABLE_IPC_FUZZER)
// Dumps IPC messages sent from renderer processes to the browser process to
// the given directory. Used primarily to gather samples for IPC fuzzing.
const char kIpcDumpDirectory[] = "ipc-dump-directory";

// Specifies the testcase used by the IPC fuzzer.
const char kIpcFuzzerTestcase[] = "ipc-fuzzer-testcase";
#endif

// Don't dump stuff here, follow the same order as the header.

}  // namespace switches
