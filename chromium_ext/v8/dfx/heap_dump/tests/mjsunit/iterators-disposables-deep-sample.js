// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep iterator helper, async-from-sync, and disposable-stack shapes alive for
// heap dump smoke. Feature-gated pieces must remain optional.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const baseArray = keep('baseArray', [
    {value: 1},
    {value: 2},
    {value: 3},
    {value: 4},
  ]);
  const map = keep('map', new Map([
    ['alpha', {value: 1}],
    ['beta', {value: 2}],
  ]));
  const set = keep('set', new Set([{value: 5}, {value: 6}]));
  const stringValue = keep('stringValue', 'heap-dump-iterator-\u0100');

  keep('arrayIterators', {
    keys: baseArray.keys(),
    values: baseArray.values(),
    entries: baseArray.entries(),
  });
  keep('mapIterators', {
    keys: map.keys(),
    values: map.values(),
    entries: map.entries(),
  });
  keep('setIterators', {
    keys: set.keys(),
    values: set.values(),
    entries: set.entries(),
  });
  keep('stringIterator', stringValue[Symbol.iterator]());
  keep('regexpStringIterator',
       'heap dump heap dump'.matchAll(/heap\s+(dump)/g));

  const customIteratorLog = keep('customIteratorLog', []);
  const customIterable = keep('customIterable', {
    state: {nextValue: 0},
    [Symbol.iterator]() {
      const state = this.state;
      return {
        next() {
          customIteratorLog.push(['next', state.nextValue]);
          if (state.nextValue >= 4) return {done: true};
          return {value: {custom: state.nextValue++}, done: false};
        },
        return() {
          customIteratorLog.push(['return', state.nextValue]);
          return {done: true};
        },
      };
    },
  });
  const customIterator = keep('customIterator', customIterable[Symbol.iterator]());
  keep('customIteratorFirst', customIterator.next());

  const iteratorHelpers = keep('iteratorHelpers', {});
  try {
    if (typeof Iterator === 'function' && typeof Iterator.from === 'function') {
      iteratorHelpers.fromArray = Iterator.from(baseArray);
      iteratorHelpers.fromCustom = Iterator.from(customIterable);
      iteratorHelpers.map =
          Iterator.from([1, 2, 3, 4]).map((value) => ({mapped: value + 1}));
      iteratorHelpers.filter =
          Iterator.from([1, 2, 3, 4]).filter((value) => value % 2 === 0);
      iteratorHelpers.take = Iterator.from([1, 2, 3, 4]).take(3);
      iteratorHelpers.drop = Iterator.from([1, 2, 3, 4]).drop(1);
      iteratorHelpers.flatMap =
          Iterator.from([1, 2]).flatMap((value) => [
            {flat: value},
            {flat: value + 10},
          ]);
    }
  } catch (e) {
    keep('iteratorHelperError', e);
  }

  let releaseAsyncFromSync;
  const asyncFromSyncBlocker = keep(
      'asyncFromSyncBlocker',
      new Promise((resolve) => {
        releaseAsyncFromSync = resolve;
      }));
  keep('releaseAsyncFromSync', releaseAsyncFromSync);
  async function consumeSyncIterableWithForAwait(iterable) {
    const seen = [];
    for await (const item of iterable) {
      seen.push(item);
      await asyncFromSyncBlocker;
    }
    return seen;
  }
  const asyncFromSyncPromise =
      keep('asyncFromSyncPromise',
           consumeSyncIterableWithForAwait([
             {asyncFromSync: 1},
             {asyncFromSync: 2},
           ]));

  const asyncIterable = keep('asyncIterable', {
    index: 0,
    async next() {
      if (this.index >= 3) return {done: true};
      return {value: {asyncValue: this.index++}, done: false};
    },
    [Symbol.asyncIterator]() {
      return this;
    },
  });
  const asyncIterator = keep('asyncIterator', asyncIterable[Symbol.asyncIterator]());
  keep('asyncIteratorFirstRequest', asyncIterator.next());

  const disposalLog = keep('disposalLog', []);
  let disposableStack = null;
  try {
    if (typeof DisposableStack === 'function') {
      disposableStack = new DisposableStack();
      disposableStack.defer(() => disposalLog.push('defer-one'));
      if (typeof Symbol.dispose === 'symbol') {
        disposableStack.use({
          resource: 'sync-resource',
          [Symbol.dispose]() {
            disposalLog.push('sync-dispose');
          },
        });
      }
      disposableStack.adopt({adopted: true}, (value) => {
        disposalLog.push(['adopt', value]);
      });
    }
  } catch (e) {
    keep('disposableStackError', e);
  }
  keep('disposableStack', disposableStack);

  let asyncDisposableStack = null;
  try {
    if (typeof AsyncDisposableStack === 'function') {
      asyncDisposableStack = new AsyncDisposableStack();
      asyncDisposableStack.defer(async () => disposalLog.push('async-defer'));
      if (typeof Symbol.asyncDispose === 'symbol') {
        asyncDisposableStack.use({
          resource: 'async-resource',
          async [Symbol.asyncDispose]() {
            disposalLog.push('async-dispose');
          },
        });
      }
      asyncDisposableStack.adopt({asyncAdopted: true}, async (value) => {
        disposalLog.push(['async-adopt', value]);
      });
    }
  } catch (e) {
    keep('asyncDisposableStackError', e);
  }
  keep('asyncDisposableStack', asyncDisposableStack);

  assertEquals(1, catalog.arrayIterators.values.next().value.value);
  assertEquals('alpha', catalog.mapIterators.keys.next().value);
  assertEquals(5, catalog.setIterators.values.next().value.value);
  assertEquals('h', catalog.stringIterator.next().value);
  assertEquals('dump', catalog.regexpStringIterator.next().value[1]);
  assertEquals(0, catalog.customIteratorFirst.value.custom);
  assertTrue(asyncFromSyncPromise instanceof Promise);
  assertTrue(catalog.asyncIteratorFirstRequest instanceof Promise);
  if (disposableStack !== null) {
    assertTrue(disposableStack instanceof DisposableStack);
  }
  if (asyncDisposableStack !== null) {
    assertTrue(asyncDisposableStack instanceof AsyncDisposableStack);
  }

  globalThis.heapDumpIteratorsDisposablesDeepSample = catalog;
})();
