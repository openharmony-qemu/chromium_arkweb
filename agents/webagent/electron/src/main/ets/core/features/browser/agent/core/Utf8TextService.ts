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

import { util } from '@kit.ArkTS';

// --- UTF-8 编解码常量，与 native Data_processor 保持一致 ---
const UTF8_PREFIX_2BYTE: number = 0xC0;
const UTF8_PREFIX_3BYTE: number = 0xE0;
const UTF8_PREFIX_4BYTE: number = 0xF0;
const UTF8_STEP_1: number = 1;
const UTF8_STEP_2: number = 2;
const UTF8_STEP_3: number = 3;
const UTF8_STEP_4: number = 4;
const UTF8_REMAIN_2BYTE: number = 1; // 2字节序列: p + 1 < end
const UTF8_REMAIN_3BYTE: number = 2; // 3字节序列: p + 2 < end
const UTF8_REMAIN_4BYTE: number = 3; // 4字节序列: p + 3 < end

// --- UTF-8 检测掩码（用于条件判断） ---
const UTF8_MASK_CHK_1BYTE: number = 0x80; // 检测单字节
const UTF8_MASK_CHK_2BYTE: number = 0xE0; // 检测2字节前缀
const UTF8_MASK_CHK_3BYTE: number = 0xF0; // 检测3字节前缀
const UTF8_MASK_CHK_4BYTE: number = 0xF8; // 检测4字节前缀

/** 获取 UTF-8 字符的字节步长，非法序列返回 0 */
function utf8Step(bytes: Uint8Array, p: number, end: number): number {
  if (p >= end) {
    return 0;
  }
  const head = bytes[p];
  if ((head & UTF8_MASK_CHK_1BYTE) === 0) {
    return UTF8_STEP_1;
  }
  if ((head & UTF8_MASK_CHK_2BYTE) === UTF8_PREFIX_2BYTE && p + UTF8_REMAIN_2BYTE < end) {
    return UTF8_STEP_2;
  }
  if ((head & UTF8_MASK_CHK_3BYTE) === UTF8_PREFIX_3BYTE && p + UTF8_REMAIN_3BYTE < end) {
    return UTF8_STEP_3;
  }
  if ((head & UTF8_MASK_CHK_4BYTE) === UTF8_PREFIX_4BYTE && p + UTF8_REMAIN_4BYTE < end) {
    return UTF8_STEP_4;
  }
  return 0;
}

/**
 * 按 Unicode 码点统计与截断文本，口径与 native Data_processor 一致。
 *
 * ArkTS 字符串以 UTF-16 存储，`String.length` 数的是编码单元：一个 emoji 会被算成 2。
 * 送入模型或注入检测的文本必须按码点计量，否则两侧对同一段文本的长度判断会分叉。
 */
export class BrowserAgentUtf8TextService {
  private static readonly encoder: util.TextEncoder = new util.TextEncoder('utf-8');
  private static readonly decoder: util.TextDecoder = util.TextDecoder.create('utf-8');

  /** 统计 UTF-8 字符串的 Unicode 码点数，不修改原字符串。 */
  static countChars(str: string): number {
    if (str.length <= 0) {
      return 0;
    }

    let count = 0;
    let p = 0;
    const bytes = BrowserAgentUtf8TextService.encoder.encodeInto(str);
    const end = bytes.length;

    while (p < end) {
      const step = utf8Step(bytes, p, end);
      if (step === 0) {
        ++p;
        continue;
      }
      ++count;
      p += step;
    }
    return count;
  }

  /** 码点数是否超过 maxChars。命中即停，不必遍历完整文本。 */
  static exceeds(str: string, maxChars: number): boolean {
    if (str.length <= maxChars) {
      return false; // UTF-16 单元数是码点数的上界，不足则必然不超限
    }
    return BrowserAgentUtf8TextService.scanTo(str, maxChars) >= 0;
  }

  /**
   * 按码点将文本截断到 maxChars，未超限时返回原字符串。
   *
   * 截断点必然落在完整码点边界上，不会把一个多字节字符切成半个。
   */
  static truncateToChars(str: string, maxChars: number): string {
    if (str.length <= 0 || maxChars <= 0) {
      return '';
    }
    if (str.length <= maxChars) {
      return str;
    }
    const bytes = BrowserAgentUtf8TextService.encoder.encodeInto(str);
    const offset = BrowserAgentUtf8TextService.scanBytesTo(bytes, maxChars);
    if (offset < 0) {
      return str;
    }
    return BrowserAgentUtf8TextService.decoder.decodeWithStream(bytes.subarray(0, offset),
      { stream: false });
  }

  /** 走满 maxChars 个码点后的字节偏移；文本不足 maxChars 个码点时返回 -1。 */
  private static scanTo(str: string, maxChars: number): number {
    return BrowserAgentUtf8TextService.scanBytesTo(
      BrowserAgentUtf8TextService.encoder.encodeInto(str), maxChars);
  }

  private static scanBytesTo(bytes: Uint8Array, maxChars: number): number {
    const end = bytes.length;
    let count = 0;
    let p = 0;
    while (p < end && count < maxChars) {
      const step = utf8Step(bytes, p, end);
      if (step === 0) {
        ++p;
        continue;
      }
      ++count;
      p += step;
    }
    return p >= end ? -1 : p;
  }
}
