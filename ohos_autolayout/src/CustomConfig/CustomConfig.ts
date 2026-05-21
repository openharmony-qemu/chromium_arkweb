/*
  Copyright (c) 2026 Huawei Device Co., Ltd.
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

import Log from '../Debug/Log';
import Tag from '../Debug/Tag';

export interface InlineStyleItemValue {
    [key: string]: string
}

export interface InlineStyleItem {
    selector: string,
    value: InlineStyleItemValue
}

export interface CustomConfigItem {
    style?: string,
    inlineStyle?: InlineStyleItem[]
}

export default class CustomConfig {

    // style config filtered from input JSON
    styleConfigs: string[] = [];

    // inline style config filtered from input JSON
    inlineStyleConfigs: InlineStyleItem[] = [];

    // style tag to append in head tag
    styleElement: HTMLElement;

    // MutationObserver instance saved
    observer: MutationObserver;

    // MutationObserver options
    observeOptions = {
        attributes: true,
        childList: true,
        subtree: true
    }

    /**
     * constructor
     * @description
     *   Create style element;
     *   Execute main process;
     * @param {string} config
     */
    constructor(config: string) {
        Log.d('Autolayout CustomConfig constructor entered.', Tag.customConfig);
        this.styleElement = document.createElement('style');
        this.initObserver();
        this.update(config);
    }

    /**
     * update
     * @param {string} config
     * @description execute update processing
     */
    update(config: string): void {
        Log.d('Autolayout CustomConfig start update.', Tag.customConfig);
        this.setConfig(config);
        this.updateStyleTag();
        this.updateElementsInlineStyle();
    }

    /**
     * initObserver
     * @description init MutationObserver instance with observeCallback
     */
    initObserver(): void {
        this.observer = new MutationObserver(() => {
            this.observeCallback();
        });
    }

    /**
     * startObserve
     */
    startObserve() {
        if (document.body) {
            this.observer.observe(document.body, this.observeOptions);
        } else {
            document.addEventListener("DOMContentLoaded", (event) => {
                this.observer.observe(document.body, this.observeOptions);
            });
        }
    }

    /**
     * stopObserve
     */
    stopObserve() {
        this.observer.disconnect()
    }

    /**
     * updateObserve
     * @description call startObserve if inlineStyleConfigs is valid
     */
    updateObserve() {
        if (this.inlineStyleConfigs.length > 0) {
            this.startObserve();
        } else {
            this.stopObserve();
        }
    }

    /**
     * observeCallback
     */
    observeCallback(): void {
        Log.d('Autolayout CustomConfig observeCallback was called.', Tag.customConfig);
        this.updateElementsInlineStyle();
    }

    /**
     * isJsonObject
     * @param {any} source
     * @returns boolean
     */
    static isJsonObject(source: any): boolean {
        return (
            (source !== null) &&
            (typeof source === 'object') &&
            !Array.isArray(source) &&
            (Object.prototype.toString.call(source) === '[object Object]')
        );
    }

    /**
     * updateElementsInlineStyle
     */
    updateElementsInlineStyle(): void {
        Log.d('Autolayout CustomConfig updateElementsInlineStyle.', Tag.customConfig);
        this.stopObserve();
        this.inlineStyleConfigs.forEach((inlineStyleItem: InlineStyleItem) => {
            try {
                const selectorList = document.querySelectorAll(inlineStyleItem.selector);
                if (selectorList.length > 0) {
                    selectorList.forEach((selectorItem: HTMLElement) => {
                        const styleKeys: string[] = Object.keys(inlineStyleItem.value);
                        styleKeys.forEach((styleKey: string) => {
                            let styleValue = inlineStyleItem.value[styleKey];
                            let stylePriority = null;
                            const propertyIndex = styleValue.lastIndexOf('!important');
                            if (propertyIndex > -1) {
                                styleValue = styleValue.substring(0, propertyIndex);
                                stylePriority = 'important';
                            }
                            (selectorItem as any).style.setProperty(styleKey, styleValue, stylePriority);
                        });
                        Log.d(`Autolayout CustomConfig ${inlineStyleItem.selector} inline style updated.`, Tag.customConfig);
                    });
                }
            } catch (error) {
                Log.d('Autolayout CustomConfig element serach and set style error.', Tag.customConfig);
            }
        });
        this.updateObserve();
    }

    /**
     * setConfig
     * @param {string} config
     * @description parse string JSON input and filter the valid config options
     */
    setConfig(config: string): void {
        try {
            const configParsed = JSON.parse(config);
            Log.d('Autolayout CustomConfig config parsed.', Tag.customConfig);
            if (configParsed && configParsed.configs && Array.isArray(configParsed.configs)) {
                this.styleConfigs = [];
                this.inlineStyleConfigs = [];
                configParsed.configs.forEach((configItem: CustomConfigItem) => {
                    if (configItem.style && (typeof configItem.style === 'string')) {
                        this.styleConfigs.push(configItem.style);
                    }
                    if (configItem.inlineStyle && Array.isArray(configItem.inlineStyle)) {
                        configItem.inlineStyle.forEach((inlineStyleItem: InlineStyleItem) => {
                            if (inlineStyleItem.selector &&
                                (typeof inlineStyleItem.selector === 'string') &&
                                CustomConfig.isJsonObject(inlineStyleItem.value) &&
                                (Object.keys(inlineStyleItem.value).length > 0)) {
                                    this.inlineStyleConfigs.push(inlineStyleItem);
                            }
                        })
                    }
                });
            }
        } catch (error) {
            this.styleConfigs = [];
            this.inlineStyleConfigs = [];
            Log.d('Autolayout CustomConfig config to parse is not valid JSON.', Tag.customConfig);
        }
    }

    /**
     * updateStyleTag
     * @description update style tag content with style configs
     */
    updateStyleTag(): void {
        if (this.styleConfigs.length === 0) {
            Log.d('Autolayout CustomConfig no valid style config was provided.', Tag.customConfig);
            if (this.styleElement.isConnected) {
                document.head.removeChild(this.styleElement);
            }
            return;
        }

        const newContent = this.styleConfigs.join('\n');
        if (this.styleElement.textContent !== newContent) {
            this.styleElement.textContent = newContent;
        }

        Log.d('Autolayout CustomConfig style element was updated.', Tag.customConfig);
        
        if (!this.styleElement.isConnected) {
            Log.d('Autolayout CustomConfig config style was injected.', Tag.customConfig);
            document.head.appendChild(this.styleElement);
        }
    }
}