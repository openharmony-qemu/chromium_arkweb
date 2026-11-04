// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep dense WeakMap/WeakSet ephemeron chains alive for heap dump smoke. This
// is adapted from V8's big-weakmap, symbol-as-weakmap-key, and weakrefs basics
// coverage, but avoids GC timing dependencies by retaining strong roots.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, callback) {
    try {
      callback();
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  const objectKeys = keep('objectKeys', []);
  const symbolKeys = keep('symbolKeys', []);
  const values = keep('values', []);
  const tokens = keep('tokens', []);

  for (let i = 0; i < 96; ++i) {
    const key = {index: i, label: 'ephemeron-key-' + i};
    const symbol = Symbol('ephemeron-symbol-' + i);
    const token = {token: i, key};
    const value = {
      index: i,
      key,
      symbol,
      token,
      next: null,
      payload: {kind: 'ephemeron-value-' + i},
    };
    objectKeys.push(key);
    symbolKeys.push(symbol);
    values.push(value);
    tokens.push(token);
  }

  for (let i = 0; i < values.length; ++i) {
    values[i].next = values[(i + 1) % values.length];
  }

  const weakMap = keep('weakMap', new WeakMap());
  const weakSet = keep('weakSet', new WeakSet());
  const symbolWeakMap = keep('symbolWeakMap', new WeakMap());
  const symbolWeakSet = keep('symbolWeakSet', new WeakSet());
  const nestedWeakMaps = keep('nestedWeakMaps', []);
  const nestedWeakSets = keep('nestedWeakSets', []);

  for (let bucket = 0; bucket < 6; ++bucket) {
    nestedWeakMaps.push(new WeakMap());
    nestedWeakSets.push(new WeakSet());
  }

  for (let i = 0; i < objectKeys.length; ++i) {
    weakMap.set(objectKeys[i], values[i]);
    weakSet.add(objectKeys[i]);
    symbolWeakMap.set(symbolKeys[i], {
      objectKey: objectKeys[i],
      value: values[i],
      mirrorSymbol: symbolKeys[(i + 1) % symbolKeys.length],
    });
    symbolWeakSet.add(symbolKeys[i]);

    const bucket = i % nestedWeakMaps.length;
    nestedWeakMaps[bucket].set(objectKeys[i], {
      bucket,
      value: values[i],
      previousKey: objectKeys[(i + objectKeys.length - 1) % objectKeys.length],
    });
    nestedWeakSets[bucket].add(objectKeys[i]);
  }

  const deletedKey = keep('deletedKey', {deleted: true});
  weakMap.set(deletedKey, {deletedValue: true});
  weakSet.add(deletedKey);
  const deletionSummary = keep('deletionSummary', {
    mapBefore: weakMap.has(deletedKey),
    mapDelete: weakMap.delete(deletedKey),
    mapAfter: weakMap.has(deletedKey),
    setBefore: weakSet.has(deletedKey),
    setDelete: weakSet.delete(deletedKey),
    setAfter: weakSet.has(deletedKey),
  });

  const registeredSymbol = keep(
      'registeredSymbol',
      Symbol.for('heap-dump-ephemeron-registered-symbol'));
  keepError('registeredSymbolWeakMapError',
            () => symbolWeakMap.set(registeredSymbol, {bad: true}));
  keepError('registeredSymbolWeakSetError',
            () => symbolWeakSet.add(registeredSymbol));

  const weakRefs = keep('weakRefs', []);
  if (typeof WeakRef === 'function') {
    for (let i = 0; i < objectKeys.length; i += 7) {
      weakRefs.push(new WeakRef(objectKeys[i]));
      weakRefs.push(new WeakRef(values[i]));
    }
    for (let i = 0; i < symbolKeys.length; i += 11) {
      try {
        weakRefs.push(new WeakRef(symbolKeys[i]));
      } catch (e) {
        keep('symbolWeakRefError' + i, e);
      }
    }
  }

  const finalizationRegistries = keep('finalizationRegistries', []);
  if (typeof FinalizationRegistry === 'function') {
    const cleanupLog = keep('cleanupLog', []);
    const registry = new FinalizationRegistry((held) => {
      cleanupLog.push(held);
    });
    const proxyRegistry = new FinalizationRegistry(new Proxy(function (held) {
      cleanupLog.push({proxyHeld: held});
    }, {
      apply(target, receiver, args) {
        cleanupLog.push({proxyApplyCount: args.length});
        return Reflect.apply(target, receiver, args);
      },
    }));

    for (let i = 0; i < objectKeys.length; i += 3) {
      registry.register(objectKeys[i], {
        heldIndex: i,
        value: values[i],
      }, tokens[i]);
    }
    for (let i = 1; i < objectKeys.length; i += 8) {
      proxyRegistry.register(objectKeys[i], {
        proxyHeldIndex: i,
        mirror: values[i],
      }, tokens[i]);
    }
    keep('unregisterExistingResult', registry.unregister(tokens[0]));
    keep('unregisterMissingResult', registry.unregister({missing: true}));
    finalizationRegistries.push(registry, proxyRegistry);
  }

  const summary = keep('summary', {
    objectKeyCount: objectKeys.length,
    symbolKeyCount: symbolKeys.length,
    nestedWeakMapCount: nestedWeakMaps.length,
    weakRefCount: weakRefs.length,
    finalizationRegistryCount: finalizationRegistries.length,
  });

  assertEquals(96, summary.objectKeyCount);
  assertEquals(96, summary.symbolKeyCount);
  assertSame(values[0], weakMap.get(objectKeys[0]));
  assertSame(values[1], values[0].next);
  assertTrue(weakSet.has(objectKeys[5]));
  assertSame(objectKeys[4], symbolWeakMap.get(symbolKeys[4]).objectKey);
  assertTrue(symbolWeakSet.has(symbolKeys[6]));
  assertSame(values[12], nestedWeakMaps[0].get(objectKeys[12]).value);
  assertTrue(nestedWeakSets[3].has(objectKeys[15]));
  assertTrue(deletionSummary.mapBefore);
  assertTrue(deletionSummary.mapDelete);
  assertFalse(deletionSummary.mapAfter);
  assertTrue(deletionSummary.setBefore);
  assertTrue(deletionSummary.setDelete);
  assertFalse(deletionSummary.setAfter);
  if (weakRefs.length > 0 && typeof weakRefs[0].deref === 'function') {
    assertSame(objectKeys[0], weakRefs[0].deref());
  }
  if (finalizationRegistries.length > 0) {
    assertTrue(finalizationRegistries[0] instanceof FinalizationRegistry);
  }

  globalThis.heapDumpWeakCollectionsEphemeronChainsDeepSample = catalog;
})();
