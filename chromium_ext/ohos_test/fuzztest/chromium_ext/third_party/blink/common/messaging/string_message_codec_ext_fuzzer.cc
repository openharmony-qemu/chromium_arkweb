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

#include "arkweb/chromium_ext/third_party/blink/public/common/messaging/string_message_codec_ext.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "base/containers/buffer_iterator.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

#if BUILDFLAG(ARKWEB_MSGPORT)

namespace blink {

// Forward declarations of internal functions from string_message_codec_ext.cc.
// These have external linkage in the .cc file (not static, not in anonymous
// namespace), so they can be called directly from the fuzzer to exercise
// code paths unreachable through the public Encode/Decode API.
void WriteString(const std::string& str, std::vector<uint8_t>* buffer);
void WriteHeader(std::vector<uint8_t>* buffer);
void WriteU16string(const std::u16string& str, std::vector<uint8_t>* buffer);
void WriteBool(const bool& value, std::vector<uint8_t>* buffer);
void WriteDouble(const double& value, std::vector<uint8_t>* buffer);
void WriteInt64(const int64_t& value, std::vector<uint8_t>* buffer);
void WriteError(const std::u16string& name,
                const std::u16string& message,
                std::vector<uint8_t>* buffer);
void WriteArrayBuffer(std::vector<uint8_t>& arrbuf,
                      TransferableMessage& message,
                      std::vector<uint8_t>* buffer);
void WriteStringArray(std::vector<std::u16string>& arr,
                      std::vector<uint8_t>* buffer);
void WriteBoolArray(std::vector<bool>& arr, std::vector<uint8_t>* buffer);
void WriteDoubleArray(std::vector<double>& arr, std::vector<uint8_t>* buffer);
void WriteInt64Array(std::vector<int64_t>& arr, std::vector<uint8_t>* buffer);

bool ReadOneByteString(base::BufferIterator<const uint8_t>& iter,
                       std::u16string& str);
bool ReadTwoByteString(base::BufferIterator<const uint8_t>& iter,
                       std::u16string& str);
bool ReadDouble(base::BufferIterator<const uint8_t>& iter, double& result);
bool ReadU16string(base::BufferIterator<const uint8_t>& iter,
                   std::u16string& str);
bool ReadInt32(base::BufferIterator<const uint8_t>& iter, int32_t& result);
bool ReadInt32InArray(base::BufferIterator<const uint8_t>& iter,
                      int32_t& result);
bool IsBoolElement(uint8_t element_tag);
bool IsStringElement(uint8_t element_tag);
bool IsNumberElement(uint8_t element_tag);
bool ReadArray(base::BufferIterator<const uint8_t>& iter,
               struct WebMessagePort::Message& decoded_msg);
bool ReadError(base::BufferIterator<const uint8_t>& iter,
               struct WebMessagePort::Message& decoded_msg);
bool ReadHeaderTag(base::BufferIterator<const uint8_t>& iter, uint8_t& tag);

namespace {
constexpr uint8_t MAX_STRING_LENGTH = 255;
constexpr size_t MAX_ARRAY_SIZE = 5;

const uint8_t kVersionTag = 0xFF;
const uint8_t kPaddingTag = '\0';
const uint8_t kOneByteStringTag = '"';
const uint8_t kTwoByteStringTag = 'c';
const uint8_t kArrayBuffer = 'B';
const uint8_t kTrue = 'T';
const uint8_t kFalse = 'F';
const uint8_t kDouble = 'N';
const uint8_t kInt32 = 'I';
const uint8_t kUint32 = 'U';
const uint8_t kError = 'r';
const uint8_t kBeginDenseJSArray = 'A';
const uint8_t kEndDenseJSArray = '$';
const uint8_t kArrayBufferTransferTag = 't';

const uint8_t kEvalErrorTag = 'E';
const uint8_t kRangeErrorTag = 'R';
const uint8_t kReferenceErrorTag = 'F';
const uint8_t kSyntaxErrorTag = 'S';
const uint8_t kTypeErrorTag = 'T';
const uint8_t kUriErrorTag = 'U';
const uint8_t kMessageTag = 'm';
const uint8_t kCauseTag = 'c';
const uint8_t kStackTag = 's';
const uint8_t kEndTag = '.';

void WriteVarintUint32(uint32_t value, std::vector<uint8_t>* buffer) {
  for (;;) {
    uint8_t b = (value & 0x7F);
    value >>= 7;
    if (!value) {
      buffer->push_back(b);
      break;
    }
    buffer->push_back(b | 0x80);
  }
}

void WriteStandardHeader(std::vector<uint8_t>* buffer) {
  buffer->push_back(kVersionTag);
  WriteVarintUint32(20, buffer);
  buffer->push_back(kVersionTag);
  WriteVarintUint32(14, buffer);
}

std::u16string FuzzToU16String(FuzzedDataProvider* fdp) {
  std::string str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  return std::u16string(str.begin(), str.end());
}

std::u16string FuzzToLatin1String(FuzzedDataProvider* fdp) {
  size_t len = fdp->ConsumeIntegralInRange<size_t>(0, MAX_STRING_LENGTH);
  std::u16string result;
  for (size_t i = 0; i < len; ++i) {
    result.push_back(static_cast<char16_t>(fdp->ConsumeIntegral<uint8_t>()));
  }
  return result;
}

std::u16string FuzzToNonLatin1String(FuzzedDataProvider* fdp) {
  size_t len = fdp->ConsumeIntegralInRange<size_t>(1, MAX_STRING_LENGTH);
  std::u16string result;
  for (size_t i = 0; i < len; ++i) {
    uint16_t c = fdp->ConsumeIntegral<uint16_t>();
    if (i == 0) {
      c = fdp->ConsumeBool() ? 0x0100
                              : (fdp->ConsumeIntegral<uint16_t>() | 0x0100);
    }
    result.push_back(static_cast<char16_t>(c));
  }
  return result;
}

void DoEncodeDecode(WebMessagePort::Message& original) {
  TransferableMessage encoded = EncodeWebMessagePayload(original);
  WebMessagePort::Message decoded;
  DecodeToWebMessagePayload(encoded, decoded);
}

void DoRawDecode(std::vector<uint8_t>& raw_bytes) {
  TransferableMessage message;
  message.owned_encoded_message = std::move(raw_bytes);
  message.encoded_message = message.owned_encoded_message;
  WebMessagePort::Message decoded;
  DecodeToWebMessagePayload(message, decoded);
}

}  // namespace

struct Environment {
  Environment() { base::CommandLine::Init(0, nullptr); }
};

// ============================================================================
// Phase 1: Encode-decode round trip tests
// ============================================================================

void FuzzEncodeDecodeString(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::STRING;
  msg.data = FuzzToU16String(fdp);
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeLatin1String(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::STRING;
  msg.data = FuzzToLatin1String(fdp);
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeNonLatin1String(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::STRING;
  msg.data = FuzzToNonLatin1String(fdp);
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeInt32Range(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::INTEGER;
  msg.int64_value_ = fdp->ConsumeIntegralInRange<int64_t>(
      std::numeric_limits<int32_t>::min(),
      std::numeric_limits<int32_t>::max());
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeUint32Range(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::INTEGER;
  msg.int64_value_ = fdp->ConsumeIntegralInRange<int64_t>(
      static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1,
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()));
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeLargeInt64(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::INTEGER;
  uint8_t choice = fdp->ConsumeIntegral<uint8_t>();
  if (choice % 2 == 0) {
    msg.int64_value_ = fdp->ConsumeIntegralInRange<int64_t>(
        std::numeric_limits<int64_t>::min(),
        static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) - 1);
  } else {
    msg.int64_value_ = fdp->ConsumeIntegralInRange<int64_t>(
        static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1,
        std::numeric_limits<int64_t>::max());
  }
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeBoolean(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::BOOLEAN;
  msg.bool_value_ = fdp->ConsumeBool();
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeDouble(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::DOUBLE;
  msg.double_value_ = fdp->ConsumeFloatingPoint<double>();
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeBinary(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::BINARY;
  size_t size = fdp->ConsumeIntegralInRange<size_t>(0, MAX_STRING_LENGTH);
  msg.array_buffer = fdp->ConsumeBytes<uint8_t>(size);
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeSpecificError(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::ERROR;
  uint8_t error_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 6);
  switch (error_type) {
    case 0: msg.err_name_ = u"EvalError"; break;
    case 1: msg.err_name_ = u"RangeError"; break;
    case 2: msg.err_name_ = u"ReferenceError"; break;
    case 3: msg.err_name_ = u"SyntaxError"; break;
    case 4: msg.err_name_ = u"TypeError"; break;
    case 5: msg.err_name_ = u"URIError"; break;
    default: msg.err_name_ = FuzzToU16String(fdp); break;
  }
  msg.err_msg_ = FuzzToU16String(fdp);
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeStringArray(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::STRINGARRAY;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
  for (size_t i = 0; i < count; ++i) {
    msg.string_arr_.push_back(FuzzToU16String(fdp));
  }
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeBoolArray(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::BOOLEANARRAY;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
  for (size_t i = 0; i < count; ++i) {
    msg.bool_arr_.push_back(fdp->ConsumeBool());
  }
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeDoubleArray(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::DOUBLEARRAY;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
  for (size_t i = 0; i < count; ++i) {
    msg.double_arr_.push_back(fdp->ConsumeFloatingPoint<double>());
  }
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeInt64Array(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::INT64ARRAY;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
  for (size_t i = 0; i < count; ++i) {
    msg.int64_arr_.push_back(fdp->ConsumeIntegral<int64_t>());
  }
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeEmptyTypes(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  uint8_t type_choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 2);
  switch (type_choice) {
    case 0: msg.type_ = WebMessagePort::Message::MessageType::NONE; break;
    case 1: msg.type_ = WebMessagePort::Message::MessageType::DICTIONARY; break;
    case 2: msg.type_ = WebMessagePort::Message::MessageType::LIST; break;
  }
  DoEncodeDecode(msg);
}

void FuzzEncodeDecodeNonLatin1StringArray(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::STRINGARRAY;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
  for (size_t i = 0; i < count; ++i) {
    msg.string_arr_.push_back(FuzzToNonLatin1String(fdp));
  }
  DoEncodeDecode(msg);
}

// Large non-Latin1 string to trigger WriteU16string padding alignment branch
void FuzzEncodeDecodeLargeNonLatin1(FuzzedDataProvider* fdp) {
  WebMessagePort::Message msg;
  msg.type_ = WebMessagePort::Message::MessageType::STRING;
  size_t len = fdp->ConsumeIntegralInRange<size_t>(64, MAX_STRING_LENGTH);
  std::u16string result;
  for (size_t i = 0; i < len; ++i) {
    result.push_back(static_cast<char16_t>(fdp->ConsumeIntegral<uint16_t>() | 0x0100));
  }
  msg.data = result;
  DoEncodeDecode(msg);
}

// ============================================================================
// Phase 2: Targeted raw decode tests
// ============================================================================

void FuzzDecodeRaw(FuzzedDataProvider* fdp) {
  size_t size = fdp->ConsumeIntegralInRange<size_t>(0, 512);
  std::vector<uint8_t> raw_bytes = fdp->ConsumeBytes<uint8_t>(size);
  if (raw_bytes.empty()) return;
  DoRawDecode(raw_bytes);
}

void FuzzDecodeSpecificTag(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  uint8_t tag_choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 12);
  WriteStandardHeader(&raw_bytes);
  switch (tag_choice) {
    case 0: {
      raw_bytes.push_back(kOneByteStringTag);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(1, 50);
      WriteVarintUint32(static_cast<uint32_t>(len), &raw_bytes);
      for (size_t i = 0; i < len; ++i)
        raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
      break;
    }
    case 1: {
      raw_bytes.push_back(kPaddingTag);
      raw_bytes.push_back(kTwoByteStringTag);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(1, 20);
      WriteVarintUint32(static_cast<uint32_t>(len * 2), &raw_bytes);
      for (size_t i = 0; i < len; ++i) {
        char16_t c = static_cast<char16_t>(fdp->ConsumeIntegral<uint16_t>());
        raw_bytes.push_back(static_cast<uint8_t>(c & 0xFF));
        raw_bytes.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
      }
      break;
    }
    case 2: raw_bytes.push_back(kTrue); break;
    case 3: raw_bytes.push_back(kFalse); break;
    case 4: {
      raw_bytes.push_back(kDouble);
      double val = fdp->ConsumeFloatingPoint<double>();
      const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
      for (size_t i = 0; i < sizeof(double); ++i) raw_bytes.push_back(p[i]);
      break;
    }
    case 5: {
      raw_bytes.push_back(kInt32);
      int32_t val = fdp->ConsumeIntegral<int32_t>();
      uint32_t zigzag = (static_cast<uint32_t>(val) << 1) ^ (val >> 31);
      WriteVarintUint32(zigzag, &raw_bytes);
      break;
    }
    case 6: {
      raw_bytes.push_back(kUint32);
      WriteVarintUint32(fdp->ConsumeIntegral<uint32_t>(), &raw_bytes);
      break;
    }
    case 7: {
      raw_bytes.push_back(kArrayBuffer);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(0, 50);
      WriteVarintUint32(static_cast<uint32_t>(len), &raw_bytes);
      for (size_t i = 0; i < len; ++i)
        raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
      break;
    }
    case 8: {
      raw_bytes.push_back(kError);
      uint8_t err_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 5);
      switch (err_type) {
        case 0: raw_bytes.push_back(kEvalErrorTag); break;
        case 1: raw_bytes.push_back(kRangeErrorTag); break;
        case 2: raw_bytes.push_back(kReferenceErrorTag); break;
        case 3: raw_bytes.push_back(kSyntaxErrorTag); break;
        case 4: raw_bytes.push_back(kTypeErrorTag); break;
        case 5: raw_bytes.push_back(kUriErrorTag); break;
      }
      raw_bytes.push_back(kMessageTag);
      raw_bytes.push_back(kOneByteStringTag);
      size_t msg_len = fdp->ConsumeIntegralInRange<size_t>(0, 30);
      WriteVarintUint32(static_cast<uint32_t>(msg_len), &raw_bytes);
      for (size_t i = 0; i < msg_len; ++i)
        raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
      raw_bytes.push_back(kStackTag);
      raw_bytes.push_back(kOneByteStringTag);
      size_t stack_len = fdp->ConsumeIntegralInRange<size_t>(0, 30);
      WriteVarintUint32(static_cast<uint32_t>(stack_len), &raw_bytes);
      for (size_t i = 0; i < stack_len; ++i)
        raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
      raw_bytes.push_back(fdp->ConsumeBool() ? kEndTag : kCauseTag);
      break;
    }
    case 9: {
      raw_bytes.push_back(kBeginDenseJSArray);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(2, 5);
      WriteVarintUint32(static_cast<uint32_t>(len), &raw_bytes);
      raw_bytes.push_back(kDouble);
      double val = fdp->ConsumeFloatingPoint<double>();
      const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
      for (size_t i = 0; i < sizeof(double); ++i) raw_bytes.push_back(p[i]);
      raw_bytes.push_back(kOneByteStringTag);
      size_t str_len = fdp->ConsumeIntegralInRange<size_t>(1, 10);
      WriteVarintUint32(static_cast<uint32_t>(str_len), &raw_bytes);
      for (size_t i = 0; i < str_len; ++i)
        raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
      break;
    }
    case 10: {
      raw_bytes.push_back(kBeginDenseJSArray);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(1, 5);
      WriteVarintUint32(static_cast<uint32_t>(len), &raw_bytes);
      for (size_t i = 0; i < len; ++i) {
        raw_bytes.push_back(kUint32);
        WriteVarintUint32(fdp->ConsumeIntegral<uint32_t>(), &raw_bytes);
      }
      raw_bytes.push_back(kEndDenseJSArray);
      WriteVarintUint32(0, &raw_bytes);
      WriteVarintUint32(static_cast<uint32_t>(len), &raw_bytes);
      break;
    }
    case 11: {
      raw_bytes.push_back(kArrayBufferTransferTag);
      WriteVarintUint32(0, &raw_bytes);
      break;
    }
    default:
      raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
      break;
  }
  DoRawDecode(raw_bytes);
}

void FuzzDecodeWithTrailer(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  raw_bytes.push_back(kVersionTag);
  WriteVarintUint32(21 + fdp->ConsumeIntegralInRange<uint32_t>(0, 10),
                    &raw_bytes);
  raw_bytes.push_back(0xFE);
  uint64_t offset = fdp->ConsumeIntegral<uint64_t>();
  uint32_t size = fdp->ConsumeIntegral<uint32_t>();
  const uint8_t* offset_ptr = reinterpret_cast<const uint8_t*>(&offset);
  const uint8_t* size_ptr = reinterpret_cast<const uint8_t*>(&size);
  for (size_t i = 0; i < sizeof(uint64_t); ++i)
    raw_bytes.push_back(offset_ptr[i]);
  for (size_t i = 0; i < sizeof(uint32_t); ++i)
    raw_bytes.push_back(size_ptr[i]);
  raw_bytes.push_back(kVersionTag);
  WriteVarintUint32(14, &raw_bytes);
  raw_bytes.push_back(kTrue);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeArrayUnsupportedElement(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  WriteVarintUint32(1, &raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(1, &raw_bytes);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeEmptyArray(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(0, &raw_bytes);
  DoRawDecode(raw_bytes);
}

void FuzzDecodePaddingVersionHeader(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  raw_bytes.push_back(kVersionTag);
  WriteVarintUint32(20, &raw_bytes);
  size_t pad_count = fdp->ConsumeIntegralInRange<size_t>(1, 5);
  for (size_t i = 0; i < pad_count; ++i) {
    raw_bytes.push_back(kPaddingTag);
  }
  raw_bytes.push_back(kVersionTag);
  WriteVarintUint32(14, &raw_bytes);
  raw_bytes.push_back(kPaddingTag);
  raw_bytes.push_back(kTrue);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeArrayInt32Elements(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  size_t count = fdp->ConsumeIntegralInRange<size_t>(1, 5);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  for (size_t i = 0; i < count; ++i) {
    raw_bytes.push_back(kInt32);
    int32_t val = fdp->ConsumeIntegral<int32_t>();
    uint32_t zigzag = (static_cast<uint32_t>(val) << 1) ^ (val >> 31);
    WriteVarintUint32(zigzag, &raw_bytes);
  }
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeErrorUnknownTag(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kTypeErrorTag);
  raw_bytes.push_back(0x01);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeArrayTwoByteStringElements(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  size_t count = fdp->ConsumeIntegralInRange<size_t>(1, 3);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  for (size_t i = 0; i < count; ++i) {
    raw_bytes.push_back(kTwoByteStringTag);
    size_t char_count = fdp->ConsumeIntegralInRange<size_t>(1, 10);
    WriteVarintUint32(static_cast<uint32_t>(char_count * 2), &raw_bytes);
    for (size_t j = 0; j < char_count; ++j) {
      char16_t c = static_cast<char16_t>(fdp->ConsumeIntegral<uint16_t>());
      raw_bytes.push_back(static_cast<uint8_t>(c & 0xFF));
      raw_bytes.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
  }
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeBareTagNoHeader(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  uint8_t tag = fdp->ConsumeIntegral<uint8_t>();
  raw_bytes.push_back(tag);
  if (tag == kOneByteStringTag) {
    WriteVarintUint32(3, &raw_bytes);
    raw_bytes.push_back('a');
    raw_bytes.push_back('b');
    raw_bytes.push_back('c');
  } else if (tag == kDouble) {
    double val = fdp->ConsumeFloatingPoint<double>();
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    for (size_t i = 0; i < sizeof(double); ++i) raw_bytes.push_back(p[i]);
  } else if (tag == kBeginDenseJSArray) {
    WriteVarintUint32(0, &raw_bytes);
    raw_bytes.push_back(kEndDenseJSArray);
    WriteVarintUint32(0, &raw_bytes);
    WriteVarintUint32(0, &raw_bytes);
  } else if (tag == kError) {
    raw_bytes.push_back(kSyntaxErrorTag);
    raw_bytes.push_back(kEndTag);
  }
  DoRawDecode(raw_bytes);
}

void FuzzDecodeVersion21InvalidTrailer(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  raw_bytes.push_back(kVersionTag);
  uint32_t version = 21 + fdp->ConsumeIntegralInRange<uint32_t>(0, 10);
  WriteVarintUint32(version, &raw_bytes);
  raw_bytes.push_back(0x01);
  for (size_t i = 0; i < 12; ++i) raw_bytes.push_back(0x00);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeErrorWithCauseAndStack(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  uint8_t err_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 5);
  switch (err_type) {
    case 0: raw_bytes.push_back(kEvalErrorTag); break;
    case 1: raw_bytes.push_back(kRangeErrorTag); break;
    case 2: raw_bytes.push_back(kReferenceErrorTag); break;
    case 3: raw_bytes.push_back(kSyntaxErrorTag); break;
    case 4: raw_bytes.push_back(kTypeErrorTag); break;
    case 5: raw_bytes.push_back(kUriErrorTag); break;
  }
  raw_bytes.push_back(kMessageTag);
  raw_bytes.push_back(kTwoByteStringTag);
  size_t msg_len = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  WriteVarintUint32(static_cast<uint32_t>(msg_len * 2), &raw_bytes);
  for (size_t i = 0; i < msg_len; ++i) {
    char16_t c = static_cast<char16_t>(fdp->ConsumeIntegral<uint16_t>());
    raw_bytes.push_back(static_cast<uint8_t>(c & 0xFF));
    raw_bytes.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
  }
  raw_bytes.push_back(kStackTag);
  raw_bytes.push_back(kOneByteStringTag);
  size_t stack_len = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  WriteVarintUint32(static_cast<uint32_t>(stack_len), &raw_bytes);
  for (size_t i = 0; i < stack_len; ++i)
    raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
  raw_bytes.push_back(kCauseTag);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeArrayDoubleElements(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  size_t count = fdp->ConsumeIntegralInRange<size_t>(1, 3);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  for (size_t i = 0; i < count; ++i) {
    raw_bytes.push_back(kDouble);
    double val = fdp->ConsumeFloatingPoint<double>();
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    for (size_t j = 0; j < sizeof(double); ++j) raw_bytes.push_back(p[j]);
  }
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeArrayBoolMixed(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  size_t count = fdp->ConsumeIntegralInRange<size_t>(2, 5);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  for (size_t i = 0; i < count; ++i) {
    raw_bytes.push_back(fdp->ConsumeBool() ? kTrue : kFalse);
  }
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeErrorMinimal(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kEndTag);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeArrayTruncated(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  WriteVarintUint32(5, &raw_bytes);
  raw_bytes.push_back(kOneByteStringTag);
  WriteVarintUint32(3, &raw_bytes);
  raw_bytes.push_back('a');
  raw_bytes.push_back('b');
  raw_bytes.push_back('c');
  DoRawDecode(raw_bytes);
}

void FuzzDecodeErrorAllPrototypes(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kEvalErrorTag);
  raw_bytes.push_back(kRangeErrorTag);
  raw_bytes.push_back(kReferenceErrorTag);
  raw_bytes.push_back(kSyntaxErrorTag);
  raw_bytes.push_back(kTypeErrorTag);
  raw_bytes.push_back(kUriErrorTag);
  raw_bytes.push_back(kMessageTag);
  raw_bytes.push_back(kOneByteStringTag);
  WriteVarintUint32(5, &raw_bytes);
  raw_bytes.push_back('e');
  raw_bytes.push_back('r');
  raw_bytes.push_back('r');
  raw_bytes.push_back('o');
  raw_bytes.push_back('r');
  raw_bytes.push_back(kEndTag);
  DoRawDecode(raw_bytes);
}

void FuzzDecodeOnlyPadding(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  size_t pad_count = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  for (size_t i = 0; i < pad_count; ++i) {
    raw_bytes.push_back(kPaddingTag);
  }
  DoRawDecode(raw_bytes);
}

void FuzzDecodeVersionTruncated(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  raw_bytes.push_back(kVersionTag);
  DoRawDecode(raw_bytes);
}

// Error with kMessage + invalid string tag -> ReadU16string default case
void FuzzDecodeErrorInvalidString(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kTypeErrorTag);
  raw_bytes.push_back(kMessageTag);
  raw_bytes.push_back(0x01);  // invalid tag for ReadU16string
  DoRawDecode(raw_bytes);
}

// Error with kStack + two-byte string through ReadError
void FuzzDecodeErrorStackTwoByte(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kEvalErrorTag);
  raw_bytes.push_back(kStackTag);
  raw_bytes.push_back(kTwoByteStringTag);
  size_t char_count = fdp->ConsumeIntegralInRange<size_t>(1, 5);
  WriteVarintUint32(static_cast<uint32_t>(char_count * 2), &raw_bytes);
  for (size_t i = 0; i < char_count; ++i) {
    char16_t c = static_cast<char16_t>(fdp->ConsumeIntegral<uint16_t>());
    raw_bytes.push_back(static_cast<uint8_t>(c & 0xFF));
    raw_bytes.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
  }
  raw_bytes.push_back(kEndTag);
  DoRawDecode(raw_bytes);
}

// Array with int32 element but no delimiter -> ReadInt32InArray failure
void FuzzDecodeArrayInt32NoDelimiter(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  WriteVarintUint32(1, &raw_bytes);
  raw_bytes.push_back(kInt32);
  size_t varint_len = fdp->ConsumeIntegralInRange<size_t>(1, 5);
  for (size_t i = 0; i < varint_len; ++i) {
    uint8_t b = fdp->ConsumeIntegral<uint8_t>();
    if (b == kEndDenseJSArray || b == kInt32) b = 0x01;
    raw_bytes.push_back(b);
  }
  DoRawDecode(raw_bytes);
}

// Array with Uint32 elements specifically
void FuzzDecodeArrayUint32Elements(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  size_t count = fdp->ConsumeIntegralInRange<size_t>(1, 5);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  for (size_t i = 0; i < count; ++i) {
    raw_bytes.push_back(kUint32);
    WriteVarintUint32(fdp->ConsumeIntegral<uint32_t>(), &raw_bytes);
  }
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  DoRawDecode(raw_bytes);
}

// Array with one-byte string elements specifically
void FuzzDecodeArrayOneByteStringElements(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  size_t count = fdp->ConsumeIntegralInRange<size_t>(1, 3);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  for (size_t i = 0; i < count; ++i) {
    raw_bytes.push_back(kOneByteStringTag);
    size_t str_len = fdp->ConsumeIntegralInRange<size_t>(1, 10);
    WriteVarintUint32(static_cast<uint32_t>(str_len), &raw_bytes);
    for (size_t j = 0; j < str_len; ++j)
      raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
  }
  raw_bytes.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &raw_bytes);
  WriteVarintUint32(static_cast<uint32_t>(count), &raw_bytes);
  DoRawDecode(raw_bytes);
}

// ArrayBuffer with large claimed size but truncated data
void FuzzDecodeArrayBufferTruncated(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kArrayBuffer);
  uint32_t claimed_size = fdp->ConsumeIntegralInRange<uint32_t>(10, 200);
  WriteVarintUint32(claimed_size, &raw_bytes);
  size_t actual_size = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < actual_size; ++i)
    raw_bytes.push_back(fdp->ConsumeIntegral<uint8_t>());
  DoRawDecode(raw_bytes);
}

// Array with explicit type mismatch (double then string)
void FuzzDecodeArrayTypeMismatch(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kBeginDenseJSArray);
  WriteVarintUint32(2, &raw_bytes);
  raw_bytes.push_back(kDouble);
  double val = fdp->ConsumeFloatingPoint<double>();
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
  for (size_t i = 0; i < sizeof(double); ++i) raw_bytes.push_back(p[i]);
  raw_bytes.push_back(kOneByteStringTag);
  WriteVarintUint32(3, &raw_bytes);
  raw_bytes.push_back('a');
  raw_bytes.push_back('b');
  raw_bytes.push_back('c');
  DoRawDecode(raw_bytes);
}

// Error with kCause as first sub-tag (immediate done=true)
void FuzzDecodeErrorCauseFirst(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  raw_bytes.push_back(kError);
  raw_bytes.push_back(kCauseTag);
  DoRawDecode(raw_bytes);
}

// Header only, no payload tag
void FuzzDecodeHeaderOnly(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> raw_bytes;
  WriteStandardHeader(&raw_bytes);
  DoRawDecode(raw_bytes);
}

// ============================================================================
// Phase 3: Direct internal function calls to exercise code paths unreachable
// through the public encode/decode API.
// ============================================================================

// Exercise IsBoolElement / IsStringElement / IsNumberElement (dead through API)
void FuzzElementCheckers(FuzzedDataProvider* fdp) {
  IsBoolElement(kTrue);
  IsBoolElement(kFalse);
  IsBoolElement(0x00);
  IsBoolElement(fdp->ConsumeIntegral<uint8_t>());

  IsStringElement(kOneByteStringTag);
  IsStringElement(kTwoByteStringTag);
  IsStringElement(0x00);
  IsStringElement(fdp->ConsumeIntegral<uint8_t>());

  IsNumberElement(kDouble);
  IsNumberElement(kInt32);
  IsNumberElement(kUint32);
  IsNumberElement(0x00);
  IsNumberElement(fdp->ConsumeIntegral<uint8_t>());
}

// Exercise WriteString (dead through API - never called by EncodeWebMessagePayload)
void FuzzWriteString(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  std::string str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  WriteString(str, &buffer);
  // Also feed the produced bytes back through DecodeToWebMessagePayload
  TransferableMessage message;
  std::vector<uint8_t> with_header;
  WriteStandardHeader(&with_header);
  with_header.insert(with_header.end(), buffer.begin(), buffer.end());
  message.owned_encoded_message = std::move(with_header);
  message.encoded_message = message.owned_encoded_message;
  WebMessagePort::Message decoded;
  DecodeToWebMessagePayload(message, decoded);
}

// Exercise WriteHeader directly
void FuzzWriteHeader(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  WriteHeader(&buffer);
}

// Exercise WriteU16string with controlled buffer alignment for padding branches
void FuzzWriteU16stringDirect(FuzzedDataProvider* fdp) {
  // Buffer empty - even alignment
  std::vector<uint8_t> buffer1;
  WriteU16string(FuzzToNonLatin1String(fdp), &buffer1);

  // Buffer with one byte - odd alignment, triggers padding
  std::vector<uint8_t> buffer2;
  buffer2.push_back(0x00);
  WriteU16string(FuzzToNonLatin1String(fdp), &buffer2);

  // Latin1 string - alternate branch
  std::vector<uint8_t> buffer3;
  WriteU16string(FuzzToLatin1String(fdp), &buffer3);

  // Empty string
  std::vector<uint8_t> buffer4;
  WriteU16string(std::u16string(), &buffer4);
}

// Exercise WriteBool / WriteDouble / WriteInt64 directly
void FuzzWritePrimitivesDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  WriteBool(true, &buffer);
  WriteBool(false, &buffer);
  WriteDouble(fdp->ConsumeFloatingPoint<double>(), &buffer);

  // All 3 WriteInt64 branches
  WriteInt64(fdp->ConsumeIntegralInRange<int64_t>(
                 std::numeric_limits<int32_t>::min(),
                 std::numeric_limits<int32_t>::max()),
             &buffer);
  WriteInt64(fdp->ConsumeIntegralInRange<int64_t>(
                 static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1,
                 static_cast<int64_t>(std::numeric_limits<uint32_t>::max())),
             &buffer);
  WriteInt64(fdp->ConsumeIntegralInRange<int64_t>(
                 static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1,
                 std::numeric_limits<int64_t>::max()),
             &buffer);
  WriteInt64(fdp->ConsumeIntegralInRange<int64_t>(
                 std::numeric_limits<int64_t>::min(),
                 static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1),
             &buffer);
}

// Exercise WriteError directly with all 6 named errors + unknown name
// (covers ConvertToErrType's loop iterating over the typeMap)
void FuzzWriteErrorDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  std::u16string name;
  uint8_t choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 7);
  switch (choice) {
    case 0: name = u"EvalError"; break;
    case 1: name = u"RangeError"; break;
    case 2: name = u"ReferenceError"; break;
    case 3: name = u"SyntaxError"; break;
    case 4: name = u"TypeError"; break;
    case 5: name = u"URIError"; break;
    case 6: name = u""; break;
    default: name = FuzzToU16String(fdp); break;
  }
  WriteError(name, FuzzToU16String(fdp), &buffer);
}

// Exercise WriteArrayBuffer directly
void FuzzWriteArrayBufferDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  std::vector<uint8_t> arrbuf;
  size_t size = fdp->ConsumeIntegralInRange<size_t>(0, 50);
  for (size_t i = 0; i < size; ++i) {
    arrbuf.push_back(fdp->ConsumeIntegral<uint8_t>());
  }
  TransferableMessage message;
  WriteArrayBuffer(arrbuf, message, &buffer);
}

// Exercise WriteStringArray with mixed Latin1/non-Latin1 strings
void FuzzWriteStringArrayDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  std::vector<std::u16string> arr;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < count; ++i) {
    if (fdp->ConsumeBool()) {
      arr.push_back(FuzzToNonLatin1String(fdp));
    } else {
      arr.push_back(FuzzToLatin1String(fdp));
    }
  }
  WriteStringArray(arr, &buffer);
}

// Exercise WriteBoolArray / WriteDoubleArray / WriteInt64Array directly
void FuzzWriteArraysDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;

  std::vector<bool> bool_arr;
  size_t bc = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < bc; ++i) bool_arr.push_back(fdp->ConsumeBool());
  WriteBoolArray(bool_arr, &buffer);

  std::vector<double> double_arr;
  size_t dc = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < dc; ++i)
    double_arr.push_back(fdp->ConsumeFloatingPoint<double>());
  WriteDoubleArray(double_arr, &buffer);

  std::vector<int64_t> int64_arr;
  size_t ic = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < ic; ++i)
    int64_arr.push_back(fdp->ConsumeIntegral<int64_t>());
  WriteInt64Array(int64_arr, &buffer);
}

// Exercise ReadU16string directly - especially the default case
// (unsupported tag, returns false)
void FuzzReadU16stringDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  uint8_t tag_choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 2);
  switch (tag_choice) {
    case 0: {
      buffer.push_back(kOneByteStringTag);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(0, 30);
      WriteVarintUint32(static_cast<uint32_t>(len), &buffer);
      for (size_t i = 0; i < len; ++i)
        buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
      break;
    }
    case 1: {
      buffer.push_back(kTwoByteStringTag);
      size_t len = fdp->ConsumeIntegralInRange<size_t>(0, 30);
      WriteVarintUint32(static_cast<uint32_t>(len * 2), &buffer);
      for (size_t i = 0; i < len; ++i) {
        char16_t c = static_cast<char16_t>(fdp->ConsumeIntegral<uint16_t>());
        buffer.push_back(static_cast<uint8_t>(c & 0xFF));
        buffer.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
      }
      break;
    }
    default: {  // Unknown tag - covers default returning false
      buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
      break;
    }
  }
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  std::u16string str;
  ReadU16string(iter, str);
}

// Exercise ReadOneByteString directly with various length/data combinations
void FuzzReadOneByteStringDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  uint32_t claimed_len = fdp->ConsumeIntegralInRange<uint32_t>(0, 100);
  WriteVarintUint32(claimed_len, &buffer);
  size_t actual = fdp->ConsumeIntegralInRange<size_t>(0, 30);
  for (size_t i = 0; i < actual; ++i)
    buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  std::u16string str;
  ReadOneByteString(iter, str);
}

// Exercise ReadTwoByteString directly
void FuzzReadTwoByteStringDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  uint32_t claimed_bytes = fdp->ConsumeIntegralInRange<uint32_t>(0, 100);
  WriteVarintUint32(claimed_bytes, &buffer);
  size_t actual = fdp->ConsumeIntegralInRange<size_t>(0, 30);
  for (size_t i = 0; i < actual; ++i)
    buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  std::u16string str;
  ReadTwoByteString(iter, str);
}

// Exercise ReadDouble directly - especially failure path (not enough bytes)
void FuzzReadDoubleDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  size_t size = fdp->ConsumeIntegralInRange<size_t>(0, 12);
  for (size_t i = 0; i < size; ++i)
    buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  double result;
  ReadDouble(iter, result);
}

// Exercise ReadInt32 directly
void FuzzReadInt32Direct(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  size_t size = fdp->ConsumeIntegralInRange<size_t>(0, 10);
  for (size_t i = 0; i < size; ++i)
    buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  int32_t result;
  ReadInt32(iter, result);
}

// Exercise ReadInt32InArray directly with various delimiter scenarios
void FuzzReadInt32InArrayDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  int32_t val = fdp->ConsumeIntegral<int32_t>();
  uint32_t zigzag = (static_cast<uint32_t>(val) << 1) ^ (val >> 31);
  WriteVarintUint32(zigzag, &buffer);
  uint8_t delim_choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 2);
  switch (delim_choice) {
    case 0: buffer.push_back(kEndDenseJSArray); break;
    case 1: buffer.push_back(kInt32); break;
    default: break;  // No delimiter - exercises while-loop exhaustion
  }
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  int32_t result;
  ReadInt32InArray(iter, result);
}

// Exercise ReadHeaderTag directly with random/edge-case buffers
void FuzzReadHeaderTagDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  size_t size = fdp->ConsumeIntegralInRange<size_t>(0, 50);
  for (size_t i = 0; i < size; ++i)
    buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
  if (buffer.empty()) return;
  base::BufferIterator<const uint8_t> iter(buffer);
  uint8_t tag;
  ReadHeaderTag(iter, tag);
}

// Exercise ReadArray directly - skip the header/tag overhead
void FuzzReadArrayDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  // Build array body: length + elements + end-tag + numProperties + length
  uint32_t len = fdp->ConsumeIntegralInRange<uint32_t>(0, 5);
  WriteVarintUint32(len, &buffer);
  uint8_t elem_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 6);
  for (uint32_t i = 0; i < len; ++i) {
    switch (elem_type) {
      case 0: buffer.push_back(kTrue); break;
      case 1: buffer.push_back(kFalse); break;
      case 2: {
        buffer.push_back(kOneByteStringTag);
        WriteVarintUint32(3, &buffer);
        buffer.push_back('a'); buffer.push_back('b'); buffer.push_back('c');
        break;
      }
      case 3: {
        buffer.push_back(kDouble);
        double v = fdp->ConsumeFloatingPoint<double>();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        for (size_t j = 0; j < sizeof(double); ++j) buffer.push_back(p[j]);
        break;
      }
      case 4: {
        buffer.push_back(kInt32);
        int32_t v = fdp->ConsumeIntegral<int32_t>();
        uint32_t zz = (static_cast<uint32_t>(v) << 1) ^ (v >> 31);
        WriteVarintUint32(zz, &buffer);
        break;
      }
      case 5: {
        buffer.push_back(kUint32);
        WriteVarintUint32(fdp->ConsumeIntegral<uint32_t>(), &buffer);
        break;
      }
      default: {  // Unsupported element - hits default case
        buffer.push_back(fdp->ConsumeIntegral<uint8_t>());
        break;
      }
    }
  }
  buffer.push_back(kEndDenseJSArray);
  WriteVarintUint32(0, &buffer);
  WriteVarintUint32(len, &buffer);

  base::BufferIterator<const uint8_t> iter(buffer);
  WebMessagePort::Message decoded;
  ReadArray(iter, decoded);
}

// Exercise ReadError directly with crafted sub-tag sequences
void FuzzReadErrorDirect(FuzzedDataProvider* fdp) {
  std::vector<uint8_t> buffer;
  size_t tag_count = fdp->ConsumeIntegralInRange<size_t>(1, 6);
  for (size_t i = 0; i < tag_count; ++i) {
    uint8_t sub_choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 10);
    switch (sub_choice) {
      case 0: buffer.push_back(kEvalErrorTag); break;
      case 1: buffer.push_back(kRangeErrorTag); break;
      case 2: buffer.push_back(kReferenceErrorTag); break;
      case 3: buffer.push_back(kSyntaxErrorTag); break;
      case 4: buffer.push_back(kTypeErrorTag); break;
      case 5: buffer.push_back(kUriErrorTag); break;
      case 6: {
        buffer.push_back(kMessageTag);
        buffer.push_back(kOneByteStringTag);
        WriteVarintUint32(3, &buffer);
        buffer.push_back('a'); buffer.push_back('b'); buffer.push_back('c');
        break;
      }
      case 7: {
        buffer.push_back(kStackTag);
        buffer.push_back(kTwoByteStringTag);
        WriteVarintUint32(4, &buffer);
        buffer.push_back('x'); buffer.push_back(0x00);
        buffer.push_back('y'); buffer.push_back(0x00);
        break;
      }
      case 8: buffer.push_back(kCauseTag); break;
      case 9: buffer.push_back(kEndTag); break;
      default: buffer.push_back(fdp->ConsumeIntegral<uint8_t>()); break;
    }
  }
  base::BufferIterator<const uint8_t> iter(buffer);
  WebMessagePort::Message decoded;
  ReadError(iter, decoded);
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_MSGPORT)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

#if BUILDFLAG(ARKWEB_MSGPORT)
  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  // Phase 1: Encode-decode round trip
  uint8_t encode_choice = fdp.ConsumeIntegralInRange<uint8_t>(0, 17);
  switch (encode_choice) {
    case 0: blink::FuzzEncodeDecodeString(&fdp); break;
    case 1: blink::FuzzEncodeDecodeLatin1String(&fdp); break;
    case 2: blink::FuzzEncodeDecodeNonLatin1String(&fdp); break;
    case 3: blink::FuzzEncodeDecodeInt32Range(&fdp); break;
    case 4: blink::FuzzEncodeDecodeUint32Range(&fdp); break;
    case 5: blink::FuzzEncodeDecodeLargeInt64(&fdp); break;
    case 6: blink::FuzzEncodeDecodeBoolean(&fdp); break;
    case 7: blink::FuzzEncodeDecodeDouble(&fdp); break;
    case 8: blink::FuzzEncodeDecodeBinary(&fdp); break;
    case 9: blink::FuzzEncodeDecodeSpecificError(&fdp); break;
    case 10: blink::FuzzEncodeDecodeStringArray(&fdp); break;
    case 11: blink::FuzzEncodeDecodeBoolArray(&fdp); break;
    case 12: blink::FuzzEncodeDecodeDoubleArray(&fdp); break;
    case 13: blink::FuzzEncodeDecodeInt64Array(&fdp); break;
    case 14: blink::FuzzEncodeDecodeEmptyTypes(&fdp); break;
    case 15: blink::FuzzEncodeDecodeNonLatin1StringArray(&fdp); break;
    case 16: blink::FuzzDecodeRaw(&fdp); break;
    case 17: blink::FuzzEncodeDecodeLargeNonLatin1(&fdp); break;
    default: blink::FuzzEncodeDecodeString(&fdp); break;
  }

  // Phase 2: Targeted raw decode
  uint8_t decode_choice = fdp.ConsumeIntegralInRange<uint8_t>(0, 28);
  switch (decode_choice) {
    case 0: blink::FuzzDecodeRaw(&fdp); break;
    case 1: blink::FuzzDecodeSpecificTag(&fdp); break;
    case 2: blink::FuzzDecodeWithTrailer(&fdp); break;
    case 3: blink::FuzzDecodeArrayUnsupportedElement(&fdp); break;
    case 4: blink::FuzzDecodeEmptyArray(&fdp); break;
    case 5: blink::FuzzDecodePaddingVersionHeader(&fdp); break;
    case 6: blink::FuzzDecodeArrayInt32Elements(&fdp); break;
    case 7: blink::FuzzDecodeErrorUnknownTag(&fdp); break;
    case 8: blink::FuzzDecodeArrayTwoByteStringElements(&fdp); break;
    case 9: blink::FuzzDecodeBareTagNoHeader(&fdp); break;
    case 10: blink::FuzzDecodeVersion21InvalidTrailer(&fdp); break;
    case 11: blink::FuzzDecodeErrorWithCauseAndStack(&fdp); break;
    case 12: blink::FuzzDecodeArrayDoubleElements(&fdp); break;
    case 13: blink::FuzzDecodeArrayBoolMixed(&fdp); break;
    case 14: blink::FuzzDecodeErrorMinimal(&fdp); break;
    case 15: blink::FuzzDecodeArrayTruncated(&fdp); break;
    case 16: blink::FuzzDecodeErrorAllPrototypes(&fdp); break;
    case 17: blink::FuzzDecodeOnlyPadding(&fdp); break;
    case 18: blink::FuzzDecodeVersionTruncated(&fdp); break;
    case 19: blink::FuzzDecodeErrorInvalidString(&fdp); break;
    case 20: blink::FuzzDecodeErrorStackTwoByte(&fdp); break;
    case 21: blink::FuzzDecodeArrayInt32NoDelimiter(&fdp); break;
    case 22: blink::FuzzDecodeArrayUint32Elements(&fdp); break;
    case 23: blink::FuzzDecodeArrayOneByteStringElements(&fdp); break;
    case 24: blink::FuzzDecodeArrayBufferTruncated(&fdp); break;
    case 25: blink::FuzzDecodeArrayTypeMismatch(&fdp); break;
    case 26: blink::FuzzDecodeErrorCauseFirst(&fdp); break;
    case 27: blink::FuzzDecodeHeaderOnly(&fdp); break;
    case 28: blink::FuzzDecodeRaw(&fdp); break;
    default: blink::FuzzDecodeRaw(&fdp); break;
  }

  // Phase 3: Direct internal function calls (covers paths unreachable through
  // the public API: IsBoolElement / IsStringElement / IsNumberElement helpers,
  // WriteString dead branch, ReadU16string default tag, and isolated Read/Write
  // edge cases.)
  uint8_t direct_choice = fdp.ConsumeIntegralInRange<uint8_t>(0, 15);
  switch (direct_choice) {
    case 0: blink::FuzzElementCheckers(&fdp); break;
    case 1: blink::FuzzWriteString(&fdp); break;
    case 2: blink::FuzzWriteHeader(&fdp); break;
    case 3: blink::FuzzWriteU16stringDirect(&fdp); break;
    case 4: blink::FuzzWritePrimitivesDirect(&fdp); break;
    case 5: blink::FuzzWriteErrorDirect(&fdp); break;
    case 6: blink::FuzzWriteArrayBufferDirect(&fdp); break;
    case 7: blink::FuzzWriteStringArrayDirect(&fdp); break;
    case 8: blink::FuzzWriteArraysDirect(&fdp); break;
    case 9: blink::FuzzReadU16stringDirect(&fdp); break;
    case 10: blink::FuzzReadOneByteStringDirect(&fdp); break;
    case 11: blink::FuzzReadTwoByteStringDirect(&fdp); break;
    case 12: blink::FuzzReadDoubleDirect(&fdp); break;
    case 13: blink::FuzzReadInt32Direct(&fdp); break;
    case 14: blink::FuzzReadInt32InArrayDirect(&fdp); break;
    case 15: blink::FuzzReadHeaderTagDirect(&fdp); break;
    default: blink::FuzzReadArrayDirect(&fdp); break;
  }

  // Always run a couple more direct calls to keep coverage high per iteration
  if (fdp.remaining_bytes() > 4) {
    blink::FuzzReadArrayDirect(&fdp);
  }
  if (fdp.remaining_bytes() > 4) {
    blink::FuzzReadErrorDirect(&fdp);
  }
#endif

  return 0;
}