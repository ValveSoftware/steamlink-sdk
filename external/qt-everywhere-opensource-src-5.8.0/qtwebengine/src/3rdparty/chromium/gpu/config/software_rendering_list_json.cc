// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Determines whether certain gpu-related features are blacklisted or not.
// The format of a valid software_rendering_list.json file is defined in
// <gpu/config/gpu_control_list_format.txt>.
// The supported "features" can be found in <gpu/config/gpu_blacklist.cc>.

#include "gpu/config/gpu_control_list_jsons.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace gpu {

const char kSoftwareRenderingListJson[] = LONG_STRING_CONST(

{
  "name": "software rendering list",
  // Please update the version number whenever you change this file.
  "version": "11.7",
  "entries": [
    {
      "id": 1,
      "description": "ATI Radeon X1900 is not compatible with WebGL on the Mac",
      "webkit_bugs": [47028],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "device_id": ["0x7249"],
      "multi_gpu_category": "any",
      "features": [
        "webgl",
        "flash_3d",
        "flash_stage3d",
        "gpu_rasterization"
      ]
    },
    {
      "id": 3,
      "description": "GL driver is software rendered. GPU acceleration is disabled",
      "cr_bugs": [59302, 315217],
      "os": {
        "type": "linux"
      },
      "gl_renderer": "(?i).*software.*",
      "features": [
        "all"
      ]
    },
    {
      "id": 4,
      "description": "The Intel Mobile 945 Express family of chipsets is not compatible with WebGL",
      "cr_bugs": [232035],
      "os": {
        "type": "any"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x27AE", "0x27A2"],
      "features": [
        "webgl",
        "flash_3d",
        "flash_stage3d",
        "accelerated_2d_canvas"
      ]
    },
    {
      "id": 5,
      "description": "ATI/AMD cards with older drivers in Linux are crash-prone",
      "cr_bugs": [71381, 76428, 73910, 101225, 136240, 357314],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x1002",
      "exceptions": [
        {
          "driver_vendor": ".*AMD.*",
          "driver_version": {
            "op": ">=",
            "style": "lexical",
            "value": "8.98"
          }
        },
        {
          "driver_vendor": "Mesa",
          "driver_version": {
            "op": ">=",
            "value": "10.0.4"
          }
        },
        {
          "driver_vendor": ".*ANGLE.*"
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 8,
      "description": "NVIDIA GeForce FX Go5200 is assumed to be buggy",
      "cr_bugs": [72938],
      "os": {
        "type": "any"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0324"],
      "features": [
        "all"
      ]
    },
    {
      "id": 10,
      "description": "NVIDIA GeForce 7300 GT on Mac does not support WebGL",
      "cr_bugs": [73794],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0393"],
      "multi_gpu_category": "any",
      "features": [
        "webgl",
        "flash_3d",
        "flash_stage3d",
        "gpu_rasterization"
      ]
    },
    {
      "id": 12,
      "description": "Drivers older than 2009-01 on Windows are possibly unreliable",
      "cr_bugs": [72979, 89802, 315205],
      "os": {
        "type": "win"
      },
      "driver_date": {
        "op": "<",
        "value": "2009.1"
      },
      "exceptions": [
        {
          "vendor_id": "0x8086",
          "device_id": ["0x29a2"],
          "driver_version": {
            "op": ">=",
            "value": "7.15.10.1624"
          }
        },
        {
          "driver_vendor": "osmesa"
        },
        {
          "vendor_id": "0x1414",
          "device_id": ["0x02c1"]
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 17,
      "description": "Older Intel mesa drivers are crash-prone",
      "cr_bugs": [76703, 164555, 225200, 340886],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x8086",
      "driver_vendor": "Mesa",
      "driver_version": {
        "op": "<",
        "value": "10.1"
      },
      "exceptions": [
        {
          "device_id": ["0x0102", "0x0106", "0x0112", "0x0116", "0x0122", "0x0126", "0x010a", "0x0152", "0x0156", "0x015a", "0x0162", "0x0166"],
          "driver_version": {
            "op": ">=",
            "value": "8.0"
          }
        },
        {
          "device_id": ["0xa001", "0xa002", "0xa011", "0xa012", "0x29a2", "0x2992", "0x2982", "0x2972", "0x2a12", "0x2a42", "0x2e02", "0x2e12", "0x2e22", "0x2e32", "0x2e42", "0x2e92"],
          "driver_version": {
            "op": ">",
            "value": "8.0.2"
          }
        },
        {
          "device_id": ["0x0042", "0x0046"],
          "driver_version": {
            "op": ">",
            "value": "8.0.4"
          }
        },
        {
          "device_id": ["0x2a02"],
          "driver_version": {
            "op": ">=",
            "value": "9.1"
          }
        },
        {
          "device_id": ["0x0a16", "0x0a26"],
          "driver_version": {
            "op": ">=",
            "value": "10.0.1"
          }
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 18,
      "description": "NVIDIA Quadro FX 1500 is buggy",
      "cr_bugs": [84701],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x029e"],
      "features": [
        "all"
      ]
    },
    {
      "id": 23,
      "description": "Mesa drivers in linux older than 7.11 are assumed to be buggy",
      "os": {
        "type": "linux"
      },
      "driver_vendor": "Mesa",
      "driver_version": {
        "op": "<",
        "value": "7.11"
      },
      "exceptions": [
        {
          "driver_vendor": "osmesa"
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 24,
      "description": "Accelerated 2d canvas is unstable in Linux at the moment",
      "os": {
        "type": "linux"
      },
      "exceptions": [
        {
          "gl_vendor": "Vivante Corporation", 
          "gl_renderer": "Vivante GC1000"
        }
      ],
      "features": [
        "accelerated_2d_canvas"
      ]
    },
    {
      "id": 27,
      "description": "ATI/AMD cards with older drivers in Linux are crash-prone",
      "cr_bugs": [95934, 94973, 136240, 357314],
      "os": {
        "type": "linux"
      },
      "gl_vendor": "ATI.*",
      "exceptions": [
        {
          "driver_vendor": ".*AMD.*",
          "driver_version": {
            "op": ">=",
            "style": "lexical",
            "value": "8.98"
          }
        },
        {
          "driver_vendor": "Mesa",
          "driver_version": {
            "op": ">=",
            "value": "10.0.4"
          }
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 28,
      "description": "ATI/AMD cards with third-party drivers in Linux are crash-prone",
      "cr_bugs": [95934, 94973, 357314],
      "os": {
        "type": "linux"
      },
      "gl_vendor": "X\\.Org.*",
      "gl_renderer": ".*AMD.*",
      "exceptions": [
        {
          "driver_vendor": "Mesa",
          "driver_version": {
            "op": ">=",
            "value": "10.0.4"
          }
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 29,
      "description": "ATI/AMD cards with third-party drivers in Linux are crash-prone",
      "cr_bugs": [95934, 94973, 357314],
      "os": {
        "type": "linux"
      },
      "gl_vendor": "X\\.Org.*",
      "gl_renderer": ".*ATI.*",
      "exceptions": [
        {
          "driver_vendor": "Mesa",
          "driver_version": {
            "op": ">=",
            "value": "10.0.4"
          }
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 30,
      "description": "NVIDIA cards with nouveau drivers in Linux are crash-prone",
      "cr_bugs": [94103],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "gl_vendor": "(?i)nouveau.*",
      "driver_vendor": "Mesa",
      "driver_version": {
        "op": "<",
        "value": "10.1"
      },
      "features": [
        "all"
      ]
    },
    {
      "id": 34,
      "description": "S3 Trio (used in Virtual PC) is not compatible",
      "cr_bugs": [119948],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x5333",
      "device_id": ["0x8811"],
      "features": [
        "all"
      ]
    },
    {
      "id": 37,
      "description": "Older drivers are unreliable for Optimus on Linux",
      "cr_bugs": [131308, 363418],
      "os": {
        "type": "linux"
      },
      "multi_gpu_style": "optimus",
      "driver_vendor": "Mesa",
      "driver_version": {
        "op": "<",
        "value": "10.1"
      },
      "gl_vendor": "Intel.*",
      "features": [
        "all"
      ]
    },
    {
      "id": 45,
      "description": "Parallels drivers older than 7 are buggy",
      "cr_bugs": [138105],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1ab8",
      "driver_version": {
        "op": "<",
        "value": "7"
      },
      "features": [
        "all"
      ]
    },
    {
      "id": 46,
      "description": "ATI FireMV 2400 cards on Windows are buggy",
      "cr_bugs": [124152],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1002",
      "device_id": ["0x3151"],
      "features": [
        "all"
      ]
    },
    {
      "id": 47,
      "description": "NVIDIA linux drivers older than 295.* are assumed to be buggy",
      "cr_bugs": [78497],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "driver_vendor": "NVIDIA",
      "driver_version": {
        "op": "<",
        "value": "295"
      },
      "features": [
        "all"
      ]
    },
    {
      "id": 48,
      "description": "Accelerated video decode is unavailable on Linux",
      "cr_bugs": [137247],
      "os": {
        "type": "linux"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 50,
      "description": "Disable VMware software renderer on older Mesa",
      "cr_bugs": [145531, 332596, 571899],
      "os": {
        "type": "linux"
      },
      "gl_vendor": "VMware.*",
      "exceptions": [
        {
          "driver_vendor": "Mesa",
          "driver_version": {
            "op": ">=",
            "value": "9.2.1"
          },
          "gl_renderer": ".*SVGA3D.*"
        },
        {
          "driver_vendor": "Mesa",
          "driver_version": {
            "op": ">=",
            "value": "10.1.3"
          },
          "gl_renderer": ".*Gallium.*llvmpipe.*"
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 53,
      "description": "The Intel GMA500 is too slow for Stage3D",
      "cr_bugs": [152096],
      "vendor_id": "0x8086",
      "device_id": ["0x8108", "0x8109"],
      "features": [
        "flash_stage3d"
      ]
    },
    {
      "id": 56,
      "description": "NVIDIA linux drivers are unstable when using multiple Open GL contexts and with low memory",
      "cr_bugs": [145600],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "driver_vendor": "NVIDIA",
      "driver_version": {
        "op": "<",
        "value": "331.38"
      },
      "features": [
        "accelerated_video_decode",
        "flash_3d",
        "flash_stage3d"
      ]
    },
    {
      // Panel fitting is only used with OS_CHROMEOS. To avoid displaying an
      // error in chrome:gpu on every other platform, this blacklist entry needs
      // to only match on chromeos. The drawback is that panel_fitting will not
      // appear to be blacklisted if accidentally queried on non-chromeos.
      "id": 57,
      "description": "Chrome OS panel fitting is only supported for Intel IVB and SNB Graphics Controllers",
      "os": {
        "type": "chromeos"
      },
      "exceptions": [
        {
          "vendor_id": "0x8086",
          "device_id": ["0x0106", "0x0116", "0x0166"]
        }
      ],
      "features": [
        "panel_fitting"
      ]
    },
    {
      "id": 59,
      "description": "NVidia driver 8.15.11.8593 is crashy on Windows",
      "cr_bugs": [155749],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "driver_version": {
        "op": "=",
        "value": "8.15.11.8593"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 64,
      "description": "Hardware video decode is only supported in win7+",
      "cr_bugs": [159458],
      "os": {
        "type": "win",
        "version": {
          "op": "<",
          "value": "6.1"
        }
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 68,
      "description": "VMware Fusion 4 has corrupt rendering with Win Vista+",
      "cr_bugs": [169470],
      "os": {
        "type": "win",
        "version": {
          "op": ">=",
          "value": "6.0"
        }
      },
      "vendor_id": "0x15ad",
      "driver_version": {
        "op": "<=",
        "value": "7.14.1.1134"
      },
      "features": [
        "all"
      ]
    },
    {
      "id": 69,
      "description": "NVIDIA driver 8.17.11.9621 is buggy with Stage3D baseline mode",
      "cr_bugs": [172771],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "driver_version": {
        "op": "=",
        "value": "8.17.11.9621"
      },
      "features": [
        "flash_stage3d_baseline"
      ]
    },
    {
      "id": 70,
      "description": "NVIDIA driver 8.17.11.8267 is buggy with Stage3D baseline mode",
      "cr_bugs": [172771],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "driver_version": {
        "op": "=",
        "value": "8.17.11.8267"
      },
      "features": [
        "flash_stage3d_baseline"
      ]
    },
    {
      "id": 71,
      "description": "All Intel drivers before 8.15.10.2021 are buggy with Stage3D baseline mode",
      "cr_bugs": [172771],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "driver_version": {
        "op": "<",
        "value": "8.15.10.2021"
      },
      "features": [
        "flash_stage3d_baseline"
      ]
    },
    {
      "id": 72,
      "description": "NVIDIA GeForce 6200 LE is buggy with WebGL",
      "cr_bugs": [232529],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0163"],
      "features": [
        "webgl"
      ]
    },
    {
      "id": 74,
      "description": "GPU access is blocked if users don't have proper graphics driver installed after Windows installation",
      "cr_bugs": [248178],
      "os": {
        "type": "win"
      },
      "driver_vendor": "Microsoft",
      "exceptions": [
        {
          "vendor_id": "0x1414",
          "device_id": ["0x02c1"]
        }
      ],
      "features": [
        "all"
      ]
    },
)  // String split to avoid MSVC char limit.
LONG_STRING_CONST(
    {
      "id": 76,
      "description": "WebGL is disabled on Android unless the GPU runs in a separate process or reset notification is supported",
      "os": {
        "type": "android"
      },
      "in_process_gpu": true,
      "exceptions": [
        {
          "gl_reset_notification_strategy": {
            "op": "=",
            "value": "33362"
          }
        },
        {
          "gl_renderer": "Mali-4.*",
          "gl_extensions": ".*EXT_robustness.*"
        }
      ],
      "features": [
        "webgl"
      ]
    },
    {
      "id": 78,
      "description": "Accelerated video decode interferes with GPU sandbox on older Intel drivers",
      "cr_bugs": [180695, 298968, 436968],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "driver_version": {
        "op": "<=",
        "value": "8.15.10.2702"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 79,
      "description": "Disable GPU on all Windows versions prior to and including Vista",
      "cr_bugs": [315199],
      "os": {
        "type": "win",
        "version": {
          "op": "<=",
          "value": "6.0"
        }
      },
      "exceptions": [
        {
          "driver_vendor": "Mesa",
          "gl_renderer": ".*Gallium.*"
        },
        {
          "driver_vendor": ".*llvmpipe.*"
        }
      ],
      "features": [
        "all"
      ]
    },
    {
      "id": 82,
      "description": "MediaCodec is still too buggy to use for encoding (b/11536167)",
      "os": {
        "type": "android"
      },
      "features": [
        "accelerated_video_encode"
      ]
    },
    {
      "id": 86,
      "description": "Intel Graphics Media Accelerator 3150 causes the GPU process to hang running WebGL",
      "cr_bugs": [305431],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "device_id": ["0xa011"],
      "features": [
        "webgl"
      ]
    },
    {
      "id": 87,
      "description": "Accelerated video decode on Intel driver 10.18.10.3308 is incompatible with the GPU sandbox",
      "cr_bugs": [298968],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "driver_version": {
        "op": "=",
        "value": "10.18.10.3308"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 88,
      "description": "Accelerated video decode on AMD driver 13.152.1.8000 is incompatible with the GPU sandbox",
      "cr_bugs": [298968],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1002",
      "driver_version": {
        "op": "=",
        "value": "13.152.1.8000"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 89,
      "description": "Accelerated video decode interferes with GPU sandbox on certain AMD drivers",
      "cr_bugs": [298968],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x1002",
      "driver_version": {
        "op": "between",
        "value": "8.810.4.5000",
        "value2": "8.970.100.1100"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 90,
      "description": "Accelerated video decode interferes with GPU sandbox on certain NVIDIA drivers",
      "cr_bugs": [298968],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "driver_version": {
        "op": "between",
        "value": "8.17.12.5729",
        "value2": "8.17.12.8026"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 91,
      "description": "Accelerated video decode interferes with GPU sandbox on certain NVIDIA drivers",
      "cr_bugs": [298968],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x10de",
      "driver_version": {
        "op": "between",
        "value": "9.18.13.783",
        "value2": "9.18.13.1090"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 92,
      "description": "Accelerated video decode does not work with the discrete GPU on AMD switchables",
      "cr_bugs": [298968],
      "os": {
        "type": "win"
      },
      "multi_gpu_style": "amd_switchable_discrete",
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 93,
      "description": "GLX indirect rendering (X remoting) is not supported",
      "cr_bugs": [72373],
      "os": {
        "type": "linux"
      },
      "direct_rendering": false,
      "features": [
        "all"
      ]
    },
    {
      "id": 94,
      "description": "Intel driver version 8.15.10.1749 causes GPU process hangs.",
      "cr_bugs": [350566],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "driver_version": {
        "op": "=",
        "value": "8.15.10.1749"
      },
      "features": [
        "all"
      ]
    },
    {
      "id": 95,
      "description": "AMD driver version 13.101 is unstable on linux.",
      "cr_bugs": [363378],
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x1002",
      "driver_vendor": ".*AMD.*",
      "driver_version": {
        "op": "=",
        "value": "13.101"
      },
      "features": [
        "all"
      ]
    },
    {
      "id": 96,
      "description": "Blacklist GPU raster/canvas on all except known good GPUs and newer Android releases",
      "cr_bugs": [362779,424970],
      "os": {
        "type": "android"
      },
      "exceptions": [
        {
          "os": {
            "type": "android"
          },
          "gl_renderer": "Adreno \\(TM\\) 3.*"
        },
        {
          "os": {
            "type": "android",
            "version": {
              "op": ">=",
              "value": "4.4"
            }
          },
          "gl_renderer": "Mali-4.*"
        },
        {
          "os": {
            "type": "android"
          },
          "gl_renderer": "NVIDIA.*"
        },
        {
          "os": {
            "type": "android",
            "version": {
              "op": ">=",
              "value": "4.4"
            }
          },
          "gl_type": "gles",
          "gl_version": {
            "op": ">=",
            "value": "3.0"
          }
        },
        {
          "os": {
            "type": "android"
          },
          "gl_renderer": ".*Google.*"
        }
      ],
      "features": [
        "gpu_rasterization",
        "accelerated_2d_canvas"
      ]
    },
    {
      "id": 100,
      "description": "GPU rasterization and canvas is blacklisted on Nexus 10",
      "cr_bugs": [407144],
      "os": {
        "type": "android"
      },
      "gl_renderer": ".*Mali-T604.*",
      "features": [
        "gpu_rasterization",
        "accelerated_2d_canvas"
      ]
    },
    {
      "id": 102,
      "description": "Accelerated 2D canvas and Ganesh broken on Galaxy Tab 2",
      "cr_bugs": [416910],
      "os": {
        "type": "android"
      },
      "gl_renderer": "PowerVR SGX 540",
      "features": [
        "accelerated_2d_canvas",
        "gpu_rasterization"
      ]
    },
    {
      "id": 104,
      "description": "GPU raster broken on PowerVR Rogue",
      "cr_bugs": [436331, 483574],
      "os": {
        "type": "android"
      },
      "gl_renderer": "PowerVR Rogue.*",
      "features": [
        "accelerated_2d_canvas",
        "gpu_rasterization"
      ]
    },
    {
      "id": 105,
      "description": "GPU raster broken on PowerVR SGX even on Lollipop",
      "cr_bugs": [461456],
      "os": {
        "type": "android"
      },
      "gl_renderer": "PowerVR SGX.*",
      "features": [
        "accelerated_2d_canvas",
        "gpu_rasterization"
      ]
    },
    {
      "id": 106,
      "description": "GPU raster broken on ES2-only Adreno 3xx drivers",
      "cr_bugs": [480149],
      "os": {
        "type": "android"
      },
      "gl_renderer": "Adreno \\(TM\\) 3.*",
      "gl_version": {
         "op": "<=",
         "value": "2.0"
      },
      "features": [
        "accelerated_2d_canvas",
        "gpu_rasterization"
      ]
    },
    {
      "id": 107,
      "description": "Haswell GT1 Intel drivers are buggy on kernels < 3.19.1",
      "cr_bugs": [463243],
      "os": {
        "type": "linux",
        "version": {
          "op": "<",
          "value": "3.19.1"
        }
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0402", "0x0406", "0x040a", "0x040b", "0x040e",
                    "0x0a02", "0x0a06", "0x0a0a", "0x0a0b", "0x0a0e",
                    "0x0d02", "0x0d06", "0x0d0a", "0x0d0b", "0x0d0e"],
      "features": [
        "all"
      ]
    },
    {
      "id": 108,
      "description": "GPU rasterization image color broken on Vivante",
      "cr_bugs": [560587],
      "os": {
        "type": "android"
      },
      "gl_renderer": ".*Vivante.*",
      "features": [
        "gpu_rasterization",
        "accelerated_2d_canvas"
      ]
    },
    {
      "id": 109,
      "description": "MediaCodec on Adreno 330 / 4.2.2 doesn't always send FORMAT_CHANGED",
      "cr_bugs": [585963],
      "os": {
        "type": "android",
        "version": {
          "op": "=",
          "value": "4.2.2"
        }
      },
      "gl_renderer": "Adreno \\(TM\\) 330",
      "driver_version": {
        "op": "=",
        "value": "45.0"
      },
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 110,
      "description": "Only enable WebGL for the Mesa Gallium llvmpipe driver",
      "cr_bugs": [571899],
      "os": {
        "type": "linux"
      },
      "driver_vendor": "Mesa",
      "gl_vendor": "VMware.*",
      "gl_renderer": ".*Gallium.*llvmpipe.*",
      "features": [
        "all",
        {"exceptions": [
          "webgl"
        ]}
      ]
    },
    {
      "id": 111,
      "description": "Apple Software Renderer used under VMWare experiences synchronization issues with GPU Raster",
      "cr_bugs": [607829],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x15ad",
      "multi_gpu_category": "any",
      "features": [
        "gpu_rasterization"
      ]
    },
    {
      "id": 112,
      "description": "Intel HD 3000 driver crashes frequently on Mac",
      "cr_bugs": [592130],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0116", "0x0126"],
      "features": [
        "all"
      ]
    },
    {
      "id": 113,
      "description": "Some GPUs on Mac can perform poorly with GPU rasterization. Disable all known Intel GPUs other than Intel 6th and 7th Generation cards, which have been tested.",
      "cr_bugs": [613272, 614468],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0126", "0x0116", "0x191e", "0x0046", "0x1912",
                    "0x2a02", "0x27a2", "0x2a42"],
      "multi_gpu_category": "any",
      "features": [
        "gpu_rasterization"
      ]
    },
    {
      "id": 114,
      "description": "Some GPUs on Mac can perform poorly with GPU rasterization. Disable all known NVidia GPUs other than the Geforce 6xx and 7xx series, which have been tested.",
      "cr_bugs": [613272, 614468],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0863", "0x08a0", "0x0a29", "0x0869", "0x0867",
                    "0x08a3", "0x11a3", "0x08a2", "0x0407", "0x0861",
                    "0x08a4", "0x0647", "0x0640", "0x0866", "0x0655",
                    "0x062e", "0x0609", "0x1187", "0x13c2", "0x0602",
                    "0x1180", "0x1401", "0x0fc8", "0x0611", "0x1189",
                    "0x11c0", "0x0870", "0x0a65", "0x06dd", "0x0fc1",
                    "0x1380", "0x11c6", "0x104a", "0x1184", "0x0fc6",
                    "0x13c0", "0x1381", "0x05e3", "0x1183", "0x05fe",
                    "0x1004", "0x17c8", "0x11ba", "0x0a20", "0x0f00",
                    "0x0ca3", "0x06fd", "0x0f02", "0x0614", "0x0402",
                    "0x13bb", "0x0401", "0x0f01", "0x1287", "0x0615",
                    "0x1402", "0x019d", "0x0400", "0x0622", "0x06e4",
                    "0x06cd", "0x1201", "0x100a", "0x10c3", "0x1086",
                    "0x17c2", "0x1005", "0x0a23", "0x0de0", "0x1040",
                    "0x0421", "0x1282", "0x0e22", "0x0e23", "0x0610",
                    "0x11c8", "0x11c2", "0x1188", "0x0de9", "0x1200",
                    "0x1244", "0x0dc4", "0x0df8", "0x0641", "0x0613",
                    "0x11fa", "0x100c", "0x0de1", "0x0ca5", "0x0cb1",
                    "0x0a6c", "0x05ff", "0x05e2", "0x0a2d", "0x06c0",
                    "0x1288", "0x1048", "0x1081", "0x0dd8", "0x05e6",
                    "0x11c4", "0x0605", "0x1080", "0x042f", "0x0ca2",
                    "0x1245", "0x124d", "0x1284", "0x0191", "0x1050",
                    "0x0ffd", "0x0193", "0x061a", "0x0422", "0x1185",
                    "0x103a", "0x0fc2", "0x0194", "0x0df5", "0x040e",
                    "0x065b", "0x0de2", "0x0a75", "0x0601", "0x1087",
                    "0x019e", "0x104b", "0x107d", "0x1382", "0x042b",
                    "0x1049", "0x0df0", "0x11a1", "0x040f", "0x0de3",
                    "0x0fc0", "0x13d8", "0x0de4", "0x11e2", "0x0644",
                    "0x0fd1", "0x0dfa"],
      "multi_gpu_category": "any",
      "features": [
        "gpu_rasterization"
      ]
    },
    {
      "id": 115,
      "description": "Some GPUs on Mac can perform poorly with GPU rasterization. Disable all known AMD GPUs other than the R200, R300, and D series, which have been tested.",
      "cr_bugs": [613272, 614468],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "device_id": ["0x6741", "0x6740", "0x9488", "0x9583", "0x6720",
                    "0x6760", "0x68c0", "0x68a1", "0x944a", "0x94c8",
                    "0x6819", "0x68b8", "0x6920", "0x6938", "0x6640",
                    "0x9588", "0x6898", "0x9440", "0x6738", "0x6739",
                    "0x6818", "0x6758", "0x6779", "0x9490", "0x68d9",
                    "0x683f", "0x683d", "0x6899", "0x6759", "0x68e0",
                    "0x68d8", "0x68ba", "0x68f9", "0x9501", "0x68a0",
                    "0x6841", "0x6840", "0x9442", "0x6658", "0x68c8",
                    "0x68c1"],
      "multi_gpu_category": "any",
      "features": [
        "gpu_rasterization"
      ]
    },
    {
      "id": 116,
      "description": "Some GPUs on Mac can perform poorly with GPU rasterization. Disable untested Virtualbox GPU.",
      "cr_bugs": [613272, 614468],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x80ee",
      "multi_gpu_category": "any",
      "features": [
        "gpu_rasterization"
      ]
    },
    {
      "id": 117,
      "description": "MediaCodec on Vivante hangs in MediaCodec often",
      "cr_bugs": [626814],
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_renderer": ".*Vivante.*",
      "features": [
        "accelerated_video_decode"
      ]
    },
    {
      "id": 118,
      "description": "webgl/canvas crashy on imporperly parsed vivante driver",
      "cr_bugs": [628059],
      "os": {
        "type": "android",
        "version": {
          "op": "<=",
          "value": "4.4.4"
        }
      },
      "gl_vendor": "Vivante.*",
      "gl_renderer": ".*PXA.*",
      "features": [
        "webgl",
        "accelerated_2d_canvas"
      ]
    },
    {
      "id": 119,
      "description": "There are display issues with GPU Raster on OSX 10.9",
      "cr_bugs": [611310],
      "os": {
        "type": "macosx",
        "version": {
          "op": "<=",
          "value": "10.9"
        }
      },
      "features": [
        "gpu_rasterization"
      ]
    }
  ]
}

);  // LONG_STRING_CONST macro

}  // namespace gpu
