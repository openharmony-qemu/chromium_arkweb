// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Keep Promise.any AggregateError, subclass, thenable, iterator-abrupt, and
// pending reaction shapes alive for heap dump smoke. The semantic paths mirror
// stable coverage from harmony/promise-any.js without using async test harness
// timers, so dump/translate can capture the settled objects directly.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function recordRejected(name, promise) {
    keep(name + 'Promise', promise);
    promise.catch((error) => {
      catalog[name + 'Error'] = error;
      if (error && typeof error === 'object' && 'errors' in error) {
        catalog[name + 'Errors'] = error.errors;
        catalog[name + 'ErrorsDescriptor'] =
            Object.getOwnPropertyDescriptor(error, 'errors');
      }
      return error;
    });
    return promise;
  }

  const emptyAny = recordRejected('emptyAny', Promise.any([]));

  const rejectionReasons = keep('rejectionReasons', [
    new Error('heap-dump-any-error-one'),
    new TypeError('heap-dump-any-type-two'),
    new RangeError('heap-dump-any-range-three'),
  ]);
  const allRejected = recordRejected(
      'allRejected',
      Promise.any(rejectionReasons.map((reason) => Promise.reject(reason))));

  const thenableLog = keep('thenableLog', []);
  const rejectingThenables = keep('rejectingThenables', [
    {
      label: 'thenable-one',
      then(resolve, reject) {
        thenableLog.push(['then', this.label]);
        reject(new SyntaxError('heap-dump-any-thenable-one'));
      },
    },
    {
      label: 'thenable-two',
      then(resolve, reject) {
        thenableLog.push(['then', this.label]);
        reject(new URIError('heap-dump-any-thenable-two'));
      },
    },
  ]);
  const thenableRejected =
      recordRejected('thenableRejected', Promise.any(rejectingThenables));

  class HeapDumpAnyPromiseSubclass extends Promise {
    constructor(executor, label) {
      super(executor);
      this.label = label;
    }

    then(resolve, reject) {
      if (this.label) return reject(this.label);
      return super.then(resolve, reject);
    }
  }
  keep('HeapDumpAnyPromiseSubclass', HeapDumpAnyPromiseSubclass);
  const subclassInputs = keep('subclassInputs', [
    new HeapDumpAnyPromiseSubclass(() => {}, 'subclass-a'),
    new HeapDumpAnyPromiseSubclass(() => {}, 'subclass-b'),
  ]);
  const subclassAny = recordRejected(
      'subclassAny', HeapDumpAnyPromiseSubclass.any(subclassInputs));

  const iteratorLog = keep('iteratorLog', []);
  const abruptIterable = keep('abruptIterable', {
    [Symbol.iterator]() {
      let index = 0;
      return {
        next() {
          iteratorLog.push(['next', index]);
          if (index === 2) {
            throw new ReferenceError('heap-dump-any-iterator-next');
          }
          return {
            value: Promise.reject(new Error('iterator-reject-' + index++)),
            done: false,
          };
        },
        return() {
          iteratorLog.push(['return', index]);
          return {done: true};
        },
      };
    },
  });
  const iteratorAbrupt =
      recordRejected('iteratorAbrupt', Promise.any(abruptIterable));

  const fulfilledInputs = keep('fulfilledInputs', [
    Promise.reject(new Error('ignored-before-fulfill')),
    Promise.resolve({kind: 'fulfilled-any', index: 1}),
    Promise.resolve({kind: 'fulfilled-any', index: 2}),
  ]);
  const fulfilledAny = keep('fulfilledAny', Promise.any(fulfilledInputs));
  fulfilledAny.then((value) => {
    catalog.fulfilledAnyValue = value;
    return value;
  });

  const pendingInputs = keep('pendingInputs', []);
  for (let i = 0; i < 6; ++i) {
    let resolve;
    let reject;
    const promise = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    pendingInputs.push({i, promise, resolve, reject});
  }
  const pendingAny =
      keep('pendingAny', Promise.any(pendingInputs.map((entry) => entry.promise)));

  %PerformMicrotaskCheckpoint();
  %PerformMicrotaskCheckpoint();

  assertTrue(emptyAny instanceof Promise);
  assertTrue(allRejected instanceof Promise);
  assertTrue(thenableRejected instanceof Promise);
  assertTrue(subclassAny instanceof HeapDumpAnyPromiseSubclass);
  assertTrue(iteratorAbrupt instanceof Promise);
  assertTrue(fulfilledAny instanceof Promise);
  assertTrue(pendingAny instanceof Promise);
  assertTrue(catalog.emptyAnyError instanceof AggregateError);
  assertEquals(0, catalog.emptyAnyErrors.length);
  assertTrue(catalog.allRejectedError instanceof AggregateError);
  assertEquals(rejectionReasons, catalog.allRejectedErrors);
  assertTrue(catalog.thenableRejectedError instanceof AggregateError);
  assertEquals(2, catalog.thenableRejectedErrors.length);
  assertEquals(['subclass-a', 'subclass-b'], catalog.subclassAnyErrors);
  assertTrue(catalog.iteratorAbruptError instanceof ReferenceError);
  assertEquals('fulfilled-any', catalog.fulfilledAnyValue.kind);
  assertEquals(true, catalog.emptyAnyErrorsDescriptor.configurable);
  assertEquals(false, catalog.emptyAnyErrorsDescriptor.enumerable);
  assertEquals(true, catalog.emptyAnyErrorsDescriptor.writable);
  assertEquals(6, pendingInputs.length);

  globalThis.heapDumpPromiseAnyAggregateDeepSample = catalog;
})();
