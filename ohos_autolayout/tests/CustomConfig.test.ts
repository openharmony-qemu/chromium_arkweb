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

const mockLog = {
    e: jest.fn(),
    info: jest.fn(),
    w: jest.fn(),
    d: jest.fn(),
};

jest.mock('../src/Debug/Log', () => ({
    __esModule: true,
    default: mockLog,
}));

import CustomConfig from '../src/CustomConfig/CustomConfig';
import { CustomConfigItem } from '../src/CustomConfig/CustomConfig';
import Tag from '../src/Debug/Tag';

interface InvalidCustomConfigStyleItem {
    backgroundColor?: string,
    color?: string
}
interface InvalidCustomConfigItem {
    style?: InvalidCustomConfigStyleItem[]
}

const invalidJSONStr: string = 'invalid json';
const validCustomConfigBg: CustomConfigItem[] = [
    {style: 'div{background-color: red;}'},
    {style: 'div{background-color: blue;}'}
];
const validCustomConfigBgRed: CustomConfigItem[] = [
    {style: 'div{background-color: red;}'}
];
const invalidCustomConfigBgRed: InvalidCustomConfigItem[] = [
    {
        style: [
            {backgroundColor: 'red'},
            {color: 'red'}
        ]
    }
];
const validCustomConfigInlineStyle: CustomConfigItem[] = [
    {inlineStyle: [
        {
            selector: 'div',
            value: {
                color: 'red'
            }
        }
    ]}
];

const invalidCustomConfigInlineStyle1 = [
    {inlineStyle: [
        {
            selector: 1
        }
    ]}
];

const invalidCustomConfigInlineStyle2 = [
    {inlineStyle: [
        {
            selector: 'div',
            value: 'value'
        }
    ]}
];

const invalidCustomConfigInlineStyle3 = [
    {inlineStyle: [
        {
            selector: 'div',
            val: {}
        }
    ]}
];

describe('CustomConfig/CustomConfig', () => {
    describe('Basic Structure', () => {
        test('CustomConfig instance', () => {
            const customConfigInstance = new CustomConfig(JSON.stringify({configs: validCustomConfigBgRed}));
            expect(typeof customConfigInstance.update).toBe('function');
            expect(typeof customConfigInstance.setConfig).toBe('function');
            expect(typeof customConfigInstance.updateStyleTag).toBe('function');
        });
    });
    describe('setConfig() method', () => {
        test('setConfig method should parse config string and push into styleConfigs.', () => {
            const customConfigInstance1 = new CustomConfig(JSON.stringify({configs: validCustomConfigBgRed}));
            expect(customConfigInstance1.styleConfigs.length).toBe(1);
            const customConfigInstance2 = new CustomConfig(JSON.stringify({configs: validCustomConfigBg}));
            expect(customConfigInstance2.styleConfigs.length).toBe(2);
            const customConfigInstance3 = new CustomConfig(JSON.stringify({configs: invalidJSONStr}));
            expect(customConfigInstance3.styleConfigs.length).toBe(0);
            const customConfigInstance4 = new CustomConfig(JSON.stringify({configs: invalidCustomConfigBgRed}));
            expect(customConfigInstance4.styleConfigs.length).toBe(0);
        });
        test('setConfig method should parse config string and push into inlineStyleConfigs.', () => {
            const customConfigInstance1 = new CustomConfig(JSON.stringify({configs: validCustomConfigInlineStyle}));
            expect(customConfigInstance1.inlineStyleConfigs.length).toBe(1);
            customConfigInstance1.stopObserve();
            const customConfigInstance2 = new CustomConfig(JSON.stringify({configs: invalidCustomConfigInlineStyle1}));
            expect(customConfigInstance2.inlineStyleConfigs.length).toBe(0);
            customConfigInstance2.stopObserve();
            const customConfigInstance3 = new CustomConfig(JSON.stringify({configs: invalidCustomConfigInlineStyle2}));
            expect(customConfigInstance3.inlineStyleConfigs.length).toBe(0);
            customConfigInstance3.stopObserve();
            const customConfigInstance4 = new CustomConfig(JSON.stringify({configs: invalidCustomConfigInlineStyle3}));
            expect(customConfigInstance4.inlineStyleConfigs.length).toBe(0);
            customConfigInstance4.stopObserve();
            document.body.innerHTML = '';
        });
    });
    describe('updateStyleTag() method', () => {
        test('updateStyleTag method should remove style tag from head.', () => {
            const customConfigInstance = new CustomConfig(JSON.stringify({configs: validCustomConfigBg}));
            expect(customConfigInstance.styleElement.isConnected).toBe(true);

            customConfigInstance.update(invalidJSONStr);
            expect(customConfigInstance.styleElement.isConnected).toBe(false);

            customConfigInstance.update(JSON.stringify({configs: validCustomConfigBgRed}));
            expect(customConfigInstance.styleElement.isConnected).toBe(true);
            expect(mockLog.d).toHaveBeenCalledWith('Autolayout CustomConfig config style was injected.', Tag.customConfig);

            customConfigInstance.update(JSON.stringify({configs: validCustomConfigBg}));
            expect(customConfigInstance.styleElement.isConnected).toBe(true);
            expect(mockLog.d).toHaveBeenCalledWith('Autolayout CustomConfig config parsed.', Tag.customConfig);
        });
    });
    describe('isJsonObject() method', () => {
        test('isJsonObject method should judge if the input is plain object.', () => {
            expect(CustomConfig.isJsonObject({})).toBe(true);
            expect(CustomConfig.isJsonObject(1)).toBe(false);
            expect(CustomConfig.isJsonObject(null)).toBe(false);
            expect(CustomConfig.isJsonObject('str')).toBe(false);
            expect(CustomConfig.isJsonObject([])).toBe(false);
        });
    });
    describe('updateElementsInlineStyle() method', () => {
        test('updateElementsInlineStyle method should update the selector with the value and catch exception.', () => {
            const customConfigInstance = new CustomConfig(JSON.stringify({configs: []}));
            customConfigInstance.inlineStyleConfigs = [
                {
                    selector: '/\/',
                    value: {
                        color: 'red'
                    }
                }
            ];
            customConfigInstance.updateElementsInlineStyle();
            expect(mockLog.d).toHaveBeenCalledWith('Autolayout CustomConfig element serach and set style error.', Tag.customConfig);

            customConfigInstance.stopObserve();
            document.body.innerHTML = '';

            const divElement = document.createElement('div');
            document.body.appendChild(divElement);
            customConfigInstance.update(JSON.stringify({configs: validCustomConfigInlineStyle}));
            expect(mockLog.d).toHaveBeenCalledWith('Autolayout CustomConfig div inline style updated.', Tag.customConfig);
            customConfigInstance.stopObserve();
            document.body.innerHTML = '';
        });
    });
    describe('observeCallback() method', () => {
        test('observeCallback method should call updateElementsInlineStyle.', () => {
            const customConfigInstance = new CustomConfig(JSON.stringify({configs: validCustomConfigInlineStyle}));
            customConfigInstance.observeCallback();
            expect(mockLog.d).toHaveBeenCalledWith('Autolayout CustomConfig observeCallback was called.', Tag.customConfig);
            expect(mockLog.d).toHaveBeenCalledWith('Autolayout CustomConfig updateElementsInlineStyle.', Tag.customConfig);
            customConfigInstance.stopObserve();
        });
    });
});