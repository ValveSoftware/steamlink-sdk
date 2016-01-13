// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/dxva_video_decode_accelerator.h"

#if !defined(OS_WIN)
#error This file should only be built on Windows.
#endif   // !defined(OS_WIN)

#include <ks.h>
#include <codecapi.h>
#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/win/windows_version.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"

namespace content {

// We only request 5 picture buffers from the client which are used to hold the
// decoded samples. These buffers are then reused when the client tells us that
// it is done with the buffer.
static const int kNumPictureBuffers = 5;

#define RETURN_ON_FAILURE(result, log, ret)  \
  do {                                       \
    if (!(result)) {                         \
      DLOG(ERROR) << log;                    \
      return ret;                            \
    }                                        \
  } while (0)

#define RETURN_ON_HR_FAILURE(result, log, ret)                    \
  RETURN_ON_FAILURE(SUCCEEDED(result),                            \
                    log << ", HRESULT: 0x" << std::hex << result, \
                    ret);

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret)  \
  do {                                                              \
    if (!(result)) {                                                \
      DVLOG(1) << log;                                              \
      StopOnError(error_code);                                      \
      return ret;                                                   \
    }                                                               \
  } while (0)

#define RETURN_AND_NOTIFY_ON_HR_FAILURE(result, log, error_code, ret)  \
  RETURN_AND_NOTIFY_ON_FAILURE(SUCCEEDED(result),                      \
                               log << ", HRESULT: 0x" << std::hex << result, \
                               error_code, ret);

// Maximum number of iterations we allow before aborting the attempt to flush
// the batched queries to the driver and allow torn/corrupt frames to be
// rendered.
enum { kMaxIterationsForD3DFlush = 10 };

static IMFSample* CreateEmptySample() {
  base::win::ScopedComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.Receive());
  RETURN_ON_HR_FAILURE(hr, "MFCreateSample failed", NULL);
  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer of length |buffer_length|
// on a |align|-byte boundary. Alignment must be a perfect power of 2 or 0.
static IMFSample* CreateEmptySampleWithBuffer(int buffer_length, int align) {
  CHECK_GT(buffer_length, 0);

  base::win::ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySample());

  base::win::ScopedComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = E_FAIL;
  if (align == 0) {
    // Note that MFCreateMemoryBuffer is same as MFCreateAlignedMemoryBuffer
    // with the align argument being 0.
    hr = MFCreateMemoryBuffer(buffer_length, buffer.Receive());
  } else {
    hr = MFCreateAlignedMemoryBuffer(buffer_length,
                                     align - 1,
                                     buffer.Receive());
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for sample", NULL);

  hr = sample->AddBuffer(buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", NULL);

  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample time are not known, provide 0.
// |min_size| specifies the minimum size of the buffer (might be required by
// the decoder for input). If no alignment is required, provide 0.
static IMFSample* CreateInputSample(const uint8* stream, int size,
                                    int min_size, int alignment) {
  CHECK(stream);
  CHECK_GT(size, 0);
  base::win::ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySampleWithBuffer(std::max(min_size, size),
                                            alignment));
  RETURN_ON_FAILURE(sample, "Failed to create empty sample", NULL);

  base::win::ScopedComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->GetBufferByIndex(0, buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from sample", NULL);

  DWORD max_length = 0;
  DWORD current_length = 0;
  uint8* destination = NULL;
  hr = buffer->Lock(&destination, &max_length, &current_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to lock buffer", NULL);

  CHECK_EQ(current_length, 0u);
  CHECK_GE(static_cast<int>(max_length), size);
  memcpy(destination, stream, size);

  hr = buffer->Unlock();
  RETURN_ON_HR_FAILURE(hr, "Failed to unlock buffer", NULL);

  hr = buffer->SetCurrentLength(size);
  RETURN_ON_HR_FAILURE(hr, "Failed to set buffer length", NULL);

  return sample.Detach();
}

static IMFSample* CreateSampleFromInputBuffer(
    const media::BitstreamBuffer& bitstream_buffer,
    DWORD stream_size,
    DWORD alignment) {
  base::SharedMemory shm(bitstream_buffer.handle(), true);
  RETURN_ON_FAILURE(shm.Map(bitstream_buffer.size()),
                    "Failed in base::SharedMemory::Map", NULL);

  return CreateInputSample(reinterpret_cast<const uint8*>(shm.memory()),
                           bitstream_buffer.size(),
                           stream_size,
                           alignment);
}

// Maintains information about a DXVA picture buffer, i.e. whether it is
// available for rendering, the texture information, etc.
struct DXVAVideoDecodeAccelerator::DXVAPictureBuffer {
 public:
  static linked_ptr<DXVAPictureBuffer> Create(
      const DXVAVideoDecodeAccelerator& decoder,
      const media::PictureBuffer& buffer,
      EGLConfig egl_config);
  ~DXVAPictureBuffer();

  void ReusePictureBuffer();
  // Copies the output sample data to the picture buffer provided by the
  // client.
  // The dest_surface parameter contains the decoded bits.
  bool CopyOutputSampleDataToPictureBuffer(
      const DXVAVideoDecodeAccelerator& decoder,
      IDirect3DSurface9* dest_surface);

  bool available() const {
    return available_;
  }

  void set_available(bool available) {
    available_ = available;
  }

  int id() const {
    return picture_buffer_.id();
  }

  gfx::Size size() const {
    return picture_buffer_.size();
  }

 private:
  explicit DXVAPictureBuffer(const media::PictureBuffer& buffer);

  bool available_;
  media::PictureBuffer picture_buffer_;
  EGLSurface decoding_surface_;
  base::win::ScopedComPtr<IDirect3DTexture9> decoding_texture_;
  // Set to true if RGB is supported by the texture.
  // Defaults to true.
  bool use_rgb_;

  DISALLOW_COPY_AND_ASSIGN(DXVAPictureBuffer);
};

// static
linked_ptr<DXVAVideoDecodeAccelerator::DXVAPictureBuffer>
DXVAVideoDecodeAccelerator::DXVAPictureBuffer::Create(
    const DXVAVideoDecodeAccelerator& decoder,
    const media::PictureBuffer& buffer,
    EGLConfig egl_config) {
  linked_ptr<DXVAPictureBuffer> picture_buffer(new DXVAPictureBuffer(buffer));

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();

  EGLint use_rgb = 1;
  eglGetConfigAttrib(egl_display, egl_config, EGL_BIND_TO_TEXTURE_RGB,
                     &use_rgb);

  EGLint attrib_list[] = {
    EGL_WIDTH, buffer.size().width(),
    EGL_HEIGHT, buffer.size().height(),
    EGL_TEXTURE_FORMAT, use_rgb ? EGL_TEXTURE_RGB : EGL_TEXTURE_RGBA,
    EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
    EGL_NONE
  };

  picture_buffer->decoding_surface_ = eglCreatePbufferSurface(
      egl_display,
      egl_config,
      attrib_list);
  RETURN_ON_FAILURE(picture_buffer->decoding_surface_,
                    "Failed to create surface",
                    linked_ptr<DXVAPictureBuffer>(NULL));

  HANDLE share_handle = NULL;
  EGLBoolean ret = eglQuerySurfacePointerANGLE(
      egl_display,
      picture_buffer->decoding_surface_,
      EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
      &share_handle);

  RETURN_ON_FAILURE(share_handle && ret == EGL_TRUE,
                    "Failed to query ANGLE surface pointer",
                    linked_ptr<DXVAPictureBuffer>(NULL));

  HRESULT hr = decoder.device_->CreateTexture(
      buffer.size().width(),
      buffer.size().height(),
      1,
      D3DUSAGE_RENDERTARGET,
      use_rgb ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
      D3DPOOL_DEFAULT,
      picture_buffer->decoding_texture_.Receive(),
      &share_handle);

  RETURN_ON_HR_FAILURE(hr, "Failed to create texture",
                       linked_ptr<DXVAPictureBuffer>(NULL));
  picture_buffer->use_rgb_ = !!use_rgb;
  return picture_buffer;
}

DXVAVideoDecodeAccelerator::DXVAPictureBuffer::DXVAPictureBuffer(
    const media::PictureBuffer& buffer)
    : available_(true),
      picture_buffer_(buffer),
      decoding_surface_(NULL),
      use_rgb_(true) {
}

DXVAVideoDecodeAccelerator::DXVAPictureBuffer::~DXVAPictureBuffer() {
  if (decoding_surface_) {
    EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();

    eglReleaseTexImage(
        egl_display,
        decoding_surface_,
        EGL_BACK_BUFFER);

    eglDestroySurface(
        egl_display,
        decoding_surface_);
    decoding_surface_ = NULL;
  }
}

void DXVAVideoDecodeAccelerator::DXVAPictureBuffer::ReusePictureBuffer() {
  DCHECK(decoding_surface_);
  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  eglReleaseTexImage(
    egl_display,
    decoding_surface_,
    EGL_BACK_BUFFER);
  set_available(true);
}

bool DXVAVideoDecodeAccelerator::DXVAPictureBuffer::
    CopyOutputSampleDataToPictureBuffer(
        const DXVAVideoDecodeAccelerator& decoder,
        IDirect3DSurface9* dest_surface) {
  DCHECK(dest_surface);

  D3DSURFACE_DESC surface_desc;
  HRESULT hr = dest_surface->GetDesc(&surface_desc);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);

  D3DSURFACE_DESC texture_desc;
  decoding_texture_->GetLevelDesc(0, &texture_desc);

  if (texture_desc.Width != surface_desc.Width ||
      texture_desc.Height != surface_desc.Height) {
    NOTREACHED() << "Decode surface of different dimension than texture";
    return false;
  }

  hr = decoder.d3d9_->CheckDeviceFormatConversion(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, surface_desc.Format,
      use_rgb_ ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8);
  RETURN_ON_HR_FAILURE(hr, "Device does not support format converision", false);

  // This function currently executes in the context of IPC handlers in the
  // GPU process which ensures that there is always an OpenGL context.
  GLint current_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

  glBindTexture(GL_TEXTURE_2D, picture_buffer_.texture_id());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  base::win::ScopedComPtr<IDirect3DSurface9> d3d_surface;
  hr = decoding_texture_->GetSurfaceLevel(0, d3d_surface.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface from texture", false);

  hr = decoder.device_->StretchRect(
      dest_surface, NULL, d3d_surface, NULL, D3DTEXF_NONE);
  RETURN_ON_HR_FAILURE(hr, "Colorspace conversion via StretchRect failed",
                        false);

  // Ideally, this should be done immediately before the draw call that uses
  // the texture. Flush it once here though.
  hr = decoder.query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END", false);

  // The DXVA decoder has its own device which it uses for decoding. ANGLE
  // has its own device which we don't have access to.
  // The above code attempts to copy the decoded picture into a surface
  // which is owned by ANGLE. As there are multiple devices involved in
  // this, the StretchRect call above is not synchronous.
  // We attempt to flush the batched operations to ensure that the picture is
  // copied to the surface owned by ANGLE.
  // We need to do this in a loop and call flush multiple times.
  // We have seen the GetData call for flushing the command buffer fail to
  // return success occassionally on multi core machines, leading to an
  // infinite loop.
  // Workaround is to have an upper limit of 10 on the number of iterations to
  // wait for the Flush to finish.
  int iterations = 0;
  while ((decoder.query_->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE) &&
         ++iterations < kMaxIterationsForD3DFlush) {
    Sleep(1);  // Poor-man's Yield().
  }
  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  eglBindTexImage(
      egl_display,
      decoding_surface_,
      EGL_BACK_BUFFER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, current_texture);
  return true;
}

DXVAVideoDecodeAccelerator::PendingSampleInfo::PendingSampleInfo(
    int32 buffer_id, IMFSample* sample)
    : input_buffer_id(buffer_id) {
  output_sample.Attach(sample);
}

DXVAVideoDecodeAccelerator::PendingSampleInfo::~PendingSampleInfo() {}

// static
bool DXVAVideoDecodeAccelerator::CreateD3DDevManager() {
  TRACE_EVENT0("gpu", "DXVAVideoDecodeAccelerator_CreateD3DDevManager");

  HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, d3d9_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Direct3DCreate9Ex failed", false);

  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = 1;
  present_params.BackBufferHeight = 1;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = ::GetShellWindow();
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  hr = d3d9_->CreateDeviceEx(D3DADAPTER_DEFAULT,
                             D3DDEVTYPE_HAL,
                             ::GetShellWindow(),
                             D3DCREATE_FPU_PRESERVE |
                             D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                             D3DCREATE_DISABLE_PSGP_THREADING |
                             D3DCREATE_MULTITHREADED,
                             &present_params,
                             NULL,
                             device_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device", false);

  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token_,
                                         device_manager_.Receive());
  RETURN_ON_HR_FAILURE(hr, "DXVA2CreateDirect3DDeviceManager9 failed", false);

  hr = device_manager_->ResetDevice(device_, dev_manager_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device", false);

  hr = device_->CreateQuery(D3DQUERYTYPE_EVENT, query_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device query", false);
  // Ensure query_ API works (to avoid an infinite loop later in
  // CopyOutputSampleDataToPictureBuffer).
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END test query", false);
  return true;
}

DXVAVideoDecodeAccelerator::DXVAVideoDecodeAccelerator(
    const base::Callback<bool(void)>& make_context_current)
    : client_(NULL),
      dev_manager_reset_token_(0),
      egl_config_(NULL),
      state_(kUninitialized),
      pictures_requested_(false),
      inputs_before_decode_(0),
      make_context_current_(make_context_current),
      weak_this_factory_(this) {
  memset(&input_stream_info_, 0, sizeof(input_stream_info_));
  memset(&output_stream_info_, 0, sizeof(output_stream_info_));
}

DXVAVideoDecodeAccelerator::~DXVAVideoDecodeAccelerator() {
  client_ = NULL;
}

bool DXVAVideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile,
                                            Client* client) {
  DCHECK(CalledOnValidThread());

  client_ = client;

  // Not all versions of Windows 7 and later include Media Foundation DLLs.
  // Instead of crashing while delay loading the DLL when calling MFStartup()
  // below, probe whether we can successfully load the DLL now.
  //
  // See http://crbug.com/339678 for details.
  HMODULE mfplat_dll = ::LoadLibrary(L"MFPlat.dll");
  RETURN_ON_FAILURE(mfplat_dll, "MFPlat.dll is required for decoding", false);

  // TODO(ananta)
  // H264PROFILE_HIGH video decoding is janky at times. Needs more
  // investigation.
  if (profile != media::H264PROFILE_BASELINE &&
      profile != media::H264PROFILE_MAIN &&
      profile != media::H264PROFILE_HIGH) {
    RETURN_AND_NOTIFY_ON_FAILURE(false,
        "Unsupported h264 profile", PLATFORM_FAILURE, false);
  }

  RETURN_AND_NOTIFY_ON_FAILURE(
      gfx::g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle,
      "EGL_ANGLE_surface_d3d_texture_2d_share_handle unavailable",
      PLATFORM_FAILURE,
      false);

  RETURN_AND_NOTIFY_ON_FAILURE((state_ == kUninitialized),
      "Initialize: invalid state: " << state_, ILLEGAL_STATE, false);

  HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "MFStartup failed.", PLATFORM_FAILURE,
      false);

  RETURN_AND_NOTIFY_ON_FAILURE(CreateD3DDevManager(),
                               "Failed to initialize D3D device and manager",
                               PLATFORM_FAILURE,
                               false);

  RETURN_AND_NOTIFY_ON_FAILURE(InitDecoder(profile),
      "Failed to initialize decoder", PLATFORM_FAILURE, false);

  RETURN_AND_NOTIFY_ON_FAILURE(GetStreamsInfoAndBufferReqs(),
      "Failed to get input/output stream info.", PLATFORM_FAILURE, false);

  RETURN_AND_NOTIFY_ON_FAILURE(
      SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0),
      "Send MFT_MESSAGE_NOTIFY_BEGIN_STREAMING notification failed",
      PLATFORM_FAILURE, false);

  RETURN_AND_NOTIFY_ON_FAILURE(
      SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0),
      "Send MFT_MESSAGE_NOTIFY_START_OF_STREAM notification failed",
      PLATFORM_FAILURE, false);

  state_ = kNormal;
  return true;
}

void DXVAVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(CalledOnValidThread());

  RETURN_AND_NOTIFY_ON_FAILURE((state_ == kNormal || state_ == kStopped ||
                                state_ == kFlushing),
      "Invalid state: " << state_, ILLEGAL_STATE,);

  base::win::ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateSampleFromInputBuffer(bitstream_buffer,
                                            input_stream_info_.cbSize,
                                            input_stream_info_.cbAlignment));
  RETURN_AND_NOTIFY_ON_FAILURE(sample, "Failed to create input sample",
                               PLATFORM_FAILURE,);

  RETURN_AND_NOTIFY_ON_HR_FAILURE(sample->SetSampleTime(bitstream_buffer.id()),
      "Failed to associate input buffer id with sample", PLATFORM_FAILURE,);

  DecodeInternal(sample);
}

void DXVAVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(CalledOnValidThread());

  RETURN_AND_NOTIFY_ON_FAILURE((state_ != kUninitialized),
      "Invalid state: " << state_, ILLEGAL_STATE,);
  RETURN_AND_NOTIFY_ON_FAILURE((kNumPictureBuffers == buffers.size()),
      "Failed to provide requested picture buffers. (Got " << buffers.size() <<
      ", requested " << kNumPictureBuffers << ")", INVALID_ARGUMENT,);

  // Copy the picture buffers provided by the client to the available list,
  // and mark these buffers as available for use.
  for (size_t buffer_index = 0; buffer_index < buffers.size();
       ++buffer_index) {
    linked_ptr<DXVAPictureBuffer> picture_buffer =
        DXVAPictureBuffer::Create(*this, buffers[buffer_index], egl_config_);
    RETURN_AND_NOTIFY_ON_FAILURE(picture_buffer.get(),
        "Failed to allocate picture buffer", PLATFORM_FAILURE,);

    bool inserted = output_picture_buffers_.insert(std::make_pair(
        buffers[buffer_index].id(), picture_buffer)).second;
    DCHECK(inserted);
  }
  ProcessPendingSamples();
  if (state_ == kFlushing && pending_output_samples_.empty())
    FlushInternal();
}

void DXVAVideoDecodeAccelerator::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());

  RETURN_AND_NOTIFY_ON_FAILURE((state_ != kUninitialized),
      "Invalid state: " << state_, ILLEGAL_STATE,);

  if (output_picture_buffers_.empty())
    return;

  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  RETURN_AND_NOTIFY_ON_FAILURE(it != output_picture_buffers_.end(),
      "Invalid picture id: " << picture_buffer_id, INVALID_ARGUMENT,);

  it->second->ReusePictureBuffer();
  ProcessPendingSamples();

  if (state_ == kFlushing && pending_output_samples_.empty())
    FlushInternal();
}

void DXVAVideoDecodeAccelerator::Flush() {
  DCHECK(CalledOnValidThread());

  DVLOG(1) << "DXVAVideoDecodeAccelerator::Flush";

  RETURN_AND_NOTIFY_ON_FAILURE((state_ == kNormal || state_ == kStopped),
      "Unexpected decoder state: " << state_, ILLEGAL_STATE,);

  state_ = kFlushing;

  RETURN_AND_NOTIFY_ON_FAILURE(SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0),
      "Failed to send drain message", PLATFORM_FAILURE,);

  if (!pending_output_samples_.empty())
    return;

  FlushInternal();
}

void DXVAVideoDecodeAccelerator::Reset() {
  DCHECK(CalledOnValidThread());

  DVLOG(1) << "DXVAVideoDecodeAccelerator::Reset";

  RETURN_AND_NOTIFY_ON_FAILURE((state_ == kNormal || state_ == kStopped),
      "Reset: invalid state: " << state_, ILLEGAL_STATE,);

  state_ = kResetting;

  pending_output_samples_.clear();

  NotifyInputBuffersDropped();

  RETURN_AND_NOTIFY_ON_FAILURE(SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH, 0),
      "Reset: Failed to send message.", PLATFORM_FAILURE,);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::NotifyResetDone,
                 weak_this_factory_.GetWeakPtr()));

  state_ = DXVAVideoDecodeAccelerator::kNormal;
}

void DXVAVideoDecodeAccelerator::Destroy() {
  DCHECK(CalledOnValidThread());
  Invalidate();
  delete this;
}

bool DXVAVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

bool DXVAVideoDecodeAccelerator::InitDecoder(media::VideoCodecProfile profile) {
  if (profile < media::H264PROFILE_MIN || profile > media::H264PROFILE_MAX)
    return false;

  // We mimic the steps CoCreateInstance uses to instantiate the object. This
  // was previously done because it failed inside the sandbox, and now is done
  // as a more minimal approach to avoid other side-effects CCI might have (as
  // we are still in a reduced sandbox).
  HMODULE decoder_dll = ::LoadLibrary(L"msmpeg2vdec.dll");
  RETURN_ON_FAILURE(decoder_dll,
                    "msmpeg2vdec.dll required for decoding is not loaded",
                    false);

  typedef HRESULT(WINAPI * GetClassObject)(
      const CLSID & clsid, const IID & iid, void * *object);

  GetClassObject get_class_object = reinterpret_cast<GetClassObject>(
      GetProcAddress(decoder_dll, "DllGetClassObject"));
  RETURN_ON_FAILURE(
      get_class_object, "Failed to get DllGetClassObject pointer", false);

  base::win::ScopedComPtr<IClassFactory> factory;
  HRESULT hr = get_class_object(__uuidof(CMSH264DecoderMFT),
                                __uuidof(IClassFactory),
                                reinterpret_cast<void**>(factory.Receive()));
  RETURN_ON_HR_FAILURE(hr, "DllGetClassObject for decoder failed", false);

  hr = factory->CreateInstance(NULL,
                               __uuidof(IMFTransform),
                               reinterpret_cast<void**>(decoder_.Receive()));
  RETURN_ON_HR_FAILURE(hr, "Failed to create decoder instance", false);

  RETURN_ON_FAILURE(CheckDecoderDxvaSupport(),
                    "Failed to check decoder DXVA support", false);

  hr = decoder_->ProcessMessage(
            MFT_MESSAGE_SET_D3D_MANAGER,
            reinterpret_cast<ULONG_PTR>(device_manager_.get()));
  RETURN_ON_HR_FAILURE(hr, "Failed to pass D3D manager to decoder", false);

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();

  EGLint config_attribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_ALPHA_SIZE, 0,
    EGL_NONE
  };

  EGLint num_configs;

  if (!eglChooseConfig(
      egl_display,
      config_attribs,
      &egl_config_,
      1,
      &num_configs))
    return false;

  return SetDecoderMediaTypes();
}

bool DXVAVideoDecodeAccelerator::CheckDecoderDxvaSupport() {
  base::win::ScopedComPtr<IMFAttributes> attributes;
  HRESULT hr = decoder_->GetAttributes(attributes.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get decoder attributes", false);

  UINT32 dxva = 0;
  hr = attributes->GetUINT32(MF_SA_D3D_AWARE, &dxva);
  RETURN_ON_HR_FAILURE(hr, "Failed to check if decoder supports DXVA", false);

  hr = attributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
  RETURN_ON_HR_FAILURE(hr, "Failed to enable DXVA H/W decoding", false);
  return true;
}

bool DXVAVideoDecodeAccelerator::SetDecoderMediaTypes() {
  RETURN_ON_FAILURE(SetDecoderInputMediaType(),
                    "Failed to set decoder input media type", false);
  return SetDecoderOutputMediaType(MFVideoFormat_NV12);
}

bool DXVAVideoDecodeAccelerator::SetDecoderInputMediaType() {
  base::win::ScopedComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.Receive());
  RETURN_ON_HR_FAILURE(hr, "MFCreateMediaType failed", false);

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Failed to set major input type", false);

  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  RETURN_ON_HR_FAILURE(hr, "Failed to set subtype", false);

  // Not sure about this. msdn recommends setting this value on the input
  // media type.
  hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_MixedInterlaceOrProgressive);
  RETURN_ON_HR_FAILURE(hr, "Failed to set interlace mode", false);

  hr = decoder_->SetInputType(0, media_type, 0);  // No flags
  RETURN_ON_HR_FAILURE(hr, "Failed to set decoder input type", false);
  return true;
}

bool DXVAVideoDecodeAccelerator::SetDecoderOutputMediaType(
    const GUID& subtype) {
  base::win::ScopedComPtr<IMFMediaType> out_media_type;

  for (uint32 i = 0;
       SUCCEEDED(decoder_->GetOutputAvailableType(0, i,
                                                  out_media_type.Receive()));
       ++i) {
    GUID out_subtype = {0};
    HRESULT hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    RETURN_ON_HR_FAILURE(hr, "Failed to get output major type", false);

    if (out_subtype == subtype) {
      hr = decoder_->SetOutputType(0, out_media_type, 0);  // No flags
      RETURN_ON_HR_FAILURE(hr, "Failed to set decoder output type", false);
      return true;
    }
    out_media_type.Release();
  }
  return false;
}

bool DXVAVideoDecodeAccelerator::SendMFTMessage(MFT_MESSAGE_TYPE msg,
                                                int32 param) {
  HRESULT hr = decoder_->ProcessMessage(msg, param);
  return SUCCEEDED(hr);
}

// Gets the minimum buffer sizes for input and output samples. The MFT will not
// allocate buffer for input nor output, so we have to do it ourselves and make
// sure they're the correct size. We only provide decoding if DXVA is enabled.
bool DXVAVideoDecodeAccelerator::GetStreamsInfoAndBufferReqs() {
  HRESULT hr = decoder_->GetInputStreamInfo(0, &input_stream_info_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get input stream info", false);

  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get decoder output stream info", false);

  DVLOG(1) << "Input stream info: ";
  DVLOG(1) << "Max latency: " << input_stream_info_.hnsMaxLatency;
  // There should be three flags, one for requiring a whole frame be in a
  // single sample, one for requiring there be one buffer only in a single
  // sample, and one that specifies a fixed sample size. (as in cbSize)
  CHECK_EQ(input_stream_info_.dwFlags, 0x7u);

  DVLOG(1) << "Min buffer size: " << input_stream_info_.cbSize;
  DVLOG(1) << "Max lookahead: " << input_stream_info_.cbMaxLookahead;
  DVLOG(1) << "Alignment: " << input_stream_info_.cbAlignment;

  DVLOG(1) << "Output stream info: ";
  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  DVLOG(1) << "Flags: "
          << std::hex << std::showbase << output_stream_info_.dwFlags;
  CHECK_EQ(output_stream_info_.dwFlags, 0x107u);
  DVLOG(1) << "Min buffer size: " << output_stream_info_.cbSize;
  DVLOG(1) << "Alignment: " << output_stream_info_.cbAlignment;
  return true;
}

void DXVAVideoDecodeAccelerator::DoDecode() {
  // This function is also called from FlushInternal in a loop which could
  // result in the state transitioning to kStopped due to no decoded output.
  RETURN_AND_NOTIFY_ON_FAILURE((state_ == kNormal || state_ == kFlushing ||
                                state_ == kStopped),
      "DoDecode: not in normal/flushing/stopped state", ILLEGAL_STATE,);

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  DWORD status = 0;

  HRESULT hr = decoder_->ProcessOutput(0,  // No flags
                                       1,  // # of out streams to pull from
                                       &output_data_buffer,
                                       &status);
  IMFCollection* events = output_data_buffer.pEvents;
  if (events != NULL) {
    VLOG(1) << "Got events from ProcessOuput, but discarding";
    events->Release();
  }
  if (FAILED(hr)) {
    // A stream change needs further ProcessInput calls to get back decoder
    // output which is why we need to set the state to stopped.
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      if (!SetDecoderOutputMediaType(MFVideoFormat_NV12)) {
        // Decoder didn't let us set NV12 output format. Not sure as to why
        // this can happen. Give up in disgust.
        NOTREACHED() << "Failed to set decoder output media type to NV12";
        state_ = kStopped;
      } else {
        DVLOG(1) << "Received output format change from the decoder."
                    " Recursively invoking DoDecode";
        DoDecode();
      }
      return;
    } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      // No more output from the decoder. Stop playback.
      state_ = kStopped;
      return;
    } else {
      NOTREACHED() << "Unhandled error in DoDecode()";
      return;
    }
  }
  TRACE_EVENT_END_ETW("DXVAVideoDecodeAccelerator.Decoding", this, "");

  TRACE_COUNTER1("DXVA Decoding", "TotalPacketsBeforeDecode",
                 inputs_before_decode_);

  inputs_before_decode_ = 0;

  RETURN_AND_NOTIFY_ON_FAILURE(ProcessOutputSample(output_data_buffer.pSample),
      "Failed to process output sample.", PLATFORM_FAILURE,);
}

bool DXVAVideoDecodeAccelerator::ProcessOutputSample(IMFSample* sample) {
  RETURN_ON_FAILURE(sample, "Decode succeeded with NULL output sample", false);

  base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = sample->GetBufferByIndex(0, output_buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  base::win::ScopedComPtr<IDirect3DSurface9> surface;
  hr = MFGetService(output_buffer, MR_BUFFER_SERVICE,
                    IID_PPV_ARGS(surface.Receive()));
  RETURN_ON_HR_FAILURE(hr, "Failed to get D3D surface from output sample",
                       false);

  LONGLONG input_buffer_id = 0;
  RETURN_ON_HR_FAILURE(sample->GetSampleTime(&input_buffer_id),
                       "Failed to get input buffer id associated with sample",
                       false);

  pending_output_samples_.push_back(
      PendingSampleInfo(input_buffer_id, sample));

  // If we have available picture buffers to copy the output data then use the
  // first one and then flag it as not being available for use.
  if (output_picture_buffers_.size()) {
    ProcessPendingSamples();
    return true;
  }
  if (pictures_requested_) {
    DVLOG(1) << "Waiting for picture slots from the client.";
    return true;
  }

  // We only read the surface description, which contains its width/height when
  // we need the picture buffers from the client. Once we have those, then they
  // are reused.
  D3DSURFACE_DESC surface_desc;
  hr = surface->GetDesc(&surface_desc);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);

  // Go ahead and request picture buffers.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 surface_desc.Width,
                 surface_desc.Height));

  pictures_requested_ = true;
  return true;
}

void DXVAVideoDecodeAccelerator::ProcessPendingSamples() {
  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_.Run(),
      "Failed to make context current", PLATFORM_FAILURE,);

  OutputBuffers::iterator index;

  for (index = output_picture_buffers_.begin();
       index != output_picture_buffers_.end() &&
       !pending_output_samples_.empty();
       ++index) {
    if (index->second->available()) {
      PendingSampleInfo sample_info = pending_output_samples_.front();

      base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
      HRESULT hr = sample_info.output_sample->GetBufferByIndex(
          0, output_buffer.Receive());
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get buffer from output sample", PLATFORM_FAILURE,);

      base::win::ScopedComPtr<IDirect3DSurface9> surface;
      hr = MFGetService(output_buffer, MR_BUFFER_SERVICE,
                        IID_PPV_ARGS(surface.Receive()));
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get D3D surface from output sample",
          PLATFORM_FAILURE,);

      D3DSURFACE_DESC surface_desc;
      hr = surface->GetDesc(&surface_desc);
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get surface description", PLATFORM_FAILURE,);

      if (surface_desc.Width !=
              static_cast<uint32>(index->second->size().width()) ||
          surface_desc.Height !=
              static_cast<uint32>(index->second->size().height())) {
        HandleResolutionChanged(surface_desc.Width, surface_desc.Height);
        return;
      }

      RETURN_AND_NOTIFY_ON_FAILURE(
          index->second->CopyOutputSampleDataToPictureBuffer(*this, surface),
          "Failed to copy output sample",
          PLATFORM_FAILURE, );

      media::Picture output_picture(index->second->id(),
                                    sample_info.input_buffer_id);
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&DXVAVideoDecodeAccelerator::NotifyPictureReady,
                     weak_this_factory_.GetWeakPtr(),
                     output_picture));

      index->second->set_available(false);
      pending_output_samples_.pop_front();
    }
  }

  if (!pending_input_buffers_.empty() && pending_output_samples_.empty()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                   weak_this_factory_.GetWeakPtr()));
  }
}

void DXVAVideoDecodeAccelerator::StopOnError(
  media::VideoDecodeAccelerator::Error error) {
  DCHECK(CalledOnValidThread());

  if (client_)
    client_->NotifyError(error);
  client_ = NULL;

  if (state_ != kUninitialized) {
    Invalidate();
  }
}

void DXVAVideoDecodeAccelerator::Invalidate() {
  if (state_ == kUninitialized)
    return;
  weak_this_factory_.InvalidateWeakPtrs();
  output_picture_buffers_.clear();
  pending_output_samples_.clear();
  pending_input_buffers_.clear();
  decoder_.Release();
  MFShutdown();
  state_ = kUninitialized;
}

void DXVAVideoDecodeAccelerator::NotifyInputBufferRead(int input_buffer_id) {
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void DXVAVideoDecodeAccelerator::NotifyFlushDone() {
  if (client_)
    client_->NotifyFlushDone();
}

void DXVAVideoDecodeAccelerator::NotifyResetDone() {
  if (client_)
    client_->NotifyResetDone();
}

void DXVAVideoDecodeAccelerator::RequestPictureBuffers(int width, int height) {
  // This task could execute after the decoder has been torn down.
  if (state_ != kUninitialized && client_) {
    client_->ProvidePictureBuffers(
        kNumPictureBuffers,
        gfx::Size(width, height),
        GL_TEXTURE_2D);
  }
}

void DXVAVideoDecodeAccelerator::NotifyPictureReady(
    const media::Picture& picture) {
  // This task could execute after the decoder has been torn down.
  if (state_ != kUninitialized && client_)
    client_->PictureReady(picture);
}

void DXVAVideoDecodeAccelerator::NotifyInputBuffersDropped() {
  if (!client_ || !pending_output_samples_.empty())
    return;

  for (PendingInputs::iterator it = pending_input_buffers_.begin();
       it != pending_input_buffers_.end(); ++it) {
    LONGLONG input_buffer_id = 0;
    RETURN_ON_HR_FAILURE((*it)->GetSampleTime(&input_buffer_id),
                         "Failed to get buffer id associated with sample",);
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
  }
  pending_input_buffers_.clear();
}

void DXVAVideoDecodeAccelerator::DecodePendingInputBuffers() {
  RETURN_AND_NOTIFY_ON_FAILURE((state_ != kUninitialized),
      "Invalid state: " << state_, ILLEGAL_STATE,);

  if (pending_input_buffers_.empty() || !pending_output_samples_.empty())
    return;

  PendingInputs pending_input_buffers_copy;
  std::swap(pending_input_buffers_, pending_input_buffers_copy);

  for (PendingInputs::iterator it = pending_input_buffers_copy.begin();
       it != pending_input_buffers_copy.end(); ++it) {
    DecodeInternal(*it);
  }
}

void DXVAVideoDecodeAccelerator::FlushInternal() {
  // The DoDecode function sets the state to kStopped when the decoder returns
  // MF_E_TRANSFORM_NEED_MORE_INPUT.
  // The MFT decoder can buffer upto 30 frames worth of input before returning
  // an output frame. This loop here attempts to retrieve as many output frames
  // as possible from the buffered set.
  while (state_ != kStopped) {
    DoDecode();
    if (!pending_output_samples_.empty())
      return;
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::NotifyFlushDone,
                 weak_this_factory_.GetWeakPtr()));

  state_ = kNormal;
}

void DXVAVideoDecodeAccelerator::DecodeInternal(
    const base::win::ScopedComPtr<IMFSample>& sample) {
  DCHECK(CalledOnValidThread());

  if (state_ == kUninitialized)
    return;

  if (!pending_output_samples_.empty() || !pending_input_buffers_.empty()) {
    pending_input_buffers_.push_back(sample);
    return;
  }

  if (!inputs_before_decode_) {
    TRACE_EVENT_BEGIN_ETW("DXVAVideoDecodeAccelerator.Decoding", this, "");
  }
  inputs_before_decode_++;

  HRESULT hr = decoder_->ProcessInput(0, sample, 0);
  // As per msdn if the decoder returns MF_E_NOTACCEPTING then it means that it
  // has enough data to produce one or more output samples. In this case the
  // recommended options are to
  // 1. Generate new output by calling IMFTransform::ProcessOutput until it
  //    returns MF_E_TRANSFORM_NEED_MORE_INPUT.
  // 2. Flush the input data
  // We implement the first option, i.e to retrieve the output sample and then
  // process the input again. Failure in either of these steps is treated as a
  // decoder failure.
  if (hr == MF_E_NOTACCEPTING) {
    DoDecode();
    RETURN_AND_NOTIFY_ON_FAILURE((state_ == kStopped || state_ == kNormal),
        "Failed to process output. Unexpected decoder state: " << state_,
        PLATFORM_FAILURE,);
    hr = decoder_->ProcessInput(0, sample, 0);
    // If we continue to get the MF_E_NOTACCEPTING error we do the following:-
    // 1. Add the input sample to the pending queue.
    // 2. If we don't have any output samples we post the
    //    DecodePendingInputBuffers task to process the pending input samples.
    //    If we have an output sample then the above task is posted when the
    //    output samples are sent to the client.
    // This is because we only support 1 pending output sample at any
    // given time due to the limitation with the Microsoft media foundation
    // decoder where it recycles the output Decoder surfaces.
    if (hr == MF_E_NOTACCEPTING) {
      pending_input_buffers_.push_back(sample);
      if (pending_output_samples_.empty()) {
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                       weak_this_factory_.GetWeakPtr()));
      }
      return;
    }
  }
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to process input sample",
      PLATFORM_FAILURE,);

  DoDecode();

  RETURN_AND_NOTIFY_ON_FAILURE((state_ == kStopped || state_ == kNormal),
      "Failed to process output. Unexpected decoder state: " << state_,
      ILLEGAL_STATE,);

  LONGLONG input_buffer_id = 0;
  RETURN_ON_HR_FAILURE(sample->GetSampleTime(&input_buffer_id),
                       "Failed to get input buffer id associated with sample",);
  // The Microsoft Media foundation decoder internally buffers up to 30 frames
  // before returning a decoded frame. We need to inform the client that this
  // input buffer is processed as it may stop sending us further input.
  // Note: This may break clients which expect every input buffer to be
  // associated with a decoded output buffer.
  // TODO(ananta)
  // Do some more investigation into whether it is possible to get the MFT
  // decoder to emit an output packet for every input packet.
  // http://code.google.com/p/chromium/issues/detail?id=108121
  // http://code.google.com/p/chromium/issues/detail?id=150925
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::NotifyInputBufferRead,
                 weak_this_factory_.GetWeakPtr(),
                 input_buffer_id));
}

void DXVAVideoDecodeAccelerator::HandleResolutionChanged(int width,
                                                         int height) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::DismissStaleBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 output_picture_buffers_));

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 width,
                 height));

  output_picture_buffers_.clear();
}

void DXVAVideoDecodeAccelerator::DismissStaleBuffers(
    const OutputBuffers& picture_buffers) {
  OutputBuffers::const_iterator index;

  for (index = picture_buffers.begin();
       index != picture_buffers.end();
       ++index) {
    DVLOG(1) << "Dismissing picture id: " << index->second->id();
    client_->DismissPictureBuffer(index->second->id());
  }
}

}  // namespace content
