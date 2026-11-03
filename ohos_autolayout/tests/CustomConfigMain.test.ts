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

import CustomConfigMain from '../src/CustomConfig/Main';
import CustomConfig from '../src/CustomConfig/CustomConfig';

describe('CustomConfig/Main', () => {
    describe('Basic Structure', () =>{
        test('CustomConfig main should have all required methods ', () => {
            expect(typeof CustomConfigMain.start).toBe('function');
        });
    });
    describe('start() method', () =>{
        test('start method should return CustomConfig instance', () => {
            const startResult = CustomConfigMain.start('{}');
            expect(startResult instanceof CustomConfig).toBe(true);
        });
        test('start method should return the same instance after first call', () => {
            const startResult = CustomConfigMain.start('{}');
            const startResultRepeat = CustomConfigMain.start('{}');
            expect(startResultRepeat === startResult).toBe(true);
        });
    });
});