// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/address_info.h"

#include <memory>
#include <optional>

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
#include <netdb.h>

#include "base/files/file.h"
#include "base/native_library.h"
#include "net/base/network_handle.h"
#include "arkweb/chromium_ext/net/dns/arkweb_address_info_ext.h"
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#endif

namespace net {
#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
namespace ohos {

NO_SANITIZE("cfi-icall")
int GetAddrInfoForNetwork(char* host,
                          char* serv,
                          struct addrinfo* hints,
                          struct addrinfo** res,
                          int32_t network) {
  if (network == handles::kInvalidNetworkHandle) {
    errno = EINVAL;
    return EAI_SYSTEM;
  }

  std::unique_ptr<OHOS::NWeb::NetConnectAdapter> net_connect_adapter =
      OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateNetConnectAdapter();
  if (!net_connect_adapter) {
    LOG(ERROR) << "net_connect_adapter is nullptr";
    errno = EINVAL;
    return EAI_SYSTEM;
  }

  return net_connect_adapter->GetAddrInfoForNetwork(host, serv, hints, res, network);
}

NO_SANITIZE("cfi-icall") int FreeDnsResult(struct addrinfo* res) {
  std::unique_ptr<OHOS::NWeb::NetConnectAdapter> net_connect_adapter =
      OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateNetConnectAdapter();
  if (!net_connect_adapter) {
    LOG(ERROR) << "net_connect_adapter is nullptr";
    errno = ENOSYS;
    return EAI_SYSTEM;
  }

  return net_connect_adapter->FreeDnsResult(res);
}

}  // namespace ohos
#endif  // ARKWEB_EXT_NETWORK_CONNECTION

}  // namespace net
