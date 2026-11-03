// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep small ordered hash tables and small dictionary property shapes alive.
// This complements collections-hash-sample.js by stressing small Map/Set
// capacities, delete/reinsert tombstones, symbol keys, null-prototype objects,
// and low-cardinality dictionary-mode objects.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const smallMaps = keep('smallMaps', []);
  const smallSets = keep('smallSets', []);
  for (let size = 0; size <= 12; ++size) {
    const map = new Map();
    const set = new Set();
    for (let i = 0; i < size; ++i) {
      const key = {kind: 'small-map-key', size, i};
      const value = {kind: 'small-map-value', size, i};
      map.set(key, value);
      set.add({kind: 'small-set-value', size, i});
    }
    smallMaps.push(map);
    smallSets.push(set);
  }

  const churnMaps = keep('churnMaps', []);
  const churnSets = keep('churnSets', []);
  for (let round = 0; round < 16; ++round) {
    const keys = [];
    const map = new Map();
    const set = new Set();
    for (let i = 0; i < 10; ++i) {
      const key = {round, i};
      keys.push(key);
      map.set(key, {value: round * 100 + i});
      set.add(key);
    }
    for (let i = 0; i < keys.length; i += 2) {
      map.delete(keys[i]);
      set.delete(keys[i]);
    }
    for (let i = 0; i < 5; ++i) {
      const key = {round, rebuilt: i};
      map.set(key, {rebuilt: true, i});
      set.add(key);
    }
    churnMaps.push({map, keys, entries: Array.from(map.entries())});
    churnSets.push({set, keys, values: Array.from(set.values())});
  }

  const symbolKeys = keep('symbolKeys', []);
  const symbolMaps = keep('symbolMaps', []);
  const symbolSets = keep('symbolSets', []);
  for (let i = 0; i < 8; ++i) {
    const local = Symbol('small-ordered-local-' + i);
    const registered = Symbol.for('small-ordered-registered-' + i);
    symbolKeys.push(local, registered);
    symbolMaps.push(new Map([
      [local, {kind: 'local-symbol-value', i}],
      [registered, {kind: 'registered-symbol-value', i}],
    ]));
    symbolSets.push(new Set([local, registered]));
  }

  function makeSmallDictionaryObject(index) {
    const object = {
      keepA: {index, slot: 'a'},
      keepB: {index, slot: 'b'},
      dropA: {index, slot: 'drop-a'},
      dropB: {index, slot: 'drop-b'},
    };
    object['computed-' + index] = {index, slot: 'computed'};
    object[index % 2 === 0 ? 'even' : 'odd'] = {index, slot: 'parity'};
    delete object.dropA;
    delete object.dropB;
    Object.defineProperty(object, 'accessor', {
      configurable: true,
      enumerable: true,
      get() {
        return object.keepA;
      },
    });
    object.readded = {index, slot: 'readded'};
    return object;
  }

  const dictionaryObjects = keep('dictionaryObjects', []);
  for (let i = 0; i < 24; ++i) {
    const object = makeSmallDictionaryObject(i);
    dictionaryObjects.push({
      object,
      keys: Object.keys(object),
      names: Object.getOwnPropertyNames(object),
      descriptors: Object.getOwnPropertyDescriptors(object),
    });
  }

  const nullPrototypeObjects = keep('nullPrototypeObjects', []);
  for (let i = 0; i < 16; ++i) {
    const object = Object.create(null);
    object.alpha = {i, name: 'alpha'};
    object.beta = {i, name: 'beta'};
    object.gamma = {i, name: 'gamma'};
    delete object.beta;
    object.delta = {i, name: 'delta'};
    Object.defineProperty(object, 'hidden', {
      configurable: true,
      enumerable: false,
      value: {i, name: 'hidden'},
    });
    nullPrototypeObjects.push({
      object,
      keys: Reflect.ownKeys(object),
      values: Object.values(object),
    });
  }

  const symbolDictionaryObjects = keep('symbolDictionaryObjects', []);
  for (let i = 0; i < 12; ++i) {
    const symbolA = Symbol('small-dictionary-a-' + i);
    const symbolB = Symbol('small-dictionary-b-' + i);
    const object = {plain: {i, name: 'plain'}};
    object[symbolA] = {i, name: 'symbol-a'};
    object[symbolB] = {i, name: 'symbol-b'};
    delete object.plain;
    object.rebuilt = {i, name: 'rebuilt'};
    symbolDictionaryObjects.push({
      object,
      symbols: [symbolA, symbolB],
      keys: Reflect.ownKeys(object),
    });
  }

  const frozenDictionaryObjects = keep('frozenDictionaryObjects', []);
  for (let i = 0; i < 8; ++i) {
    const object = makeSmallDictionaryObject(100 + i);
    if (i % 2 === 0) {
      Object.preventExtensions(object);
    } else {
      Object.seal(object);
    }
    frozenDictionaryObjects.push({
      object,
      extensible: Object.isExtensible(object),
      sealed: Object.isSealed(object),
      keys: Reflect.ownKeys(object),
    });
  }

  const iterators = keep('iterators', {
    emptyMapEntries: smallMaps[0].entries(),
    oneMapKeys: smallMaps[1].keys(),
    twoMapValues: smallMaps[2].values(),
    churnMapEntries: churnMaps[3].map.entries(),
    emptySetValues: smallSets[0].values(),
    oneSetKeys: smallSets[1].keys(),
    churnSetEntries: churnSets[3].set.entries(),
  });

  const snapshots = keep('snapshots', {
    smallMapSizes: smallMaps.map((map) => map.size),
    smallSetSizes: smallSets.map((set) => set.size),
    churnMapSizes: churnMaps.map((entry) => entry.map.size),
    churnSetSizes: churnSets.map((entry) => entry.set.size),
    dictionaryKeyCounts: dictionaryObjects.map((entry) => entry.keys.length),
  });

  assertEquals(13, smallMaps.length);
  assertEquals(0, smallMaps[0].size);
  assertEquals(12, smallMaps[12].size);
  assertEquals(10, churnMaps[0].map.size);
  assertEquals(10, churnSets[0].set.size);
  assertEquals('keepA', dictionaryObjects[0].keys[0]);
  assertTrue(dictionaryObjects[0].descriptors.accessor.enumerable);
  assertTrue(nullPrototypeObjects[0].keys.includes('hidden'));
  assertTrue(symbolDictionaryObjects[0].keys.some(
      (key) => typeof key === 'symbol'));
  assertFalse(frozenDictionaryObjects[0].extensible);
  assertEquals(13, snapshots.smallMapSizes.length);
  assertEquals(7, Object.keys(iterators).length);

  globalThis.heapDumpSmallOrderedHashDictionariesDeepSample = catalog;
})();
