// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Promise reaction/capability and async continuation shapes alive for heap
// dump smoke. The sample intentionally leaves many promises pending so their
// reaction lists and queued async-generator requests remain reachable.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makePending(label) {
    let resolve;
    let reject;
    const promise = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    return {label, promise, resolve, reject};
  }

  const roots = keep('roots', []);
  const reactionChains = keep('reactionChains', []);
  for (let i = 0; i < 24; ++i) {
    const root = makePending('root-' + i);
    roots.push(root);
    const chain = {
      thenValue: root.promise.then((value) => ({i, value, kind: 'then'})),
      thenThrow: root.promise.then(() => {
        throw new Error('reaction-throw-' + i);
      }),
      catchOnly: root.promise.catch((reason) => ({i, reason, kind: 'catch'})),
      finallyOnly: root.promise.finally(() => ({i, kind: 'finally'})),
      mixed: root.promise.then(
          (value) => ({i, value, kind: 'fulfilled'}),
          (reason) => ({i, reason, kind: 'rejected'})),
    };
    chain.thenThrow.catch((error) => ({i, handled: error.message}));
    reactionChains.push(chain);
  }

  const combinatorInputs = keep('combinatorInputs', []);
  for (let i = 0; i < 8; ++i) {
    combinatorInputs.push(makePending('combinator-' + i));
  }
  const combinators = keep('combinators', {
    allPending: Promise.all(combinatorInputs.map((entry) => entry.promise)),
    racePending: Promise.race(combinatorInputs.map((entry) => entry.promise)),
    allSettledPending: typeof Promise.allSettled === 'function'
        ? Promise.allSettled(combinatorInputs.map((entry) => entry.promise))
        : null,
    anyPending: typeof Promise.any === 'function'
        ? Promise.any(combinatorInputs.map((entry) => entry.promise))
        : null,
  });

  const thenableLog = keep('thenableLog', []);
  const thenables = keep('thenables', []);
  for (let i = 0; i < 12; ++i) {
    const blocker = makePending('thenable-blocker-' + i);
    const thenable = {
      i,
      blocker,
      then(resolve, reject) {
        thenableLog.push({i, resolve, reject});
        blocker.promise.then(resolve, reject);
      },
    };
    thenables.push(thenable);
  }
  const thenablePromises = keep(
      'thenablePromises', thenables.map((thenable) => Promise.resolve(thenable)));

  class HeapDumpPromiseReactionSubclass extends Promise {
    static get [Symbol.species]() {
      return Promise;
    }
  }
  const subclassRoots = keep('subclassRoots', []);
  for (let i = 0; i < 8; ++i) {
    let resolve;
    const promise = new HeapDumpPromiseReactionSubclass((res) => {
      resolve = res;
    });
    subclassRoots.push({
      promise,
      resolve,
      then: promise.then((value) => ({i, value})),
      finally: promise.finally(() => ({i, subclassFinally: true})),
    });
  }
  keep('promiseReactionSubclass', HeapDumpPromiseReactionSubclass);

  let releaseAsync;
  const asyncBlocker = keep('asyncBlocker', new Promise((resolve) => {
    releaseAsync = resolve;
  }));
  keep('releaseAsync', releaseAsync);
  async function suspendedAsync(seed) {
    const local = {seed, kind: 'async-local'};
    await asyncBlocker;
    return local;
  }
  const suspendedAsyncPromises = keep('suspendedAsyncPromises', []);
  for (let i = 0; i < 10; ++i) {
    suspendedAsyncPromises.push(suspendedAsync(i));
  }

  let releaseAsyncGenerator;
  const asyncGeneratorBlocker = keep(
      'asyncGeneratorBlocker',
      new Promise((resolve) => {
        releaseAsyncGenerator = resolve;
      }));
  keep('releaseAsyncGenerator', releaseAsyncGenerator);
  async function* queuedGenerator(seed) {
    yield {seed, first: true};
    await asyncGeneratorBlocker;
    yield {seed, second: true};
  }
  const queuedGenerators = keep('queuedGenerators', []);
  for (let i = 0; i < 6; ++i) {
    const generator = queuedGenerator(i);
    queuedGenerators.push({
      generator,
      first: generator.next(),
      second: generator.next(),
      thrown: generator.throw(new Error('queued-generator-error-' + i))
          .catch((error) => error),
    });
  }

  const alreadyHandledRejects = keep('alreadyHandledRejects', []);
  for (let i = 0; i < 12; ++i) {
    const rejected = Promise.reject(new Error('handled-reject-' + i));
    alreadyHandledRejects.push({
      rejected,
      handled: rejected.catch((error) => ({i, message: error.message})),
    });
  }

  assertEquals(24, roots.length);
  assertTrue(reactionChains[0].thenValue instanceof Promise);
  assertTrue(combinators.allPending instanceof Promise);
  assertTrue(thenablePromises[0] instanceof Promise);
  assertTrue(subclassRoots[0].promise instanceof HeapDumpPromiseReactionSubclass);
  assertTrue(subclassRoots[0].then instanceof Promise);
  assertTrue(suspendedAsyncPromises[0] instanceof Promise);
  assertTrue(queuedGenerators[0].first instanceof Promise);
  assertTrue(alreadyHandledRejects[0].handled instanceof Promise);

  globalThis.heapDumpPromiseReactionDeepSample = catalog;
})();
