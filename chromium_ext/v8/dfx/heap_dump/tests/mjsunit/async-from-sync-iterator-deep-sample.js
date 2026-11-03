// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep Async-from-Sync iterator state alive for heap-dump smoke coverage. This
// follows stable portions of V8's harmony/async-from-sync-iterator.js,
// harmony/async-iterators-resolve.js, and
// harmony/async-generators-return-broken-promise.js mjsunit coverage.

(function() {
  'use strict';

  const catalog = {};
  let finished = false;
  let failure = undefined;

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function rejectLater(value) {
    return new Promise((resolve, reject) => {
      Promise.resolve().then(() => reject(value));
    });
  }

  class HeapDumpAsyncFromSyncError extends Error {}

  async function main() {
    function* promiseValueGenerator() {
      const local = {kind: 'async-from-sync-generator-local'};
      yield {kind: 'plain-sync-value', local};
      yield Promise.resolve({kind: 'promise-sync-value', local});
      yield rejectLater(new HeapDumpAsyncFromSyncError('rejected sync value'));
    }

    const generator = keep('generator', promiseValueGenerator());
    const asyncFromGenerator =
        keep('asyncFromGenerator', %CreateAsyncFromSyncIterator(generator));

    const firstNext = keep('firstNextPromise', asyncFromGenerator.next('first'));
    const firstResult = keep('firstNextResult', await firstNext);
    assertEquals('plain-sync-value', firstResult.value.kind);
    assertFalse(firstResult.done);

    const secondNext =
        keep('secondNextPromise', asyncFromGenerator.next('second'));
    const secondResult = keep('secondNextResult', await secondNext);
    assertEquals('promise-sync-value', secondResult.value.kind);
    assertFalse(secondResult.done);

    const rejectedNext =
        keep('rejectedNextPromise', asyncFromGenerator.next('third'));
    const rejectedError = keep(
        'rejectedNextError',
        await rejectedNext.then(
            () => assertUnreachable('rejected sync value resolved'),
            (error) => error));
    assertInstanceof(rejectedError, HeapDumpAsyncFromSyncError);

    const returned =
        keep('returnAfterRejectedPromise',
             await asyncFromGenerator.return({kind: 'manual-return'}));
    keep('returnAfterRejectedResult', returned);
    assertEquals('manual-return', returned.value.kind);
    assertTrue(returned.done);

    const log = keep('customIteratorLog', []);
    const customSyncIterator = keep('customSyncIterator', {
      index: 0,
      next(sent) {
        log.push({method: 'next', sent});
        return {value: {kind: 'custom-value', index: this.index++}, done: false};
      },
      return(sent) {
        log.push({method: 'return', sent});
        return {value: {kind: 'custom-return', sent}, done: true};
      },
      throw(sent) {
        log.push({method: 'throw', sent});
        throw sent;
      },
      [Symbol.iterator]() {
        return this;
      },
    });
    const asyncFromCustom =
        keep('asyncFromCustom', %CreateAsyncFromSyncIterator(customSyncIterator));
    keep('customNextResult',
         await keep('customNextPromise', asyncFromCustom.next('payload')));
    keep('customReturnResult',
         await keep('customReturnPromise',
                    asyncFromCustom.return({kind: 'close-payload'})));
    const throwSent = keep(
        'throwSentError',
        new HeapDumpAsyncFromSyncError('custom throw path'));
    const customThrowError = keep(
        'customThrowError',
        await keep('customThrowPromise', asyncFromCustom.throw(throwSent)).then(
            () => assertUnreachable('custom throw resolved'),
            (error) => error));
    assertSame(throwSent, customThrowError);
    assertEquals(['next', 'return', 'throw', 'return'],
                 log.map((entry) => entry.method));

    const throwingNextIterator = keep('throwingNextIterator', {
      next(sent) {
        throw new HeapDumpAsyncFromSyncError('next throws ' + sent);
      },
      [Symbol.iterator]() {
        return this;
      },
    });
    const asyncFromThrowingNext = keep(
        'asyncFromThrowingNext',
        %CreateAsyncFromSyncIterator(throwingNextIterator));
    const throwingNextError = keep(
        'throwingNextError',
        await keep('throwingNextPromise',
                   asyncFromThrowingNext.next('marker')).then(
            () => assertUnreachable('throwing next resolved'),
            (error) => error));
    assertEquals('next throws marker', throwingNextError.message);

    const missingNextIterator = keep('missingNextIterator', {
      [Symbol.iterator]() {
        return this;
      },
    });
    const asyncFromMissingNext = keep(
        'asyncFromMissingNext',
        %CreateAsyncFromSyncIterator(missingNextIterator));
    keep('missingNextError',
         await keep('missingNextPromise', asyncFromMissingNext.next()).then(
             () => assertUnreachable('missing next resolved'),
             (error) => error));
    assertInstanceof(catalog.missingNextError, TypeError);

    const brokenPromise = keep('brokenPromise', Promise.resolve(42));
    Object.defineProperty(brokenPromise, 'constructor', {
      configurable: true,
      get() {
        throw new HeapDumpAsyncFromSyncError('broken promise constructor');
      },
    });
    const forAwaitLog = keep('forAwaitLog', []);
    async function consumeBrokenPromise() {
      forAwaitLog.push('start');
      for await (const value of [brokenPromise]) {
        forAwaitLog.push(value);
      }
      forAwaitLog.push('unreachable');
    }
    keep('forAwaitError',
         await keep('forAwaitPromise', consumeBrokenPromise()).then(
             () => assertUnreachable('for-await broken promise resolved'),
             (error) => error));
    assertEquals(['start'], forAwaitLog);
    assertInstanceof(catalog.forAwaitError, HeapDumpAsyncFromSyncError);

    keep('summary', {
      firstKind: firstResult.value.kind,
      secondKind: secondResult.value.kind,
      customLogLength: log.length,
      forAwaitLogLength: forAwaitLog.length,
    });
    globalThis.heapDumpAsyncFromSyncIteratorDeepSample = catalog;
  }

  main().then(
      () => {
        finished = true;
      },
      (error) => {
        failure = error;
        finished = true;
      });

  %PerformMicrotaskCheckpoint();

  if (failure !== undefined) {
    throw failure;
  }
  assertTrue(finished);
})();
