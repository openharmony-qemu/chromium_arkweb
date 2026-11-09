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

import {
  BridgeResponse,
  BridgeResponseType,
  BrowserExecutorEvent,
  BrowserLiteDomElement,
  BrowserLiteDomNode,
  BrowserLiteDomRect,
  BrowserPerceptionPayload,
  BrowserViewport,
  BrowserReadyPayload
} from '../../../shared/domain/BrowserAutomationTypes';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'BrowserTransportProtocol';

/**
 * 扩展侧业务协议的 JSON 解析和构造工具。
 *
 * 负责把 bridge 响应、ready/perception/executor payload、PING/PONG 和 action request
 * 在松散 JSON 与内部领域模型之间转换。
 */
export class BrowserTransportProtocolService {
  private static readonly MAX_LITE_DOM_NODES: number = 2048;
  private static readonly MAX_LITE_DOM_RECTS_PER_NODE: number = 8;
  private static readonly MAX_LITE_DOM_OPTIONS_PER_NODE: number = 128;
  private static readonly MAX_LITE_DOM_TEXT_LENGTH: number = 2048;
  private static readonly MAX_LITE_DOM_SELECTOR_LENGTH: number = 4096;
  private static readonly MAX_LITE_DOM_FIELD_LENGTH: number = 512;
  private static readonly MAX_LITE_DOM_OPTION_LENGTH: number = 1024;

  static parseExtensionMessage(text: string): BridgeResponse {
    const response = new BridgeResponse();
    if (text.length === 0) {
      return response;
    }
    try {
      const parsed = JSON.parse(text) as Record<string, Object>;
      response.msgId = BrowserTransportProtocolService.readString(parsed, 'msgId');
      response.packetType = BrowserTransportProtocolService.readString(parsed, 'packetType');
      if (response.packetType.length === 0) {
        response.packetType = 'response';
      }
      response.browserId = BrowserTransportProtocolService.readNumber(parsed, 'browserId');
      response.type = BrowserTransportProtocolService.readString(parsed, 'type');
      response.replyToId = BrowserTransportProtocolService.readString(parsed, 'replyToId');
      response.taskId = BrowserTransportProtocolService.readString(parsed, 'taskId');
      response.status = BrowserTransportProtocolService.readString(parsed, 'status');
      response.timestamp = BrowserTransportProtocolService.readNumber(parsed, 'timestamp');
      if (response.timestamp === 0) {
        response.timestamp = BrowserTransportProtocolService.readNumber(parsed, 'ts');
      }
      response.errorType = BrowserTransportProtocolService.readString(parsed, 'errorType');
      response.errorMessage = BrowserTransportProtocolService.readString(parsed, 'errorMessage');

      const payloadRaw = parsed.payload;
      if (typeof payloadRaw === 'object' && payloadRaw !== null) {
        response.payload = payloadRaw as Record<string, Object>;
        response.payloadText = JSON.stringify(payloadRaw);
      } else if (typeof payloadRaw === 'string') {
        response.payloadText = payloadRaw;
        try {
          response.payload = JSON.parse(payloadRaw) as Record<string, Object>;
        } catch (e) {
          hilog.warn(DOMAIN, TAG, 'parseExtensionMessage parse error: %{public}s', e.message);
          response.payload = BrowserTransportProtocolService.createEmptyRecord();
        }
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseExtensionMessage error: %{public}s', e.message);
      return response;
    }
    return response;
  }

  static buildActionRequest(action: string, params: Record<string, Object>, reqId: string, taskId: string, agentReason: string = ''): string {
    const ret = JSON.stringify({
      action: action,
      params: params,
      reqId: reqId,
      taskId: taskId,
      cursorText: agentReason
    });
    hilog.debug(DOMAIN, TAG, 'built action request = %{private}s', ret);
    return ret;
  }

  static buildPongMessage(ts: number): string {
    return JSON.stringify({ type: BridgeResponseType.PONG, ts: ts });
  }

  static isPing(response: BridgeResponse): boolean {
    return response.type === BridgeResponseType.PING;
  }

  static isAsyncEvent(response: BridgeResponse): boolean {
    if (response.replyToId.length > 0) {
      return false;
    }
    return response.type === BridgeResponseType.DIALOG_OPENING ||
      response.type === BridgeResponseType.FILE_CHOOSER_OPENED ||
      response.type === BridgeResponseType.SCREENCAST_FRAME;
  }

  static isResponseMessage(response: BridgeResponse): boolean {
    return response.replyToId.length > 0;
  }

  static parseExecutorEvent(eventJson: string): BrowserExecutorEvent {
    const event = new BrowserExecutorEvent();
    if (eventJson.length === 0) {
      return event;
    }
    try {
      const parsed = JSON.parse(eventJson) as Record<string, Object>;
      event.eventType = BrowserTransportProtocolService.readString(parsed, 'eventType');
      event.browserId = BrowserTransportProtocolService.readNumber(parsed, 'browserId');
      event.messageType = BrowserTransportProtocolService.readString(parsed, 'messageType');
      event.replyToId = BrowserTransportProtocolService.readString(parsed, 'replyToId');
      event.status = BrowserTransportProtocolService.readString(parsed, 'status');
      event.payloadText = BrowserTransportProtocolService.readString(parsed, 'payloadText');
      event.errorMessage = BrowserTransportProtocolService.readString(parsed, 'errorMessage');
      event.timestamp = BrowserTransportProtocolService.readNumber(parsed, 'timestamp');
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseExecutorEvent error: %{public}s', e.message);
      return event;
    }
    return event;
  }

  static parseReadyPayload(payloadText: string): BrowserReadyPayload {
    const payload = new BrowserReadyPayload();
    if (payloadText.length === 0) {
      return payload;
    }
    try {
      const parsed = JSON.parse(payloadText) as Record<string, Object>;
      payload.status = BrowserTransportProtocolService.readString(parsed, 'status');
      payload.viewId = BrowserTransportProtocolService.readNumber(parsed, 'viewId');
      payload.automationSessionKey = BrowserTransportProtocolService.readString(parsed, 'automationSessionKey');
      payload.transportOwner = BrowserTransportProtocolService.readString(parsed, 'transportOwner');
      payload.protocolVersion = BrowserTransportProtocolService.readString(parsed, 'protocolVersion');
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseReadyPayload error: %{public}s',  e.message);
      return payload;
    }
    return payload;
  }

  static parsePerceptionPayload(responseOrPayloadText: BridgeResponse | string): BrowserPerceptionPayload {
    const perception = new BrowserPerceptionPayload();
    let payloadText = '';
    let payload: Record<string, Object> = BrowserTransportProtocolService.createEmptyRecord();
    if (typeof responseOrPayloadText === 'string') {
      payloadText = responseOrPayloadText;
      try {
        payload = JSON.parse(payloadText) as Record<string, Object>;
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'parsePerceptionPayload (string) failed: %{public}s', e.message);
        payload = BrowserTransportProtocolService.createEmptyRecord();
      }
    } else {
      payloadText = responseOrPayloadText.payloadText;
      payload = responseOrPayloadText.payload;
      if (Object.keys(payload).length <= 0 && payloadText.length > 0) {
        try {
          payload = JSON.parse(payloadText) as Record<string, Object>;
        } catch (e) {
          hilog.warn(DOMAIN, TAG, 'parsePerceptionPayload (BridgeResponse) failed: %{public}s', e.message);
          payload = BrowserTransportProtocolService.createEmptyRecord();
        }
      }
    }
    perception.rawPayloadText = payloadText;
    if (Object.keys(payload).length <= 0) {
      return perception;
    }

    const pageInfo = BrowserTransportProtocolService.readObject(payload, 'pageInfo');
    const source = Object.keys(pageInfo).length > 0 ? pageInfo : payload;
    perception.tabId = BrowserTransportProtocolService.readNumber(payload, 'tabId');
    perception.currentUrl = BrowserTransportProtocolService.readString(source, 'url');
    if (perception.currentUrl.length <= 0) {
      perception.currentUrl = BrowserTransportProtocolService.resolveCurrentUrl(
        BrowserTransportProtocolService.readArray(payload, 'tabList')
      );
    }
    perception.currentTitle = BrowserTransportProtocolService.readString(source, 'title');
    perception.originalScreenshot = BrowserTransportProtocolService.readString(source, 'originalScreenshot');
    perception.annotatedScreenshot = BrowserTransportProtocolService.readString(source, 'annotatedScreenshot');
    perception.viewport = BrowserTransportProtocolService.parseViewport(
      BrowserTransportProtocolService.readObject(source, 'viewport')
    );
    perception.hasOpenDialog = BrowserTransportProtocolService.readBoolean(source, 'hasOpenDialog');
    const dialogState = source.dialogState;
    if (typeof dialogState === 'object' && dialogState !== null) {
      perception.dialogState = dialogState as Record<string, Object>;
    }

    const liteDomRaw = source.liteDomTree;
    perception.liteDomTree = BrowserTransportProtocolService.parseLiteDomTree(liteDomRaw);
    const nearbyLiteDomRaw = source.nearbyLiteDomTree;
    perception.nearbyLiteDomTree = BrowserTransportProtocolService.parseLiteDomTree(nearbyLiteDomRaw);
    return perception;
  }

  static parsePageStatus(payloadText: string): string {
    if (payloadText.length === 0) {
      return '';
    }
    try {
      const parsed = JSON.parse(payloadText) as Record<string, Object>;
      return BrowserTransportProtocolService.readString(parsed, 'pageStatus');
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parsePageStatus failed: %{public}s', e.message);
      return '';
    }
  }

  private static parseLiteDomTree(rawValue: Object): BrowserLiteDomNode[] {
    const limits = new LiteDomParseLimitStats();
    const nodes = BrowserTransportProtocolService.parseLiteDomTreeWithLimits(rawValue, limits);
    BrowserTransportProtocolService.warnLiteDomLimitStats(limits);
    return nodes;
  }

  private static parseLiteDomTreeWithLimits(
    rawValue: Object,
    limits: LiteDomParseLimitStats
  ): BrowserLiteDomNode[] {
    if (Array.isArray(rawValue)) {
      const nodes: BrowserLiteDomNode[] = [];
      const nodeCount = Math.min(rawValue.length, BrowserTransportProtocolService.MAX_LITE_DOM_NODES);
      if (rawValue.length > nodeCount) {
        limits.truncatedNodes = rawValue.length - nodeCount;
      }
      for (let i = 0; i < nodeCount; i++) {
        nodes.push(BrowserTransportProtocolService.parseLiteDomNode(rawValue[i], limits));
      }
      return nodes;
    }
    if (typeof rawValue === 'object' && rawValue !== null) {
      const nodes: BrowserLiteDomNode[] = [];
      const record = rawValue as Record<string, Object>;
      const keys = Object.keys(record);
      const nodeCount = Math.min(keys.length, BrowserTransportProtocolService.MAX_LITE_DOM_NODES);
      if (keys.length > nodeCount) {
        limits.truncatedNodes = keys.length - nodeCount;
      }
      for (let i = 0; i < nodeCount; i++) {
        nodes.push(BrowserTransportProtocolService.parseLiteDomNode(record[keys[i]], limits));
      }
      return nodes;
    }
    return [];
  }

  private static parseLiteDomNode(rawValue: Object, limits: LiteDomParseLimitStats): BrowserLiteDomNode {
    const node = new BrowserLiteDomNode();
    const record = typeof rawValue === 'object' && rawValue !== null ?
      rawValue as Record<string, Object> :
    BrowserTransportProtocolService.createEmptyRecord();
    node.text = BrowserTransportProtocolService.readBoundedString(
      record, 'text', BrowserTransportProtocolService.MAX_LITE_DOM_TEXT_LENGTH, limits);
    node.include = BrowserTransportProtocolService.readBoolean(record, 'include');
    node.area = BrowserTransportProtocolService.readNumber(record, 'area');
    node.scrollViewportWidth = BrowserTransportProtocolService.readNumber(record, 'scrollViewportWidth');
    node.scrollViewportHeight = BrowserTransportProtocolService.readNumber(record, 'scrollViewportHeight');
    node.xpath = BrowserTransportProtocolService.readStrictBoundedStringWithAlias(
      record, 'xpath', '_xpath', BrowserTransportProtocolService.MAX_LITE_DOM_SELECTOR_LENGTH, limits);
    node.selector = BrowserTransportProtocolService.readStrictBoundedStringWithAlias(
      record, 'selector', '_selector', BrowserTransportProtocolService.MAX_LITE_DOM_SELECTOR_LENGTH, limits);
    node.type = BrowserTransportProtocolService.readBoundedStringWithAlias(
      record, 'type', '_type', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    node.element = BrowserTransportProtocolService.parseLiteDomElement(
      BrowserTransportProtocolService.readObject(record, 'element'), limits
    );
    node.rects = BrowserTransportProtocolService.parseLiteDomRects(
      BrowserTransportProtocolService.readArray(record, 'rects'), limits
    );
    return node;
  }

  private static parseLiteDomElement(
    value: Record<string, Object>,
    limits: LiteDomParseLimitStats
  ): BrowserLiteDomElement {
    const element = new BrowserLiteDomElement();
    element.tagName = BrowserTransportProtocolService.readBoundedString(
      value, 'tagName', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    element.type = BrowserTransportProtocolService.readBoundedString(
      value, 'type', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    element.ariaLabel = BrowserTransportProtocolService.readBoundedString(
      value, 'ariaLabel', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    element.dataId = BrowserTransportProtocolService.readStrictBoundedString(
      value, 'dataId', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    element.title = BrowserTransportProtocolService.readBoundedString(
      value, 'title', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    element.value = BrowserTransportProtocolService.readBoundedString(
      value, 'value', BrowserTransportProtocolService.MAX_LITE_DOM_TEXT_LENGTH, limits);
    element.placeholder = BrowserTransportProtocolService.readNonBlankStrictBoundedString(
      value, 'placeholder', BrowserTransportProtocolService.MAX_LITE_DOM_FIELD_LENGTH, limits);
    element.options = BrowserTransportProtocolService.readBoundedStringArray(
      value,
      'options',
      BrowserTransportProtocolService.MAX_LITE_DOM_OPTIONS_PER_NODE,
      BrowserTransportProtocolService.MAX_LITE_DOM_OPTION_LENGTH,
      limits
    );
    return element;
  }

  private static parseLiteDomRects(items: Object[], limits: LiteDomParseLimitStats): BrowserLiteDomRect[] {
    const rects: BrowserLiteDomRect[] = [];
    const rectCount = Math.min(items.length, BrowserTransportProtocolService.MAX_LITE_DOM_RECTS_PER_NODE);
    if (items.length > rectCount) {
      limits.truncatedRects += items.length - rectCount;
    }
    for (let i = 0; i < rectCount; i++) {
      const record = typeof items[i] === 'object' && items[i] !== null ?
        items[i] as Record<string, Object> :
        BrowserTransportProtocolService.createEmptyRecord();
      const rect = new BrowserLiteDomRect();
      rect.left = BrowserTransportProtocolService.readNumber(record, 'left');
      rect.top = BrowserTransportProtocolService.readNumber(record, 'top');
      rect.right = BrowserTransportProtocolService.readNumber(record, 'right');
      rect.bottom = BrowserTransportProtocolService.readNumber(record, 'bottom');
      rect.width = BrowserTransportProtocolService.readNumber(record, 'width');
      rect.height = BrowserTransportProtocolService.readNumber(record, 'height');
      rects.push(rect);
    }
    return rects;
  }

  private static parseViewport(value: Record<string, Object>): BrowserViewport {
    const viewport = new BrowserViewport();
    viewport.width = BrowserTransportProtocolService.readNumber(value, 'width');
    viewport.height = BrowserTransportProtocolService.readNumber(value, 'height');
    return viewport;
  }

  private static resolveCurrentUrl(tabList: Object[]): string {
    for (let i = 0; i < tabList.length; i++) {
      const item = tabList[i] as Record<string, Object>;
      if (BrowserTransportProtocolService.readBoolean(item, 'isActive') ||
        BrowserTransportProtocolService.readBoolean(item, 'active')) {
        return BrowserTransportProtocolService.readString(item, 'url');
      }
    }
    return '';
  }

  private static readString(value: Record<string, Object>, key: string): string {
    const nextValue = value[key];
    return typeof nextValue === 'string' ? nextValue : '';
  }

  private static readBoundedString(
    value: Record<string, Object>,
    key: string,
    maxLength: number,
    limits: LiteDomParseLimitStats
  ): string {
    return BrowserTransportProtocolService.truncateString(
      BrowserTransportProtocolService.readString(value, key), maxLength, limits);
  }

  private static readStringWithAlias(value: Record<string, Object>, key: string, aliasKey: string): string {
    const primary = BrowserTransportProtocolService.readString(value, key);
    if (primary.length > 0) {
      return primary;
    }
    return BrowserTransportProtocolService.readString(value, aliasKey);
  }

  private static readBoundedStringWithAlias(
    value: Record<string, Object>,
    key: string,
    aliasKey: string,
    maxLength: number,
    limits: LiteDomParseLimitStats
  ): string {
    return BrowserTransportProtocolService.truncateString(
      BrowserTransportProtocolService.readStringWithAlias(value, key, aliasKey), maxLength, limits);
  }

  private static readStrictBoundedString(value: Record<string, Object>, key: string, maxLength: number,
    limits: LiteDomParseLimitStats): string {
    return BrowserTransportProtocolService.rejectOverlongString(
      BrowserTransportProtocolService.readString(value, key), maxLength, limits);
  }

  private static readNonBlankStrictBoundedString(value: Record<string, Object>, key: string, maxLength: number,
    limits: LiteDomParseLimitStats): string {
    const parsed = BrowserTransportProtocolService.readStrictBoundedString(value, key, maxLength, limits);
    return parsed.trim().length > 0 ? parsed : '';
  }

  private static readStrictBoundedStringWithAlias(
    value: Record<string, Object>,
    key: string,
    aliasKey: string,
    maxLength: number,
    limits: LiteDomParseLimitStats
  ): string {
    return BrowserTransportProtocolService.rejectOverlongString(
      BrowserTransportProtocolService.readStringWithAlias(value, key, aliasKey), maxLength, limits);
  }

  private static readNumber(value: Record<string, Object>, key: string): number {
    const nextValue = value[key];
    return typeof nextValue === 'number' ? nextValue : 0;
  }

  private static readBoolean(value: Record<string, Object>, key: string): boolean {
    const nextValue = value[key];
    return typeof nextValue === 'boolean' ? nextValue : false;
  }

  private static readObject(value: Record<string, Object>, key: string): Record<string, Object> {
    const nextValue = value[key];
    return typeof nextValue === 'object' && nextValue !== null ?
      nextValue as Record<string, Object> :
      BrowserTransportProtocolService.createEmptyRecord();
  }

  private static readArray(value: Record<string, Object>, key: string): Object[] {
    const nextValue = value[key];
    return Array.isArray(nextValue) ? nextValue : [];
  }

  private static readBoundedStringArray(
    value: Record<string, Object>,
    key: string,
    maxItems: number,
    maxItemLength: number,
    limits: LiteDomParseLimitStats
  ): string[] {
    const items = BrowserTransportProtocolService.readArray(value, key);
    const result: string[] = [];
    const itemCount = Math.min(items.length, maxItems);
    if (items.length > itemCount) {
      limits.truncatedOptions += items.length - itemCount;
    }
    for (let i = 0; i < itemCount; i++) {
      if (typeof items[i] === 'string') {
        const item = items[i] as string;
        if (item.length <= maxItemLength) {
          result.push(item);
        } else {
          limits.rejectedOptions++;
        }
      }
    }
    return result;
  }

  private static truncateString(value: string, maxLength: number, limits: LiteDomParseLimitStats): string {
    if (value.length <= maxLength) {
      return value;
    }
    limits.truncatedStrings++;
    return value.substring(0, maxLength);
  }

  private static rejectOverlongString(value: string, maxLength: number, limits: LiteDomParseLimitStats): string {
    if (value.length <= maxLength) {
      return value;
    }
    limits.rejectedFields++;
    return '';
  }

  private static warnLiteDomLimitStats(limits: LiteDomParseLimitStats): void {
    if (limits.truncatedNodes <= 0 && limits.truncatedRects <= 0 &&
      limits.truncatedOptions <= 0 && limits.truncatedStrings <= 0 &&
      limits.rejectedFields <= 0 && limits.rejectedOptions <= 0) {
      return;
    }
    hilog.warn(DOMAIN, TAG,
      'parseLiteDomTree applied limits: nodes=%{public}d rects=%{public}d options=%{public}d ' +
      'strings=%{public}d fields=%{public}d optionItems=%{public}d',
      limits.truncatedNodes, limits.truncatedRects, limits.truncatedOptions,
      limits.truncatedStrings, limits.rejectedFields, limits.rejectedOptions);
  }

  private static createEmptyRecord(): Record<string, Object> {
    return JSON.parse('{}') as Record<string, Object>;
  }
}

class LiteDomParseLimitStats {
  public truncatedNodes: number = 0;
  public truncatedRects: number = 0;
  public truncatedOptions: number = 0;
  public truncatedStrings: number = 0;
  public rejectedFields: number = 0;
  public rejectedOptions: number = 0;
}
