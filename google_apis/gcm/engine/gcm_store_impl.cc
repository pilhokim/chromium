// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_store_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/tracked_objects.h"
#include "components/os_crypt/os_crypt.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace gcm {

namespace {

// Limit to the number of outstanding messages per app.
const int kMessagesPerAppLimit = 20;

// ---- LevelDB keys. ----
// Key for this device's android id.
const char kDeviceAIDKey[] = "device_aid_key";
// Key for this device's android security token.
const char kDeviceTokenKey[] = "device_token_key";
// Lowest lexicographically ordered app ids.
// Used for prefixing app id.
const char kRegistrationKeyStart[] = "reg1-";
// Key guaranteed to be higher than all app ids.
// Used for limiting iteration.
const char kRegistrationKeyEnd[] = "reg2-";
// Lowest lexicographically ordered incoming message key.
// Used for prefixing messages.
const char kIncomingMsgKeyStart[] = "incoming1-";
// Key guaranteed to be higher than all incoming message keys.
// Used for limiting iteration.
const char kIncomingMsgKeyEnd[] = "incoming2-";
// Lowest lexicographically ordered outgoing message key.
// Used for prefixing outgoing messages.
const char kOutgoingMsgKeyStart[] = "outgoing1-";
// Key guaranteed to be higher than all outgoing message keys.
// Used for limiting iteration.
const char kOutgoingMsgKeyEnd[] = "outgoing2-";

std::string MakeRegistrationKey(const std::string& app_id) {
  return kRegistrationKeyStart + app_id;
}

std::string ParseRegistrationKey(const std::string& key) {
  return key.substr(arraysize(kRegistrationKeyStart) - 1);
}

std::string MakeIncomingKey(const std::string& persistent_id) {
  return kIncomingMsgKeyStart + persistent_id;
}

std::string MakeOutgoingKey(const std::string& persistent_id) {
  return kOutgoingMsgKeyStart + persistent_id;
}

std::string ParseOutgoingKey(const std::string& key) {
  return key.substr(arraysize(kOutgoingMsgKeyStart) - 1);
}

// Note: leveldb::Slice keeps a pointer to the data in |s|, which must therefore
// outlive the slice.
// For example: MakeSlice(MakeOutgoingKey(x)) is invalid.
leveldb::Slice MakeSlice(const base::StringPiece& s) {
  return leveldb::Slice(s.begin(), s.size());
}

}  // namespace

class GCMStoreImpl::Backend
    : public base::RefCountedThreadSafe<GCMStoreImpl::Backend> {
 public:
  Backend(const base::FilePath& path,
          scoped_refptr<base::SequencedTaskRunner> foreground_runner);

  // Blocking implementations of GCMStoreImpl methods.
  void Load(const LoadCallback& callback);
  void Close();
  void Destroy(const UpdateCallback& callback);
  void SetDeviceCredentials(uint64 device_android_id,
                            uint64 device_security_token,
                            const UpdateCallback& callback);
  void AddRegistration(const std::string& app_id,
                       const linked_ptr<RegistrationInfo>& registration,
                       const UpdateCallback& callback);
  void RemoveRegistration(const std::string& app_id,
                          const UpdateCallback& callback);
  void AddIncomingMessage(const std::string& persistent_id,
                          const UpdateCallback& callback);
  void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                              const UpdateCallback& callback);
  void AddOutgoingMessage(const std::string& persistent_id,
                          const MCSMessage& message,
                          const UpdateCallback& callback);
  void RemoveOutgoingMessages(
      const PersistentIdList& persistent_ids,
      const base::Callback<void(bool, const AppIdToMessageCountMap&)>
          callback);
  void AddUserSerialNumber(const std::string& username,
                           int64 serial_number,
                           const UpdateCallback& callback);
  void RemoveUserSerialNumber(const std::string& username,
                              const UpdateCallback& callback);
  void SetNextSerialNumber(int64 serial_number, const UpdateCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<Backend>;
  ~Backend();

  bool LoadDeviceCredentials(uint64* android_id, uint64* security_token);
  bool LoadRegistrations(RegistrationInfoMap* registrations);
  bool LoadIncomingMessages(std::vector<std::string>* incoming_messages);
  bool LoadOutgoingMessages(OutgoingMessageMap* outgoing_messages);

  const base::FilePath path_;
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;

  scoped_ptr<leveldb::DB> db_;
};

GCMStoreImpl::Backend::Backend(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> foreground_task_runner)
    : path_(path), foreground_task_runner_(foreground_task_runner) {}

GCMStoreImpl::Backend::~Backend() {}

void GCMStoreImpl::Backend::Load(const LoadCallback& callback) {
  scoped_ptr<LoadResult> result(new LoadResult());
  if (db_.get()) {
    LOG(ERROR) << "Attempting to reload open database.";
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback,
                                                 base::Passed(&result)));
    return;
  }

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status =
      leveldb::DB::Open(options, path_.AsUTF8Unsafe(), &db);
  UMA_HISTOGRAM_BOOLEAN("GCM.LoadSucceeded", status.ok());
  if (!status.ok()) {
    LOG(ERROR) << "Failed to open database " << path_.value() << ": "
               << status.ToString();
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback,
                                                 base::Passed(&result)));
    return;
  }
  db_.reset(db);

  if (!LoadDeviceCredentials(&result->device_android_id,
                             &result->device_security_token) ||
      !LoadRegistrations(&result->registrations) ||
      !LoadIncomingMessages(&result->incoming_messages) ||
      !LoadOutgoingMessages(&result->outgoing_messages)) {
    result->device_android_id = 0;
    result->device_security_token = 0;
    result->registrations.clear();
    result->incoming_messages.clear();
    result->outgoing_messages.clear();
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback,
                                                 base::Passed(&result)));
    return;
  }

  // Only record histograms if GCM had already been set up for this device.
  if (result->device_android_id != 0 && result->device_security_token != 0) {
    int64 file_size = 0;
    if (base::GetFileSize(path_, &file_size)) {
      UMA_HISTOGRAM_COUNTS("GCM.StoreSizeKB",
                           static_cast<int>(file_size / 1024));
    }
    UMA_HISTOGRAM_COUNTS("GCM.RestoredRegistrations",
                         result->registrations.size());
    UMA_HISTOGRAM_COUNTS("GCM.RestoredOutgoingMessages",
                         result->outgoing_messages.size());
    UMA_HISTOGRAM_COUNTS("GCM.RestoredIncomingMessages",
                         result->incoming_messages.size());
  }

  DVLOG(1) << "Succeeded in loading " << result->registrations.size()
           << " registrations, "
           << result->incoming_messages.size()
           << " unacknowledged incoming messages and "
           << result->outgoing_messages.size()
           << " unacknowledged outgoing messages.";
  result->success = true;
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback,
                                               base::Passed(&result)));
  return;
}

void GCMStoreImpl::Backend::Close() {
  DVLOG(1) << "Closing GCM store.";
  db_.reset();
}

void GCMStoreImpl::Backend::Destroy(const UpdateCallback& callback) {
  DVLOG(1) << "Destroying GCM store.";
  db_.reset();
  const leveldb::Status s =
      leveldb::DestroyDB(path_.AsUTF8Unsafe(), leveldb::Options());
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "Destroy failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::SetDeviceCredentials(
    uint64 device_android_id,
    uint64 device_security_token,
    const UpdateCallback& callback) {
  DVLOG(1) << "Saving device credentials with AID " << device_android_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string encrypted_token;
  OSCrypt::EncryptString(base::Uint64ToString(device_security_token),
                         &encrypted_token);
  std::string android_id_str = base::Uint64ToString(device_android_id);
  leveldb::Status s =
      db_->Put(write_options,
               MakeSlice(kDeviceAIDKey),
               MakeSlice(android_id_str));
  if (s.ok()) {
    s = db_->Put(
        write_options, MakeSlice(kDeviceTokenKey), MakeSlice(encrypted_token));
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::AddRegistration(
    const std::string& app_id,
    const linked_ptr<RegistrationInfo>& registration,
    const UpdateCallback& callback) {
  DVLOG(1) << "Saving registration info for app: " << app_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string key = MakeRegistrationKey(app_id);
  std::string value = registration->SerializeAsString();
  const leveldb::Status status = db_->Put(write_options,
                                          MakeSlice(key),
                                          MakeSlice(value));
  if (status.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << status.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::RemoveRegistration(const std::string& app_id,
                                               const UpdateCallback& callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status status = db_->Delete(write_options, MakeSlice(app_id));
  if (status.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << status.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::AddIncomingMessage(const std::string& persistent_id,
                                               const UpdateCallback& callback) {
  DVLOG(1) << "Saving incoming message with id " << persistent_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string key = MakeIncomingKey(persistent_id);
  const leveldb::Status s = db_->Put(write_options,
                                     MakeSlice(key),
                                     MakeSlice(persistent_id));
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::RemoveIncomingMessages(
    const PersistentIdList& persistent_ids,
    const UpdateCallback& callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s;
  for (PersistentIdList::const_iterator iter = persistent_ids.begin();
       iter != persistent_ids.end();
       ++iter) {
    DVLOG(1) << "Removing incoming message with id " << *iter;
    std::string key = MakeIncomingKey(*iter);
    s = db_->Delete(write_options, MakeSlice(key));
    if (!s.ok())
      break;
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::AddOutgoingMessage(const std::string& persistent_id,
                                               const MCSMessage& message,
                                               const UpdateCallback& callback) {
  DVLOG(1) << "Saving outgoing message with id " << persistent_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string data =
      static_cast<char>(message.tag()) + message.SerializeAsString();
  std::string key = MakeOutgoingKey(persistent_id);
  const leveldb::Status s = db_->Put(write_options,
                                     MakeSlice(key),
                                     MakeSlice(data));
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
}

void GCMStoreImpl::Backend::RemoveOutgoingMessages(
    const PersistentIdList& persistent_ids,
    const base::Callback<void(bool, const AppIdToMessageCountMap&)>
        callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback,
                                                 false,
                                                 AppIdToMessageCountMap()));
    return;
  }
  leveldb::ReadOptions read_options;
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  AppIdToMessageCountMap removed_message_counts;

  leveldb::Status s;
  for (PersistentIdList::const_iterator iter = persistent_ids.begin();
       iter != persistent_ids.end();
       ++iter) {
    DVLOG(1) << "Removing outgoing message with id " << *iter;
    std::string outgoing_message;
    std::string key = MakeOutgoingKey(*iter);
    s = db_->Get(read_options,
                 MakeSlice(key),
                 &outgoing_message);
    if (!s.ok())
      break;
    mcs_proto::DataMessageStanza data_message;
    // Skip the initial tag byte and parse the rest to extract the message.
    if (data_message.ParseFromString(outgoing_message.substr(1))) {
      DCHECK(!data_message.category().empty());
      if (removed_message_counts.count(data_message.category()) != 0)
        removed_message_counts[data_message.category()]++;
      else
        removed_message_counts[data_message.category()] = 1;
    }
    DVLOG(1) << "Removing outgoing message with id " << *iter;
    s = db_->Delete(write_options, MakeSlice(key));
    if (!s.ok())
      break;
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback,
                                                 true,
                                                 removed_message_counts));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback,
                                               false,
                                               AppIdToMessageCountMap()));
}

bool GCMStoreImpl::Backend::LoadDeviceCredentials(uint64* android_id,
                                                  uint64* security_token) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::string result;
  leveldb::Status s = db_->Get(read_options, MakeSlice(kDeviceAIDKey), &result);
  if (s.ok()) {
    if (!base::StringToUint64(result, android_id)) {
      LOG(ERROR) << "Failed to restore device id.";
      return false;
    }
    result.clear();
    s = db_->Get(read_options, MakeSlice(kDeviceTokenKey), &result);
  }
  if (s.ok()) {
    std::string decrypted_token;
    OSCrypt::DecryptString(result, &decrypted_token);
    if (!base::StringToUint64(decrypted_token, security_token)) {
      LOG(ERROR) << "Failed to restore security token.";
      return false;
    }
    return true;
  }

  if (s.IsNotFound()) {
    DVLOG(1) << "No credentials found.";
    return true;
  }

  LOG(ERROR) << "Error reading credentials from store.";
  return false;
}

bool GCMStoreImpl::Backend::LoadRegistrations(
    RegistrationInfoMap* registrations) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kRegistrationKeyStart));
       iter->Valid() && iter->key().ToString() < kRegistrationKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.size() <= 1) {
      LOG(ERROR) << "Error reading registration with key " << s.ToString();
      return false;
    }
    std::string app_id = ParseRegistrationKey(iter->key().ToString());
    linked_ptr<RegistrationInfo> registration(new RegistrationInfo);
    if (!registration->ParseFromString(iter->value().ToString())) {
      LOG(ERROR) << "Failed to parse registration with app id " << app_id;
      return false;
    }
    DVLOG(1) << "Found registration with app id " << app_id;
    (*registrations)[app_id] = registration;
  }

  return true;
}

bool GCMStoreImpl::Backend::LoadIncomingMessages(
    std::vector<std::string>* incoming_messages) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kIncomingMsgKeyStart));
       iter->Valid() && iter->key().ToString() < kIncomingMsgKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.empty()) {
      LOG(ERROR) << "Error reading incoming message with key "
                 << iter->key().ToString();
      return false;
    }
    DVLOG(1) << "Found incoming message with id " << s.ToString();
    incoming_messages->push_back(s.ToString());
  }

  return true;
}

bool GCMStoreImpl::Backend::LoadOutgoingMessages(
    OutgoingMessageMap* outgoing_messages) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kOutgoingMsgKeyStart));
       iter->Valid() && iter->key().ToString() < kOutgoingMsgKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.size() <= 1) {
      LOG(ERROR) << "Error reading incoming message with key " << s.ToString();
      return false;
    }
    uint8 tag = iter->value().data()[0];
    std::string id = ParseOutgoingKey(iter->key().ToString());
    scoped_ptr<google::protobuf::MessageLite> message(
        BuildProtobufFromTag(tag));
    if (!message.get() ||
        !message->ParseFromString(iter->value().ToString().substr(1))) {
      LOG(ERROR) << "Failed to parse outgoing message with id " << id
                 << " and tag " << tag;
      return false;
    }
    DVLOG(1) << "Found outgoing message with id " << id << " of type "
             << base::IntToString(tag);
    (*outgoing_messages)[id] = make_linked_ptr(message.release());
  }

  return true;
}

GCMStoreImpl::GCMStoreImpl(
    bool use_mock_keychain,
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : backend_(new Backend(path, base::MessageLoopProxy::current())),
      blocking_task_runner_(blocking_task_runner),
      weak_ptr_factory_(this) {
// On OSX, prevent the Keychain permissions popup during unit tests.
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(use_mock_keychain);
#endif
}

GCMStoreImpl::~GCMStoreImpl() {}

void GCMStoreImpl::Load(const LoadCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::Load,
                 backend_,
                 base::Bind(&GCMStoreImpl::LoadContinuation,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback)));
}

void GCMStoreImpl::Close() {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::Close, backend_));
}

void GCMStoreImpl::Destroy(const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::Destroy, backend_, callback));
}

void GCMStoreImpl::SetDeviceCredentials(uint64 device_android_id,
                                        uint64 device_security_token,
                                        const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::SetDeviceCredentials,
                 backend_,
                 device_android_id,
                 device_security_token,
                 callback));
}

void GCMStoreImpl::AddRegistration(
    const std::string& app_id,
    const linked_ptr<RegistrationInfo>& registration,
    const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::AddRegistration,
                 backend_,
                 app_id,
                 registration,
                 callback));
}

void GCMStoreImpl::RemoveRegistration(const std::string& app_id,
                                          const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::RemoveRegistration,
                 backend_,
                 app_id,
                 callback));
}

void GCMStoreImpl::AddIncomingMessage(const std::string& persistent_id,
                                      const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::AddIncomingMessage,
                 backend_,
                 persistent_id,
                 callback));
}

void GCMStoreImpl::RemoveIncomingMessage(const std::string& persistent_id,
                                         const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::RemoveIncomingMessages,
                 backend_,
                 PersistentIdList(1, persistent_id),
                 callback));
}

void GCMStoreImpl::RemoveIncomingMessages(
    const PersistentIdList& persistent_ids,
    const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::RemoveIncomingMessages,
                 backend_,
                 persistent_ids,
                 callback));
}

bool GCMStoreImpl::AddOutgoingMessage(const std::string& persistent_id,
                                      const MCSMessage& message,
                                      const UpdateCallback& callback) {
  DCHECK_EQ(message.tag(), kDataMessageStanzaTag);
  std::string app_id = reinterpret_cast<const mcs_proto::DataMessageStanza*>(
                           &message.GetProtobuf())->category();
  DCHECK(!app_id.empty());
  if (app_message_counts_.count(app_id) == 0)
    app_message_counts_[app_id] = 0;
  if (app_message_counts_[app_id] < kMessagesPerAppLimit) {
    app_message_counts_[app_id]++;

    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GCMStoreImpl::Backend::AddOutgoingMessage,
                   backend_,
                   persistent_id,
                   message,
                   base::Bind(&GCMStoreImpl::AddOutgoingMessageContinuation,
                              weak_ptr_factory_.GetWeakPtr(),
                              callback,
                              app_id)));
    return true;
  }
  return false;
}

void GCMStoreImpl::OverwriteOutgoingMessage(const std::string& persistent_id,
                                            const MCSMessage& message,
                                            const UpdateCallback& callback) {
  DCHECK_EQ(message.tag(), kDataMessageStanzaTag);
  std::string app_id = reinterpret_cast<const mcs_proto::DataMessageStanza*>(
                           &message.GetProtobuf())->category();
  DCHECK(!app_id.empty());
  // There should already be pending messages for this app.
  DCHECK(app_message_counts_.count(app_id));
  // TODO(zea): consider verifying the specific message already exists.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::AddOutgoingMessage,
                 backend_,
                 persistent_id,
                 message,
                 callback));
}

void GCMStoreImpl::RemoveOutgoingMessage(const std::string& persistent_id,
                                         const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::RemoveOutgoingMessages,
                 backend_,
                 PersistentIdList(1, persistent_id),
                 base::Bind(&GCMStoreImpl::RemoveOutgoingMessagesContinuation,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback)));
}

void GCMStoreImpl::RemoveOutgoingMessages(
    const PersistentIdList& persistent_ids,
    const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMStoreImpl::Backend::RemoveOutgoingMessages,
                 backend_,
                 persistent_ids,
                 base::Bind(&GCMStoreImpl::RemoveOutgoingMessagesContinuation,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback)));
}

void GCMStoreImpl::LoadContinuation(const LoadCallback& callback,
                                    scoped_ptr<LoadResult> result) {
  if (!result->success) {
    callback.Run(result.Pass());
    return;
  }
  int num_throttled_apps = 0;
  for (OutgoingMessageMap::const_iterator
           iter = result->outgoing_messages.begin();
       iter != result->outgoing_messages.end(); ++iter) {
    const mcs_proto::DataMessageStanza* data_message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(iter->second.get());
    DCHECK(!data_message->category().empty());
    if (app_message_counts_.count(data_message->category()) == 0)
      app_message_counts_[data_message->category()] = 1;
    else
      app_message_counts_[data_message->category()]++;
    if (app_message_counts_[data_message->category()] == kMessagesPerAppLimit)
      num_throttled_apps++;
  }
  UMA_HISTOGRAM_COUNTS("GCM.NumThrottledApps", num_throttled_apps);
  callback.Run(result.Pass());
}

void GCMStoreImpl::AddOutgoingMessageContinuation(
    const UpdateCallback& callback,
    const std::string& app_id,
    bool success) {
  if (!success) {
    DCHECK(app_message_counts_[app_id] > 0);
    app_message_counts_[app_id]--;
  }
  callback.Run(success);
}

void GCMStoreImpl::RemoveOutgoingMessagesContinuation(
    const UpdateCallback& callback,
    bool success,
    const AppIdToMessageCountMap& removed_message_counts) {
  if (!success) {
    callback.Run(false);
    return;
  }
  for (AppIdToMessageCountMap::const_iterator iter =
           removed_message_counts.begin();
       iter != removed_message_counts.end(); ++iter) {
    DCHECK_NE(app_message_counts_.count(iter->first), 0U);
    app_message_counts_[iter->first] -= iter->second;
    DCHECK_GE(app_message_counts_[iter->first], 0);
  }
  callback.Run(true);
}

}  // namespace gcm
