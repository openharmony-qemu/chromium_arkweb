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

#ifndef OHOS_NWEB_SRC_CAPI_NWEB_MODAL_DIALOG_HANDLER_H_
#define OHOS_NWEB_SRC_CAPI_NWEB_MODAL_DIALOG_HANDLER_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "common/common.h"

/**
 * Dialog string type for on-demand resolution.
 */
enum DialogStringType {
  DIALOG_STRING_TITLE = 0,
  DIALOG_STRING_CONTENT = 1,
  DIALOG_STRING_ACCEPT_BUTTON = 2,
  DIALOG_STRING_CANCEL_BUTTON = 3,
};

/**
 * Dialog modal type.
 */
enum DialogModal {
  DIALOG_MODAL_NONE = 0,
  DIALOG_MODAL_WEB = 1,
  DIALOG_MODAL_TAB = 2,
  DIALOG_MODAL_BROWSER = 3,
};

/**
 * Member-function-pointer table for cross-so dialog dispatch.
 *
 * The handler object is created by ArkWeb (CEF side) and passed as a raw
 * pointer to browser_service.  browser_service calls methods through the
 * member function pointers, which dispatch to the ArkWeb-side virtual
 * implementations.
 *
 * Lifetime: ArkWeb allocates with its own allocator; browser_service must
 * call delete_this() (NOT delete) to release — this dispatches back to
 * ArkWeb's allocator.
 *
 * Pattern follows NWebDevtoolsMessageHandlerBase<T>.
 */
template <typename T>
struct NWebModalDialogHandlerBase {
  const size_t struct_size = sizeof(NWebModalDialogHandlerBase);
  void (T::*delete_this)() = nullptr;
  const char* (T::*title)() = nullptr;
  const char* (T::*content)() = nullptr;
  const char* (T::*accept_button_label)() = nullptr;
  const char* (T::*cancel_button_label)() = nullptr;
  int (T::*modal)() = nullptr;
  void (T::*on_dialog_result)(bool accepted) = nullptr;

  NWebModalDialogHandlerBase() = default;
  NWebModalDialogHandlerBase(const NWebModalDialogHandlerBase&) = delete;
  NWebModalDialogHandlerBase(NWebModalDialogHandlerBase&&) = delete;
  NWebModalDialogHandlerBase& operator=(const NWebModalDialogHandlerBase&) =
      delete;
  NWebModalDialogHandlerBase& operator=(NWebModalDialogHandlerBase&&) = delete;
};

/**
 * Concrete handler struct.  Subclass and override the protected virtual
 * methods to provide dialog strings and result handling.
 *
 * The destructor is protected to prevent cross-allocator delete.
 * Use DeleteThis() instead.
 */
struct NWebModalDialogHandler
    : public NWebModalDialogHandlerBase<NWebModalDialogHandler> {
 public:
  using Base = NWebModalDialogHandlerBase<NWebModalDialogHandler>;

  NWebModalDialogHandler() : Base() {
    this->delete_this = &NWebModalDialogHandler::DeleteThis;
    this->title = &NWebModalDialogHandler::Title;
    this->content = &NWebModalDialogHandler::Content;
    this->accept_button_label = &NWebModalDialogHandler::AcceptButtonLabel;
    this->cancel_button_label = &NWebModalDialogHandler::CancelButtonLabel;
    this->modal = &NWebModalDialogHandler::Modal;
    this->on_dialog_result = &NWebModalDialogHandler::OnDialogResult;
  }

  // Called by the holder to delete this object.  Must be dispatched through
  // the member function pointer so that the correct allocator is used.
  virtual void DeleteThis() {}

 protected:
  // Destructor is protected to prevent cross-allocator delete.
  // Use DeleteThis() instead.
  virtual ~NWebModalDialogHandler() = default;

  // can't call delete directly. use DeleteThis instead.
  virtual const char* Title() { return ""; }
  virtual const char* Content() { return ""; }
  virtual const char* AcceptButtonLabel() { return ""; }
  virtual const char* CancelButtonLabel() { return ""; }
  virtual int Modal() { return DIALOG_MODAL_NONE; }
  virtual void OnDialogResult(bool accepted) {}

 private:
  static_assert(offsetof(Base, struct_size) == 0,
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, delete_this) - offsetof(Base, struct_size) ==
                    sizeof(Base::struct_size),
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, title) - offsetof(Base, delete_this) ==
                    sizeof(Base::delete_this),
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, content) - offsetof(Base, title) ==
                    sizeof(Base::title),
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, accept_button_label) - offsetof(Base, content) ==
                    sizeof(Base::content),
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, cancel_button_label) -
                        offsetof(Base, accept_button_label) ==
                    sizeof(Base::accept_button_label),
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, modal) - offsetof(Base, cancel_button_label) ==
                    sizeof(Base::cancel_button_label),
                "Must NOT break the order of Base members.");
  static_assert(offsetof(Base, on_dialog_result) - offsetof(Base, modal) ==
                    sizeof(Base::modal),
                "Must NOT break the order of Base members.");
  static_assert(sizeof(Base) - offsetof(Base, on_dialog_result) ==
                    sizeof(Base::on_dialog_result),
                "Must NOT break the order of Base members.");
};

struct NWebModalDialogHandlerDeleter {
  void operator()(NWebModalDialogHandler* handler) const {
    CheckAndCall(handler, &handler->delete_this);
  }
};

using NWebModalDialogHandlerPtr =
    std::unique_ptr<NWebModalDialogHandler, NWebModalDialogHandlerDeleter>;

#endif  // OHOS_NWEB_SRC_CAPI_NWEB_MODAL_DIALOG_HANDLER_H_
