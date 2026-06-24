// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Heap-dump shape sample for SourceTextModule re-export/live-binding states,
// module namespace objects, module contexts, import.meta, and dynamic import.

export {
  bumpLiveCounter as reexportedBump,
  liveCounter as reexportedCounter,
  liveObject as reexportedLiveObject,
  readLiveObject as reexportedReadLiveObject,
} from './module-live-binding-helper.mjs';
export * as liveNamespace from './module-live-binding-helper.mjs';

import defaultValue, * as namespaceObject from './module-live-binding-helper.mjs';
import {
  LiveBindingClass,
  bumpLiveCounter,
  liveCounter,
  liveObject,
  readLiveObject,
  stableArray,
  stableObject,
} from './module-live-binding-helper.mjs';

const before = liveCounter;
const first = bumpLiveCounter(3);
const afterFirstObject = liveObject;
const second = namespaceObject.bumpLiveCounter(4);
const afterSecondObject = readLiveObject();
const dynamicNamespace = await import('./module-live-binding-helper.mjs');
const dynamicValue = dynamicNamespace.bumpLiveCounter(5);

const namespaceKeys = Reflect.ownKeys(namespaceObject);
const namespaceDescriptors = Object.fromEntries(
    namespaceKeys.map((key) => [
      key,
      Object.getOwnPropertyDescriptor(namespaceObject, key),
    ]));
const namespaceCopy = {...namespaceObject};
const instance = new LiveBindingClass(10);
const dynamicImportPromise = import('./module-live-binding-helper.mjs');

assertEquals(0, before);
assertEquals(3, first);
assertEquals('updated-live-object', afterFirstObject.kind);
assertEquals(7, second);
assertEquals(7, afterSecondObject.value);
assertEquals(12, dynamicValue);
assertEquals(12, namespaceObject.liveCounter);
assertEquals(22, instance.read());
assertSame(stableObject, namespaceObject.stableObject);
assertSame(stableArray, defaultValue.stableArray);
assertTrue(namespaceKeys.includes('liveCounter'));
assertTrue(namespaceDescriptors.liveCounter.enumerable);
assertTrue(dynamicImportPromise instanceof Promise);

globalThis.__heapDumpModuleReexportLiveBindingSample = {
  before,
  first,
  second,
  dynamicValue,
  namespaceObject,
  dynamicNamespace,
  namespaceKeys,
  namespaceDescriptors,
  namespaceCopy,
  defaultValue,
  stableObject,
  stableArray,
  afterFirstObject,
  afterSecondObject,
  instance,
  LiveBindingClass,
  dynamicImportPromise,
  importMetaUrl: import.meta.url,
};

