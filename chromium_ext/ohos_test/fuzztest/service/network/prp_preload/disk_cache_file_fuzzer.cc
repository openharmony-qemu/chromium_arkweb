/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <fuzzer/FuzzedDataProvider.h>
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_file.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_backend_factory.h"
#undef protected
#undef private

namespace ohos_prp_preload {

constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxUrlLen = 256;
constexpr size_t kMaxContentLen = 1024;

static std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_ = nullptr;

struct Environment {
    Environment() {
        base::CommandLine::Init(0, nullptr);
        task_environment_ = std::make_unique<base::test::SingleThreadTaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    }
    ~Environment() {
        task_environment_.reset();
    }
};

scoped_refptr<DiskCacheBackendFactory> CreateDiskCacheBackendFactory() {
    return base::WrapRefCounted(new DiskCacheBackendFactory());
}

EntryLoadedCallback CreateEntryLoadedCallback(FuzzedDataProvider* fdp) {
    if (fdp->ConsumeBool()) {
        return base::BindRepeating([](const std::string& content) {});
    }
    return EntryLoadedCallback();
}

void FuzzDiskCacheEntryConstructor(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    std::string content = fdp->ConsumeRandomLengthString(kMaxContentLen);
    
    DiskCacheEntry entry(nullptr, url, content);
}

void FuzzDiskCacheEntryWithFactory(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    std::string content = fdp->ConsumeRandomLengthString(kMaxContentLen);
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    
    DiskCacheEntry entry(cache_file.get(), url, content);
}

void FuzzDiskCacheEntryOpenCallback(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    DiskCacheEntry entry(cache_file.get(), url, "fake-content");
    
    entry.op_type_ = DiskCacheEntry::OPEN_ENTRY;
    int rv = entry.OpenCallback(net::OK);
    (void)rv;
}

void FuzzDiskCacheEntryOpenCallbackNullCache(FuzzedDataProvider* fdp) {
    DiskCacheEntry entry(nullptr, "", "fake-content");
    entry.op_type_ = DiskCacheEntry::OPEN_ENTRY;
    entry.cache_ = nullptr;
    
    int rv = entry.OpenCallback(net::OK);
    (void)rv;
}

void FuzzDiskCacheEntryWriteCallbackNoEntry(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    auto entry = std::make_unique<DiskCacheEntry>(cache_file.get(), url, "fake-content");
    
    int rv = entry->WriteCallback(net::OK);
    (void)rv;
}

void FuzzDiskCacheEntryWriteCallbackError(FuzzedDataProvider* fdp) {
    DiskCacheEntry entry(nullptr, "http://example.com/test", "fake-content");
    entry.op_type_ = DiskCacheEntry::CREATE_ENTRY;
    
    int rv = entry.WriteCallback(net::ERR_FAILED);
    (void)rv;
}

void FuzzDiskCacheEntryIOComplete(FuzzedDataProvider* fdp) {
    DiskCacheEntry entry(nullptr, "http://example.com/test", "fake-content");
    
    int rv = entry.IOComplete(net::OK);
    (void)rv;
}

void FuzzDiskCacheEntryIOCompleteWithCache(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    DiskCacheEntry entry(cache_file.get(), url, "fake-content");
    
    int rv = entry.IOComplete(net::OK);
    (void)rv;
}

void FuzzDiskCacheEntryOnOpComplete(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    auto entry = std::make_unique<DiskCacheEntry>(cache_file.get(), url, "fake-content");
    
    entry->op_type_ = DiskCacheEntry::WRITE_DATA;
    entry->OnOpComplete(net::ERR_IO_PENDING);
}

void FuzzDiskCacheEntryDestructor(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    auto entry = std::make_unique<DiskCacheEntry>(cache_file.get(), url, "fake-content");
    
    entry.reset();
}

void FuzzDiskCacheReadHelperConstructor(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = CreateEntryLoadedCallback(fdp);
    
    DiskCacheReadHelper helper(nullptr, url, entry_loaded_cb);
}

void FuzzDiskCacheReadHelperWithFactory(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    DiskCacheReadHelper helper(cache_file.get(), url, entry_loaded_cb);
}

void FuzzDiskCacheReadHelperOpenCallback(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    auto helper = std::make_unique<DiskCacheReadHelper>(cache_file.get(), url, entry_loaded_cb);
    
    helper->op_type_ = DiskCacheReadHelper::OPEN_ENTRY;
    int rv = helper->OpenCallback(net::OK);
    (void)rv;
}

void FuzzDiskCacheReadHelperOpenCallbackNull(FuzzedDataProvider* fdp) {
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    DiskCacheReadHelper helper(nullptr, "", entry_loaded_cb);
    
    int rv = helper.OpenCallback(net::ERR_FAILED);
    (void)rv;
}

void FuzzDiskCacheReadHelperReadCallback(FuzzedDataProvider* fdp) {
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    DiskCacheReadHelper helper(nullptr, "", entry_loaded_cb);
    
    int rv = helper.ReadCallback(net::ERR_FAILED);
    (void)rv;
}

void FuzzDiskCacheReadHelperIOComplete(FuzzedDataProvider* fdp) {
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    DiskCacheReadHelper helper(nullptr, "", entry_loaded_cb);
    
    int rv = helper.IOComplete(net::ERR_FAILED);
    (void)rv;
}

void FuzzDiskCacheReadHelperIOCompleteWithBuffer(FuzzedDataProvider* fdp) {
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    DiskCacheReadHelper helper(nullptr, "", entry_loaded_cb);
    
    helper.buf_ = base::MakeRefCounted<net::IOBufferWithSize>(1);
    int rv = helper.IOComplete(1);
    (void)rv;
}

void FuzzDiskCacheReadHelperOnOpComplete(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    auto helper = std::make_unique<DiskCacheReadHelper>(cache_file.get(), url, entry_loaded_cb);
    
    helper->op_type_ = DiskCacheReadHelper::READ_DATA;
    helper->buf_ = base::MakeRefCounted<net::IOBufferWithSize>(1);
    helper->OnOpComplete(net::ERR_IO_PENDING);
}

void FuzzDiskCacheReadHelperDestructor(FuzzedDataProvider* fdp) {
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    auto helper = std::make_unique<DiskCacheReadHelper>(nullptr, "http://example.com/test", entry_loaded_cb);
    
    helper->buf_ = base::MakeRefCounted<net::IOBufferWithSize>(1);
    helper.reset();
}

void FuzzDiskCacheFileConstructor(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = CreateEntryLoadedCallback(fdp);
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
}

void FuzzDiskCacheFileConstructorNullFactory(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = CreateEntryLoadedCallback(fdp);
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
}

void FuzzDiskCacheFileStoreInfoAsync(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    factory->is_inited_ = true;
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->StoreInfoAsync("entry_content");
}

void FuzzDiskCacheFileStoreInfoAsyncNullFactory(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    cache_file->StoreInfoAsync("entry_content");
}

void FuzzDiskCacheFileLoadInfoAsync(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->LoadInfoAsync();
}

void FuzzDiskCacheFileLoadInfoAsyncNullFactory(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    cache_file->LoadInfoAsync();
}

void FuzzDiskCacheFileDoLoadInfo(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->DoLoadInfo();
}

void FuzzDiskCacheFileEntryReadComplete(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->EntryReadComplete();
}

void FuzzDiskCacheFileEntryWriteComplete(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    std::string content = "fake-content";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    
    DiskCacheEntry entry(cache_file.get(), url, content);
    cache_file->EntryWriteComplete(&entry);
}

void FuzzDiskCacheFileEntryWriteCompleteNull(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->EntryWriteComplete(nullptr);
}

void FuzzDiskCacheFileSetDelayedStoreTask(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    bool called = false;
    cache_file->SetDelayedStoreTask(
        base::BindOnce([](bool* called_ptr) { *called_ptr = true; }, &called));
    cache_file->RunStoreTask(false);
}

void FuzzDiskCacheFileSetDelayedLoadTask(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    bool called = false;
    cache_file->SetDelayedLoadTask(
        base::BindOnce([](bool* called_ptr) { *called_ptr = true; }, &called));
    cache_file->RunLoadTask(false);
}

void FuzzDiskCacheFileRunStoreTaskClear(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    cache_file->SetDelayedStoreTask(base::BindOnce([]() {}));
    cache_file->RunStoreTask(true);
}

void FuzzDiskCacheFileRunLoadTaskClear(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    cache_file->SetDelayedLoadTask(base::BindOnce([]() {}));
    cache_file->RunLoadTask(true);
}

void FuzzDiskCacheFileBackendComplete(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    bool called = false;
    cache_file->SetDelayedLoadTask(
        base::BindOnce([](bool* called_ptr) { *called_ptr = true; }, &called));
    cache_file->BackendComplete();
}

void FuzzDiskCacheFileBackend(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    
    disk_cache::Backend* backend = cache_file->Backend();
    (void)backend;
}

void FuzzDiskCacheFileBackendNullFactory(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    if (cache_file->disk_cache_backend_factory_) {
        disk_cache::Backend* backend = cache_file->Backend();
        (void)backend;
    }
}

void FuzzDiskCacheFileWithHelper(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->helper_ = std::make_unique<DiskCacheReadHelper>(cache_file.get(), url, entry_loaded_cb);
    cache_file->LoadInfoAsync();
    cache_file->helper_ = nullptr;
    cache_file->LoadInfoAsync();
}

void FuzzDiskCacheEntryOpTypeValues(FuzzedDataProvider* fdp) {
    DiskCacheEntry entry(nullptr, "http://example.com/test", "content");
    
    int op_type = fdp->ConsumeIntegralInRange<int>(0, 2);
    entry.op_type_ = static_cast<DiskCacheEntry::OpType>(op_type);
}

void FuzzDiskCacheReadHelperOpTypeValues(FuzzedDataProvider* fdp) {
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    DiskCacheReadHelper helper(nullptr, "http://example.com/test", entry_loaded_cb);
    
    int op_type = fdp->ConsumeIntegralInRange<int>(0, 2);
    helper.op_type_ = static_cast<DiskCacheReadHelper::OpType>(op_type);
}

void FuzzDiskCacheFileUrlAccess(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
    
    const std::string& stored_url = *cache_file->url_;
    (void)stored_url;
}

void FuzzDiskCacheEntryUrlAccess(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    DiskCacheEntry entry(nullptr, url, "content");
    
    const std::string& stored_url = *entry.url_;
    (void)stored_url;
}

void FuzzDiskCacheReadHelperUrlAccess(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    DiskCacheReadHelper helper(nullptr, url, entry_loaded_cb);
    
    const std::string& stored_url = *helper.url_;
    (void)stored_url;
}

void FuzzDiskCacheFileHelperReset(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->helper_ = std::make_unique<DiskCacheReadHelper>(cache_file.get(), url, entry_loaded_cb);
    cache_file->helper_->buf_ = base::MakeRefCounted<net::IOBufferWithSize>(1);
    cache_file->helper_.reset();
    cache_file->helper_ = nullptr;
}

void FuzzDiskCacheFileEntryReset(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    std::string content = "fake-content";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    cache_file->entry_ = std::make_unique<DiskCacheEntry>(cache_file.get(), url, content);
    cache_file->entry_.reset();
    cache_file->entry_ = nullptr;
}

void FuzzDiskCacheFileMultipleInstances(FuzzedDataProvider* fdp) {
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 5); ++i) {
        auto factory = CreateDiskCacheBackendFactory();
        std::string url = "http://example" + std::to_string(i) + ".com/test";
        auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
        
        auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    }
}

void FuzzDiskCacheFileEmptyUrl(FuzzedDataProvider* fdp) {
    std::string url;
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
}

void FuzzDiskCacheFileLongUrl(FuzzedDataProvider* fdp) {
    std::string url;
    for (int i = 0; i < 500; ++i) {
        url += "a";
    }
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, entry_loaded_cb);
}

void FuzzDiskCacheFileNullCallbackRunLoadTask(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, EntryLoadedCallback());
    cache_file->SetDelayedLoadTask(base::BindOnce([]() {}));
    cache_file->RunLoadTask(true);
}

void FuzzDiskCacheFileNullCallbackRunStoreTask(FuzzedDataProvider* fdp) {
    std::string url = "http://example.com/test";
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(nullptr, url, EntryLoadedCallback());
    cache_file->SetDelayedStoreTask(base::BindOnce([]() {}));
    cache_file->RunStoreTask(true);
}

void FuzzDiskCacheFileIsolatedTests(FuzzedDataProvider* fdp) {
    auto factory = CreateDiskCacheBackendFactory();
    std::string url = "http://example.com/test";
    auto entry_loaded_cb = base::BindRepeating([](const std::string& content) {});
    
    auto cache_file = base::MakeRefCounted<DiskCacheFile>(factory, url, entry_loaded_cb);
    factory->is_inited_ = true;
    
    cache_file->DoLoadInfo();
    cache_file->StoreInfoAsync("entry_content");
    
    cache_file->EntryReadComplete();
}

} // namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size == 0)) {
        return 0;
    }

    static ohos_prp_preload::Environment env;

    FuzzedDataProvider fdp(data, size);
    
    size_t iterations = fdp.ConsumeIntegralInRange<size_t>(1, ohos_prp_preload::kMaxIterations);
    
    for (size_t i = 0; i < iterations && fdp.remaining_bytes() > 0; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 44);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzDiskCacheEntryConstructor(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzDiskCacheEntryWithFactory(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzDiskCacheEntryOpenCallback(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzDiskCacheEntryOpenCallbackNullCache(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzDiskCacheEntryWriteCallbackNoEntry(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzDiskCacheEntryWriteCallbackError(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzDiskCacheEntryIOComplete(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzDiskCacheEntryIOCompleteWithCache(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzDiskCacheEntryOnOpComplete(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzDiskCacheEntryDestructor(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzDiskCacheReadHelperConstructor(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzDiskCacheReadHelperWithFactory(&fdp);
                break;
            case 12:
                ohos_prp_preload::FuzzDiskCacheReadHelperOpenCallback(&fdp);
                break;
            case 13:
                ohos_prp_preload::FuzzDiskCacheReadHelperOpenCallbackNull(&fdp);
                break;
            case 14:
                ohos_prp_preload::FuzzDiskCacheReadHelperReadCallback(&fdp);
                break;
            case 15:
                ohos_prp_preload::FuzzDiskCacheReadHelperIOComplete(&fdp);
                break;
            case 16:
                ohos_prp_preload::FuzzDiskCacheReadHelperIOCompleteWithBuffer(&fdp);
                break;
            case 17:
                ohos_prp_preload::FuzzDiskCacheReadHelperOnOpComplete(&fdp);
                break;
            case 18:
                ohos_prp_preload::FuzzDiskCacheReadHelperDestructor(&fdp);
                break;
            case 19:
                ohos_prp_preload::FuzzDiskCacheFileConstructor(&fdp);
                break;
            case 20:
                ohos_prp_preload::FuzzDiskCacheFileConstructorNullFactory(&fdp);
                break;
            case 21:
                ohos_prp_preload::FuzzDiskCacheFileStoreInfoAsync(&fdp);
                break;
            case 22:
                ohos_prp_preload::FuzzDiskCacheFileStoreInfoAsyncNullFactory(&fdp);
                break;
            case 23:
                ohos_prp_preload::FuzzDiskCacheFileLoadInfoAsync(&fdp);
                break;
            case 24:
                ohos_prp_preload::FuzzDiskCacheFileLoadInfoAsyncNullFactory(&fdp);
                break;
            case 25:
                ohos_prp_preload::FuzzDiskCacheFileDoLoadInfo(&fdp);
                break;
            case 26:
                ohos_prp_preload::FuzzDiskCacheFileEntryReadComplete(&fdp);
                break;
            case 27:
                ohos_prp_preload::FuzzDiskCacheFileEntryWriteComplete(&fdp);
                break;
            case 28:
                ohos_prp_preload::FuzzDiskCacheFileEntryWriteCompleteNull(&fdp);
                break;
            case 29:
                ohos_prp_preload::FuzzDiskCacheFileSetDelayedStoreTask(&fdp);
                break;
            case 30:
                ohos_prp_preload::FuzzDiskCacheFileSetDelayedLoadTask(&fdp);
                break;
            case 31:
                ohos_prp_preload::FuzzDiskCacheFileRunStoreTaskClear(&fdp);
                break;
            case 32:
                ohos_prp_preload::FuzzDiskCacheFileRunLoadTaskClear(&fdp);
                break;
            case 33:
                ohos_prp_preload::FuzzDiskCacheFileBackendComplete(&fdp);
                break;
            case 34:
                ohos_prp_preload::FuzzDiskCacheFileBackend(&fdp);
                break;
            case 35:
                ohos_prp_preload::FuzzDiskCacheFileBackendNullFactory(&fdp);
                break;
            case 36:
                ohos_prp_preload::FuzzDiskCacheFileWithHelper(&fdp);
                break;
            case 37:
                ohos_prp_preload::FuzzDiskCacheFileHelperReset(&fdp);
                break;
            case 38:
                ohos_prp_preload::FuzzDiskCacheFileEntryReset(&fdp);
                break;
            case 39:
                ohos_prp_preload::FuzzDiskCacheFileMultipleInstances(&fdp);
                break;
            case 40:
                ohos_prp_preload::FuzzDiskCacheFileEmptyUrl(&fdp);
                break;
            case 41:
                ohos_prp_preload::FuzzDiskCacheFileLongUrl(&fdp);
                break;
            case 42:
                ohos_prp_preload::FuzzDiskCacheFileNullCallbackRunLoadTask(&fdp);
                break;
            case 43:
                ohos_prp_preload::FuzzDiskCacheFileNullCallbackRunStoreTask(&fdp);
                break;
            case 44:
                ohos_prp_preload::FuzzDiskCacheFileIsolatedTests(&fdp);
                break;
            default:
                break;
        }
    }
    
    return 0;
}