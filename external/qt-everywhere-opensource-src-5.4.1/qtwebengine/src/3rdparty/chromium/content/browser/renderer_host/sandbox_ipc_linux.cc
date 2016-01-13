// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/sandbox_ipc_linux.h"

#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/linux_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/set_process_title.h"
#include "content/public/common/content_switches.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "third_party/WebKit/public/platform/linux/WebFontInfo.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "ui/gfx/font_render_params_linux.h"

using blink::WebCString;
using blink::WebFontInfo;
using blink::WebUChar;
using blink::WebUChar32;

namespace {

// MSCharSetToFontconfig translates a Microsoft charset identifier to a
// fontconfig language set by appending to |langset|.
// Returns true if |langset| is Latin/Greek/Cyrillic.
bool MSCharSetToFontconfig(FcLangSet* langset, unsigned fdwCharSet) {
  // We have need to translate raw fdwCharSet values into terms that
  // fontconfig can understand. (See the description of fdwCharSet in the MSDN
  // documentation for CreateFont:
  // http://msdn.microsoft.com/en-us/library/dd183499(VS.85).aspx )
  //
  // Although the argument is /called/ 'charset', the actual values conflate
  // character sets (which are sets of Unicode code points) and character
  // encodings (which are algorithms for turning a series of bits into a
  // series of code points.) Sometimes the values will name a language,
  // sometimes they'll name an encoding. In the latter case I'm assuming that
  // they mean the set of code points in the domain of that encoding.
  //
  // fontconfig deals with ISO 639-1 language codes:
  //   http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
  //
  // So, for each of the documented fdwCharSet values I've had to take a
  // guess at the set of ISO 639-1 languages intended.

  bool is_lgc = false;
  switch (fdwCharSet) {
    case NPCharsetAnsi:
    // These values I don't really know what to do with, so I'm going to map
    // them to English also.
    case NPCharsetDefault:
    case NPCharsetMac:
    case NPCharsetOEM:
    case NPCharsetSymbol:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("en"));
      break;
    case NPCharsetBaltic:
      // The three baltic languages.
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("et"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("lv"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("lt"));
      break;
    case NPCharsetChineseBIG5:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("zh-tw"));
      break;
    case NPCharsetGB2312:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("zh-cn"));
      break;
    case NPCharsetEastEurope:
      // A scattering of eastern European languages.
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("pl"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("cs"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("sk"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("hu"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("hr"));
      break;
    case NPCharsetGreek:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("el"));
      break;
    case NPCharsetHangul:
    case NPCharsetJohab:
      // Korean
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ko"));
      break;
    case NPCharsetRussian:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ru"));
      break;
    case NPCharsetShiftJIS:
      // Japanese
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ja"));
      break;
    case NPCharsetTurkish:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("tr"));
      break;
    case NPCharsetVietnamese:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("vi"));
      break;
    case NPCharsetArabic:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ar"));
      break;
    case NPCharsetHebrew:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("he"));
      break;
    case NPCharsetThai:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("th"));
      break;
      // default:
      // Don't add any languages in that case that we don't recognise the
      // constant.
  }
  return is_lgc;
}

}  // namespace

namespace content {

SandboxIPCHandler::SandboxIPCHandler(int lifeline_fd, int browser_socket)
    : lifeline_fd_(lifeline_fd), browser_socket_(browser_socket) {
  // FontConfig doesn't provide a standard property to control subpixel
  // positioning, so we pass the current setting through to WebKit.
  WebFontInfo::setSubpixelPositioning(
      gfx::GetDefaultWebkitSubpixelPositioning());
}

void SandboxIPCHandler::Run() {
  struct pollfd pfds[2];
  pfds[0].fd = lifeline_fd_;
  pfds[0].events = POLLIN;
  pfds[1].fd = browser_socket_;
  pfds[1].events = POLLIN;

  int failed_polls = 0;
  for (;;) {
    const int r =
        HANDLE_EINTR(poll(pfds, arraysize(pfds), -1 /* no timeout */));
    // '0' is not a possible return value with no timeout.
    DCHECK_NE(0, r);
    if (r < 0) {
      PLOG(WARNING) << "poll";
      if (failed_polls++ == 3) {
        LOG(FATAL) << "poll(2) failing. SandboxIPCHandler aborting.";
        return;
      }
      continue;
    }

    failed_polls = 0;

    // The browser process will close the other end of this pipe on shutdown,
    // so we should exit.
    if (pfds[0].revents) {
      break;
    }

    // If poll(2) reports an error condition in this fd,
    // we assume the zygote is gone and we exit the loop.
    if (pfds[1].revents & (POLLERR | POLLHUP)) {
      break;
    }

    if (pfds[1].revents & POLLIN) {
      HandleRequestFromRenderer(browser_socket_);
    }
  }

  VLOG(1) << "SandboxIPCHandler stopping.";
}

void SandboxIPCHandler::HandleRequestFromRenderer(int fd) {
  ScopedVector<base::ScopedFD> fds;

  // A FontConfigIPC::METHOD_MATCH message could be kMaxFontFamilyLength
  // bytes long (this is the largest message type).
  // 128 bytes padding are necessary so recvmsg() does not return MSG_TRUNC
  // error for a maximum length message.
  char buf[FontConfigIPC::kMaxFontFamilyLength + 128];

  const ssize_t len = UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);
  if (len == -1) {
    // TODO: should send an error reply, or the sender might block forever.
    NOTREACHED() << "Sandbox host message is larger than kMaxFontFamilyLength";
    return;
  }
  if (fds.empty())
    return;

  Pickle pickle(buf, len);
  PickleIterator iter(pickle);

  int kind;
  if (!pickle.ReadInt(&iter, &kind))
    return;

  if (kind == FontConfigIPC::METHOD_MATCH) {
    HandleFontMatchRequest(fd, pickle, iter, fds.get());
  } else if (kind == FontConfigIPC::METHOD_OPEN) {
    HandleFontOpenRequest(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_GET_FALLBACK_FONT_FOR_CHAR) {
    HandleGetFallbackFontForChar(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_LOCALTIME) {
    HandleLocaltime(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_GET_STYLE_FOR_STRIKE) {
    HandleGetStyleForStrike(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_MAKE_SHARED_MEMORY_SEGMENT) {
    HandleMakeSharedMemorySegment(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_MATCH_WITH_FALLBACK) {
    HandleMatchWithFallback(fd, pickle, iter, fds.get());
  }
}

int SandboxIPCHandler::FindOrAddPath(const SkString& path) {
  int count = paths_.count();
  for (int i = 0; i < count; ++i) {
    if (path == *paths_[i])
      return i;
  }
  *paths_.append() = new SkString(path);
  return count;
}

void SandboxIPCHandler::HandleFontMatchRequest(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  uint32_t requested_style;
  std::string family;
  if (!pickle.ReadString(&iter, &family) ||
      !pickle.ReadUInt32(&iter, &requested_style))
    return;

  SkFontConfigInterface::FontIdentity result_identity;
  SkString result_family;
  SkTypeface::Style result_style;
  SkFontConfigInterface* fc =
      SkFontConfigInterface::GetSingletonDirectInterface();
  const bool r =
      fc->matchFamilyName(family.c_str(),
                          static_cast<SkTypeface::Style>(requested_style),
                          &result_identity,
                          &result_family,
                          &result_style);

  Pickle reply;
  if (!r) {
    reply.WriteBool(false);
  } else {
    // Stash away the returned path, so we can give it an ID (index)
    // which will later be given to us in a request to open the file.
    int index = FindOrAddPath(result_identity.fString);
    result_identity.fID = static_cast<uint32_t>(index);

    reply.WriteBool(true);
    skia::WriteSkString(&reply, result_family);
    skia::WriteSkFontIdentity(&reply, result_identity);
    reply.WriteUInt32(result_style);
  }
  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleFontOpenRequest(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  uint32_t index;
  if (!pickle.ReadUInt32(&iter, &index))
    return;
  if (index >= static_cast<uint32_t>(paths_.count()))
    return;
  const int result_fd = open(paths_[index]->c_str(), O_RDONLY);

  Pickle reply;
  if (result_fd == -1) {
    reply.WriteBool(false);
  } else {
    reply.WriteBool(true);
  }

  // The receiver will have its own access to the file, so we will close it
  // after this send.
  SendRendererReply(fds, reply, result_fd);

  if (result_fd >= 0) {
    int err = IGNORE_EINTR(close(result_fd));
    DCHECK(!err);
  }
}

void SandboxIPCHandler::HandleGetFallbackFontForChar(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  // The other side of this call is
  // content/common/child_process_sandbox_support_impl_linux.cc

  EnsureWebKitInitialized();
  WebUChar32 c;
  if (!pickle.ReadInt(&iter, &c))
    return;

  std::string preferred_locale;
  if (!pickle.ReadString(&iter, &preferred_locale))
    return;

  blink::WebFallbackFont fallbackFont;
  WebFontInfo::fallbackFontForChar(c, preferred_locale.c_str(), &fallbackFont);

  Pickle reply;
  if (fallbackFont.name.data()) {
    reply.WriteString(fallbackFont.name.data());
  } else {
    reply.WriteString(std::string());
  }
  if (fallbackFont.filename.data()) {
    reply.WriteString(fallbackFont.filename.data());
  } else {
    reply.WriteString(std::string());
  }
  reply.WriteInt(fallbackFont.ttcIndex);
  reply.WriteBool(fallbackFont.isBold);
  reply.WriteBool(fallbackFont.isItalic);
  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleGetStyleForStrike(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  std::string family;
  int sizeAndStyle;

  if (!pickle.ReadString(&iter, &family) ||
      !pickle.ReadInt(&iter, &sizeAndStyle)) {
    return;
  }

  EnsureWebKitInitialized();
  blink::WebFontRenderStyle style;
  WebFontInfo::renderStyleForStrike(family.c_str(), sizeAndStyle, &style);

  Pickle reply;
  reply.WriteInt(style.useBitmaps);
  reply.WriteInt(style.useAutoHint);
  reply.WriteInt(style.useHinting);
  reply.WriteInt(style.hintStyle);
  reply.WriteInt(style.useAntiAlias);
  reply.WriteInt(style.useSubpixelRendering);
  reply.WriteInt(style.useSubpixelPositioning);

  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleLocaltime(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  // The other side of this call is in zygote_main_linux.cc

  std::string time_string;
  if (!pickle.ReadString(&iter, &time_string) ||
      time_string.size() != sizeof(time_t)) {
    return;
  }

  time_t time;
  memcpy(&time, time_string.data(), sizeof(time));
  // We use localtime here because we need the tm_zone field to be filled
  // out. Since we are a single-threaded process, this is safe.
  const struct tm* expanded_time = localtime(&time);

  std::string result_string;
  const char* time_zone_string = "";
  if (expanded_time != NULL) {
    result_string = std::string(reinterpret_cast<const char*>(expanded_time),
                                sizeof(struct tm));
    time_zone_string = expanded_time->tm_zone;
  }

  Pickle reply;
  reply.WriteString(result_string);
  reply.WriteString(time_zone_string);
  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleMakeSharedMemorySegment(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  base::SharedMemoryCreateOptions options;
  uint32_t size;
  if (!pickle.ReadUInt32(&iter, &size))
    return;
  options.size = size;
  if (!pickle.ReadBool(&iter, &options.executable))
    return;
  int shm_fd = -1;
  base::SharedMemory shm;
  if (shm.Create(options))
    shm_fd = shm.handle().fd;
  Pickle reply;
  SendRendererReply(fds, reply, shm_fd);
}

void SandboxIPCHandler::HandleMatchWithFallback(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  // Unlike the other calls, for which we are an indirection in front of
  // WebKit or Skia, this call is always made via this sandbox helper
  // process. Therefore the fontconfig code goes in here directly.

  std::string face;
  bool is_bold, is_italic;
  uint32 charset, fallback_family;

  if (!pickle.ReadString(&iter, &face) || face.empty() ||
      !pickle.ReadBool(&iter, &is_bold) ||
      !pickle.ReadBool(&iter, &is_italic) ||
      !pickle.ReadUInt32(&iter, &charset) ||
      !pickle.ReadUInt32(&iter, &fallback_family)) {
    return;
  }

  FcLangSet* langset = FcLangSetCreate();
  bool is_lgc = MSCharSetToFontconfig(langset, charset);

  FcPattern* pattern = FcPatternCreate();
  FcPatternAddString(
      pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(face.c_str()));

  // TODO(thestig) Check if we can access Chrome's per-script font preference
  // here and select better default fonts for non-LGC case.
  std::string generic_font_name;
  if (is_lgc) {
    switch (fallback_family) {
      case PP_BROWSERFONT_TRUSTED_FAMILY_SERIF:
        generic_font_name = "Times New Roman";
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF:
        generic_font_name = "Arial";
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE:
        generic_font_name = "Courier New";
        break;
    }
  }
  if (!generic_font_name.empty()) {
    const FcChar8* fc_generic_font_name =
        reinterpret_cast<const FcChar8*>(generic_font_name.c_str());
    FcPatternAddString(pattern, FC_FAMILY, fc_generic_font_name);
  }

  if (is_bold)
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
  if (is_italic)
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
  FcPatternAddLangSet(pattern, FC_LANG, langset);
  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcFontSet* font_set = FcFontSort(0, pattern, 0, 0, &result);
  int font_fd = -1;
  int good_enough_index = -1;
  bool good_enough_index_set = false;

  if (font_set) {
    for (int i = 0; i < font_set->nfont; ++i) {
      FcPattern* current = font_set->fonts[i];

      // Older versions of fontconfig have a bug where they cannot select
      // only scalable fonts so we have to manually filter the results.
      FcBool is_scalable;
      if (FcPatternGetBool(current, FC_SCALABLE, 0, &is_scalable) !=
              FcResultMatch ||
          !is_scalable) {
        continue;
      }

      FcChar8* c_filename;
      if (FcPatternGetString(current, FC_FILE, 0, &c_filename) !=
          FcResultMatch) {
        continue;
      }

      // We only want to return sfnt (TrueType) based fonts. We don't have a
      // very good way of detecting this so we'll filter based on the
      // filename.
      bool is_sfnt = false;
      static const char kSFNTExtensions[][5] = {".ttf", ".otc", ".TTF", ".ttc",
                                                ""};
      const size_t filename_len = strlen(reinterpret_cast<char*>(c_filename));
      for (unsigned j = 0;; j++) {
        if (kSFNTExtensions[j][0] == 0) {
          // None of the extensions matched.
          break;
        }
        const size_t ext_len = strlen(kSFNTExtensions[j]);
        if (filename_len > ext_len &&
            memcmp(c_filename + filename_len - ext_len,
                   kSFNTExtensions[j],
                   ext_len) == 0) {
          is_sfnt = true;
          break;
        }
      }

      if (!is_sfnt)
        continue;

      // This font is good enough to pass muster, but we might be able to do
      // better with subsequent ones.
      if (!good_enough_index_set) {
        good_enough_index = i;
        good_enough_index_set = true;
      }

      FcValue matrix;
      bool have_matrix = FcPatternGet(current, FC_MATRIX, 0, &matrix) == 0;

      if (is_italic && have_matrix) {
        // we asked for an italic font, but fontconfig is giving us a
        // non-italic font with a transformation matrix.
        continue;
      }

      FcValue embolden;
      const bool have_embolden =
          FcPatternGet(current, FC_EMBOLDEN, 0, &embolden) == 0;

      if (is_bold && have_embolden) {
        // we asked for a bold font, but fontconfig gave us a non-bold font
        // and asked us to apply fake bolding.
        continue;
      }

      font_fd = open(reinterpret_cast<char*>(c_filename), O_RDONLY);
      if (font_fd >= 0)
        break;
    }
  }

  if (font_fd == -1 && good_enough_index_set) {
    // We didn't find a font that we liked, so we fallback to something
    // acceptable.
    FcPattern* current = font_set->fonts[good_enough_index];
    FcChar8* c_filename;
    FcPatternGetString(current, FC_FILE, 0, &c_filename);
    font_fd = open(reinterpret_cast<char*>(c_filename), O_RDONLY);
  }

  if (font_set)
    FcFontSetDestroy(font_set);
  FcPatternDestroy(pattern);

  Pickle reply;
  SendRendererReply(fds, reply, font_fd);

  if (font_fd >= 0) {
    if (IGNORE_EINTR(close(font_fd)) < 0)
      PLOG(ERROR) << "close";
  }
}

void SandboxIPCHandler::SendRendererReply(
    const std::vector<base::ScopedFD*>& fds,
    const Pickle& reply,
    int reply_fd) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov = {const_cast<void*>(reply.data()), reply.size()};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char control_buffer[CMSG_SPACE(sizeof(int))];

  if (reply_fd != -1) {
    struct stat st;
    if (fstat(reply_fd, &st) == 0 && S_ISDIR(st.st_mode)) {
      LOG(FATAL) << "Tried to send a directory descriptor over sandbox IPC";
      // We must never send directory descriptors to a sandboxed process
      // because they can use openat with ".." elements in the path in order
      // to escape the sandbox and reach the real filesystem.
    }

    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = sizeof(control_buffer);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &reply_fd, sizeof(reply_fd));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  if (HANDLE_EINTR(sendmsg(fds[0]->get(), &msg, MSG_DONTWAIT)) < 0)
    PLOG(ERROR) << "sendmsg";
}

SandboxIPCHandler::~SandboxIPCHandler() {
  paths_.deleteAll();
  if (webkit_platform_support_)
    blink::shutdownWithoutV8();

  if (IGNORE_EINTR(close(lifeline_fd_)) < 0)
    PLOG(ERROR) << "close";
  if (IGNORE_EINTR(close(browser_socket_)) < 0)
    PLOG(ERROR) << "close";
}

void SandboxIPCHandler::EnsureWebKitInitialized() {
  if (webkit_platform_support_)
    return;
  webkit_platform_support_.reset(new BlinkPlatformImpl);
  blink::initializeWithoutV8(webkit_platform_support_.get());
}

}  // namespace content
