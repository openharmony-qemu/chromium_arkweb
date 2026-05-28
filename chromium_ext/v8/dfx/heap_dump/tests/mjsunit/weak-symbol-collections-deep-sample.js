// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep non-registered Symbol weak-key shapes alive for heap dump smoke. This
// follows V8's symbol-as-weakmap-key coverage without depending on GC timing.

(function() {
  'use strict';

  const catalog = {};

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  const localSymbols = keep('localSymbols', []);
  const symbolValues = keep('symbolValues', []);
  for (let i = 0; i < 24; ++i) {
    localSymbols.push(Symbol('heap-dump-weak-symbol-' + i));
    symbolValues.push({
      index: i,
      payload: {kind: 'weak-symbol-value-' + i},
    });
  }

  const weakMap = keep('weakMap', new WeakMap());
  const weakSet = keep('weakSet', new WeakSet());
  const nestedWeakMap = keep('nestedWeakMap', new WeakMap());
  const nestedWeakSet = keep('nestedWeakSet', new WeakSet());

  for (let i = 0; i < localSymbols.length; ++i) {
    weakMap.set(localSymbols[i], symbolValues[i]);
    weakSet.add(localSymbols[i]);
    if ((i & 1) === 0) {
      nestedWeakMap.set(localSymbols[i], {
        value: symbolValues[i],
        next: localSymbols[(i + 1) % localSymbols.length],
      });
      nestedWeakSet.add(localSymbols[i]);
    }
  }

  const deletedSymbol = keep('deletedSymbol', Symbol('heap-dump-deleted-symbol'));
  weakMap.set(deletedSymbol, {deleted: true});
  weakSet.add(deletedSymbol);
  const deleteResults = keep('deleteResults', {
    mapBeforeDelete: weakMap.has(deletedSymbol),
    mapDelete: weakMap.delete(deletedSymbol),
    mapAfterDelete: weakMap.has(deletedSymbol),
    setBeforeDelete: weakSet.has(deletedSymbol),
    setDelete: weakSet.delete(deletedSymbol),
    setAfterDelete: weakSet.has(deletedSymbol),
  });

  const registeredSymbol = keep(
      'registeredSymbol',
      Symbol.for('heap-dump-registered-weak-symbol'));
  keepError('registeredWeakMapSetError',
            () => weakMap.set(registeredSymbol, {registered: true}));
  keepError('registeredWeakSetAddError',
            () => weakSet.add(registeredSymbol));

  const weakRefs = keep('weakRefs', []);
  if (typeof WeakRef === 'function') {
    for (let i = 0; i < localSymbols.length; i += 3) {
      try {
        weakRefs.push(new WeakRef(localSymbols[i]));
      } catch (e) {
        keep('weakRefSymbolError' + i, e);
      }
    }
    keepError('registeredWeakRefError',
              () => new WeakRef(registeredSymbol));
  }

  const finalizationRegistries = keep('finalizationRegistries', []);
  const unregisterTokens = keep('unregisterTokens', []);
  if (typeof FinalizationRegistry === 'function') {
    const registry = new FinalizationRegistry((held) => held);
    for (let i = 0; i < localSymbols.length; i += 4) {
      const token = {token: i};
      unregisterTokens.push(token);
      registry.register(localSymbols[i], {heldSymbolIndex: i}, token);
    }
    const unregisterResult = registry.unregister(unregisterTokens[0]);
    keep('symbolUnregisterResult', unregisterResult);
    finalizationRegistries.push(registry);
    keepError('registeredFinalizationRegistryError',
              () => registry.register(registeredSymbol, {registered: true}));
  }

  const summary = keep('summary', {
    localSymbolCount: localSymbols.length,
    weakRefCount: weakRefs.length,
    finalizationRegistryCount: finalizationRegistries.length,
    unregisterTokenCount: unregisterTokens.length,
  });

  assertEquals(24, summary.localSymbolCount);
  assertSame(symbolValues[0], weakMap.get(localSymbols[0]));
  assertTrue(weakSet.has(localSymbols[1]));
  assertSame(localSymbols[1], nestedWeakMap.get(localSymbols[0]).next);
  assertTrue(nestedWeakSet.has(localSymbols[2]));
  assertTrue(deleteResults.mapBeforeDelete);
  assertTrue(deleteResults.mapDelete);
  assertFalse(deleteResults.mapAfterDelete);
  assertTrue(deleteResults.setBeforeDelete);
  assertTrue(deleteResults.setDelete);
  assertFalse(deleteResults.setAfterDelete);
  if (weakRefs.length > 0 && typeof weakRefs[0].deref === 'function') {
    assertSame(localSymbols[0], weakRefs[0].deref());
  }
  if (finalizationRegistries.length > 0) {
    assertTrue(finalizationRegistries[0] instanceof FinalizationRegistry);
  }

  globalThis.heapDumpWeakSymbolCollectionsDeepSample = catalog;
})();
