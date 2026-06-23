/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if BUILDFLAG(IS_ARKWEB)
#include "arkweb/chromium_ext/components/os_crypt/sync/os_crypt_linux_for_include.h"
#endif

#if BUILDFLAG(ARKWEB_ENCRYPT)
#include "base/containers/span.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#endif

#if BUILDFLAG(ARKWEB_ENCRYPT)
#define GCM_TAG_SIZE 16
#define GCM_IV_SIZE 12
#endif

namespace {

// LCOV_EXCL_START
#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
constexpr base::FilePath::CharType kNWebAssetHandleDir[] =
    FILE_PATH_LITERAL("migrate_bak");
constexpr char kNewbAssetHandleAlias[] = "asset_data_key";
#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
base::FilePath GetMigrateBasePath() {
  base::FilePath path;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &path)) {
    base::FilePath migratePath = path.Append(FILE_PATH_LITERAL("migrate"));
    if (base::PathExists(migratePath)) {
      return path;
    }
  }
 
  base::PathService::Get(base::DIR_CACHE, &path);
  return path;
}

void SetMigratePasswordsFlagToFile() {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SetMigratePasswordsFlagToFile));
    return;
  }

  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(browser_prefs::kMigratePasswordsToPasswordVault, true);
    g_browser_process->local_state()->CommitPendingWrite();
  }
}

static std::string AssetQuery(base::FilePath key_file) {
  std::string assetHandle;
  bool res = base::ReadFileToString(key_file, &assetHandle);
  if (!res) {
    LOG(ERROR) << "[Autofill] Read assethandle file failed.";
    std::string err_msg = "Read assethandle file failed, error_code:" +
                          std::to_string(ASSET_QUERY_FAILED);
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentBrowser, base::ohos::kDefaultUrl,
                                  base::ohos::kPasswordManagerError, err_msg);
    SetMigratePasswordsFlagToFile();
    return std::string();
  }

  if (assetHandle.empty()) {
    LOG(INFO) << "[Autofill] Assethandle is empty, not need to migrate.";
    std::string err_msg = "Assethandle is empty, not need to migrate, error_code:" +
                          std::to_string(MIGRATE_SUCCESS);
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentBrowser, base::ohos::kDefaultUrl,
                                  base::ohos::kPasswordManagerError, err_msg);
    SetMigratePasswordsFlagToFile();
    return std::string();
  }

  std::string local_key = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                          .GetKeystoreAdapterInstance().AssetQuery(assetHandle);
  if (local_key.empty()) {
    LOG(ERROR) << "[Autofill] Get key from asset failed.";
    std::string err_msg = "Get key from asset failed, error_code:" +
                          std::to_string(ASSET_QUERY_FAILED);
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentBrowser, base::ohos::kDefaultUrl,
                                  base::ohos::kPasswordManagerError, err_msg);
    SetMigratePasswordsFlagToFile();
    return std::string();
  }
  LOG(INFO) << "[Autofill] get key from asset success.";
  return local_key;
}

static std::string GetKeyFromAsset() {
  base::FilePath cache_path = GetMigrateBasePath();
  if (cache_path.empty()) {
    return std::string();
  }

  base::FilePath key_dir =
      cache_path.Append(FILE_PATH_LITERAL(kNWebAssetHandleDir));
  if (!base::PathExists(key_dir)) {
    LOG(ERROR) << "[Autofill] Assethandle dir not exist.";
    std::string err_msg = "Assethandle dir not exist, error_code:" +
                          std::to_string(DIRECTORY_OR_FILE_NOT_EXIST) + ", data_dir:" +
                          cache_path.value();
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentBrowser, base::ohos::kDefaultUrl,
                                  base::ohos::kPasswordManagerError, err_msg);
    SetMigratePasswordsFlagToFile();
    return std::string();
  }

  base::FilePath key_file = key_dir.Append(
    FILE_PATH_LITERAL(crypto::ohos::get_asset_handle_file_256(kNewbAssetHandleAlias)));
  if (!base::PathExists(key_file)) {
    LOG(ERROR) << "[Autofill] Assethandle file not exist.";
    std::string err_msg = "Assethandle file not exist, error_code:" +
                          std::to_string(DIRECTORY_OR_FILE_NOT_EXIST) + ", data_dir:" +
                          cache_path.value();
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentBrowser, base::ohos::kDefaultUrl,
                                  base::ohos::kPasswordManagerError, err_msg);
    SetMigratePasswordsFlagToFile();
    return std::string();
  }

  std::string local_key = AssetQuery(key_file);
  return local_key;
}

std::unique_ptr<crypto::SymmetricKey> GenerateEncryptionKeyForMigrate() {
  std::string asset_key = GetKeyFromAsset();
  if (asset_key.empty()) {
    return nullptr;
  }

  std::vector<uint8_t> key_byte_array;

  base::HexStringToBytes(asset_key, &key_byte_array);

  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::Import(
          crypto::SymmetricKey::AES,
          std::string((std::string::value_type*)(key_byte_array.data()),
                      key_byte_array.size())));

  DCHECK(encryption_key);

  return encryption_key;
}
#endif

#if BUILDFLAG(ARKWEB_ENCRYPT)
// Generates a newly allocated SymmetricKey object compatibility with ota.
// Ownership of the key is passed to the caller. Returns null key if a key
// generation error occurs.
std::unique_ptr<crypto::SymmetricKey> GenerateEncryptionKeyForOtaFail() {
  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::Import(
          crypto::SymmetricKey::AES,
          crypto::ohos::get_symmetric_key_256_for_ota(kDataKeyAlias)));

  return encryption_key;
}
#endif  // BUILDFLAG(ARKWEB_ENCRYPT)
// LCOV_EXCL_STOP

#if BUILDFLAG(ARKWEB_ENCRYPT)
bool DecryptWithIv(const std::string& ciphertext,
                   crypto::SymmetricKey* encryption_key,
                   std::string* plaintext,
                   std::string& iv) {
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::GCM, iv)) {
    return false;
  }

  return encryptor.Decrypt(ciphertext, plaintext);
}
#endif

}  // namespace

namespace OSCrypt {
#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
bool DecryptString16ForMigrate(const std::string& ciphertext,
                               std::u16string* plaintext) {
  return OSCryptImpl::GetInstance()->DecryptString16ForMigrate(ciphertext,
                                                               plaintext);
}
#endif
#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
bool DecryptStringForMigrate(const std::string& ciphertext,
                             std::string* plaintext) {
  return OSCryptImpl::GetInstance()->DecryptStringForMigrate(ciphertext,
                                                             plaintext);
}
#endif
}  // namespace OSCrypt

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
bool OSCryptImpl::DecryptString16ForMigrate(const std::string& ciphertext,
                                            std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptStringForMigrate(ciphertext, &utf8)) {
    return false;
  }

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}
#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
bool OSCryptImpl::DecryptStringForMigrate(const std::string& ciphertext,
                                          std::string* plaintext) {
  if (ciphertext.empty()) {
    plaintext->clear();
    return true;
  }

  // the incoming ciphertext was encrypted and with V10 version.
  crypto::SymmetricKey* encryption_key = GetPasswordV10ForMigrate();
  std::string obfuscation_prefix = kObfuscationPrefixV10;

  if (!encryption_key) {
    LOG(ERROR) << "[Autofill] Decryption failed: could not get the key";
    return false;
  }

  if (ciphertext.length() < (obfuscation_prefix.length() + kIVSizeAESGCM)) {
    return true;
  }
  std::string raw_ciphertext =
      ciphertext.substr(obfuscation_prefix.length() + kIVSizeAESGCM);
  std::string iv =
      ciphertext.substr(obfuscation_prefix.length(), kIVSizeAESGCM);

  if (DecryptWithIv(raw_ciphertext, encryption_key, plaintext, iv)) {
    return true;
  }

  LOG(ERROR) << "[Autofill] Decryption failed";
  base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, false);
  return false;
}

void OSCryptImpl::SetMigrationCountCurrent(const int& count) {
  migration_count_current_ = count;
}
#endif

// LCOV_EXCL_START
#if BUILDFLAG(ARKWEB_ENCRYPT)
crypto::SymmetricKey* OSCryptImpl::GetPasswordForOtaFail() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  if (!password_ota_cache_.get()) {
    password_ota_cache_ = GenerateEncryptionKeyForOtaFail();
  }
  return password_ota_cache_.get();
}

#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
crypto::SymmetricKey* OSCryptImpl::GetPasswordV10ForMigrate() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
#if BUILDFLAG(ARKWEB_TEST)
  int count = 1;
#else
  int count = migration_count_current_;
#endif
  if (!is_password_migrate_cached_ || migration_count_ < count) {
    migration_count_ = count;
    password_migrate_cache_ = GenerateEncryptionKeyForMigrate();
    is_password_migrate_cached_ = true;
  }
  return password_migrate_cache_.get();
}
#endif
// LCOV_EXCL_STOP

#if BUILDFLAG(ARKWEB_ENCRYPT)
// Generates a newly allocated SymmetricKey object based on a password.
// Ownership of the key is passed to the caller. Returns null key if a key
// generation error occurs.
std::unique_ptr<crypto::SymmetricKey> GenerateEncryptionKey() {
  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::Import(
          crypto::SymmetricKey::AES,
          crypto::ohos::get_symmetric_key_256(kDataKeyAlias)));

  DCHECK(encryption_key);

  return encryption_key;
}

// Returns a cached string of "peanuts". Is thread-safe.
crypto::SymmetricKey* OSCryptImpl::GetPasswordV10() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  if (!password_v10_cache_.get()) {
    password_v10_cache_ = GenerateEncryptionKey();
  }
  return password_v10_cache_.get();
}

// Caches and returns the password from the KeyStorage or null if there is no
// service. Is thread-safe.
crypto::SymmetricKey* OSCryptImpl::GetPasswordV11(bool probe) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  if (is_password_v11_cached_) {
    if (!config_) {
      return nullptr;
    }
    return password_v11_cache_.get();
  }

  std::unique_ptr<KeyStorageLinux> key_storage;
  if (storage_provider_factory_for_testing_) {
    key_storage = std::move(storage_provider_factory_for_testing_).Run();
  } else {
    CHECK(probe || config_);
    if (config_) {
      key_storage = KeyStorageLinux::CreateService(*config_);
      config_.reset();
    }
  }

  if (key_storage) {
    std::optional<std::string> key = key_storage->GetKey();
    if (key.has_value()) {
      password_v11_cache_ = GenerateEncryptionKey();
    }
  }

  is_password_v11_cached_ = true;
  return password_v11_cache_.get();
}
#endif

bool DecryptWithIvForInclude(const std::string& raw_ciphertext,
                             crypto::SymmetricKey* encryption_key,
                             crypto::SymmetricKey* encryption_key_ota,
                             std::string* plaintext,
                             std::string& iv,
                             bool& result) {
  if (DecryptWithIv(raw_ciphertext, encryption_key, plaintext, iv)) {
    result = true;
    return true;
  } else {
    // Retry use before second encrypted key to decrypt password
    if (!encryption_key_ota) {
      VLOG(1) << "Decryption failed: could not get the key in ota";
      result = false;
      return true;
    }
    if (DecryptWithIv(raw_ciphertext, encryption_key_ota, plaintext, iv)) {
      LOG(INFO) << "decryption success with ota compatible key";
      result = true;
      return true;
    }
  }
  return false;
}

#if BUILDFLAG(ARKWEB_ENCRYPT)
namespace crypto {

namespace {
  bool IsValidKeySize(size_t key_size_in_bytes) {
  // Nobody should ever be using other symmetric key sizes without consulting
  // with CRYPTO_OWNERS first, who can modify this check if need be.
  return key_size_in_bytes == 16 || key_size_in_bytes == 32;
}

const EVP_CIPHER* GetCipherForKey(const SymmetricKey* key) {
  switch (key->key().length()) {
    case 16: return EVP_aes_128_cbc();
    case 32: return EVP_aes_256_cbc();
    default:
      return nullptr;
  }
}

const EVP_CIPHER* GetCipherForKeyGCM(const SymmetricKey* key) {
  switch (key->key().length()) {
    case 16:
      return EVP_aes_128_gcm();
    case 32:
      return EVP_aes_256_gcm();
    default:
      return nullptr;
  }
}
} // namespace

SymmetricKey::SymmetricKey(base::span<const uint8_t> key_bytes)
    : key_(base::as_string_view(key_bytes)) {}

SymmetricKey::SymmetricKey(const SymmetricKey& other) = default;
SymmetricKey& SymmetricKey::operator=(const SymmetricKey& other) = default;

SymmetricKey::~SymmetricKey() {
  std::fill(key_.begin(), key_.end(), '\0');  // Zero out the confidential key.
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::Import(Algorithm,
                                                   const std::string& raw_key) {
  if (!IsValidKeySize(raw_key.size())) {
    return nullptr;
  }
  return std::make_unique<SymmetricKey>(base::as_byte_span(raw_key));
}

Encryptor::Encryptor() : key_(nullptr), mode_(CBC) {}

Encryptor::~Encryptor() = default;

bool Encryptor::Init(const SymmetricKey* key, Mode mode, std::string_view iv) {
  return Init(key, mode, base::as_bytes(base::span(iv)));
}

bool Encryptor::Init(const SymmetricKey* key,
                     Mode mode,
                     base::span<const uint8_t> iv) {
  DCHECK(key);
  DCHECK(mode == CBC || mode == CTR || mode == GCM);

  if (mode == CBC && iv.size() != AES_BLOCK_SIZE)
    return false;
  // CTR mode passes the starting counter separately, via SetCounter().
  if (mode == CTR && !iv.empty())
    return false;

  if (GetCipherForKey(key) == nullptr)
    return false;

  if (mode == GCM && iv.size() != GCM_IV_SIZE) {
    return false;
  }

  key_ = key;
  mode_ = mode;
  iv_.assign(iv.begin(), iv.end());
  return true;
}

bool Encryptor::Encrypt(std::string_view plaintext, std::string* ciphertext) {
  return CryptString(/*do_encrypt=*/true, plaintext, ciphertext);
}

bool Encryptor::Decrypt(std::string_view ciphertext, std::string* plaintext) {
  return CryptString(/*do_encrypt=*/false, ciphertext, plaintext);
}

bool Encryptor::CryptString(bool do_encrypt,
                            std::string_view input,
                            std::string* output) {
  std::string result(MaxOutput(do_encrypt, input.size()), '\0');

  std::optional<size_t> len;
  std::string tag(GCM_TAG_SIZE, 0);
  if (mode_ == GCM) {
    // only support DecryptGCM
    if (do_encrypt) {
      len = EncryptGCM(base::as_bytes(base::span{input}),
                       base::as_writable_bytes(base::span{result}), &tag);
    } else {
      // Get the tag that we attached with cipher during encryption from input
      if (input.length() <= GCM_TAG_SIZE) {
        LOG(WARNING) << "input size less than gcm tag size";
        return false;
      }

      tag = std::string(
          input.substr(input.length() - GCM_TAG_SIZE, GCM_TAG_SIZE));
      // Get the cipher part only from input
      std::string ciphertext =
          std::string(input.substr(0, input.length() - GCM_TAG_SIZE));
      const size_t output_size = ciphertext.length();
      if (output_size + 1 <= ciphertext.length()) {
        LOG(WARNING) << "output size occur overflow";
        return false;
      }
      len = DecryptGCM(ciphertext,
                       base::as_writable_bytes(base::span(result)), &tag);
    }
  } else {
    return false;
  }

  if (!len)
    return false;

  result.resize(*len);

  // concat tag with cipher at the end.
  if (mode_ == GCM && do_encrypt) {
    result += tag;
  }

  *output = std::move(result);
  return true;
}

size_t Encryptor::MaxOutput(bool do_encrypt, size_t length) {
  size_t result = length + ((do_encrypt && mode_ == CBC)   ? 16
                            : (do_encrypt && mode_ == GCM) ? 12
                                                           : 0);

  CHECK_GE(result, length);  // Overflow
  return result;
}

std::optional<size_t> Encryptor::EncryptGCM(base::span<const uint8_t> input,
                                            base::span<uint8_t> output,
                                            std::string* tag) {
  DCHECK(key_);
  DCHECK(output.data());
 
  const EVP_CIPHER* cipher = GetCipherForKeyGCM(key_);
  DCHECK(cipher);
 
  const std::string& key = key_->key();
  DCHECK_EQ(EVP_CIPHER_iv_length(cipher), iv_.size());
  DCHECK_EQ(EVP_CIPHER_key_length(cipher), key.size());
 
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
 
  bssl::ScopedEVP_CIPHER_CTX ctx;
  /* Initialise the encryption operation */
  if (!EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr)) {
    return std::nullopt;
  }

  /* Set the IV length, default is 12 byte which is not appropriate */
  if (!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv_.size(),
                           nullptr)) {
    return std::nullopt;
  }

  /* Initialise Key and IV */
  if (!EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
                          reinterpret_cast<const uint8_t*>(key.data()),
                          iv_.data())) {
    return std::nullopt;
  }

  const size_t output_size = input.size() + (iv_.size());
  CHECK_GT(output_size, 0u);
  CHECK_GT(output_size + 1, input.size());

  int out_len;

  /* Provide the plaintext and obtain the encrypted update
   *  EVP_EncryptUpdate may call multiples times if necessary
   */
  if (!EVP_EncryptUpdate(ctx.get(), output.data(), &out_len,
                         reinterpret_cast<const uint8_t*>(input.data()),
                         input.size())) {
    return std::nullopt;
  }

  /* Finalise the encryption. Usually ciphertext byte may be
   * written in this stage but this does not occure in GCM mode
   */
  int len;
  if (!EVP_EncryptFinal_ex(ctx.get(), output.data() + out_len, &len)) {
    return std::nullopt;
  }

  out_len += len;

  /*Get the tag */
  if (!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE,
                           (void*)tag->data())) {
    return std::nullopt;
  }

  DCHECK_LE(out_len, static_cast<int>(output_size));

  return out_len;
}

std::optional<size_t> Encryptor::DecryptGCM(const std::string& input,
                                             base::span<uint8_t> output,
                                             std::string* tag) {
  DCHECK(key_);
  DCHECK(output.data());

  const EVP_CIPHER* cipher = GetCipherForKeyGCM(key_);
  DCHECK(cipher);

  const std::string& key = key_->key();
  DCHECK_EQ(EVP_CIPHER_iv_length(cipher), iv_.size());
  DCHECK_EQ(EVP_CIPHER_key_length(cipher), key.size());

  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_CIPHER_CTX ctx;

  /* Initialise the decryption process */
  if (!EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr)) {
    return std::nullopt;
  }

  /* Set the IV length */
  if (!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv_.size(),
                           nullptr)) {
    return std::nullopt;
  }

  /* Initialise Key and IV */
  if (!EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
                          reinterpret_cast<const uint8_t*>(key.data()),
                          iv_.data())) {
    return std::nullopt;
  }

  int out_len;

  /* Provide the cipher text and get the output as plaintext */
  if (!EVP_DecryptUpdate(ctx.get(), output.data(), &out_len,
                         reinterpret_cast<const uint8_t*>(input.data()),
                         input.length())) {
    return std::nullopt;
  }

  /* Set expected tag which is used during encryption */
  if (!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE,
                           (void*)tag->data())) {
    return std::nullopt;
  }
  /* Finalise the decryption. If plaintext is not trustworthy then
   * it return as failure.
   */
  int len;
  int ret = EVP_DecryptFinal_ex(ctx.get(), output.data() + out_len, &len);
  if (ret > 0) {
    out_len += len;
  } else {
    return std::nullopt;
  }

  DCHECK_LE(out_len, static_cast<int>(input.length()));

  return out_len;
}
} // namespace crypto
#endif
