/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "datashare_adapter_impl.h"

#include <string>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>

#include "arkweb/ohos_nweb/src/nweb_hilog.h"
#include <filemanagement/file_uri/oh_file_uri.h>

namespace OHOS::NWeb {
namespace {
// OH_FileUri_GetPathFromUri 底层的 FileUri::GetRealPath() 只做字符串替换，
// 不做路径校验。此处仅放行其合法产物前缀：
//   /data/storage/  应用沙箱视图（el2/base、el2/media、el2/share 等）
//   /storage/       文件管理器视图（file://docs/... 的换算结果）
// "datashare://"、"dataability://" 前缀的 uri 经该接口换算得到的是无效路径
// （如 "/datashare://..."），一律落在白名单之外被拒绝。
constexpr const char* ALLOWED_REAL_PATH_PREFIXES[] = {
    "/data/storage/",
    "/storage/",
};

// 拒绝 ".." 路径组件。系统生成的 uri 不含该组件，仅构造输入才会出现。
bool ContainsParentComponent(const std::string& path)
{
    size_t pos = path.find("../");
    while (pos != std::string::npos) {
        if (pos == 0 || path[pos - 1] == '/') {
            return true;
        }
        pos = path.find("../", pos + 3);
    }
    size_t tail = path.rfind("/..");
    return tail != std::string::npos && (path.size() - tail) == 3;
}

bool HasAllowedRealPathPrefix(const std::string& path)
{
    for (const char* prefix : ALLOWED_REAL_PATH_PREFIXES) {
        if (path.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

// static
std::string DatashareAdapterImpl::ResolveSymlinksForExistingPrefix(
    const std::string& path)
{
    // uri 换算出的路径可能整体或部分不存在，只能规范化"最长已存在前缀"：
    // 先试整条路径，失败则逐级回退，未存在的尾部原样保留。
    size_t cut = path.size();
    while (cut != std::string::npos && cut != 0) {
        std::string prefix = path.substr(0, cut);
        char* resolved = realpath(prefix.c_str(), nullptr);
        if (resolved != nullptr) {
            std::string result(resolved);
            free(resolved);
            result.append(path, cut, std::string::npos);
            return result;
        }
        cut = path.find_last_of('/', cut - 1);
    }
    return path;
}

// static
// uri -> path 换算并校验结果。返回 false 表示 uri 非法或换算结果不可信。
bool DatashareAdapterImpl::ConvertUriToValidPath(const std::string& uriStr,
                                                 std::string& path)
{
    // "%00" 会被换算接口解码为 NUL 并截断 C 字符串，导致路径被静默缩短。
    if (uriStr.empty() || uriStr.find("%00") != std::string::npos) {
        return false;
    }
    char *uriResult = nullptr;
    FileManagement_ErrCode ret =
        OH_FileUri_GetPathFromUri(uriStr.c_str(), uriStr.length(), &uriResult);
    if (ret != ERR_OK || uriResult == nullptr) {
        return false;
    }
    path = std::string(uriResult);
    free(uriResult);
    if (path.empty() || !HasAllowedRealPathPrefix(path) ||
        ContainsParentComponent(path)) {
        return false;
    }
    // 词法前缀与 ".." 校验约束不了物理路径：路径中间或末尾可能是指向沙箱外的
    // 软链。展开已存在前缀的软链后必须重新复核白名单，逃出沙箱即拒绝。
    path = ResolveSymlinksForExistingPrefix(path);
    return !path.empty() && HasAllowedRealPathPrefix(path);
}

// static
DatashareAdapterImpl& DatashareAdapterImpl::GetInstance()
{
    static DatashareAdapterImpl instance;
    return instance;
}

std::string DatashareAdapterImpl::GetRealPath(const std::string& uriStr)
{
    std::string realPath;
    if (!ConvertUriToValidPath(uriStr, realPath)) {
        WVLOG_E("get real path rejected, uri is invalid or converted path is illegal");
        return std::string();
    }
    return realPath;
}

int DatashareAdapterImpl::OpenDataShareUriForRead(const std::string& uriStr)
{
    std::string realPath;
    if (!ConvertUriToValidPath(uriStr, realPath)) {
        WVLOG_E("open datashare uri rejected, uri is invalid or converted path is illegal");
        return -1;
    }
    return open(realPath.c_str(), O_RDONLY);
}

std::string DatashareAdapterImpl::GetFileDisplayName(const std::string& uriStr)
{
    const char *uri = uriStr.c_str();
    unsigned int length = strlen(uri);
    char *name = nullptr;

    FileManagement_ErrCode ret = OH_FileUri_GetFileName(uri, length, &name);
    if (ret == ERR_OK) {
        std::string fileName(name);
        free(name);
        return fileName;
    } else {
        return std::string();
    }
}
} // namespace OHOS::NWeb
