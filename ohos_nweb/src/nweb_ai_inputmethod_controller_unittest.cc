#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "cef/include/cef_base.h"

#define private public
#include "nweb_ai_inputmethod_controller.h"
#undef private

#include "nweb_errors.h"
#include "nweb_delegate_interface.h"
#include "nweb_input_handler.h"
#include "nweb_inputmethod_handler.h"
#include "mock_nweb_delegate.h"

using namespace testing;
using namespace OHOS::NWeb;

class MockNWebInputHandler : public NWebInputHandler {
public:
    MockNWebInputHandler() : NWebInputHandler(nullptr) {}
    MOCK_METHOD(bool, WebSendKeyEvent, (int32_t keyCode, int32_t keyAction, const std::vector<int32_t>& pressedCodes));
};

class MockNWebInputMethodHandler : public NWebInputMethodHandler {
public:
    MOCK_METHOD(bool, IsAttached, ());
    MOCK_METHOD(bool, GetFocusRect, (int& x, int& y, int& width, int& height));
};

namespace {
int32_t ExtractCodeIntFromJson(const std::string& json) {
    auto value = base::JSONReader::Read(json, base::JSON_PARSE_RFC);
    if (!value || !value->is_dict()) {
        return -1;
    }
    const auto& dict = value->GetDict();
    auto code = dict.FindInt("code");
    if (code) {
        return *code;
    }
    return -1;
}

std::string ExtractMessageFromJson(const std::string& json) {
    auto value = base::JSONReader::Read(json, base::JSON_PARSE_RFC);
    if (!value || !value->is_dict()) {
        return "";
    }
    const auto& dict = value->GetDict();
    auto message = dict.FindString("message");
    if (message) {
        return *message;
    }
    auto error = dict.FindString("error");
    if (error) {
        return *error;
    }
    return "";
}
}

class NWebAIInputMethodControllerTest : public Test {
protected:
    void SetUp() override {
        mockInputHandler_ = std::make_shared<MockNWebInputHandler>();
        mockDelegate_ = std::make_shared<MockNWebDelegate>();
        mockInputMethodHandler_ = new MockNWebInputMethodHandler();
        controller_ = std::make_shared<NWebAIInputMethodController>(
            mockInputHandler_, mockDelegate_, mockInputMethodHandler_);
    }

    base::Value::Dict BuildParams(const std::string& xpath = "//input", const std::string& text = "test") {
        base::Value::Dict params;
        params.Set("XPath/nodeid", xpath);
        if (!text.empty()) {
            params.Set("text", text);
        }
        return params;
    }

    base::Value::Dict BuildParamsWithKey(const std::string& key = "Enter") {
        base::Value::Dict params;
        params.Set("key", key);
        return params;
    }

    std::shared_ptr<MockNWebInputHandler> mockInputHandler_;
    std::shared_ptr<MockNWebDelegate> mockDelegate_;
    CefRefPtr<MockNWebInputMethodHandler> mockInputMethodHandler_;
    std::shared_ptr<NWebAIInputMethodController> controller_;
};

// ===== ExecuteJsonAction tests =====
TEST_F(NWebAIInputMethodControllerTest, ExecuteJsonAction_MethodNotFound_ErrorCode_0100) {
    base::Value::Dict cmd;
    std::string result = controller_->ExecuteJsonAction(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_METHOD_NOT_FOUND));
    EXPECT_EQ(ExtractMessageFromJson(result), "method not found");
}

TEST_F(NWebAIInputMethodControllerTest, ExecuteJsonAction_UnknownMethod_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "unknown_method");
    
    std::string result = controller_->ExecuteJsonAction(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_METHOD_UNKNOWN));
    EXPECT_EQ(ExtractMessageFromJson(result), "unknown method");
}

TEST_F(NWebAIInputMethodControllerTest, ExecuteJsonAction_Click_Success_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    cmd.Set("params", base::Value(BuildParams().Clone()));
    
    EXPECT_CALL(*mockDelegate_, IsElementExist(testing::_)).WillOnce(Return(true));
    
    std::string result = controller_->ExecuteJsonAction(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
    EXPECT_EQ(ExtractMessageFromJson(result), "success");
}

TEST_F(NWebAIInputMethodControllerTest, ExecuteJsonAction_Focus_Success_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "focus");
    cmd.Set("params", base::Value(BuildParams().Clone()));
    
    EXPECT_CALL(*mockDelegate_, IsElementExist(testing::_)).WillOnce(Return(true));
    
    std::string result = controller_->ExecuteJsonAction(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
    EXPECT_EQ(ExtractMessageFromJson(result), "success");
}

// ===== HandleClickOrFocus tests =====
TEST_F(NWebAIInputMethodControllerTest, HandleClickOrFocus_ParamsMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    
    std::string result = controller_->HandleClickOrFocus(cmd, "click");
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_PARAMS_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "click params not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleClickOrFocus_NodeidMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    base::Value::Dict params;
    cmd.Set("params", base::Value(params.Clone()));
    
    std::string result = controller_->HandleClickOrFocus(cmd, "click");
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_NODEID_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "click nodeid not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleClickOrFocus_NodeidEmpty_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    cmd.Set("params", base::Value(BuildParams("").Clone()));
    
    std::string result = controller_->HandleClickOrFocus(cmd, "click");
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_NODEID_EMPTY));
    EXPECT_EQ(ExtractMessageFromJson(result), "click nodeid is empty");
}

TEST_F(NWebAIInputMethodControllerTest, HandleClickOrFocus_DelegateNull_ErrorCode_0100) {
    auto controllerWithNullDelegate = std::make_shared<NWebAIInputMethodController>(
        mockInputHandler_, nullptr, mockInputMethodHandler_);
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    cmd.Set("params", base::Value(BuildParams().Clone()));
    
    std::string result = controllerWithNullDelegate->HandleClickOrFocus(cmd, "click");
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_DELEGATE_NULL));
    EXPECT_EQ(ExtractMessageFromJson(result), "click delegate not initialized");
}

TEST_F(NWebAIInputMethodControllerTest, HandleClickOrFocus_ElementNotFound_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    cmd.Set("params", base::Value(BuildParams().Clone()));
    
    std::string xpath = "//input";
    EXPECT_CALL(*mockDelegate_, IsElementExist(testing::_)).WillOnce(Return(false));
    
    std::string result = controller_->HandleClickOrFocus(cmd, "click");
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_ELEMENT_NOT_FOUND));
    EXPECT_EQ(ExtractMessageFromJson(result), "click element not exist");
}

TEST_F(NWebAIInputMethodControllerTest, HandleClickOrFocus_Success_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "click");
    cmd.Set("params", base::Value(BuildParams().Clone()));
    
    EXPECT_CALL(*mockDelegate_, IsElementExist(testing::_)).WillOnce(Return(true));
    
    std::string result = controller_->HandleClickOrFocus(cmd, "click");
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
    EXPECT_EQ(ExtractMessageFromJson(result), "success");
}

// ===== HandleType tests =====
TEST_F(NWebAIInputMethodControllerTest, HandleType_ParamsMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    
    std::string result = controller_->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_PARAMS_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "type params not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleType_NodeidMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    base::Value::Dict params;
    params.Set("text", "hello");
    cmd.Set("params", base::Value(params.Clone()));
    
    std::string result = controller_->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_NODEID_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "type nodeid not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleType_TextMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    cmd.Set("params", base::Value(BuildParams("//input", "").Clone()));
    
    std::string result = controller_->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_TEXT_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "type text not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleType_XpathEmpty_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    cmd.Set("params", base::Value(BuildParams("", "hello").Clone()));
    
    std::string result = controller_->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_XPATH_EMPTY));
    EXPECT_EQ(ExtractMessageFromJson(result), "type xpath is empty");
}

TEST_F(NWebAIInputMethodControllerTest, HandleType_DelegateNull_ErrorCode_0100) {
    auto controllerWithNullDelegate = std::make_shared<NWebAIInputMethodController>(
        mockInputHandler_, nullptr, mockInputMethodHandler_);
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    cmd.Set("params", base::Value(BuildParams("//input", "hello").Clone()));
    
    std::string result = controllerWithNullDelegate->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_DELEGATE_NULL));
    EXPECT_EQ(ExtractMessageFromJson(result), "type delegate not initialized");
}

TEST_F(NWebAIInputMethodControllerTest, HandleType_ElementNotFound_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    cmd.Set("params", base::Value(BuildParams("//input", "hello").Clone()));
    
    std::string xpath = "//input";
    EXPECT_CALL(*mockDelegate_, IsElementExist(testing::_)).WillOnce(Return(false));
    
    std::string result = controller_->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_ELEMENT_NOT_FOUND));
    EXPECT_EQ(ExtractMessageFromJson(result), "type element not exist");
}

TEST_F(NWebAIInputMethodControllerTest, HandleType_Success_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "type");
    cmd.Set("params", base::Value(BuildParams("//input", "hello").Clone()));
    
    EXPECT_CALL(*mockDelegate_, IsElementExist(testing::_)).WillOnce(Return(true));
    
    std::string result = controller_->HandleType(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
    EXPECT_EQ(ExtractMessageFromJson(result), "success");
}

// ===== HandleSendKeys tests =====
TEST_F(NWebAIInputMethodControllerTest, HandleSendKeys_ParamsMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "send_keys");
    
    std::string result = controller_->HandleSendKeys(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_PARAMS_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "send_keys params not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleSendKeys_KeyMissing_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "send_keys");
    base::Value::Dict params;
    cmd.Set("params", base::Value(params.Clone()));
    
    std::string result = controller_->HandleSendKeys(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_KEY_MISSING));
    EXPECT_EQ(ExtractMessageFromJson(result), "send_keys keys not found");
}

TEST_F(NWebAIInputMethodControllerTest, HandleSendKeys_NoValidKeys_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "send_keys");
    cmd.Set("params", base::Value(BuildParamsWithKey("").Clone()));
    
    std::string result = controller_->HandleSendKeys(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_NO_VALID_KEYS));
    EXPECT_EQ(ExtractMessageFromJson(result), "send_keys no valid keys");
}

TEST_F(NWebAIInputMethodControllerTest, HandleSendKeys_UnknownKey_ErrorCode_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "send_keys");
    cmd.Set("params", base::Value(BuildParamsWithKey("InvalidKey123").Clone()));
    
    std::string result = controller_->HandleSendKeys(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_UNKNOWN_KEY));
    EXPECT_EQ(ExtractMessageFromJson(result), "send_keys unknown main key");
}

TEST_F(NWebAIInputMethodControllerTest, HandleSendKeys_Success_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "send_keys");
    cmd.Set("params", base::Value(BuildParamsWithKey("Enter").Clone()));
    
    EXPECT_CALL(*mockInputHandler_, WebSendKeyEvent(2054, 0, _)).Times(1);
    EXPECT_CALL(*mockInputHandler_, WebSendKeyEvent(2054, 1, _)).Times(1);
    
    std::string result = controller_->HandleSendKeys(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
    EXPECT_EQ(ExtractMessageFromJson(result), "success");
}

TEST_F(NWebAIInputMethodControllerTest, HandleSendKeys_WithModifier_Success_0100) {
    base::Value::Dict cmd;
    cmd.Set("method", "send_keys");
    cmd.Set("params", base::Value(BuildParamsWithKey("Ctrl+A").Clone()));
    
    std::vector<int32_t> modifiers = {2072};
    EXPECT_CALL(*mockInputHandler_, WebSendKeyEvent(1952 + 'A', 0, modifiers)).Times(1);
    EXPECT_CALL(*mockInputHandler_, WebSendKeyEvent(1952 + 'A', 1, modifiers)).Times(1);
    
    std::string result = controller_->HandleSendKeys(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== HandleCursorPosition tests =====
TEST_F(NWebAIInputMethodControllerTest, HandleCursorPosition_MethodNotBound_ErrorCode_0100) {
    EXPECT_CALL(*mockInputMethodHandler_, IsAttached()).WillOnce(Return(false));
    
    base::Value::Dict cmd;
    std::string result = controller_->HandleCursorPosition(cmd);
    
    EXPECT_EQ(ExtractCodeIntFromJson(result), static_cast<int32_t>(WebCommandErrorCode::INPUT_METHOD_NOT_BOUND));
    EXPECT_EQ(ExtractMessageFromJson(result), "input method not bound");
}

TEST_F(NWebAIInputMethodControllerTest, HandleCursorPosition_NullHandler_Success_0100) {
    auto controllerWithNullHandler = std::make_shared<NWebAIInputMethodController>(
        nullptr, mockDelegate_, nullptr);
    base::Value::Dict cmd;
    
    std::string result = controllerWithNullHandler->HandleCursorPosition(cmd);
    
    EXPECT_TRUE(result.find("\"result\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"x\":0") != std::string::npos);
    EXPECT_TRUE(result.find("\"y\":0") != std::string::npos);
}

TEST_F(NWebAIInputMethodControllerTest, HandleCursorPosition_GetFocusRect_Success_0100) {
    EXPECT_CALL(*mockInputMethodHandler_, IsAttached()).WillOnce(Return(true));
    EXPECT_CALL(*mockInputMethodHandler_, GetFocusRect(_, _, _, _))
        .WillOnce([](int& x, int& y, int& width, int& height) {
            x = 10; y = 20; width = 100; height = 50;
            return true;
        });
    
    base::Value::Dict cmd;
    std::string result = controller_->HandleCursorPosition(cmd);
    
    EXPECT_TRUE(result.find("\"x\":10") != std::string::npos);
    EXPECT_TRUE(result.find("\"y\":20") != std::string::npos);
}

// ===== Helper function tests =====
TEST_F(NWebAIInputMethodControllerTest, SplitKeys_SingleKey_0100) {
    auto result = controller_->SplitKeys("Enter");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "Enter");
}

TEST_F(NWebAIInputMethodControllerTest, SplitKeys_WithModifiers_0100) {
    auto result = controller_->SplitKeys("Ctrl+Shift+A");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "Ctrl");
    EXPECT_EQ(result[1], "Shift");
    EXPECT_EQ(result[2], "A");
}

TEST_F(NWebAIInputMethodControllerTest, BuildModifierKeyCodes_Shift_0100) {
    std::vector<std::string> modifiers = {"Shift"};
    auto result = controller_->BuildModifierKeyCodes(modifiers);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 2047);
}

TEST_F(NWebAIInputMethodControllerTest, BuildModifierKeyCodes_Ctrl_0100) {
    std::vector<std::string> modifiers = {"Ctrl"};
    auto result = controller_->BuildModifierKeyCodes(modifiers);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 2072);
}

TEST_F(NWebAIInputMethodControllerTest, BuildModifierKeyCodes_Alt_0100) {
    std::vector<std::string> modifiers = {"Alt"};
    auto result = controller_->BuildModifierKeyCodes(modifiers);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 2045);
}

TEST_F(NWebAIInputMethodControllerTest, BuildModifierKeyCodes_Control_0100) {
    std::vector<std::string> modifiers = {"Control"};
    auto result = controller_->BuildModifierKeyCodes(modifiers);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 2072);
}

TEST_F(NWebAIInputMethodControllerTest, BuildModifierKeyCodes_Empty_0100) {
    std::vector<std::string> modifiers;
    auto result = controller_->BuildModifierKeyCodes(modifiers);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(NWebAIInputMethodControllerTest, NeedShiftForMainKey_Uppercase_0100) {
    EXPECT_TRUE(controller_->NeedShiftForMainKey("A"));
    EXPECT_TRUE(controller_->NeedShiftForMainKey("Z"));
}

TEST_F(NWebAIInputMethodControllerTest, NeedShiftForMainKey_Lowercase_0100) {
    EXPECT_FALSE(controller_->NeedShiftForMainKey("a"));
    EXPECT_FALSE(controller_->NeedShiftForMainKey("z"));
}

TEST_F(NWebAIInputMethodControllerTest, NeedShiftForMainKey_Number_0100) {
    EXPECT_FALSE(controller_->NeedShiftForMainKey("1"));
}

TEST_F(NWebAIInputMethodControllerTest, NeedShiftForMainKey_MultiChar_0100) {
    EXPECT_FALSE(controller_->NeedShiftForMainKey("Enter"));
    EXPECT_FALSE(controller_->NeedShiftForMainKey("Tab"));
}

TEST_F(NWebAIInputMethodControllerTest, EnsureShiftInModifiers_AddShift_0100) {
    std::vector<int32_t> codes;
    controller_->EnsureShiftInModifiers(codes);
    EXPECT_EQ(codes.size(), 1);
    EXPECT_EQ(codes[0], 2047);
}

TEST_F(NWebAIInputMethodControllerTest, EnsureShiftInModifiers_AlreadyHasShift_0100) {
    std::vector<int32_t> codes = {2047};
    controller_->EnsureShiftInModifiers(codes);
    EXPECT_EQ(codes.size(), 1);
}

// ===== 按键映射测试 =====
TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Enter_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Enter"), 0x0D);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Return"), 0x0D);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Tab_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Tab"), 0x09);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Escape_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Escape"), 0x1B);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Esc"), 0x1B);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Space_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Space"), 0x20);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Backspace_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Backspace"), 0x08);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Delete_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Delete"), 0x2E);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Del"), 0x2E);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_ArrowKeys_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("ArrowUp"), 0x26);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Up"), 0x26);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("ArrowDown"), 0x28);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Down"), 0x28);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("ArrowLeft"), 0x25);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Left"), 0x25);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("ArrowRight"), 0x27);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Right"), 0x27);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Modifiers_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Control"), 0x11);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Ctrl"), 0x11);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Shift"), 0x10);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Alt"), 0x12);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_PageKeys_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("PageUp"), 0x21);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("PageDown"), 0x22);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("End"), 0x23);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Home"), 0x24);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_FKeys_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("F1"), 0x70);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("F2"), 0x71);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("F10"), 0x79);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("F12"), 0x7B);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Alphabet_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("a"), 'A');
    EXPECT_EQ(controller_->GetVKeyFromWebKey("z"), 'Z');
    EXPECT_EQ(controller_->GetVKeyFromWebKey("A"), 'A');
    EXPECT_EQ(controller_->GetVKeyFromWebKey("Z"), 'Z');
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Number_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("0"), '0');
    EXPECT_EQ(controller_->GetVKeyFromWebKey("9"), '9');
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_OemKeys_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("`"), 0xC0);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("-"), 0xBD);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("="), 0xBB);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("["), 0xDB);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("]"), 0xDD);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("\\"), 0xDC);
    EXPECT_EQ(controller_->GetVKeyFromWebKey(";"), 0xBA);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("'"), 0xDE);
    EXPECT_EQ(controller_->GetVKeyFromWebKey(","), 0xBC);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("."), 0xBE);
    EXPECT_EQ(controller_->GetVKeyFromWebKey("/"), 0xBF);
}

TEST_F(NWebAIInputMethodControllerTest, GetVKeyFromWebKey_Unknown_0100) {
    EXPECT_EQ(controller_->GetVKeyFromWebKey("UnknownKey"), 0);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_Alphabet_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey('A'), 1952 + 'A');
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey('Z'), 1952 + 'Z');
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_Number_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey('0'), 1952 + '0');
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey('9'), 1952 + '9');
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_FKeys_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x70), 2090);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x7B), 2090 + 11);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_SpecialKeys_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x0D), 2054);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x09), 2049);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x1B), 2070);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x14), 2074);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x20), 2050);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x08), 2055);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x2E), 2071);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_ArrowKeys_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x26), 2012);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x28), 2013);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x25), 2014);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x27), 2015);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_Modifiers_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x11), 2072);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x10), 2047);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x12), 2045);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_NavigationKeys_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x2D), 2083);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x21), 2068);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x22), 2069);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x23), 2082);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0x24), 2081);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_OemKeys_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xC0), 2056);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xBD), 2057);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xBB), 2058);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xDB), 2059);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xDD), 2060);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xDC), 2061);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xBA), 2062);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xDE), 2063);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xBC), 2043);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xBE), 2044);
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xBF), 2064);
}

TEST_F(NWebAIInputMethodControllerTest, GetOhosKeyCodeFromVKey_Unknown_0100) {
    EXPECT_EQ(controller_->GetOhosKeyCodeFromVKey(0xFF), 0);
}
