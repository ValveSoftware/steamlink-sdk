// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu_process_launch_causes.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_switches.h"

namespace {

constexpr content::CauseForGpuLaunch kInitCause =
    content::CAUSE_FOR_GPU_LAUNCH_BROWSER_STARTUP;

scoped_refptr<content::ContextProviderCommandBuffer> CreateContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  // This is for an offscreen context, so the default framebuffer doesn't need
  // any alpha, depth, stencil, antialiasing.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  return make_scoped_refptr(new content::ContextProviderCommandBuffer(
      std::move(gpu_channel_host), gpu::GPU_STREAM_DEFAULT,
      gpu::GpuStreamPriority::NORMAL, gpu::kNullSurfaceHandle, GURL(),
      automatic_flushes, support_locking, gpu::SharedMemoryLimits(), attributes,
      nullptr, content::command_buffer_metrics::OFFSCREEN_CONTEXT_FOR_TESTING));
}

class ContextTestBase : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // This may leave the provider_ null in some cases, so tests need to early
    // out.
    if (!content::BrowserGpuChannelHostFactory::CanUseForTesting())
      return;

    if (!content::BrowserGpuChannelHostFactory::instance())
      content::BrowserGpuChannelHostFactory::Initialize(true);

    content::BrowserGpuChannelHostFactory* factory =
        content::BrowserGpuChannelHostFactory::instance();
    CHECK(factory);
    base::RunLoop run_loop;
    factory->EstablishGpuChannel(kInitCause, run_loop.QuitClosure());
    run_loop.Run();
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
        factory->GetGpuChannel());
    CHECK(gpu_channel_host);

    provider_ = CreateContext(std::move(gpu_channel_host));
    bool bound = provider_->BindToCurrentThread();
    CHECK(bound);
    gl_ = provider_->ContextGL();
    context_support_ = provider_->ContextSupport();

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Must delete the context first.
    provider_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  gpu::ContextSupport* context_support_ = nullptr;

 private:
  scoped_refptr<content::ContextProviderCommandBuffer> provider_;
};

}  // namespace

// Include the shared tests.
#define CONTEXT_TEST_F IN_PROC_BROWSER_TEST_F
#include "gpu/ipc/client/gpu_context_tests.h"

namespace content {

class BrowserGpuChannelHostFactoryTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    if (!BrowserGpuChannelHostFactory::CanUseForTesting())
      return;

    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    if (!BrowserGpuChannelHostFactory::instance())
      BrowserGpuChannelHostFactory::Initialize(false);

    CHECK(GetFactory());

    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    command_line->AppendSwitch(switches::kDisableGpuEarlyInit);
  }

  void OnContextLost(const base::Closure callback, int* counter) {
    (*counter)++;
    callback.Run();
  }

 protected:
  BrowserGpuChannelHostFactory* GetFactory() {
    return BrowserGpuChannelHostFactory::instance();
  }

  bool IsChannelEstablished() {
    return GetFactory()->GetGpuChannel() != NULL;
  }

  void EstablishAndWait() {
    base::RunLoop run_loop;
    GetFactory()->EstablishGpuChannel(kInitCause, run_loop.QuitClosure());
    run_loop.Run();
  }

  gpu::GpuChannelHost* GetGpuChannel() { return GetFactory()->GetGpuChannel(); }

  static void Signal(bool *event) {
    CHECK_EQ(*event, false);
    *event = true;
  }
};

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, MAYBE_Basic) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(GetGpuChannel() != NULL);
}

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_EstablishAndTerminate EstablishAndTerminate
#else
#define MAYBE_EstablishAndTerminate DISABLED_EstablishAndTerminate
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_EstablishAndTerminate) {
  DCHECK(!IsChannelEstablished());
  base::RunLoop run_loop;
  GetFactory()->EstablishGpuChannel(kInitCause, run_loop.QuitClosure());
  GetFactory()->Terminate();

  // The callback should still trigger.
  run_loop.Run();
}

#if !defined(OS_ANDROID)
// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_AlreadyEstablished AlreadyEstablished
#else
#define MAYBE_AlreadyEstablished DISABLED_AlreadyEstablished
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_AlreadyEstablished) {
  DCHECK(!IsChannelEstablished());
  scoped_refptr<gpu::GpuChannelHost> gpu_channel =
      GetFactory()->EstablishGpuChannelSync(kInitCause);

  // Expect established callback immediately.
  bool event = false;
  GetFactory()->EstablishGpuChannel(
      kInitCause,
      base::Bind(&BrowserGpuChannelHostFactoryTest::Signal, &event));
  EXPECT_TRUE(event);
  EXPECT_EQ(gpu_channel.get(), GetGpuChannel());
}
#endif

// Test fails on Windows because GPU Channel set-up fails.
#if !defined(OS_WIN)
#define MAYBE_GrContextKeepsGpuChannelAlive GrContextKeepsGpuChannelAlive
#else
#define MAYBE_GrContextKeepsGpuChannelAlive \
    DISABLED_GrContextKeepsGpuChannelAlive
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_GrContextKeepsGpuChannelAlive) {
  // Test for crbug.com/551143
  // This test verifies that holding a reference to the GrContext created by
  // a ContextProviderCommandBuffer will keep the gpu channel alive after the
  // provider has been destroyed. Without this behavior, user code would have
  // to be careful to destroy objects in the right order to avoid using freed
  // memory as a function pointer in the GrContext's GrGLInterface instance.
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();

  // Step 2: verify that holding onto the provider's GrContext will
  // retain the host after provider is destroyed.
  scoped_refptr<ContextProviderCommandBuffer> provider =
      CreateContext(GetGpuChannel());
  EXPECT_TRUE(provider->BindToCurrentThread());

  sk_sp<GrContext> gr_context = sk_ref_sp(provider->GrContext());

  SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
  sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(
      gr_context.get(), SkBudgeted::kNo, info);
  EXPECT_TRUE(surface);

  // Destroy the GL context after we made a surface.
  provider = nullptr;

  // New surfaces will fail to create now.
  sk_sp<SkSurface> surface2 =
      SkSurface::MakeRenderTarget(gr_context.get(), SkBudgeted::kNo, info);
  EXPECT_FALSE(surface2);

  // Drop our reference to the gr_context also.
  gr_context = nullptr;

  // After the context provider is destroyed, the surface no longer has access
  // to the GrContext, even though it's alive. Use the canvas after the provider
  // and GrContext have been locally unref'ed. This should work fine as the
  // GrContext has been abandoned when the GL context provider was destroyed
  // above.
  SkPaint greenFillPaint;
  greenFillPaint.setColor(SK_ColorGREEN);
  greenFillPaint.setStyle(SkPaint::kFill_Style);
  // Passes by not crashing
  surface->getCanvas()->drawRect(SkRect::MakeWH(100, 100), greenFillPaint);
}

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_CrashAndRecover
#else
#define MAYBE_CrashAndRecover DISABLED_CrashAndRecover
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_CrashAndRecover) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  scoped_refptr<gpu::GpuChannelHost> host = GetGpuChannel();

  scoped_refptr<ContextProviderCommandBuffer> provider =
      CreateContext(GetGpuChannel());
  base::RunLoop run_loop;
  int counter = 0;
  provider->SetLostContextCallback(
      base::Bind(&BrowserGpuChannelHostFactoryTest::OnContextLost,
                 base::Unretained(this), run_loop.QuitClosure(), &counter));
  EXPECT_TRUE(provider->BindToCurrentThread());
  GpuProcessHostUIShim* shim =
      GpuProcessHostUIShim::FromID(GetFactory()->GpuProcessHostId());
  EXPECT_TRUE(shim != NULL);
  shim->SimulateCrash();
  run_loop.Run();

  EXPECT_EQ(1, counter);
  EXPECT_FALSE(IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(IsChannelEstablished());
}

}  // namespace content
