// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep collection-method result shapes alive for heap dump smoke. This
// complements collections-hash-sample.js by following stable V8 Map.groupBy,
// Object.groupBy, and Set method test paths when those features are available.

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

  const objects = keep('objects', []);
  for (let i = 0; i < 24; ++i) {
    objects.push({
      index: i,
      group: i % 4,
      parity: i % 2 === 0 ? 'even' : 'odd',
      payload: {value: 'collection-method-' + i},
    });
  }

  const firstSet = keep('firstSet', new Set(objects.slice(0, 16)));
  const secondSet = keep('secondSet', new Set(objects.slice(8, 24)));
  const objectMap = keep(
      'objectMap',
      new Map(objects.map((object) => [object, {mappedIndex: object.index}])));
  const setLikeMap = keep(
      'setLikeMap',
      new Map(objects.slice(4, 12).map((object) => [object, true])));

  const setMethodResults = keep('setMethodResults', {});
  if (typeof Set.prototype.union === 'function') {
    setMethodResults.unionSet = firstSet.union(secondSet);
    setMethodResults.unionMap = firstSet.union(setLikeMap);
    setMethodResults.unionIterator = setMethodResults.unionSet.values();
    setMethodResults.unionFirst = setMethodResults.unionIterator.next();
  }
  if (typeof Set.prototype.intersection === 'function') {
    setMethodResults.intersectionSet = firstSet.intersection(secondSet);
    setMethodResults.intersectionMap = firstSet.intersection(setLikeMap);
    setMethodResults.intersectionEntries =
        Array.from(setMethodResults.intersectionSet.entries());
  }
  if (typeof Set.prototype.difference === 'function') {
    setMethodResults.differenceSet = firstSet.difference(secondSet);
    setMethodResults.differenceMap = firstSet.difference(setLikeMap);
  }
  if (typeof Set.prototype.symmetricDifference === 'function') {
    setMethodResults.symmetricDifference =
        firstSet.symmetricDifference(secondSet);
  }
  if (typeof Set.prototype.isSubsetOf === 'function') {
    setMethodResults.isSubsetOf = firstSet.isSubsetOf(
        new Set(objects.slice(0, 20)));
  }
  if (typeof Set.prototype.isSupersetOf === 'function') {
    setMethodResults.isSupersetOf = firstSet.isSupersetOf(
        new Set(objects.slice(0, 4)));
  }
  if (typeof Set.prototype.isDisjointFrom === 'function') {
    setMethodResults.isDisjointFrom = firstSet.isDisjointFrom(
        new Set(objects.slice(20, 24)));
  }

  if (typeof Set.prototype.union === 'function') {
    keepError('setUnionBadReceiverError',
              () => Set.prototype.union.call({}, secondSet));
  }
  if (typeof Set.prototype.intersection === 'function') {
    keepError('setIntersectionBadArgumentError',
              () => firstSet.intersection({keys: 1, size: 0}));
  }

  const groupingInput = keep('groupingInput', [
    ...objects.slice(0, 12),
    {index: 100, group: 'extra', parity: 'even', payload: {extra: true}},
  ]);

  if (typeof Map.groupBy === 'function') {
    const groupedByParity = keep(
        'mapGroupedByParity',
        Map.groupBy(groupingInput, (value) => value.parity));
    const groupedByObject = keep(
        'mapGroupedByObject',
        Map.groupBy(groupingInput, (value) => objects[value.group] || value));
    keep('mapGroupIterators', {
      parityKeys: groupedByParity.keys(),
      parityValues: groupedByParity.values(),
      objectEntries: groupedByObject.entries(),
    });
    keep('mapGroupSnapshots', {
      parityEntries: Array.from(groupedByParity.entries()),
      objectEntries: Array.from(groupedByObject.entries()).slice(0, 6),
    });
    keepError('mapGroupByCallbackError',
              () => Map.groupBy(groupingInput, () => {
                throw new TypeError('heap-dump map group callback');
              }));
  }

  if (typeof Object.groupBy === 'function') {
    const groupedObject = keep(
        'objectGroupedByParity',
        Object.groupBy(groupingInput, (value) => value.parity));
    const groupedByIndex = keep(
        'objectGroupedByIndex',
        Object.groupBy(groupingInput.slice(0, 4), (value) => value.index));
    keep('objectGroupDescriptors', {
      parity: Object.getOwnPropertyDescriptors(groupedObject),
      index: Object.getOwnPropertyDescriptors(groupedByIndex),
    });
    keepError('objectGroupByCallbackError',
              () => Object.groupBy(groupingInput, () => {
                throw new RangeError('heap-dump object group callback');
              }));
  }

  if (typeof Iterator === 'function' && typeof Iterator.from === 'function') {
    const wrappedSetIterator = keep(
        'wrappedSetIterator',
        Iterator.from(firstSet.values()));
    const wrappedMapIterator = keep(
        'wrappedMapIterator',
        Iterator.from(objectMap.entries()));
    keep('wrappedIteratorResults', {
      setFirst: wrappedSetIterator.next(),
      mapFirst: wrappedMapIterator.next(),
    });
    keepError('iteratorFromBadIteratorError',
              () => Iterator.from({[Symbol.iterator]: 1}));
  }

  keep('summary', {
    firstSetSize: firstSet.size,
    secondSetSize: secondSet.size,
    objectMapSize: objectMap.size,
    setMethodResultNames: Object.keys(setMethodResults),
  });

  assertEquals(16, firstSet.size);
  assertEquals(16, secondSet.size);
  assertTrue(objectMap.has(objects[0]));
  if (setMethodResults.unionSet) {
    assertTrue(setMethodResults.unionSet.has(objects[23]));
  }
  if (catalog.mapGroupedByParity) {
    assertTrue(catalog.mapGroupedByParity.get('even').length > 0);
  }
  if (catalog.objectGroupedByParity) {
    assertTrue(Array.isArray(catalog.objectGroupedByParity.even));
  }

  globalThis.heapDumpCollectionsMethodsDeepSample = catalog;
})();
