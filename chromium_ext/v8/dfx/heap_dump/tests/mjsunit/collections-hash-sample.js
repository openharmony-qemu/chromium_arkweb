// Heap-dump smoke sample for collection and hash-table backing objects.
//
// Exact weak/ordered-hash edge semantics are covered by cctests. This sample
// keeps large/small Map/Set, weak collections, weak refs, and iterators alive as
// end-to-end dump/translate input.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const objectKeys = [];
  const objectValues = [];
  for (let i = 0; i < 96; ++i) {
    objectKeys.push({key: i, group: i % 7});
    objectValues.push({value: i, payload: 'value-' + i});
  }

  const smallMap = new Map([
    ['one', {n: 1}],
    ['two', {n: 2}],
  ]);
  const smallSet = new Set([{small: 1}, {small: 2}]);

  const largeMap = new Map();
  const largeSet = new Set();
  for (let i = 0; i < objectKeys.length; ++i) {
    largeMap.set(objectKeys[i], objectValues[i]);
    largeSet.add(objectKeys[i]);
  }
  for (let i = 0; i < objectKeys.length; i += 5) {
    largeMap.delete(objectKeys[i]);
    largeSet.delete(objectKeys[i]);
  }
  for (let i = 0; i < 32; ++i) {
    largeMap.set('string-key-' + i, {stringValue: i});
    largeSet.add('string-value-' + i);
  }

  const numericMap = new Map();
  const numericSet = new Set();
  for (let i = -32; i <= 32; ++i) {
    numericMap.set(i, {square: i * i});
    numericSet.add(i);
  }
  numericMap.set(NaN, {nan: true});
  numericMap.set(-0, {minusZero: true});
  numericSet.add(NaN);
  numericSet.add(-0);

  const symbolMap = new Map();
  const symbolSet = new Set();
  const symbols = [];
  for (let i = 0; i < 16; ++i) {
    const symbol = Symbol('collection-symbol-' + i);
    symbols.push(symbol);
    symbolMap.set(symbol, {symbolIndex: i});
    symbolSet.add(symbol);
  }

  const weakKeys = [];
  const weakValues = [];
  const weakMap = new WeakMap();
  const weakSet = new WeakSet();
  for (let i = 0; i < 32; ++i) {
    const key = {weakKey: i};
    const value = {weakValue: i};
    weakKeys.push(key);
    weakValues.push(value);
    weakMap.set(key, value);
    weakSet.add(key);
  }

  const weakRefs = [];
  const finalizationRegistries = [];
  if (typeof WeakRef === 'function') {
    for (let i = 0; i < weakValues.length; i += 4) {
      weakRefs.push(new WeakRef(weakValues[i]));
    }
  }
  if (typeof FinalizationRegistry === 'function') {
    for (let i = 0; i < 3; ++i) {
      const registry = new FinalizationRegistry((held) => held);
      for (let j = i; j < weakKeys.length; j += 3) {
        registry.register(weakKeys[j], {held: j}, weakValues[j]);
      }
      finalizationRegistries.push(registry);
    }
  }

  const mapIterators = {
    smallKeys: smallMap.keys(),
    smallValues: smallMap.values(),
    smallEntries: smallMap.entries(),
    largeKeys: largeMap.keys(),
    largeValues: largeMap.values(),
    largeEntries: largeMap.entries(),
    numericEntries: numericMap.entries(),
    symbolKeys: symbolMap.keys(),
  };
  const setIterators = {
    smallKeys: smallSet.keys(),
    smallValues: smallSet.values(),
    smallEntries: smallSet.entries(),
    largeKeys: largeSet.keys(),
    largeValues: largeSet.values(),
    largeEntries: largeSet.entries(),
    numericValues: numericSet.values(),
    symbolValues: symbolSet.values(),
  };

  const mapSnapshots = {
    largeFirstEntries: Array.from(largeMap.entries()).slice(0, 12),
    numericValues: Array.from(numericMap.values()).slice(0, 12),
    symbolEntries: Array.from(symbolMap.entries()).slice(0, 8),
  };
  const setSnapshots = {
    largeValues: Array.from(largeSet.values()).slice(0, 12),
    numericValues: Array.from(numericSet.values()).slice(0, 12),
    symbolValues: Array.from(symbolSet.values()).slice(0, 8),
  };

  const nestedCollections = new Map([
    ['map', largeMap],
    ['set', largeSet],
    ['weakMap', weakMap],
    ['weakSet', weakSet],
    ['symbols', symbolSet],
  ]);
  const collectionArray = [
    smallMap,
    smallSet,
    largeMap,
    largeSet,
    numericMap,
    numericSet,
    symbolMap,
    symbolSet,
    nestedCollections,
  ];

  keep('objectKeys', objectKeys);
  keep('objectValues', objectValues);
  keep('smallMap', smallMap);
  keep('smallSet', smallSet);
  keep('largeMap', largeMap);
  keep('largeSet', largeSet);
  keep('numericMap', numericMap);
  keep('numericSet', numericSet);
  keep('symbolMap', symbolMap);
  keep('symbolSet', symbolSet);
  keep('symbols', symbols);
  keep('weakKeys', weakKeys);
  keep('weakValues', weakValues);
  keep('weakMap', weakMap);
  keep('weakSet', weakSet);
  keep('weakRefs', weakRefs);
  keep('finalizationRegistries', finalizationRegistries);
  keep('mapIterators', mapIterators);
  keep('setIterators', setIterators);
  keep('mapSnapshots', mapSnapshots);
  keep('setSnapshots', setSnapshots);
  keep('nestedCollections', nestedCollections);
  keep('collectionArray', collectionArray);

  assertEquals(2, smallMap.size);
  assertEquals(2, smallSet.size);
  assertTrue(largeMap.size > 80);
  assertTrue(largeSet.size > 80);
  assertTrue(weakMap.has(weakKeys[0]));
  assertTrue(weakSet.has(weakKeys[0]));

  globalThis.heapDumpCollectionsHashSample = catalog;
})();
