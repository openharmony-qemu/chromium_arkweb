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

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H_
#error "must be include form CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H_"
#endif

#include "ohos_nweb/src/cef_delegate/nweb_extension_tab_cef_delegate.h"

namespace extensions {
namespace {
void UpdateWithNWebExtensionTabInfo(api::tabs::Tab& tab) {
  if (!tab.id) {
    return;
  }
  int tab_id = tab.id.value();
  if (tab_id < 0) {
    return;
  }
  auto nweb_tab = OHOS::NWeb::NWebExtensionTabCefDelegate::GetTab(tab_id);
  if (!nweb_tab) {
    return;
  }
  tab.active = nweb_tab->tab_v1.active;
  if (nweb_tab->tab_v1.audible) {
    tab.audible = nweb_tab->tab_v1.audible.value();
  }
  tab.auto_discardable = nweb_tab->tab_v1.autoDiscardable;
  tab.index = nweb_tab->tab_v1.index;
  tab.group_id = nweb_tab->tab_v1.groupId;
  tab.window_id = nweb_tab->tab_v1.windowId;
  if (nweb_tab->tab_v1.openerTabId) {
    tab.opener_tab_id = nweb_tab->tab_v1.openerTabId;
  }
  if (nweb_tab->last_accessed) {
    tab.last_accessed = nweb_tab->last_accessed.value();
  }
  if (nweb_tab->frozen) {
    tab.frozen = nweb_tab->frozen.value();
  }
  if (nweb_tab->split_view_id) {
    tab.split_view_id = nweb_tab->split_view_id.value();
  }
  tab.highlighted = nweb_tab->tab_v1.highlighted;
  tab.pinned = nweb_tab->tab_v1.pinned;
  tab.discarded = nweb_tab->tab_v1.discarded;
  if (nweb_tab->tab_v1.mutedInfo) {
    if (!tab.muted_info) {
      api::tabs::MutedInfo muted_info;
      tab.muted_info = std::move(muted_info);
    }
    tab.muted_info->muted = nweb_tab->tab_v1.mutedInfo->muted;
    if (nweb_tab->tab_v1.mutedInfo->extensionId) {
      tab.muted_info->extension_id = nweb_tab->tab_v1.mutedInfo->extensionId.value();
    }
    if (nweb_tab->tab_v1.mutedInfo->reason) {
      tab.muted_info->reason =
          api::tabs::ParseMutedInfoReason(nweb_tab->tab_v1.mutedInfo->reason.value());
    }
  }
  if (nweb_tab->tab_v1.url) {
    tab.url = nweb_tab->tab_v1.url.value();
  }
  if (nweb_tab->tab_v1.pendingUrl) {
    tab.pending_url = nweb_tab->tab_v1.pendingUrl.value();
  }
  if (nweb_tab->tab_v1.title) {
    tab.title = nweb_tab->tab_v1.title.value();
  }
  if (nweb_tab->tab_v1.favIconUrl) {
    tab.fav_icon_url = nweb_tab->tab_v1.favIconUrl.value();
  }
  if (nweb_tab->tab_v1.status) {
    tab.status = api::tabs::ParseTabStatus(nweb_tab->tab_v1.status.value());
  }
  tab.incognito = nweb_tab->tab_v1.incognito;
  if (nweb_tab->tab_v1.sessionId) {
    tab.session_id = nweb_tab->tab_v1.sessionId.value();
  }
  if (nweb_tab->tab_v1.width) {
    tab.width = nweb_tab->tab_v1.width;
  }
  if (nweb_tab->tab_v1.height) {
    tab.height = nweb_tab->tab_v1.height;
  }
}
}  // namespace

ExtensionTabUtil::ScrubTabBehavior ExtensionTabUtil::GetScrubTabBehaviorExt(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    int tab_id) {
  if (context == mojom::ContextType::kWebUi) {
    return {ExtensionTabUtil::kDontScrubTab, ExtensionTabUtil::kDontScrubTab};
  }
 
  if (context == mojom::ContextType::kUntrustedWebUi) {
    return {ExtensionTabUtil::kScrubTabFully, ExtensionTabUtil::kScrubTabFully};
  }
 
  bool has_permission = false;
 
  if (extension) {
    bool api_permission = false;
    if (tab_id == api::tabs::TAB_ID_NONE) {
      api_permission = extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kTab);
    } else {
      api_permission = extension->permissions_data()->HasAPIPermissionForTab(
          tab_id, mojom::APIPermissionID::kTab);
    }
 
    bool host_permission = extension->permissions_data()
                               ->active_permissions()
                               .HasExplicitAccessToOrigin(url);
    has_permission = api_permission || host_permission;
  }
 
  if (!has_permission) {
    return {ExtensionTabUtil::kScrubTabFully, ExtensionTabUtil::kScrubTabFully};
  }
 
  return {ExtensionTabUtil::kDontScrubTab, ExtensionTabUtil::kDontScrubTab};
}

}  // namespace extensions
