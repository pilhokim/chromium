// Copyright (c) 2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "base/debug/trace_event.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chromium_logger.h"
#include "env_chromium_stdio.h"
#include "env_chromium_win.h"

using namespace leveldb;

namespace leveldb_env {

namespace {

static std::string GetWindowsErrorMessage(DWORD err) {
  LPTSTR errorText(NULL);
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                err,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) & errorText,
                0,
                NULL);
  if (errorText != NULL) {
    std::string message(base::UTF16ToUTF8(errorText));
    // FormatMessage adds CR/LF to messages so we remove it.
    TrimWhitespace(message, TRIM_TRAILING, &message);
    LocalFree(errorText);
    return message;
  } else {
    return std::string();
  }
}

class ChromiumSequentialFileWin : public SequentialFile {
 private:
  std::string filename_;
  HANDLE file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumSequentialFileWin(const std::string& fname,
                         HANDLE f,
                         const UMALogger* uma_logger)
      : filename_(fname), file_(f), uma_logger_(uma_logger) {
    DCHECK(file_ != INVALID_HANDLE_VALUE);
  }
  virtual ~ChromiumSequentialFileWin() {
    DCHECK(file_ != INVALID_HANDLE_VALUE);
    CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
  }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s;
    DWORD bytes_read(0);
    DCHECK(file_ != INVALID_HANDLE_VALUE);
    if (ReadFile(file_, scratch, n, &bytes_read, NULL)) {
      *result = Slice(scratch, bytes_read);
    } else {
      DWORD err = GetLastError();
      s = MakeIOErrorWin(
          filename_, GetWindowsErrorMessage(err), kSequentialFileRead, err);
      uma_logger_->RecordErrorAt(kSequentialFileRead);
      if (bytes_read > 0)
        *result = Slice(scratch, bytes_read);
    }
    return s;
  }

  virtual Status Skip(uint64_t n) {
    LARGE_INTEGER li;
    li.QuadPart = n;
    if (SetFilePointer(file_, li.LowPart, &li.HighPart, FILE_CURRENT) ==
        INVALID_SET_FILE_POINTER) {
      DWORD err = GetLastError();
      uma_logger_->RecordErrorAt(kSequentialFileSkip);
      return MakeIOErrorWin(
          filename_, GetWindowsErrorMessage(err), kSequentialFileSkip, err);
    }
    return Status::OK();
  }
};

class ChromiumRandomAccessFileWin : public RandomAccessFile {
 private:
  std::string filename_;
  ::base::PlatformFile file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumRandomAccessFileWin(const std::string& fname,
                           ::base::PlatformFile file,
                           const UMALogger* uma_logger)
      : filename_(fname), file_(file), uma_logger_(uma_logger) {}
  virtual ~ChromiumRandomAccessFileWin() { ::base::ClosePlatformFile(file_); }

  virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch)
      const {
    Status s;
    int r = ::base::ReadPlatformFile(file_, offset, scratch, n);
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = MakeIOError(
          filename_, "Could not perform read", kRandomAccessFileRead);
      uma_logger_->RecordErrorAt(kRandomAccessFileRead);
    }
    return s;
  }
};

}  // unnamed namespace

Status MakeIOErrorWin(Slice filename,
                        const std::string& message,
                        MethodID method,
                        DWORD error) {
  char buf[512];
  if (snprintf(buf,
               sizeof(buf),
               "%s (ChromeMethodErrno: %d::%s::%u)",
               message.c_str(),
               method,
               MethodIDToString(method),
               error) >= 0) {
    return Status::IOError(filename, buf);
  } else {
    return Status::IOError(filename, "<unknown>");
  }
}

ChromiumWritableFileWin::ChromiumWritableFileWin(
    const std::string& fname,
    HANDLE f,
    const UMALogger* uma_logger,
    WriteTracker* tracker,
    bool make_backup)
    : filename_(fname),
      file_(f),
      uma_logger_(uma_logger),
      tracker_(tracker),
      file_type_(kOther),
      make_backup_(make_backup) {
  DCHECK(f != INVALID_HANDLE_VALUE);
  base::FilePath path = base::FilePath::FromUTF8Unsafe(fname);
  if (FilePathToString(path.BaseName()).find("MANIFEST") == 0)
    file_type_ = kManifest;
  else if (ChromiumEnv::HasTableExtension(path))
    file_type_ = kTable;
  if (file_type_ != kManifest)
    tracker_->DidCreateNewFile(filename_);
  parent_dir_ = FilePathToString(ChromiumEnv::CreateFilePath(fname).DirName());
}

ChromiumWritableFileWin::~ChromiumWritableFileWin() {
  if (file_ != INVALID_HANDLE_VALUE) {
    // Ignoring any potential errors

    CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
  }
}

Status ChromiumWritableFileWin::SyncParent() {
  // On Windows no need to sync parent directory. It's metadata will be
  // updated via the creation of the new file, without an explicit sync.
  return Status();
}

Status ChromiumWritableFileWin::Append(const Slice& data) {
  if (file_type_ == kManifest && tracker_->DoesDirNeedSync(filename_)) {
    Status s = SyncParent();
    if (!s.ok())
      return s;
    tracker_->DidSyncDir(filename_);
  }

  DWORD written(0);
  if (!WriteFile(file_, data.data(), data.size(), &written, NULL)) {
    DWORD err = GetLastError();
    uma_logger_->RecordOSError(kWritableFileAppend,
                               base::LastErrorToPlatformFileError(err));
    return MakeIOErrorWin(
        filename_, GetWindowsErrorMessage(err), kWritableFileAppend, err);
  }
  return Status::OK();
}

Status ChromiumWritableFileWin::Close() {
  Status result;
  DCHECK(file_ != INVALID_HANDLE_VALUE);
  if (!CloseHandle(file_)) {
    DWORD err = GetLastError();
    result = MakeIOErrorWin(
        filename_, GetWindowsErrorMessage(err), kWritableFileClose, err);
    uma_logger_->RecordErrorAt(kWritableFileClose);
  }
  file_ = INVALID_HANDLE_VALUE;
  return result;
}

Status ChromiumWritableFileWin::Flush() {
  Status result;
  if (!FlushFileBuffers(file_)) {
    DWORD err = GetLastError();
    result = MakeIOErrorWin(
        filename_, GetWindowsErrorMessage(err), kWritableFileFlush, err);
    uma_logger_->RecordOSError(kWritableFileFlush,
                               base::LastErrorToPlatformFileError(err));
  }
  return result;
}

Status ChromiumWritableFileWin::Sync() {
  TRACE_EVENT0("leveldb", "ChromiumEnvWin::Sync");
  Status result;
  DWORD error = ERROR_SUCCESS;

  DCHECK(file_ != INVALID_HANDLE_VALUE);
  if (!FlushFileBuffers(file_))
    error = GetLastError();
  if (error != ERROR_SUCCESS) {
    result = MakeIOErrorWin(
        filename_, GetWindowsErrorMessage(error), kWritableFileSync, error);
    uma_logger_->RecordErrorAt(kWritableFileSync);
  } else if (make_backup_ && file_type_ == kTable) {
    bool success = ChromiumEnv::MakeBackup(filename_);
    uma_logger_->RecordBackupResult(success);
  }
  return result;
}

ChromiumEnvWin::ChromiumEnvWin() {}

ChromiumEnvWin::~ChromiumEnvWin() {}

Status ChromiumEnvWin::NewSequentialFile(const std::string& fname,
                                           SequentialFile** result) {
  HANDLE f = CreateFile(base::UTF8ToUTF16(fname).c_str(),
                        GENERIC_READ,
                        FILE_SHARE_READ,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
  if (f == INVALID_HANDLE_VALUE) {
    *result = NULL;
    DWORD err = GetLastError();
    RecordOSError(kNewSequentialFile, err);
    return MakeIOErrorWin(
        fname, GetWindowsErrorMessage(err), kNewSequentialFile, err);
  } else {
    *result = new ChromiumSequentialFileWin(fname, f, this);
    return Status::OK();
  }
}

void ChromiumEnvWin::RecordOpenFilesLimit(const std::string& type) {
  // The Windows POSIX implementation (which this class doesn't use)
  // has an open file limit, but when using the Win32 API this is limited by
  // available memory, so no value to report.
}

Status ChromiumEnvWin::NewRandomAccessFile(const std::string& fname,
                                             RandomAccessFile** result) {
  int flags = ::base::PLATFORM_FILE_READ | ::base::PLATFORM_FILE_OPEN;
  bool created;
  ::base::PlatformFileError error_code;
  ::base::PlatformFile file = ::base::CreatePlatformFile(
      ChromiumEnv::CreateFilePath(fname), flags, &created, &error_code);
  if (error_code == ::base::PLATFORM_FILE_OK) {
    *result = new ChromiumRandomAccessFileWin(fname, file, this);
    RecordOpenFilesLimit("Success");
    return Status::OK();
  }
  if (error_code == ::base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED)
    RecordOpenFilesLimit("TooManyOpened");
  else
    RecordOpenFilesLimit("OtherError");
  *result = NULL;
  RecordOSError(kNewRandomAccessFile, error_code);
  return MakeIOErrorWin(fname,
                          PlatformFileErrorString(error_code),
                          kNewRandomAccessFile,
                          error_code);
}

Status ChromiumEnvWin::NewWritableFile(const std::string& fname,
                                         WritableFile** result) {
  *result = NULL;
  HANDLE f = CreateFile(base::UTF8ToUTF16(fname).c_str(),
                        GENERIC_WRITE,
                        FILE_SHARE_READ,
                        NULL,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
  if (f == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    RecordErrorAt(kNewWritableFile);
    return MakeIOErrorWin(
        fname, GetWindowsErrorMessage(err), kNewWritableFile, err);
  } else {
    *result = new ChromiumWritableFileWin(fname, f, this, this, make_backup_);
    return Status::OK();
  }
}

base::PlatformFileError ChromiumEnvWin::GetDirectoryEntries(
    const base::FilePath& dir_param,
    std::vector<base::FilePath>* result) const {
  result->clear();
  base::FilePath dir_filepath = dir_param.Append(FILE_PATH_LITERAL("*"));
  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFile(dir_filepath.value().c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_FILE_NOT_FOUND)
      return base::PLATFORM_FILE_OK;
    return base::LastErrorToPlatformFileError(last_error);
  }
  do {
    base::FilePath filepath(find_data.cFileName);
    base::FilePath::StringType basename = filepath.BaseName().value();
    if (basename == FILE_PATH_LITERAL(".") ||
        basename == FILE_PATH_LITERAL(".."))
      continue;
    result->push_back(filepath.BaseName());
  } while (FindNextFile(find_handle, &find_data));
  DWORD last_error = GetLastError();
  base::PlatformFileError return_value = base::PLATFORM_FILE_OK;
  if (last_error != ERROR_NO_MORE_FILES)
    return_value = base::LastErrorToPlatformFileError(last_error);
  FindClose(find_handle);
  return return_value;
}

Status ChromiumEnvWin::NewLogger(const std::string& fname, Logger** result) {
  FILE* f = _wfopen(base::UTF8ToUTF16(fname).c_str(), L"w");
  if (f == NULL) {
    *result = NULL;
    int saved_errno = errno;
    RecordOSError(kNewLogger, saved_errno);
    return MakeIOError(fname, strerror(saved_errno), kNewLogger, saved_errno);
  } else {
    *result = new ChromiumLogger(f);
    return Status::OK();
  }
}

void ChromiumEnvWin::RecordOSError(MethodID method, DWORD error) const {
  RecordErrorAt(method);
  GetOSErrorHistogram(method, ERANGE + 1)->Add(error);
}

}  // namespace leveldb_env