// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Heap-dump sample corpus:
// - JSON modules loaded through import attributes
// - JSModuleNamespace/default export objects for JSON payloads
// - dynamic import attributes promises/results and rejection objects

import jsonDefault from './helpers/import-attributes-payload.json'
    with { type: 'json' };
import * as jsonNamespace from './helpers/import-attributes-payload.json'
    with { type: 'json', notARealAssertion: 'value' };

const dynamicJsonModule = await import(
    './helpers/import-attributes-payload.json', { with: { type: 'json' } });
const badTypeError = await import('./helpers/import-attributes-payload.json',
                                  { with: { type: 'javascript' } })
    .then(() => null, (e) => e);

const namespaceKeys = Reflect.ownKeys(jsonNamespace);
const namespaceDescriptor =
    Object.getOwnPropertyDescriptor(jsonNamespace, 'default');
const defaultKeys = Reflect.ownKeys(jsonDefault);
const nestedValues = jsonDefault.nested.values.slice();

globalThis.__heapDumpModuleImportAttributesJson = {
  jsonDefault,
  jsonNamespace,
  dynamicJsonModule,
  namespaceKeys,
  namespaceDescriptor,
  defaultKeys,
  nestedValues,
  badTypeError,
  dynamicPromise: import('./helpers/import-attributes-payload.json',
                         { with: { type: 'json' } }),
};

assertEquals(42, jsonDefault.life);
assertEquals('heap-dump-json-module', jsonDefault.label);
assertTrue(jsonDefault.nested.enabled);
assertArrayEquals([1, 2, 3], nestedValues);
assertSame(jsonDefault, jsonNamespace.default);
assertSame(jsonDefault, dynamicJsonModule.default);
assertTrue(namespaceKeys.includes('default'));
assertEquals('object', typeof namespaceDescriptor.value);
assertEquals('object', typeof badTypeError);
assertTrue(typeof badTypeError.message === 'string');
