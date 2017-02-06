// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Determines whether a certain driver bug exists in the current system.
// The format of a valid gpu_driver_bug_list.json file is defined in
// <gpu/config/gpu_control_list_format.txt>.
// The supported "features" can be found in
// <gpu/config/gpu_driver_bug_workaround_type.h>.

#include "gpu/config/gpu_control_list_jsons.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace gpu {

const char kGpuDriverBugListJson[] = LONG_STRING_CONST(

{
  "name": "gpu driver bug list",
  // Please update the version number whenever you change this file.
  "version": "8.93",
  "entries": [
    {
      "id": 1,
      "description": "Imagination driver doesn't like uploading lots of buffer data constantly",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "features": [
        "use_client_side_arrays_for_stream_buffers"
      ]
    },
    {
      "id": 2,
      "description": "ARM driver doesn't like uploading lots of buffer data constantly",
      "os": {
        "type": "android"
      },
      "gl_vendor": "ARM.*",
      "features": [
        "use_client_side_arrays_for_stream_buffers"
      ]
    },
    {
      "id": 5,
      "description": "Always call glUseProgram after a successful link to avoid a driver bug",
      "cr_bugs": [349137],
      "vendor_id": "0x10de",
      "exceptions": [
        {
          "os": {
            "type": "macosx",
            "version": {
              "op": ">=",
              "value": "10.9"
            }
          }
        }
      ],
      "features": [
        "use_current_program_after_successful_link"
      ]
    },
    {
      "id": 6,
      "description": "Restore scissor on FBO change with Qualcomm GPUs on older versions of Android",
      "cr_bugs": [165493, 222018],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "restore_scissor_on_fbo_change"
      ]
    },
    {
      "id": 7,
      "cr_bugs": [89557],
      "description": "Work around a bug in offscreen buffers on NVIDIA GPUs on Macs",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "features": [
        "needs_offscreen_buffer_workaround"
      ]
    },
    {
      "id": 16,
      "description": "EXT_occlusion_query appears to be buggy with Intel GPUs on Linux",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x8086",
      "disabled_extensions": [
        "GL_ARB_occlusion_query2",
        "GL_ARB_occlusion_query"
      ]
    },
    {
      "id": 17,
      "description": "Some drivers are unable to reset the D3D device in the GPU process sandbox",
      "os": {
        "type": "win"
      },
      "features": [
        "exit_on_context_lost"
      ]
    },
    {
      "id": 19,
      "description": "Disable depth textures on Android with Qualcomm GPUs",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "disable_depth_texture"
      ],
      "disabled_extensions": [
        "GL_OES_depth_texture"
      ]
    },
    {
      "id": 20,
      "description": "Disable EXT_draw_buffers on GeForce GT 650M on Mac OS X due to driver bugs",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0fd5"],
      "multi_gpu_category": "any",
      "features": [
        "disable_ext_draw_buffers"
      ]
    },
    {
      "id": 21,
      "description": "Vivante GPUs are buggy with context switching",
      "cr_bugs": [179250, 235935],
      "os": {
        "type": "android"
      },
      "gl_extensions": ".*GL_VIV_shader_binary.*",
      "features": [
        "unbind_fbo_on_context_switch"
      ]
    },
    {
      "id": 22,
      "description": "Imagination drivers are buggy with context switching",
      "cr_bugs": [230896],
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "features": [
        "unbind_fbo_on_context_switch"
      ]
    },
    {
      "id": 23,
      "cr_bugs": [243038],
      "description": "Disable OES_standard_derivative on Intel Pineview M Gallium drivers",
      "os": {
        "type": "chromeos"
      },
      "vendor_id": "0x8086",
      "device_id": ["0xa011", "0xa012"],
      "disabled_extensions": [
        "GL_OES_standard_derivatives"
      ]
    },
    {
      "id": 24,
      "cr_bugs": [231082],
      "description": "Mali-4xx drivers throw an error when a buffer object's size is set to 0",
      "os": {
        "type": "android"
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "use_non_zero_size_for_client_side_stream_buffers"
      ]
    },
    {
      "id": 26,
      "description": "Disable use of Direct3D 11 on Windows Vista and lower",
      "os": {
        "type": "win",
        "version": {
          "op": "<=",
          "value": "6.0"
        }
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 27,
      "cr_bugs": [265115],
      "description": "Async Readpixels with GL_BGRA format is broken on Haswell chipset on Macs",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0402", "0x0406", "0x040a", "0x0412", "0x0416", "0x041a",
                    "0x0a04", "0x0a16", "0x0a22", "0x0a26", "0x0a2a"],
      "features": [
        "swizzle_rgba_for_async_readpixels"
      ]
    },
    {
      "id": 30,
      "cr_bugs": [237931],
      "description": "Multisampling is buggy on OSX when multiple monitors are connected",
      "os": {
        "type": "macosx"
      },
      "features": [
        "disable_multimonitor_multisampling"
      ]
    },
    {
      "id": 31,
      "cr_bugs": [154715, 10068, 269829, 294779, 285292],
      "description": "The Mali-Txxx driver does not guarantee flush ordering",
      "gl_vendor": "ARM.*",
      "gl_renderer": "Mali-T.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 32,
      "cr_bugs": [179815],
      "description": "Share groups are not working on (older?) Broadcom drivers",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Broadcom.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 33,
      "description": "Share group-related crashes and poor context switching perf on Imagination drivers",
      "gl_vendor": "Imagination.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 34,
      "cr_bugs": [179250, 229643, 230896],
      "description": "Share groups are not working on (older?) Vivante drivers",
      "os": {
        "type": "android"
      },
      "gl_extensions": ".*GL_VIV_shader_binary.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 35,
      "cr_bugs": [163464],
      "description": "Share-group related crashes on older NVIDIA drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": "NVIDIA.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 36,
      "cr_bugs": [163464, 233612],
      "description": "Share-group related crashes on Qualcomm drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 37,
      "cr_bugs": [286468],
      "description": "Program link fails in NVIDIA Linux if gl_Position is not set",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "features": [
        "init_gl_position_in_vertex_shader"
      ]
    },
    {
      "id": 38,
      "cr_bugs": [289461],
      "description": "Non-virtual contexts on Qualcomm sometimes cause out-of-order frames",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 39,
      "cr_bugs": [290391],
      "description": "Multisampled renderbuffer allocation must be validated on some Macs",
      "os": {
        "type": "macosx"
      },
      "features": [
        "validate_multisample_buffer_allocation"
      ]
    },
    {
      "id": 40,
      "cr_bugs": [290876],
      "description": "Framebuffer discarding causes flickering on old ARM drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.4"
        }
      },
      "gl_vendor": "ARM.*",
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 42,
      "cr_bugs": [290876, 488463],
      "description": "Framebuffer discarding causes flickering on older IMG drivers",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR SGX 5.*",
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 43,
      "cr_bugs": [299494],
      "description": "Framebuffer discarding doesn't accept trivial attachments on Vivante",
      "os": {
        "type": "android"
      },
      "gl_extensions": ".*GL_VIV_shader_binary.*",
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 44,
      "cr_bugs": [301988],
      "description": "Framebuffer discarding causes jumpy scrolling on Mali drivers",
      "os": {
        "type": "chromeos"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 45,
      "cr_bugs": [307751],
      "description": "Unfold short circuit on Mac OS X",
      "os": {
        "type": "macosx"
      },
      "features": [
        "unfold_short_circuit_as_ternary_operation"
      ]
    },
    {
      "id": 48,
      "description": "Force to use discrete GPU on older MacBookPro models",
      "cr_bugs": [113703],
      "os": {
        "type": "macosx"
      },
      "machine_model_name": ["MacBookPro"],
      "machine_model_version": {
        "op": "<",
        "value": "8"
      },
      "gpu_count": {
        "op": "=",
        "value": "2"
      },
      "features": [
        "force_discrete_gpu"
      ]
    },
    {
      "id": 49,
      "cr_bugs": [309734],
      "description": "The first draw operation from an idle state is slow",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "wake_up_gpu_before_drawing"
      ]
    },
    {
      "id": 51,
      "description": "TexSubImage is faster for full uploads on ANGLE",
      "os": {
        "type": "win"
      },
      "gl_renderer": "ANGLE.*",
      "features": [
        "texsubimage_faster_than_teximage"
      ]
    },
    {
      "id": 52,
      "description": "ES3 MSAA is broken on Qualcomm",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "disable_chromium_framebuffer_multisample"
      ]
    },
    {
      "id": 54,
      "cr_bugs": [124764, 349137],
      "description": "Clear uniforms before first program use on all platforms",
      "exceptions": [
        {
          "os": {
            "type": "macosx"
          }
        }
      ],
      "features": [
        "clear_uniforms_before_first_program_use"
      ]
    },
    {
      "id": 55,
      "cr_bugs": [333885],
      "description": "Mesa drivers in Linux handle varyings without static use incorrectly",
      "os": {
        "type": "linux"
      },
      "driver_vendor": "Mesa",
      "features": [
        "count_all_in_varyings_packing"
      ]
    },
    {
      "id": 56,
      "cr_bugs": [333885],
      "description": "Mesa drivers in ChromeOS handle varyings without static use incorrectly",
      "os": {
        "type": "chromeos"
      },
      "driver_vendor": "Mesa",
      "features": [
        "count_all_in_varyings_packing"
      ]
    },
    {
      "id": 57,
      "cr_bugs": [322760],
      "description": "Mac drivers handle varyings without static use incorrectly",
      "os": {
        "type": "macosx"
      },
      "features": [
        "init_varyings_without_static_use"
      ]
    },
    {
      "id": 59,
      "description": "Multisampling is buggy in Intel IvyBridge",
      "cr_bugs": [116370],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0152", "0x0156", "0x015a", "0x0162", "0x0166"],
      "features": [
        "disable_chromium_framebuffer_multisample"
      ]
    },
    {
      "id": 64,
      "description": "Linux AMD drivers incorrectly return initial value of 1 for TEXTURE_MAX_ANISOTROPY",
      "cr_bugs": [348237],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x1002",
      "features": [
        "init_texture_max_anisotropy"
      ]
    },
    {
      "id": 65,
      "description": "Linux NVIDIA drivers don't have the correct defaults for vertex attributes",
      "cr_bugs": [351528],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "features": [
        "init_vertex_attributes"
      ]
    },
    {
      "id": 68,
      "description": "Disable partial swaps on linux drivers",
      "cr_bugs": [339493],
      "os": {
        "type": "linux"
      },
      "gl_type": "gl",
      "driver_vendor": "Mesa",
      "features": [
        "disable_post_sub_buffers_for_onscreen_surfaces"
      ]
    },
    {
      "id": 69,
      "description": "Some shaders in Skia need more than the min available vertex and fragment shader uniform vectors in case of OSMesa",
      "cr_bugs": [174845],
      "driver_vendor": "osmesa",
      "features": [
       "max_fragment_uniform_vectors_32",
       "max_varying_vectors_16",
       "max_vertex_uniform_vectors_256"
      ]
    },
    {
      "id": 70,
      "description": "Disable D3D11 on older nVidia drivers",
      "cr_bugs": [349929],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "driver_version": {
        "op": "<=",
        "value": "8.17.12.6973"
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 71,
      "description": "Vivante's support of OES_standard_derivatives is buggy",
      "cr_bugs": [368005],
      "os": {
        "type": "android"
      },
      "gl_extensions": ".*GL_VIV_shader_binary.*",
      "disabled_extensions": [
        "GL_OES_standard_derivatives"
      ]
    },
    {
      "id": 72,
      "description": "Use virtual contexts on NVIDIA with GLES 3.1",
      "cr_bugs": [369316],
      "os": {
        "type": "android"
      },
      "gl_type": "gles",
      "gl_version": {
        "op": "=",
        "value": "3.1"
      },
      "gl_vendor": "NVIDIA.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
)  // LONG_STRING_CONST macro
// Avoid C2026 (string too big) error on VisualStudio.
LONG_STRING_CONST(
    {
      "id": 74,
      "cr_bugs": [278606, 382686],
      "description": "Testing EGL sync fences was broken on most Qualcomm drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "disabled_extensions": [
        "EGL_KHR_fence_sync"
      ]
    },
    {
      "id": 75,
      "description": "Mali-4xx support of EXT_multisampled_render_to_texture is buggy on Android < 4.3",
      "cr_bugs": [362435],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "disable_multisampled_render_to_texture"
      ]
    },
    {
      "id": 76,
      "cr_bugs": [371530],
      "description": "Testing EGL sync fences was broken on IMG",
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_vendor": "Imagination Technologies.*",
      "disabled_extensions": [
        "EGL_KHR_fence_sync"
      ]
    },
    {
      "id": 77,
      "cr_bugs": [378691, 373360, 371530, 398964],
      "description": "Testing fences was broken on Mali ES2 drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": "Mali.*",
      "gl_type": "gles",
      "gl_version": {
        "op": "<",
        "value": "3.0"
      },
      "disabled_extensions": [
        "EGL_KHR_fence_sync"
      ]
    },
    {
      "id": 78,
      "cr_bugs": [378691, 373360, 371530],
      "description": "Testing fences was broken on Broadcom drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_vendor": "Broadcom.*",
      "disabled_extensions": [
        "EGL_KHR_fence_sync"
      ]
    },
    {
      "id": 82,
      "description": "PBO mappings segfault on certain older Qualcomm drivers",
      "cr_bugs": [394510],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "disable_async_readpixels"
      ]
    },
    {
      "id": 86,
      "description": "Disable use of Direct3D 11 on Matrox video cards",
      "cr_bugs": [395861],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x102b",
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 87,
      "description": "Disable use of Direct3D 11 on older AMD drivers",
      "cr_bugs": [402134],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1002",
      "driver_date": {
        "op": "<",
        "value": "2011.1"
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 88,
      "description": "Always rewrite vec/mat constructors to be consistent",
      "cr_bugs": [398694],
      "features": [
        "scalarize_vec_and_mat_constructor_args"
      ]
    },
    {
      "id": 89,
      "description": "Mac drivers handle struct scopes incorrectly",
      "cr_bugs": [403957],
      "os": {
        "type": "macosx"
      },
      "features": [
        "regenerate_struct_names"
      ]
    },
    {
      "id": 90,
      "description": "Linux AMD drivers handle struct scopes incorrectly",
      "cr_bugs": [403957],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x1002",
      "features": [
        "regenerate_struct_names"
      ]
    },
    {
      "id": 91,
      "cr_bugs": [150500, 414816],
      "description": "ETC1 non-power-of-two sized textures crash older IMG drivers",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR SGX 5.*",
      "features": [
        "etc1_power_of_two_only"
      ]
    },
    {
      "id": 92,
      "description": "Old Intel drivers cannot reliably support D3D11",
      "cr_bugs": [363721],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "driver_version": {
        "op": "<",
        "value": "8.16"
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 93,
      "description": "The GL implementation on the Android emulator has problems with PBOs.",
      "cr_bugs": [340882],
      "os": {
        "type": "android"
      },
      "gl_vendor": "VMware.*",
      "gl_renderer": "Gallium.*",
      "gl_type": "gles",
      "gl_version": {
        "op": "=",
        "value": "3.0"
      },
      "features": [
        "disable_async_readpixels"
      ]
    },
    {
      "id": 94,
      "description": "Disable EGL_KHR_wait_sync on NVIDIA with GLES 3.1",
      "cr_bugs": [433057],
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "5.0.2"
        }
      },
      "gl_vendor": "NVIDIA.*",
      "gl_type": "gles",
      "gl_version": {
        "op": "=",
        "value": "3.1"
      },
      "disabled_extensions": [
        "EGL_KHR_wait_sync"
      ]
    },
    {
      "id": 95,
      "cr_bugs": [421271],
      "description": "glClear does not always work on these drivers",
      "os": {
        "type": "android"
      },
      "gl_type": "gles",
      "gl_version": {
        "op": "<",
        "value": "3.0"
      },
      "gl_vendor": "Imagination.*",
      "features": [
        "gl_clear_broken"
      ]
    },
    {
      "id": 97,
      "description": "Multisampling has poor performance in Intel BayTrail",
      "cr_bugs": [443517],
      "os": {
        "type": "android"
      },
      "gl_vendor": "Intel",
      "gl_renderer": "Intel.*BayTrail",
      "features": [
        "disable_chromium_framebuffer_multisample"
      ]
    },
    {
      "id": 98,
      "description": "PowerVR SGX 540 drivers throw GL_OUT_OF_MEMORY error when a buffer object's size is set to 0",
      "cr_bugs": [451501],
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR SGX 540",
      "features": [
        "use_non_zero_size_for_client_side_stream_buffers"
      ]
    },
    {
      "id": 99,
      "description": "Qualcomm driver before Lollipop deletes egl sync objects after context destruction",
      "cr_bugs": [453857],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "5.0.0"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "features": [
        "ignore_egl_sync_failures"
      ]
    },
    {
      "id": 100,
      "description": "Disable Direct3D11 on systems with AMD switchable graphics",
      "cr_bugs": [451420],
      "os": {
        "type": "win"
      },
      "multi_gpu_style": "amd_switchable",
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 101,
      "description": "The Mali-Txxx driver hangs when reading from currently displayed buffer",
      "cr_bugs": [457511],
      "os": {
        "type": "chromeos"
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": "Mali-T.*",
      "features": [
        "disable_post_sub_buffers_for_onscreen_surfaces"
      ]
    },
    {
      "id": 102,
      "description": "Adreno 420 driver loses FBO attachment contents on bound FBO deletion",
      "cr_bugs": [457027],
      "os": {
        "type": "android",
        "version": {
          "op": ">",
          "value": "5.0.2"
        }
      },
      "gl_renderer": "Adreno \\(TM\\) 4.*",
      "features": [
        "unbind_attachments_on_bound_render_fbo_delete"
      ]
    },
    {
      "id": 103,
      "description": "Adreno 420 driver drops draw calls after FBO invalidation",
      "cr_bugs": [443060],
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno \\(TM\\) 4.*",
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 104,
      "description": "EXT_occlusion_query_boolean hangs on MediaTek MT8135 pre-Lollipop",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "5.0.0"
        }
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR Rogue Han",
      "disabled_extensions": [
        "GL_EXT_occlusion_query_boolean"
      ]
    },
    {
      "id": 105,
      "cr_bugs": [449488,451230],
      "description": "Framebuffer discarding causes corruption on Mali-4xx",
      "gl_renderer": "Mali-4.*",
      "os": {
        "type": "android"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 106,
      "description": "EXT_occlusion_query_boolean hangs on PowerVR SGX 544 (IMG) drivers",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR SGX 544",
      "disabled_extensions": [
        "GL_EXT_occlusion_query_boolean"
      ]
    },
    {
      "id": 107,
      "description": "Workaround IMG PowerVR G6xxx drivers bugs",
      "cr_bugs": [480992],
      "os": {
        "type": "android",
        "version": {
          "op": "between",
          "value": "5.0.0",
          "value2": "5.1.99"
        }
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR Rogue.*",
      "driver_version": {
        "op": "between",
        "value": "1.3",
        "value2": "1.4"
      },
      "features": [
        "avoid_egl_image_target_texture_reuse"
      ],
      "disabled_extensions": [
        "EGL_KHR_wait_sync"
      ]
    },
    {
      "id": 108,
      "cr_bugs": [449150],
      "description": "Mali-4xx does not support GL_RGB format",
      "gl_vendor": "ARM.*",
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "disable_gl_rgb_format"
      ]
    },
    {
      "id": 109,
      "cr_bugs": [449150, 514510],
      "description": "MakeCurrent is slow on Linux with NVIDIA drivers",
      "vendor_id": "0x10de",
      "os": {
        "type": "linux"
      },
      "gl_vendor": "NVIDIA.*",
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 110,
      "description": "EGL Sync server causes crashes on Adreno 2xx and 3xx drivers",
      "cr_bugs": [482298],
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "gl_renderer": "Adreno \\(TM\\) [23].*",
      "driver_version": {
        "op": "<",
        "value": "95"
      },
      "disabled_extensions": [
        "EGL_KHR_wait_sync"
      ]
    },
    {
      "id": 111,
      "description": "Discard Framebuffer breaks WebGL on Mali-4xx Linux",
      "cr_bugs": [485814],
      "os": {
        "type": "linux"
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 112,
      "cr_bugs": [477514],
      "description": "EXT_disjoint_timer_query fails after 2 queries on adreno 3xx in lollypop",
      "os": {
        "type": "android",
        "version": {
          "op": ">=",
          "value": "5.0.0"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "gl_renderer": "Adreno \\(TM\\) 3.*",
      "features": [
        "disable_timestamp_queries"
      ]
    },
    {
      "id": 113,
      "cr_bugs": [477514],
      "description": "EXT_disjoint_timer_query fails after 256 queries on adreno 4xx",
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno \\(TM\\) 4.*",
      "disabled_extensions": [
        "GL_EXT_disjoint_timer_query"
      ]
    },
    {
      "id": 115,
      "cr_bugs": [462553],
      "description": "glGetIntegerv with GL_GPU_DISJOINT_EXT causes GL_INVALID_ENUM error",
      "os": {
        "type": "android"
      },
      "gl_vendor": "NVIDIA.*",
      "gl_type": "gles",
      "gl_version": {
        "op": ">=",
        "value": "3.0"
      },
      "disabled_extensions": [
        "GL_EXT_disjoint_timer_query"
      ]
    },
    {
      "id": 116,
      "description": "Adreno 420 support for EXT_multisampled_render_to_texture is buggy on Android < 5.1",
      "cr_bugs": [490379],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "5.1"
        }
      },
      "gl_renderer": "Adreno \\(TM\\) 4.*",
      "features": [
        "disable_multisampled_render_to_texture"
      ]
    },
    {
      "id": 117,
      "description": "GL_KHR_blend_equation_advanced breaks blending on Adreno 4xx",
      "cr_bugs": [488485],
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "gl_renderer": ".*4\\d\\d",
      "features": [
        "disable_blend_equation_advanced"
      ]
    },
    {
      "id": 118,
      "cr_bugs": [477306],
      "description": "NVIDIA 331 series drivers shader compiler may crash when attempting to optimize pow()",
      "os": {
        "type": "linux"
      },
      "driver_version": {
        "op": "<=",
        "value": "331"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "features": [
        "remove_pow_with_constant_exponent"
      ]
    },
    {
      "id": 119,
      "description": "Context lost recovery often fails on Mali-400/450 on Android.",
      "cr_bugs": [496438],
      "os": {
        "type": "android"
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "exit_on_context_lost"
      ]
    },
    {
      "id": 120,
      "description": "CHROMIUM_copy_texture is slow on Mali pre-Lollipop",
      "cr_bugs": [498443],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "5.0.0"
        }
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": "Mali.*",
      "features": [
        "max_copy_texture_chromium_size_262144"
      ]
    },
    {
      "id": 123,
      "cr_bugs": [344330],
      "description": "NVIDIA drivers before 346 lack features in NV_path_rendering and related extensions to implement driver level path rendering.",
      "vendor_id": "0x10de",
      "os": {
        "type": "linux"
      },
      "driver_version": {
        "op": "<",
        "value": "346"
      },
      "disabled_extensions": ["GL_NV_path_rendering"]
    },
    {
      "id": 124,
      "description": "Certain Adreno 4xx and 5xx drivers often crash in glProgramBinary.",
      "cr_bugs": [486117, 598060],
      "os": {
        "type": "android"
      },
      "driver_version": {
        "op": ">=",
        "value": "103.0"
      },
      "gl_renderer": "Adreno \\(TM\\) [45].*",
      "features": [
        "disable_program_cache"
      ]
    },
    {
      "id": 125,
      "description": "glFinish doesn't clear caches on Android",
      "cr_bugs": [509727],
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno.*",
      "features": [
        "unbind_egl_context_to_flush_driver_caches"
      ]
    },
    {
      "id": 126,
      "description": "Program binaries contain incorrect bound attribute locations on Adreno 3xx GPUs",
      "cr_bugs": [510637],
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno \\(TM\\) 3.*",
      "features": [
        "disable_program_cache"
      ]
    },
    {
      "id": 127,
      "description": "Android Adreno crashes on binding incomplete cube map texture to FBO",
      "cr_bugs": [518889],
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno.*",
      "features": [
        "force_cube_map_positive_x_allocation"
      ]
    },
    {
      "id": 128,
      "description": "Linux ATI drivers crash on binding incomplete cube map texture to FBO",
      "cr_bugs": [518889],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x1002",
      "features": [
        "force_cube_map_positive_x_allocation"
      ]
    },
    {
      "id": 129,
      // TODO(dshwang): Fix ANGLE crash. crbug.com/518889
      "description": "ANGLE crash on glReadPixels from incomplete cube map texture",
      "cr_bugs": [518889],
      "os": {
        "type": "win"
      },
      "gl_renderer": "ANGLE.*",
      "features": [
        "force_cube_complete"
      ]
    },
    {
      "id": 130,
      "description": "NVIDIA fails glReadPixels from incomplete cube map texture",
      "cr_bugs": [518889],
      "vendor_id": "0x10de",
      "os": {
        "type": "linux"
      },
      "gl_vendor": "NVIDIA.*",
      "features": [
        "force_cube_complete"
      ]
    },
    {
      "id": 131,
      "description": "Linux Mesa drivers crash on glTexSubImage2D() to texture storage bound to FBO",
      "cr_bugs": [521904],
      "os": {
        "type": "linux"
      },
      "driver_vendor": "Mesa",
      "driver_version": {
        "op": "<",
        "value": "10.6"
      },
      "features": [
        "disable_texture_storage"
      ]
    },
    {
      "id": 132,
      "description": "On Intel GPUs MSAA performance is not acceptable for GPU rasterization",
      "cr_bugs": [527565],
      "vendor_id": "0x8086",
      "multi_gpu_category": "active",
      "features": [
        "msaa_is_slow"
      ]
    },
    {
      "id": 133,
      "description": "CHROMIUM_copy_texture with 1MB copy per flush to avoid unwanted cache growth on Adreno",
      "cr_bugs": [542478],
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno.*",
      "features": [
        "max_copy_texture_chromium_size_1048576"
      ]
    },
    {
      "id": 134,
      "description": "glReadPixels fails on FBOs with SRGB_ALPHA textures",
      "cr_bugs": [550292],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "5.0"
        }
      },
      "gl_vendor": "Qualcomm.*",
      "disabled_extensions": ["GL_EXT_sRGB"]
    },
    {
      "id": 135,
      "description": "Screen flickers on 2009 iMacs",
      "cr_bugs": [543324],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "device_id": ["0x9440", "0x944a", "0x9488", "0x9490"],
      "features": [
        "disable_overlay_ca_layers",
        "disable_post_sub_buffers_for_onscreen_surfaces"
      ]
    },
    {
      "id": 136,
      "description": "glGenerateMipmap fails if the zero texture level is not set on some Mac drivers",
      "cr_bugs": [560499],
      "os": {
        "type": "macosx"
      },
      "features": [
        "set_zero_level_before_generating_mipmap"
      ]
    },
    {
      "id": 137,
      "description": "NVIDIA fails glReadPixels from incomplete cube map texture",
      "cr_bugs": [518889],
      "os": {
        "type": "android"
      },
      "gl_vendor": "NVIDIA.*",
      "features": [
        "force_cube_complete"
      ]
    },
    {
      "id": 138,
      "description": "NVIDIA drivers before 346 lack features in NV_path_rendering and related extensions to implement driver level path rendering.",
      "cr_bugs": [344330],
      "os": {
        "type": "android"
      },
      "gl_vendor": "NVIDIA.*",
      "driver_version": {
        "op": "<",
        "value": "346"
      },
      "disabled_extensions": ["GL_NV_path_rendering"]
    },
    {
      "id": 139,
      "description": "Mesa drivers wrongly report supporting GL_EXT_texture_rg with GLES 2.0 prior version 11.1",
      "cr_bugs": [545904],
      "os": {
        "type": "linux"
      },
      "driver_vendor": "Mesa",
      "driver_version": {
        "op": "<",
        "value": "11.1"
      },
      "gl_type": "gles",
      "gl_version": {
        "op": "<",
        "value": "3.0"
      },
      "disabled_extensions": [
        "GL_EXT_texture_rg"
      ]
    },
    {
      "id": 140,
      "description": "glReadPixels fails on FBOs with SRGB_ALPHA textures, Nexus 5X",
      "cr_bugs": [550292, 565179],
      "os": {
        "type": "android"
        // Originally on Android 6.0. Expect it to fail in later versions.
      },
      "gl_vendor": "Qualcomm",
      "gl_renderer": "Adreno \\(TM\\) 4.*", // Originally on 418.
      "disabled_extensions": ["GL_EXT_sRGB"]
    },
    {
      "id": 141,
      "cr_bugs": [570897],
      "description": "Framebuffer discarding can hurt performance on non-tilers",
      "os": {
        "type": "win"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 142,
      "cr_bugs": [563714],
      "description": "Pack parameters work incorrectly with pack buffer bound",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "features": [
        "pack_parameters_workaround_with_pack_buffer"
      ]
    },
    {
      "id": 143,
      "description": "Timer queries crash on Intel GPUs on Linux",
      "cr_bugs": [540543, 576991],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x8086",
      "driver_vendor": "Mesa",
      "disabled_extensions": [
        "GL_ARB_timer_query",
        "GL_EXT_timer_query"
      ]
    },
    {
      "id": 144,
      "cr_bugs": [563714],
      "description": "Pack parameters work incorrectly with pack buffer bound",
      "os": {
        "type": "macosx"
      },
      "features": [
        "pack_parameters_workaround_with_pack_buffer"
      ]
    },
    {
      "id": 145,
      "cr_bugs": [585250],
      "description": "EGLImage ref counting across EGLContext/threads is broken",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Qualcomm.*",
      "gl_renderer": "Adreno \\(TM\\) 4.*",
      "driver_version": {
        "op": "<",
        "value": "141.0"
      },
      "features": [
        "broken_egl_image_ref_counting"
      ]
    },
)  // LONG_STRING_CONST macro
// Avoid C2026 (string too big) error on VisualStudio.
LONG_STRING_CONST(
    {
      "id": 146,
      "description": "Crashes in D3D11 on specific AMD drivers",
      "cr_bugs": [517040],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1002",
      "driver_version": {
        "op": "between",
        "value": "15.200",
        "value2": "15.201"
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 147,
      "description": "Limit max texure size to 4096 on all of Android",
      "os": {
        "type": "android"
      },
      "features": [
        "max_texture_size_limit_4096"
      ]
    },
    {
      "id": 148,
      "description": "Mali-4xx GPU on JB doesn't support DetachGLContext",
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "surface_texture_cant_detach"
      ]
    },
    {
      "id": 149,
      "description": "Direct composition flashes black initially on Win <10",
      "cr_bugs": [588588],
      "os": {
        "type": "win",
        "version": {
          "op": "<",
          "value": "10.0"
        }
      },
      "features": [
        "disable_direct_composition"
      ]
    },
    {
      "id": 150,
      "cr_bugs": [563714],
      "description": "Alignment works incorrectly with unpack buffer bound",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "features": [
        "unpack_alignment_workaround_with_unpack_buffer"
      ]
    },
    {
      "id": 151,
      "cr_bugs": [563714],
      "description": "Alignment works incorrectly with unpack buffer bound",
      "os": {
        "type": "macosx"
      },
      "features": [
        "unpack_alignment_workaround_with_unpack_buffer"
      ]
    },
    {
      "id": 152,
      "cr_bugs": [581777],
      "description": "copyTexImage2D fails when reading from IOSurface on multiple GPU types.",
      "os": {
        "type": "macosx"
      },
      "features": [
        "use_intermediary_for_copy_texture_image"
      ]
    },
    {
      "id": 153,
      "cr_bugs": [594016],
      "description": "Vivante GC1000 with EXT_multisampled_render_to_texture fails glReadPixels",
      "os": {
        "type": "linux"
      },
      "gl_vendor": "Vivante Corporation",
      "gl_renderer": "Vivante GC1000",
      "features": [
        "disable_multisampled_render_to_texture"
      ]
    },
    {
      "id": 156,
      "cr_bugs": [598474],
      "description": "glEGLImageTargetTexture2DOES crashes",
      "os": {
        "type": "android",
        "version": {
          "op": "between",
          "value": "4.4",
          "value2": "4.4.4"
        }
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR SGX 544MP",
      "features": [
        "avda_dont_copy_pictures"
      ]
    },
    {
      "id": 157,
      "description": "Testing fences was broken on Mali ES2 drivers for specific phone models",
      "cr_bugs": [589814],
      "os": {
        "type": "android"
      },
      "machine_model_name": ["SM-G361H", "SM-G531H"],
      "gl_vendor": "ARM.*",
      "gl_renderer": "Mali.*",
      "gl_type": "gles",
      "gl_version": {
        "op": "<",
        "value": "3.0"
      },
      "disabled_extensions": [
        "EGL_KHR_fence_sync"
      ]
    },
    {
      "id": 158,
      "description": "IOSurface use becomes pathologically slow over time on 10.10.",
      "cr_bugs": [580616],
      "os": {
        "type": "macosx",
        "version": {
          "op": "=",
          "value": "10.10"
        }
      },
      "features": [
        "disable_overlay_ca_layers"
      ]
    },
    {
      "id": 159,
      "cr_bugs": [570897],
      "description": "Framebuffer discarding can hurt performance on non-tilers",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "gl_type": "gl",
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 160,
      "cr_bugs": [601753],
      "description": "Framebuffer discarding not useful on NVIDIA Kepler architecture and later",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "gl_type": "gles",
      "gl_version": {
        "op": ">=",
        "value": "3.0"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 161,
      "cr_bugs": [601753],
      "description": "Framebuffer discarding not useful on NVIDIA Kepler architecture and later",
      "os": {
        "type": "chromeos"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "NVIDIA.*",
      "gl_type": "gles",
      "gl_version": {
        "op": ">=",
        "value": "3.0"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 162,
      "cr_bugs": [601753],
      "description": "Framebuffer discarding not useful on NVIDIA Kepler architecture and later",
      "os": {
        "type": "android"
      },
      "gl_vendor": "NVIDIA.*",
      "gl_type": "gles",
      "gl_version": {
        "op": ">=",
        "value": "3.0"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 163,
      "cr_bugs": [607130],
      "description": "Multisample renderbuffers with format GL_RGB8 have performance issues on Intel GPUs.",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "features": [
        "disable_webgl_rgb_multisampling_usage"
      ]
    },
    {
      "id": 164,
      "cr_bugs": [595948],
      "description": "glColorMask does not work for multisample renderbuffers on old AMD GPUs.",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "device_id": ["0x68b8"],
      "features": [
        "disable_webgl_multisampling_color_mask_usage"
      ]
    },
    {
      "id": 165,
      "cr_bugs": [596774],
      "description": "Unpacking overlapping rows from unpack buffers is unstable on NVIDIA GL driver",
      "gl_vendor": "NVIDIA.*",
      "features": [
        "unpack_overlapping_rows_separately_unpack_buffer"
      ]
    },
    {
      "id": 166,
      "cr_bugs": [612474],
      "description": "Crash reports for glDiscardFramebuffer on Adreno 530",
      "gl_renderer": "Adreno \\(TM\\) 5.*",
      "os": {
        "type": "android"
      },
      "features": [
        "disable_discard_framebuffer"
      ]
    },
    {
      "id": 167,
      "cr_bugs": [610516],
      "description": "glEGLImageTargetTexture2DOES crashes on Mali-400",
      "os": {
        "type": "android"
      },
      "gl_vendor": "ARM.*",
      "gl_renderer": ".*Mali-4.*",
      "features": [
        "avda_dont_copy_pictures"
      ]
    },
    {
      "id": 168,
      "description": "VirtualBox driver doesn't correctly support partial swaps.",
      "cr_bugs": [613722],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x80ee",
      "features": [
        "disable_post_sub_buffers_for_onscreen_surfaces"
      ]
    },
    {
      "id": 169,
      "description": "Mac Drivers store texture level parameters on int16_t that overflow",
      "cr_bugs": [610153],
      "os": {
        "type": "macosx"
      },
      "features": [
        "use_shadowed_tex_level_params"
      ]
    },
    {
      "id": 170,
      "description": "Zero copy DXGI video hangs on shutdown on Win < 8.1",
      "cr_bugs": [621190],
      "os": {
        "type": "win",
        "version": {
          "op": "<",
          "value": "8.1"
        }
      },
      "features": [
        "disable_dxgi_zero_copy_video"
      ]
    },
    {
      "id": 171,
      "description": "NV12 DXGI video hangs or displays incorrect colors on AMD drivers",
      "cr_bugs": [623029, 644293],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1002",
      "features": [
        "disable_dxgi_zero_copy_video",
        "disable_nv12_dxgi_video"
      ]
    },
    {
      "id": 172,
      "description": "Limited enabling of Chromium GL_INTEL_framebuffer_CMAA",
      "cr_bugs": [535198],
      "features": [
        "disable_framebuffer_cmaa"
      ]
    },
    {
      "id": 174,
      "description": "Adreno 4xx support for EXT_multisampled_render_to_texture is buggy on Android 7.0",
      "cr_bugs": [612474],
      "os": {
        "type": "android",
        "version": {
          "op": "between",
          "value": "7.0.0",
          "value2": "7.0.99"
          // Only initial version of N.
        }
      },
      "gl_renderer": "Adreno \\(TM\\) 4.*",
      "disabled_extensions": [
        "GL_EXT_multisampled_render_to_texture"
      ]
    },
    {
      "id": 175,
      "description": "Adreno 5xx support for EXT_multisampled_render_to_texture is buggy on Android < 7.0",
      "cr_bugs": [612474],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "7.0"
        }
      },
      "gl_renderer": "Adreno \\(TM\\) 5.*",
      "disabled_extensions": [
        "GL_EXT_multisampled_render_to_texture"
      ]
    },
    {
      "id": 176,
      "description": "glClear does not work on Acer Predator GT-810",
      "cr_bugs": [633634],
      "os": {
        "type": "android"
      },
      "gl_vendor": "Intel",
      "gl_renderer": ".*Atom.*x5/x7.*",
      "features": [
        "gl_clear_broken"
      ]
    },
    {
      "id": 177,
      "cr_bugs": [632461],
      "description": "eglCreateImageKHR fails for L8 textures on PowerVR",
      "os": {
        "type": "android"
      },
      "gl_vendor": "Imagination.*",
      "gl_renderer": "PowerVR SGX.*",
      "features": [
        "avda_no_eglimage_for_luminance_tex"
      ]
    }
  ]
}

);  // LONG_STRING_CONST macro

}  // namespace gpu
