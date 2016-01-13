// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_mac.h"

#import <Cocoa/Cocoa.h>

#include <CoreFoundation/CFTimeZone.h>
extern "C" {
#include <sandbox.h>
}
#include <signal.h>
#include <sys/param.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "grit/content_resources.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/base/layout.h"
#include "ui/gl/gl_surface.h"

namespace content {
namespace {

// Is the sandbox currently active.
bool gSandboxIsActive = false;

struct SandboxTypeToResourceIDMapping {
  SandboxType sandbox_type;
  int sandbox_profile_resource_id;
};

// Mapping from sandbox process types to resource IDs containing the sandbox
// profile for all process types known to content.
SandboxTypeToResourceIDMapping kDefaultSandboxTypeToResourceIDMapping[] = {
  { SANDBOX_TYPE_RENDERER, IDR_RENDERER_SANDBOX_PROFILE },
  { SANDBOX_TYPE_WORKER,   IDR_WORKER_SANDBOX_PROFILE },
  { SANDBOX_TYPE_UTILITY,  IDR_UTILITY_SANDBOX_PROFILE },
  { SANDBOX_TYPE_GPU,      IDR_GPU_SANDBOX_PROFILE },
  { SANDBOX_TYPE_PPAPI,    IDR_PPAPI_SANDBOX_PROFILE },
};

COMPILE_ASSERT(arraysize(kDefaultSandboxTypeToResourceIDMapping) == \
               size_t(SANDBOX_TYPE_AFTER_LAST_TYPE), \
               sandbox_type_to_resource_id_mapping_incorrect);

// Try to escape |c| as a "SingleEscapeCharacter" (\n, etc).  If successful,
// returns true and appends the escape sequence to |dst|.
bool EscapeSingleChar(char c, std::string* dst) {
  const char *append = NULL;
  switch (c) {
    case '\b':
      append = "\\b";
      break;
    case '\f':
      append = "\\f";
      break;
    case '\n':
      append = "\\n";
      break;
    case '\r':
      append = "\\r";
      break;
    case '\t':
      append = "\\t";
      break;
    case '\\':
      append = "\\\\";
      break;
    case '"':
      append = "\\\"";
      break;
  }

  if (!append) {
    return false;
  }

  dst->append(append);
  return true;
}

// Errors quoting strings for the Sandbox profile are always fatal, report them
// in a central place.
NOINLINE void FatalStringQuoteException(const std::string& str) {
  // Copy bad string to the stack so it's recorded in the crash dump.
  char bad_string[256] = {0};
  base::strlcpy(bad_string, str.c_str(), arraysize(bad_string));
  DLOG(FATAL) << "String quoting failed " << bad_string;
}

}  // namespace

// static
NSString* Sandbox::AllowMetadataForPath(const base::FilePath& allowed_path) {
  // Collect a list of all parent directories.
  base::FilePath last_path = allowed_path;
  std::vector<base::FilePath> subpaths;
  for (base::FilePath path = allowed_path;
       path.value() != last_path.value();
       path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through all parents and allow stat() on them explicitly.
  NSString* sandbox_command = @"(allow file-read-metadata ";
  for (std::vector<base::FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend();
       ++i) {
    std::string subdir_escaped;
    if (!QuotePlainString(i->value(), &subdir_escaped)) {
      FatalStringQuoteException(i->value());
      return nil;
    }

    NSString* subdir_escaped_ns =
        base::SysUTF8ToNSString(subdir_escaped.c_str());
    sandbox_command =
        [sandbox_command stringByAppendingFormat:@"(literal \"%@\")",
            subdir_escaped_ns];
  }

  return [sandbox_command stringByAppendingString:@")"];
}

// static
bool Sandbox::QuotePlainString(const std::string& src_utf8, std::string* dst) {
  dst->clear();

  const char* src = src_utf8.c_str();
  int32_t length = src_utf8.length();
  int32_t position = 0;
  while (position < length) {
    UChar32 c;
    U8_NEXT(src, position, length, c);  // Macro increments |position|.
    DCHECK_GE(c, 0);
    if (c < 0)
      return false;

    if (c < 128) {  // EscapeSingleChar only handles ASCII.
      char as_char = static_cast<char>(c);
      if (EscapeSingleChar(as_char, dst)) {
        continue;
      }
    }

    if (c < 32 || c > 126) {
      // Any characters that aren't printable ASCII get the \u treatment.
      unsigned int as_uint = static_cast<unsigned int>(c);
      base::StringAppendF(dst, "\\u%04X", as_uint);
      continue;
    }

    // If we got here we know that the character in question is strictly
    // in the ASCII range so there's no need to do any kind of encoding
    // conversion.
    dst->push_back(static_cast<char>(c));
  }
  return true;
}

// static
bool Sandbox::QuoteStringForRegex(const std::string& str_utf8,
                                  std::string* dst) {
  // Characters with special meanings in sandbox profile syntax.
  const char regex_special_chars[] = {
    '\\',

    // Metacharacters
    '^',
    '.',
    '[',
    ']',
    '$',
    '(',
    ')',
    '|',

    // Quantifiers
    '*',
    '+',
    '?',
    '{',
    '}',
  };

  // Anchor regex at start of path.
  dst->assign("^");

  const char* src = str_utf8.c_str();
  int32_t length = str_utf8.length();
  int32_t position = 0;
  while (position < length) {
    UChar32 c;
    U8_NEXT(src, position, length, c);  // Macro increments |position|.
    DCHECK_GE(c, 0);
    if (c < 0)
      return false;

    // The Mac sandbox regex parser only handles printable ASCII characters.
    // 33 >= c <= 126
    if (c < 32 || c > 125) {
      return false;
    }

    for (size_t i = 0; i < arraysize(regex_special_chars); ++i) {
      if (c == regex_special_chars[i]) {
        dst->push_back('\\');
        break;
      }
    }

    dst->push_back(static_cast<char>(c));
  }

  // Make sure last element of path is interpreted as a directory. Leaving this
  // off would allow access to files if they start with the same name as the
  // directory.
  dst->append("(/|$)");

  return true;
}

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6.0

// static
void Sandbox::SandboxWarmup(int sandbox_type) {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  { // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
    base::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

    // Allocate a 1x1 image.
    char data[4];
    base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
        data,
        1,
        1,
        8,
        1 * 4,
        rgb_colorspace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

    // Load in the color profiles we'll need (as a side effect).
    (void) base::mac::GetSRGBColorSpace();
    (void) base::mac::GetSystemColorSpace();

    // CGColorSpaceCreateSystemDefaultCMYK - 10.6
    base::ScopedCFTypeRef<CGColorSpaceRef> cmyk_colorspace(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK));
  }

  { // localtime() - 10.5.6
    time_t tv = {0};
    localtime(&tv);
  }

  { // Gestalt() tries to read /System/Library/CoreServices/SystemVersion.plist
    // on 10.5.6
    int32 tmp;
    base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
  }

  {  // CGImageSourceGetStatus() - 10.6
     // Create a png with just enough data to get everything warmed up...
    char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    NSData* data = [NSData dataWithBytes:png_header
                                  length:arraysize(png_header)];
    base::ScopedCFTypeRef<CGImageSourceRef> img(
        CGImageSourceCreateWithData((CFDataRef)data, NULL));
    CGImageSourceGetStatus(img);
  }

  {
    // Allow access to /dev/urandom.
    base::GetUrandomFD();
  }

  { // IOSurfaceLookup() - 10.7
    // Needed by zero-copy texture update framework - crbug.com/323338
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceLookup(0));
  }

  // Process-type dependent warm-up.
  if (sandbox_type == SANDBOX_TYPE_UTILITY) {
    // CFTimeZoneCopyZone() tries to read /etc and /private/etc/localtime - 10.8
    // Needed by Media Galleries API Picasa - crbug.com/151701
    CFTimeZoneCopySystem();
  }

  if (sandbox_type == SANDBOX_TYPE_GPU) {
    // Preload either the desktop GL or the osmesa so, depending on the
    // --use-gl flag.
    gfx::GLSurface::InitializeOneOff();
  }

  if (sandbox_type == SANDBOX_TYPE_PPAPI) {
    // Preload AppKit color spaces used for Flash/ppapi. http://crbug.com/348304
    NSColor* color = [NSColor controlTextColor];
    [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  }
}

// static
NSString* Sandbox::BuildAllowDirectoryAccessSandboxString(
    const base::FilePath& allowed_dir,
    SandboxVariableSubstitions* substitutions) {
  // A whitelist is used to determine which directories can be statted
  // This means that in the case of an /a/b/c/d/ directory, we may be able to
  // stat the leaf directory, but not its parent.
  // The extension code in Chrome calls realpath() which fails if it can't call
  // stat() on one of the parent directories in the path.
  // The solution to this is to allow statting the parent directories themselves
  // but not their contents.  We need to add a separate rule for each parent
  // directory.

  // The sandbox only understands "real" paths.  This resolving step is
  // needed so the caller doesn't need to worry about things like /var
  // being a link to /private/var (like in the paths CreateNewTempDirectory()
  // returns).
  base::FilePath allowed_dir_canonical = GetCanonicalSandboxPath(allowed_dir);

  NSString* sandbox_command = AllowMetadataForPath(allowed_dir_canonical);
  sandbox_command = [sandbox_command
      substringToIndex:[sandbox_command length] - 1];  // strip trailing ')'

  // Finally append the leaf directory.  Unlike its parents (for which only
  // stat() should be allowed), the leaf directory needs full access.
  (*substitutions)["ALLOWED_DIR"] =
      SandboxSubstring(allowed_dir_canonical.value(),
                       SandboxSubstring::REGEX);
  sandbox_command =
      [sandbox_command
          stringByAppendingString:@") (allow file-read* file-write*"
                                   " (regex #\"@ALLOWED_DIR@\") )"];
  return sandbox_command;
}

// Load the appropriate template for the given sandbox type.
// Returns the template as an NSString or nil on error.
NSString* LoadSandboxTemplate(int sandbox_type) {
  // We use a custom sandbox definition to lock things down as tightly as
  // possible.
  int sandbox_profile_resource_id = -1;

  // Find resource id for sandbox profile to use for the specific sandbox type.
  for (size_t i = 0;
       i < arraysize(kDefaultSandboxTypeToResourceIDMapping);
       ++i) {
    if (kDefaultSandboxTypeToResourceIDMapping[i].sandbox_type ==
        sandbox_type) {
      sandbox_profile_resource_id =
          kDefaultSandboxTypeToResourceIDMapping[i].sandbox_profile_resource_id;
      break;
    }
  }
  if (sandbox_profile_resource_id == -1) {
    // Check if the embedder knows about this sandbox process type.
    bool sandbox_type_found =
        GetContentClient()->GetSandboxProfileForSandboxType(
            sandbox_type, &sandbox_profile_resource_id);
    CHECK(sandbox_type_found) << "Unknown sandbox type " << sandbox_type;
  }

  base::StringPiece sandbox_definition =
      GetContentClient()->GetDataResource(
          sandbox_profile_resource_id, ui::SCALE_FACTOR_NONE);
  if (sandbox_definition.empty()) {
    LOG(FATAL) << "Failed to load the sandbox profile (resource id "
               << sandbox_profile_resource_id << ")";
    return nil;
  }

  base::StringPiece common_sandbox_definition =
      GetContentClient()->GetDataResource(
          IDR_COMMON_SANDBOX_PROFILE, ui::SCALE_FACTOR_NONE);
  if (common_sandbox_definition.empty()) {
    LOG(FATAL) << "Failed to load the common sandbox profile";
    return nil;
  }

  base::scoped_nsobject<NSString> common_sandbox_prefix_data(
      [[NSString alloc] initWithBytes:common_sandbox_definition.data()
                               length:common_sandbox_definition.length()
                             encoding:NSUTF8StringEncoding]);

  base::scoped_nsobject<NSString> sandbox_data(
      [[NSString alloc] initWithBytes:sandbox_definition.data()
                               length:sandbox_definition.length()
                             encoding:NSUTF8StringEncoding]);

  // Prefix sandbox_data with common_sandbox_prefix_data.
  return [common_sandbox_prefix_data stringByAppendingString:sandbox_data];
}

// static
bool Sandbox::PostProcessSandboxProfile(
        NSString* sandbox_template,
        NSArray* comments_to_remove,
        SandboxVariableSubstitions& substitutions,
        std::string *final_sandbox_profile_str) {
  NSString* sandbox_data = [[sandbox_template copy] autorelease];

  // Remove comments, e.g. ;10.7_OR_ABOVE .
  for (NSString* to_remove in comments_to_remove) {
    sandbox_data = [sandbox_data stringByReplacingOccurrencesOfString:to_remove
                                                           withString:@""];
  }

  // Split string on "@" characters.
  std::vector<std::string> raw_sandbox_pieces;
  if (Tokenize([sandbox_data UTF8String], "@", &raw_sandbox_pieces) == 0) {
    DLOG(FATAL) << "Bad Sandbox profile, should contain at least one token ("
                << [sandbox_data UTF8String]
                << ")";
    return false;
  }

  // Iterate over string pieces and substitute variables, escaping as necessary.
  size_t output_string_length = 0;
  std::vector<std::string> processed_sandbox_pieces(raw_sandbox_pieces.size());
  for (std::vector<std::string>::iterator it = raw_sandbox_pieces.begin();
       it != raw_sandbox_pieces.end();
       ++it) {
    std::string new_piece;
    SandboxVariableSubstitions::iterator replacement_it =
        substitutions.find(*it);
    if (replacement_it == substitutions.end()) {
      new_piece = *it;
    } else {
      // Found something to substitute.
      SandboxSubstring& replacement = replacement_it->second;
      switch (replacement.type()) {
        case SandboxSubstring::PLAIN:
          new_piece = replacement.value();
          break;

        case SandboxSubstring::LITERAL:
          if (!QuotePlainString(replacement.value(), &new_piece))
            FatalStringQuoteException(replacement.value());
          break;

        case SandboxSubstring::REGEX:
          if (!QuoteStringForRegex(replacement.value(), &new_piece))
            FatalStringQuoteException(replacement.value());
          break;
      }
    }
    output_string_length += new_piece.size();
    processed_sandbox_pieces.push_back(new_piece);
  }

  // Build final output string.
  final_sandbox_profile_str->reserve(output_string_length);

  for (std::vector<std::string>::iterator it = processed_sandbox_pieces.begin();
       it != processed_sandbox_pieces.end();
       ++it) {
    final_sandbox_profile_str->append(*it);
  }
  return true;
}


// Turns on the OS X sandbox for this process.

// static
bool Sandbox::EnableSandbox(int sandbox_type,
                            const base::FilePath& allowed_dir) {
  // Sanity - currently only SANDBOX_TYPE_UTILITY supports a directory being
  // passed in.
  if (sandbox_type < SANDBOX_TYPE_AFTER_LAST_TYPE &&
      sandbox_type != SANDBOX_TYPE_UTILITY) {
    DCHECK(allowed_dir.empty())
        << "Only SANDBOX_TYPE_UTILITY allows a custom directory parameter.";
  }

  NSString* sandbox_data = LoadSandboxTemplate(sandbox_type);
  if (!sandbox_data) {
    return false;
  }

  SandboxVariableSubstitions substitutions;
  if (!allowed_dir.empty()) {
    // Add the sandbox commands necessary to access the given directory.
    // Note: this function must be called before PostProcessSandboxProfile()
    // since the string it inserts contains variables that need substitution.
    NSString* allowed_dir_sandbox_command =
        BuildAllowDirectoryAccessSandboxString(allowed_dir, &substitutions);

    if (allowed_dir_sandbox_command) {  // May be nil if function fails.
      sandbox_data = [sandbox_data
          stringByReplacingOccurrencesOfString:@";ENABLE_DIRECTORY_ACCESS"
                                    withString:allowed_dir_sandbox_command];
    }
  }

  NSMutableArray* tokens_to_remove = [NSMutableArray array];

  // Enable verbose logging if enabled on the command line. (See common.sb
  // for details).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(switches::kEnableSandboxLogging);;
  if (enable_logging) {
    [tokens_to_remove addObject:@";ENABLE_LOGGING"];
  }

  bool lion_or_later = base::mac::IsOSLionOrLater();

  // Without this, the sandbox will print a message to the system log every
  // time it denies a request.  This floods the console with useless spew.
  if (!enable_logging) {
    substitutions["DISABLE_SANDBOX_DENIAL_LOGGING"] =
        SandboxSubstring("(with no-log)");
  } else {
    substitutions["DISABLE_SANDBOX_DENIAL_LOGGING"] = SandboxSubstring("");
  }

  // Splice the path of the user's home directory into the sandbox profile
  // (see renderer.sb for details).
  std::string home_dir = [NSHomeDirectory() fileSystemRepresentation];

  base::FilePath home_dir_canonical =
      GetCanonicalSandboxPath(base::FilePath(home_dir));

  substitutions["USER_HOMEDIR_AS_LITERAL"] =
      SandboxSubstring(home_dir_canonical.value(),
          SandboxSubstring::LITERAL);

  if (lion_or_later) {
    // >=10.7 Sandbox rules.
    [tokens_to_remove addObject:@";10.7_OR_ABOVE"];
  }

  substitutions["COMPONENT_BUILD_WORKAROUND"] = SandboxSubstring("");
#if defined(COMPONENT_BUILD)
  // dlopen() fails without file-read-metadata access if the executable image
  // contains LC_RPATH load commands. The components build uses those.
  // See http://crbug.com/127465
  if (base::mac::IsOSSnowLeopard()) {
    base::FilePath bundle_executable = base::mac::NSStringToFilePath(
        [base::mac::MainBundle() executablePath]);
    NSString* sandbox_command = AllowMetadataForPath(
        GetCanonicalSandboxPath(bundle_executable));
    substitutions["COMPONENT_BUILD_WORKAROUND"] =
        SandboxSubstring(base::SysNSStringToUTF8(sandbox_command));
  }
#endif

  // All information needed to assemble the final profile has been collected.
  // Merge it all together.
  std::string final_sandbox_profile_str;
  if (!PostProcessSandboxProfile(sandbox_data, tokens_to_remove, substitutions,
                                 &final_sandbox_profile_str)) {
    return false;
  }

  // Initialize sandbox.
  char* error_buff = NULL;
  int error = sandbox_init(final_sandbox_profile_str.c_str(), 0, &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  DLOG_IF(FATAL, !success) << "Failed to initialize sandbox: "
                           << error
                           << " "
                           << error_buff;
  sandbox_free_error(error_buff);
  gSandboxIsActive = success;
  return success;
}

// static
bool Sandbox::SandboxIsCurrentlyActive() {
  return gSandboxIsActive;
}

// static
base::FilePath Sandbox::GetCanonicalSandboxPath(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(FATAL) << "GetCanonicalSandboxPath() failed for: "
                 << path.value();
    return path;
  }

  base::FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd.get(), F_GETPATH, canonical_path)) != 0) {
    DPLOG(FATAL) << "GetCanonicalSandboxPath() failed for: "
                 << path.value();
    return path;
  }

  return base::FilePath(canonical_path);
}

}  // namespace content
