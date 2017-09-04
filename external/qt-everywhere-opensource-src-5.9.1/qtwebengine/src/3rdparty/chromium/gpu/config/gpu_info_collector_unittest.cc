// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/strings/string_split.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::gl::MockGLInterface;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

namespace {

// Allows testing of all configurations on all operating systems.
enum MockedOperatingSystemKind {
  kMockedAndroid,
  kMockedLinux,
  kMockedMacOSX,
  kMockedWindows
};

}  // anonymous namespace

namespace gpu {

static const MockedOperatingSystemKind kMockedOperatingSystemKinds[] = {
  kMockedAndroid,
  kMockedLinux,
  kMockedMacOSX,
  kMockedWindows
};

class GPUInfoCollectorTest
    : public testing::Test,
      public ::testing::WithParamInterface<MockedOperatingSystemKind> {
 public:
  GPUInfoCollectorTest() {}
  ~GPUInfoCollectorTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
    gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_.reset(new ::testing::StrictMock<::gl::MockGLInterface>());
    ::gl::MockGLInterface::SetGLInterface(gl_.get());
    switch (GetParam()) {
      case kMockedAndroid: {
        test_values_.gpu.vendor_id = 0;  // not implemented
        test_values_.gpu.device_id = 0;  // not implemented
        test_values_.driver_vendor = "";  // not implemented
        test_values_.driver_version = "14.0";
        test_values_.pixel_shader_version = "1.00";
        test_values_.vertex_shader_version = "1.00";
        test_values_.gl_renderer = "Adreno (TM) 320";
        test_values_.gl_vendor = "Qualcomm";
        test_values_.gl_version = "OpenGL ES 2.0 V@14.0 AU@04.02 (CL@3206)";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra GL_EXT_multisampled_render_to_texture";
        gl_shading_language_version_ = "1.00";
        break;
      }
      case kMockedLinux: {
        test_values_.gpu.vendor_id = 0x10de;
        test_values_.gpu.device_id = 0x0658;
        test_values_.driver_vendor = "NVIDIA";
        test_values_.driver_version = "195.36.24";
        test_values_.pixel_shader_version = "1.50";
        test_values_.vertex_shader_version = "1.50";
        test_values_.gl_renderer = "Quadro FX 380/PCI/SSE2";
        test_values_.gl_vendor = "NVIDIA Corporation";
        test_values_.gl_version = "3.2.0 NVIDIA 195.36.24";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra";
        gl_shading_language_version_ = "1.50 NVIDIA via Cg compiler";
        break;
      }
      case kMockedMacOSX: {
        test_values_.gpu.vendor_id = 0x10de;
        test_values_.gpu.device_id = 0x0640;
        test_values_.driver_vendor = "";  // not implemented
        test_values_.driver_version = "1.6.18";
        test_values_.pixel_shader_version = "1.20";
        test_values_.vertex_shader_version = "1.20";
        test_values_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
        test_values_.gl_vendor = "NVIDIA Corporation";
        test_values_.gl_version = "2.1 NVIDIA-1.6.18";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra GL_EXT_framebuffer_multisample";
        gl_shading_language_version_ = "1.20 ";
        break;
      }
      case kMockedWindows: {
        test_values_.gpu.vendor_id = 0x10de;
        test_values_.gpu.device_id = 0x0658;
        test_values_.driver_vendor = "";  // not implemented
        test_values_.driver_version = "";
        test_values_.pixel_shader_version = "1.40";
        test_values_.vertex_shader_version = "1.40";
        test_values_.gl_renderer = "Quadro FX 380/PCI/SSE2";
        test_values_.gl_vendor = "NVIDIA Corporation";
        test_values_.gl_version = "3.1.0";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra";
        gl_shading_language_version_ = "1.40 NVIDIA via Cg compiler";
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }

    EXPECT_CALL(*gl_, GetString(GL_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gl_version.c_str())));

    // Now that that expectation is set up, we can call this helper function.
    if (gl::WillUseGLGetStringForExtensions()) {
      EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
          .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
              test_values_.gl_extensions.c_str())));
    } else {
      split_extensions_.clear();
      split_extensions_ = base::SplitString(
          test_values_.gl_extensions, " ",
          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      EXPECT_CALL(*gl_, GetIntegerv(GL_NUM_EXTENSIONS, _))
          .WillRepeatedly(SetArgPointee<1>(split_extensions_.size()));
      for (size_t ii = 0; ii < split_extensions_.size(); ++ii) {
        EXPECT_CALL(*gl_, GetStringi(GL_EXTENSIONS, ii))
            .WillRepeatedly(Return(reinterpret_cast<const uint8_t*>(
                split_extensions_[ii].c_str())));
      }
    }
    EXPECT_CALL(*gl_, GetString(GL_SHADING_LANGUAGE_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_shading_language_version_)));
    EXPECT_CALL(*gl_, GetString(GL_VENDOR))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gl_vendor.c_str())));
    EXPECT_CALL(*gl_, GetString(GL_RENDERER))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gl_renderer.c_str())));
    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_SAMPLES, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
  }

  void TearDown() override {
    ::gl::MockGLInterface::SetGLInterface(NULL);
    gl_.reset();
    gl::init::ClearGLBindings();

    testing::Test::TearDown();
  }

 public:
  // Use StrictMock to make 100% sure we know how GL will be called.
  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;
  GPUInfo test_values_;
  const char* gl_shading_language_version_ = nullptr;

  // Persistent storage is needed for the split extension string.
  std::vector<std::string> split_extensions_;
};

INSTANTIATE_TEST_CASE_P(GPUConfig,
                        GPUInfoCollectorTest,
                        ::testing::ValuesIn(kMockedOperatingSystemKinds));

// TODO(rlp): Test the vendor and device id collection if deemed necessary as
//            it involves several complicated mocks for each platform.

// TODO(kbr): This test still has platform-dependent behavior because
// CollectDriverInfoGL behaves differently per platform. This should
// be fixed.
TEST_P(GPUInfoCollectorTest, CollectGraphicsInfoGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
#if defined(OS_WIN)
  if (GetParam() == kMockedWindows) {
    EXPECT_EQ(test_values_.driver_vendor,
              gpu_info.driver_vendor);
    // Skip testing the driver version on Windows because it's
    // obtained from the bot's registry.
  }
#elif defined(OS_MACOSX)
  if (GetParam() == kMockedMacOSX) {
    EXPECT_EQ(test_values_.driver_vendor,
              gpu_info.driver_vendor);
    EXPECT_EQ(test_values_.driver_version,
              gpu_info.driver_version);
  }
#elif defined(OS_ANDROID)
  if (GetParam() == kMockedAndroid) {
    EXPECT_EQ(test_values_.driver_vendor,
              gpu_info.driver_vendor);
    EXPECT_EQ(test_values_.driver_version,
              gpu_info.driver_version);
  }
#else  // defined (OS_LINUX)
  if (GetParam() == kMockedLinux) {
    EXPECT_EQ(test_values_.driver_vendor,
              gpu_info.driver_vendor);
    EXPECT_EQ(test_values_.driver_version,
              gpu_info.driver_version);
  }
#endif

  EXPECT_EQ(test_values_.pixel_shader_version,
            gpu_info.pixel_shader_version);
  EXPECT_EQ(test_values_.vertex_shader_version,
            gpu_info.vertex_shader_version);
  EXPECT_EQ(test_values_.gl_version, gpu_info.gl_version);
  EXPECT_EQ(test_values_.gl_renderer, gpu_info.gl_renderer);
  EXPECT_EQ(test_values_.gl_vendor, gpu_info.gl_vendor);
  EXPECT_EQ(test_values_.gl_extensions, gpu_info.gl_extensions);
}

class CollectDriverInfoGLTest : public testing::Test {
 public:
  CollectDriverInfoGLTest() {}
  ~CollectDriverInfoGLTest() override {}

  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(CollectDriverInfoGLTest, CollectDriverInfoGL) {
  // clang-format off
  const struct {
    const char* driver_vendor;
    const char* driver_version;
    const char* gl_renderer;
    const char* gl_vendor;
    const char* gl_version;
    const char* expected_driver_vendor;
    const char* expected_driver_version;
  } kTestStrings[] = {
#if defined(OS_ANDROID)
    {"Unknown",
     "-1",
     "Adreno (TM) 320",
     "Qualcomm",
     "OpenGL ES 2.0 V@14.0 AU@04.02 (CL@3206)",
     "Unknown",
     "14.0"},
    {"Unknown",
     "-1",
     "Adreno (TM) 420",
     "Qualcomm",
     "OpenGL ES 3.0 V@84.0 AU@ (CL@)",
     "Unknown",
     "84.0"},
    {"Unknown",
     "-1",
     "PowerVR Rogue G6430",
     "Imagination Technologies",
     "OpenGL ES 3.1 build 1.4@3283119",
     "Unknown",
     "1.4"},
    {"Unknown",
     "-1",
     "Mali-T604",
     "ARM",
     "OpenGL ES 3.1",
     "Unknown",
     "0"},
    {"Unknown",
     "-1",
     "NVIDIA Tegra",
     "NVIDIA Corporation",
     "OpenGL ES 3.1 NVIDIA 343.00",
     "Unknown",
     "343.00"},
    {"Unknown",
     "-1",
     "NVIDIA Tegra 3",
     "NVIDIA Corporation",
     "OpenGL ES 2.0 14.01003",
     "Unknown",
     "14.01003"},
    {"Unknown",
     "-1",
     "random GPU",
     "random vendor",
     "OpenGL ES 2.0 with_long_version_string=1.2.3.4",
     NULL,
     "1.2"},
    {"Unknown",
     "-1",
     "random GPU",
     "random vendor",
     "OpenGL ES 2.0 with_short_version_string=1",
     NULL,
     "0"},
    {"Unknown",
     "-1",
     "random GPU",
     "random vendor",
     "OpenGL ES 2.0 with_no_version_string",
     NULL,
     "0"},
#elif defined(OS_MACOSX)
    {"Unknown",
     "-1",
     "Intel Iris Pro OpenGL Engine",
     "Intel Inc.",
     "2.1 INTEL-10.6.20",
     "Unknown",
     "10.6.20"},
#elif defined(OS_LINUX)
    {"",
     "",
     "Quadro K2000/PCIe/SSE2",
     "NVIDIA Corporation",
     "4.4.0 NVIDIA 331.79",
     "NVIDIA",
     "331.79"},
    {"",
     "",
     "Gallium 0.4 on NVE7",
     "nouveau",
     "3.3 (Core Profile) Mesa 10.5.9",
     "Mesa",
     "10.5.9"},
    {"",
     "",
     "Mesa DRI Intel(R) Haswell Mobile",
     "Intel Open Source Technology Center",
     "OpenGL ES 3.0 Mesa 12.1.0-devel (git-ed9dd3b)",
     "Mesa",
     "12.1.0"},
    {"ATI / AMD",
     "15.201.1151",
     "ASUS R5 230 Series",
     "ATI Technologies Inc.",
     "4.5.13399 Compatibility Profile Context 14.0",
     "ATI / AMD",
     "15.201.1151"},
#endif
    {NULL, NULL, NULL, NULL, NULL, NULL, NULL}
  };
  // clang-format on

  for (int i = 0; kTestStrings[i].gl_renderer != NULL; ++i) {
    GPUInfo gpu_info;
    const auto& testStrings = kTestStrings[i];
    gpu_info.driver_vendor = testStrings.driver_vendor;
    gpu_info.driver_version = testStrings.driver_version;
    gpu_info.gl_renderer = testStrings.gl_renderer;
    gpu_info.gl_vendor = testStrings.gl_vendor;
    gpu_info.gl_version = testStrings.gl_version;
    EXPECT_EQ(kCollectInfoSuccess, CollectDriverInfoGL(&gpu_info));
    EXPECT_EQ(testStrings.expected_driver_version, gpu_info.driver_version);
    if (testStrings.expected_driver_vendor) {
      EXPECT_EQ(testStrings.expected_driver_vendor, gpu_info.driver_vendor);
    }
  }
}

TEST(MultiGPUsTest, IdentifyActiveGPU) {
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x0416;

  GPUInfo gpu_info;
  gpu_info.gpu = nvidia_gpu;
  gpu_info.secondary_gpus.push_back(intel_gpu);

  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  gpu_info.gl_vendor = "Intel Open Source Technology Center";
  gpu_info.gl_renderer = "Mesa DRI Intel(R) Haswell Mobile";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_TRUE(gpu_info.secondary_gpus[0].active);

  gpu_info.gl_vendor = "NVIDIA Corporation";
  gpu_info.gl_renderer = "Quadro 600/PCIe/SSE2";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_TRUE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
}

TEST(MultiGPUsTest, IdentifyActiveGPUAvoidFalseMatch) {
  // Verify that "Corporation" won't be matched with "ati".
  GPUInfo::GPUDevice amd_gpu;
  amd_gpu.vendor_id = 0x1002;
  amd_gpu.device_id = 0x0df8;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x0416;

  GPUInfo gpu_info;
  gpu_info.gpu = amd_gpu;
  gpu_info.secondary_gpus.push_back(intel_gpu);

  gpu_info.gl_vendor = "Google Corporation";
  gpu_info.gl_renderer = "Chrome GPU Team";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
}

}  // namespace gpu

