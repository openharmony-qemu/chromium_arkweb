// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Array.fromAsync, async-from-sync iterator, and async iterator result
// shapes alive for heap dump smoke. This follows V8's array-from-async mjsunit
// patterns and retains fulfilled results, pending conversion promises, iterator
// close records, subclass results, and representative error objects.

(function() {
  'use strict';

  const catalog = {
    promises: [],
    results: [],
    errors: [],
  };
  globalThis.__heapDumpArrayFromAsyncVariants = catalog;

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  async function keepErrorAsync(name, factory) {
    try {
      await factory();
    } catch (e) {
      catalog.errors.push(e);
      return keep(name, e);
    }
    return keep(name, null);
  }

  async function* asyncGen(count) {
    for (let i = 0; i < count; ++i) {
      yield {kind: 'async-gen-value', index: i};
    }
  }

  function* syncGen() {
    yield {kind: 'sync-gen-value', index: 0};
    yield {kind: 'sync-gen-value', index: 1};
    yield {kind: 'sync-gen-value', index: 2};
  }

  const closeLog = keep('closeLog', []);
  const closableSyncIterable = keep('closableSyncIterable', {
    [Symbol.iterator]() {
      let index = 0;
      return {
        next() {
          return {value: {kind: 'closable-sync-value', index: index++},
                  done: false};
        },
        return(value) {
          closeLog.push({kind: 'sync-return', value});
          return {done: true, value};
        },
      };
    },
  });

  const closableAsyncIterable = keep('closableAsyncIterable', {
    [Symbol.asyncIterator]() {
      let index = 0;
      return {
        async next() {
          return {value: {kind: 'closable-async-value', index: index++},
                  done: false};
        },
        async return(value) {
          closeLog.push({kind: 'async-return', value});
          return {done: true, value};
        },
      };
    },
  });

  let releaseBlocked;
  const blockedGate = keep('blockedGate', new Promise((resolve) => {
    releaseBlocked = resolve;
  }));
  keep('releaseBlocked', releaseBlocked);
  const blockedAsyncIterable = keep('blockedAsyncIterable', {
    [Symbol.asyncIterator]() {
      let index = 0;
      return {
        async next() {
          if (index++ === 0) return {value: {blocked: 'first'}, done: false};
          await blockedGate;
          return {done: true};
        },
      };
    },
  });

  class HeapDumpFromAsyncArray extends Array {}
  keep('fromAsyncSubclass', HeapDumpFromAsyncArray);

  async function runCatalog() {
    const asyncResult = await Array.fromAsync(
        asyncGen(4), (value) => ({mapped: value.index * 2, source: value}));
    catalog.results.push(keep('asyncResult', asyncResult));

    const syncResult = await Array.fromAsync(
        syncGen(), (value) => Promise.resolve(value.index + 10));
    catalog.results.push(keep('syncResult', syncResult));

    const arrayLike = keep('arrayLikeInput', {
      length: 3,
      0: Promise.resolve({arrayLike: 0}),
      1: {arrayLike: 1},
      2: Promise.resolve({arrayLike: 2}),
    });
    const arrayLikeResult = await Array.fromAsync(
        arrayLike, (value, index) => ({index, value}));
    catalog.results.push(keep('arrayLikeResult', arrayLikeResult));

    const subclassResult = await Array.fromAsync.call(
        HeapDumpFromAsyncArray, [1, Promise.resolve(2), 3],
        (value) => value + 20);
    catalog.results.push(keep('subclassResult', subclassResult));

    const blockedPromise =
        keep('blockedFromAsyncPromise',
             Array.fromAsync(blockedAsyncIterable, (value) => value));
    catalog.promises.push(blockedPromise);

    await keepErrorAsync('syncMapperThrowError', () => Array.fromAsync(
        closableSyncIterable,
        (value) => {
          if (value.index === 1) {
            throw new TypeError('heap-dump-array-from-async-sync-close');
          }
          return value;
        }));

    await keepErrorAsync('asyncMapperThrowError', () => Array.fromAsync(
        closableAsyncIterable,
        async (value) => {
          if (value.index === 1) {
            throw new RangeError('heap-dump-array-from-async-async-close');
          }
          return value;
        }));

    await keepErrorAsync(
        'badAsyncIteratorError',
        () => Array.fromAsync({[Symbol.asyncIterator]: 1}));
    await keepErrorAsync(
        'badMapperError',
        () => Array.fromAsync([1, 2], 17));

    return catalog;
  }

  keep('drivers', {
    asyncGen,
    syncGen,
    runCatalog,
  });

  const catalogPromise = keep('catalogPromise', runCatalog());
  catalog.promises.push(catalogPromise);

  assertPromiseResult(catalogPromise, () => {
    assertEquals(4, catalog.asyncResult.length);
    assertEquals(10, catalog.syncResult[0]);
    assertEquals(3, catalog.arrayLikeResult.length);
    assertTrue(catalog.subclassResult instanceof HeapDumpFromAsyncArray);
    assertTrue(catalog.blockedFromAsyncPromise instanceof Promise);
    assertTrue(catalog.syncMapperThrowError instanceof TypeError);
    assertTrue(catalog.asyncMapperThrowError instanceof RangeError);
    assertTrue(catalog.badAsyncIteratorError instanceof TypeError);
    assertTrue(catalog.badMapperError instanceof TypeError);
    assertTrue(closeLog.some((entry) => entry.kind === 'sync-return'));
    assertTrue(closeLog.some((entry) => entry.kind === 'async-return'));
  });
})();
