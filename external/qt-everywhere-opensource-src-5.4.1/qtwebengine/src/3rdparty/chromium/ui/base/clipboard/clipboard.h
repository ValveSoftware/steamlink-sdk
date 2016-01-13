// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/ui_base_export.h"

#if defined(OS_WIN)
#include <objidl.h>
#elif defined(OS_ANDROID)
#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include "base/memory/scoped_ptr.h"
#endif

namespace base {
class FilePath;

namespace win {
class MessageWindow;
}  // namespace win
}  // namespace base

// TODO(dcheng): Temporary until the IPC layer doesn't use WriteObjects().
namespace content {
class ClipboardMessageFilter;
}

namespace gfx {
class Size;
}

class SkBitmap;

#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

namespace ui {
class ClipboardTest;
class ScopedClipboardWriter;

class UI_BASE_EXPORT Clipboard : NON_EXPORTED_BASE(public base::ThreadChecker) {
 public:
  // MIME type constants.
  static const char kMimeTypeText[];
  static const char kMimeTypeURIList[];
  static const char kMimeTypeDownloadURL[];
  static const char kMimeTypeHTML[];
  static const char kMimeTypeRTF[];
  static const char kMimeTypePNG[];

  // Platform neutral holder for native data representation of a clipboard type.
  struct UI_BASE_EXPORT FormatType {
    FormatType();
    ~FormatType();

    // Serializes and deserializes a FormatType for use in IPC messages.
    std::string Serialize() const;
    static FormatType Deserialize(const std::string& serialization);

#if defined(USE_AURA)
    // FormatType can be used in a set on some platforms.
    bool operator<(const FormatType& other) const;
#endif

#if !defined(TOOLKIT_QT)
#if defined(OS_WIN)
    const FORMATETC& ToFormatEtc() const { return data_; }
#elif defined(USE_AURA)
    const std::string& ToString() const { return data_; }
#elif defined(OS_MACOSX)
    // Custom copy and assignment constructor to handle NSString.
    FormatType(const FormatType& other);
    FormatType& operator=(const FormatType& other);
#endif
#else
    const std::string& ToString() const { return data_; }
#endif // !defined(TOOLKIT_QT)

    bool Equals(const FormatType& other) const;

   private:
    friend class Clipboard;

    // Platform-specific glue used internally by the Clipboard class. Each
    // plaform should define,at least one of each of the following:
    // 1. A constructor that wraps that native clipboard format descriptor.
    // 2. An accessor to retrieve the wrapped descriptor.
    // 3. A data member to hold the wrapped descriptor.
    //
    // Note that in some cases, the accessor for the wrapped descriptor may be
    // public, as these format types can be used by drag and drop code as well.
#if defined(TOOLKIT_QT)
    explicit FormatType(const std::string& native_format);
    std::string data_;
#elif defined(OS_WIN)
    explicit FormatType(UINT native_format);
    FormatType(UINT native_format, LONG index);
    UINT ToUINT() const { return data_.cfFormat; }
    FORMATETC data_;
#elif defined(USE_AURA)
    explicit FormatType(const std::string& native_format);
    const std::string& data() const { return data_; }
    std::string data_;
#elif defined(OS_MACOSX)
    explicit FormatType(NSString* native_format);
    NSString* ToNSString() const { return data_; }
    NSString* data_;
#elif defined(OS_ANDROID)
    explicit FormatType(const std::string& native_format);
    const std::string& data() const { return data_; }
    std::string data_;
#else
#error No FormatType definition.
#endif

    // Copyable and assignable, since this is essentially an opaque value type.
  };

  // TODO(dcheng): Make this private once the IPC layer no longer needs to
  // serialize this information.
  // ObjectType designates the type of data to be stored in the clipboard. This
  // designation is shared across all OSes. The system-specific designation
  // is defined by FormatType. A single ObjectType might be represented by
  // several system-specific FormatTypes. For example, on Linux the CBF_TEXT
  // ObjectType maps to "text/plain", "STRING", and several other formats. On
  // windows it maps to CF_UNICODETEXT.
  enum ObjectType {
    CBF_TEXT,
    CBF_HTML,
    CBF_RTF,
    CBF_BOOKMARK,
    CBF_WEBKIT,
    CBF_SMBITMAP,  // Bitmap from shared memory.
    CBF_DATA,  // Arbitrary block of bytes.
  };

  // ObjectMap is a map from ObjectType to associated data.
  // The data is organized differently for each ObjectType. The following
  // table summarizes what kind of data is stored for each key.
  // * indicates an optional argument.
  //
  // Key           Arguments    Type
  // -------------------------------------
  // CBF_TEXT      text         char array
  // CBF_HTML      html         char array
  //               url*         char array
  // CBF_RTF       data         byte array
  // CBF_BOOKMARK  html         char array
  //               url          char array
  // CBF_WEBKIT    none         empty vector
  // CBF_SMBITMAP  shared_mem   A pointer to an unmapped base::SharedMemory
  //                            object containing the bitmap data. The bitmap
  //                            data should be premultiplied.
  //               size         gfx::Size struct
  // CBF_DATA      format       char array
  //               data         byte array
  typedef std::vector<char> ObjectMapParam;
  typedef std::vector<ObjectMapParam> ObjectMapParams;
  typedef std::map<int /* ObjectType */, ObjectMapParams> ObjectMap;

  static bool IsSupportedClipboardType(int32 type) {
    switch (type) {
      case CLIPBOARD_TYPE_COPY_PASTE:
        return true;
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
      case CLIPBOARD_TYPE_SELECTION:
        return true;
#endif
    }
    return false;
  }

  static ClipboardType FromInt(int32 type) {
    return static_cast<ClipboardType>(type);
  }

  // Sets the list of threads that are allowed to access the clipboard.
  static void SetAllowedThreads(
      const std::vector<base::PlatformThreadId>& allowed_threads);

  // Returns the clipboard object for the current thread.
  //
  // Most implementations will have at most one clipboard which will live on
  // the main UI thread, but Windows has tricky semantics where there have to
  // be two clipboards: one that lives on the UI thread and one that lives on
  // the IO thread.
  static Clipboard* GetForCurrentThread();

  // Destroys the clipboard for the current thread. Usually, this will clean up
  // all clipboards, except on Windows. (Previous code leaks the IO thread
  // clipboard, so it shouldn't be a problem.)
  static void DestroyClipboardForCurrentThread();

  // Returns a sequence number which uniquely identifies clipboard state.
  // This can be used to version the data on the clipboard and determine
  // whether it has changed.
  uint64 GetSequenceNumber(ClipboardType type);

  // Tests whether the clipboard contains a certain format
  bool IsFormatAvailable(const FormatType& format, ClipboardType type) const;

  // Clear the clipboard data.
  void Clear(ClipboardType type);

  void ReadAvailableTypes(ClipboardType type,
                          std::vector<base::string16>* types,
                          bool* contains_filenames) const;

  // Reads UNICODE text from the clipboard, if available.
  void ReadText(ClipboardType type, base::string16* result) const;

  // Reads ASCII text from the clipboard, if available.
  void ReadAsciiText(ClipboardType type, std::string* result) const;

  // Reads HTML from the clipboard, if available. If the HTML fragment requires
  // context to parse, |fragment_start| and |fragment_end| are indexes into
  // markup indicating the beginning and end of the actual fragment. Otherwise,
  // they will contain 0 and markup->size().
  void ReadHTML(ClipboardType type,
                base::string16* markup,
                std::string* src_url,
                uint32* fragment_start,
                uint32* fragment_end) const;

  // Reads RTF from the clipboard, if available. Stores the result as a byte
  // vector.
  void ReadRTF(ClipboardType type, std::string* result) const;

  // Reads an image from the clipboard, if available.
  SkBitmap ReadImage(ClipboardType type) const;

  void ReadCustomData(ClipboardType clipboard_type,
                      const base::string16& type,
                      base::string16* result) const;

  // Reads a bookmark from the clipboard, if available.
  void ReadBookmark(base::string16* title, std::string* url) const;

  // Reads raw data from the clipboard with the given format type. Stores result
  // as a byte vector.
  void ReadData(const FormatType& format, std::string* result) const;

  // Gets the FormatType corresponding to an arbitrary format string,
  // registering it with the system if needed. Due to Windows/Linux
  // limitiations, |format_string| must never be controlled by the user.
  static FormatType GetFormatType(const std::string& format_string);

  // Get format identifiers for various types.
  static const FormatType& GetUrlFormatType();
  static const FormatType& GetUrlWFormatType();
  static const FormatType& GetMozUrlFormatType();
  static const FormatType& GetPlainTextFormatType();
  static const FormatType& GetPlainTextWFormatType();
  static const FormatType& GetFilenameFormatType();
  static const FormatType& GetFilenameWFormatType();
  static const FormatType& GetWebKitSmartPasteFormatType();
  // Win: MS HTML Format, Other: Generic HTML format
  static const FormatType& GetHtmlFormatType();
  static const FormatType& GetRtfFormatType();
  static const FormatType& GetBitmapFormatType();
  // TODO(raymes): Unify web custom data and pepper custom data:
  // crbug.com/158399.
  static const FormatType& GetWebCustomDataFormatType();
  static const FormatType& GetPepperCustomDataFormatType();

  // Embeds a pointer to a SharedMemory object pointed to by |bitmap_handle|
  // belonging to |process| into a shared bitmap [CBF_SMBITMAP] slot in
  // |objects|.  The pointer is deleted by DispatchObjects().
  //
  // On non-Windows platforms, |process| is ignored.
  static bool ReplaceSharedMemHandle(ObjectMap* objects,
                                     base::SharedMemoryHandle bitmap_handle,
                                     base::ProcessHandle process)
      WARN_UNUSED_RESULT;
#if defined(OS_WIN)
  // Firefox text/html
  static const FormatType& GetTextHtmlFormatType();
  static const FormatType& GetCFHDropFormatType();
  static const FormatType& GetFileDescriptorFormatType();
  static const FormatType& GetFileContentZeroFormatType();
  static const FormatType& GetIDListFormatType();
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardTest, SharedBitmapTest);
  FRIEND_TEST_ALL_PREFIXES(ClipboardTest, EmptyHTMLTest);
  friend class ClipboardTest;
  // For access to WriteObjects().
  // TODO(dcheng): Remove the temporary exception for content.
  friend class content::ClipboardMessageFilter;
  friend class ScopedClipboardWriter;

  Clipboard();
  ~Clipboard();

  // Write a bunch of objects to the system clipboard. Copies are made of the
  // contents of |objects|.
  void WriteObjects(ClipboardType type, const ObjectMap& objects);

  void DispatchObject(ObjectType type, const ObjectMapParams& params);

  void WriteText(const char* text_data, size_t text_len);

  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len);

  void WriteRTF(const char* rtf_data, size_t data_len);

  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len);

  void WriteWebSmartPaste();

  void WriteBitmap(const SkBitmap& bitmap);

  void WriteData(const FormatType& format,
                 const char* data_data,
                 size_t data_len);
#if !defined(TOOLKIT_QT) && defined(OS_WIN)
  void WriteBitmapFromHandle(HBITMAP source_hbitmap,
                             const gfx::Size& size);

  // Safely write to system clipboard. Free |handle| on failure.
  void WriteToClipboard(unsigned int format, HANDLE handle);

  static void ParseBookmarkClipboardFormat(const base::string16& bookmark,
                                           base::string16* title,
                                           std::string* url);

  // Free a handle depending on its type (as intuited from format)
  static void FreeData(unsigned int format, HANDLE data);

  // Return the window that should be the clipboard owner, creating it
  // if neccessary.  Marked const for lazily initialization by const methods.
  HWND GetClipboardWindow() const;

  // Mark this as mutable so const methods can still do lazy initialization.
  mutable scoped_ptr<base::win::MessageWindow> clipboard_owner_;

#elif defined(USE_CLIPBOARD_AURAX11)
 private:
  // We keep our implementation details private because otherwise we bring in
  // the X11 headers and break chrome compile.
  class AuraX11Details;
  scoped_ptr<AuraX11Details> aurax11_details_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_H_
