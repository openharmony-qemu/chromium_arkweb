// Heap-dump sample corpus:
// - SourceTextModule and module context objects
// - JSModuleNamespace objects through static and dynamic import
// - module export cells and namespace property access paths

import defaultValue, * as namespaceObject from './module-namespace-helper.mjs';
import {
  exportedArray,
  exportedObject,
  mutableCounter,
  mutate,
} from './module-namespace-helper.mjs';

const dynamicNamespace = await import('./module-namespace-helper.mjs');
const namespaceKeys = Reflect.ownKeys(namespaceObject);
const namespaceValues = Object.values(namespaceObject);
const namespaceCopy = { ...namespaceObject };
const before = mutableCounter;
const after = mutate(17);

globalThis.heapDumpModuleNamespaceSample = {
  namespaceObject,
  dynamicNamespace,
  namespaceKeys,
  namespaceValues,
  namespaceCopy,
  defaultValue,
  exportedObject,
  exportedArray,
  before,
  after,
  modulePromise: import('./module-namespace-helper.mjs'),
};

assertTrue(namespaceKeys.includes('exportedObject'));
assertTrue(namespaceKeys.includes('mutableCounter'));
assertEquals(1, before);
assertEquals(17, after);
assertEquals(17, namespaceObject.mutableCounter);
assertSame(exportedObject, namespaceObject.exportedObject);
assertSame(exportedArray, dynamicNamespace.exportedArray);
assertEquals('heap-dump-module-helper', defaultValue.name);
