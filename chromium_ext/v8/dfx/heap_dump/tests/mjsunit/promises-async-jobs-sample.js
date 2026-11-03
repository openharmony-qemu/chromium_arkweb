// Heap-dump smoke sample for Promise and async job/task shaped objects.
//
// Exact PromiseReaction/PromiseCapability/job task edges are covered by cctests.
// This sample keeps naturally-created pending reactions, thenables, async
// functions, and async-generator requests alive as dump/translate input.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  let resolvePending;
  let rejectPending;
  const pending = new Promise((resolve, reject) => {
    resolvePending = resolve;
    rejectPending = reject;
  });

  const fulfillChain = pending
      .then((value) => ({fulfilled: value}))
      .then((value) => ({wrapped: value}));
  const rejectChain = pending
      .catch((reason) => ({rejected: reason}))
      .finally(() => ({finallyRan: true}));
  const mixedChain = pending.then(
      (value) => ({ok: value}),
      (reason) => ({bad: reason}));

  const alreadyResolved = Promise.resolve({resolved: true});
  const alreadyRejected = Promise.reject(new Error('heap-dump rejected'));
  const handledRejected = alreadyRejected.catch((error) => error.message);

  const thenable = {
    marker: {thenable: true},
    then(resolve) {
      resolve({fromThenable: true, marker: this.marker});
    },
  };
  const thenablePromise = Promise.resolve(thenable);
  const returnedThenable = Promise.resolve()
      .then(() => thenable)
      .then((value) => ({returnedThenableValue: value}));

  const promiseCombinators = {
    all: Promise.all([alreadyResolved, Promise.resolve(7)]),
    race: Promise.race([pending, alreadyResolved]),
    allSettled: typeof Promise.allSettled === 'function'
        ? Promise.allSettled([alreadyResolved, handledRejected])
        : null,
    any: typeof Promise.any === 'function'
        ? Promise.any([Promise.reject('ignored'), alreadyResolved])
        : null,
  };

  async function asyncFunctionWithAwait(input) {
    const first = await Promise.resolve(input);
    const second = await thenable;
    return {first, second};
  }
  const asyncFunctionPromise = asyncFunctionWithAwait({asyncInput: true});

  let releaseAsyncGenerator;
  const asyncGeneratorBlocker = new Promise((resolve) => {
    releaseAsyncGenerator = resolve;
  });
  async function* asyncGeneratorWithQueue() {
    yield {firstYield: true};
    await asyncGeneratorBlocker;
    yield {secondYield: true};
  }
  const asyncGenerator = asyncGeneratorWithQueue();
  const asyncGeneratorRequests = [
    asyncGenerator.next(),
    asyncGenerator.next(),
    asyncGenerator.return({returnValue: true}),
  ];

  const nestedPendingPromises = [];
  for (let i = 0; i < 16; ++i) {
    const blocker = new Promise(() => {});
    nestedPendingPromises.push({
      blocker,
      then: blocker.then((value) => ({i, value})),
      catch: blocker.catch((reason) => ({i, reason})),
    });
  }

  class HeapDumpPromiseSubclass extends Promise {}
  const subclassPromise = new HeapDumpPromiseSubclass((resolve) => {
    resolve({subclass: true});
  });
  const subclassChain = subclassPromise.then((value) => value);

  keep('pending', pending);
  keep('resolvePending', resolvePending);
  keep('rejectPending', rejectPending);
  keep('fulfillChain', fulfillChain);
  keep('rejectChain', rejectChain);
  keep('mixedChain', mixedChain);
  keep('alreadyResolved', alreadyResolved);
  keep('alreadyRejected', alreadyRejected);
  keep('handledRejected', handledRejected);
  keep('thenable', thenable);
  keep('thenablePromise', thenablePromise);
  keep('returnedThenable', returnedThenable);
  keep('promiseCombinators', promiseCombinators);
  keep('asyncFunctionPromise', asyncFunctionPromise);
  keep('asyncGeneratorBlocker', asyncGeneratorBlocker);
  keep('releaseAsyncGenerator', releaseAsyncGenerator);
  keep('asyncGenerator', asyncGenerator);
  keep('asyncGeneratorRequests', asyncGeneratorRequests);
  keep('nestedPendingPromises', nestedPendingPromises);
  keep('promiseSubclass', HeapDumpPromiseSubclass);
  keep('subclassPromise', subclassPromise);
  keep('subclassChain', subclassChain);

  assertTrue(pending instanceof Promise);
  assertTrue(thenablePromise instanceof Promise);
  assertTrue(typeof asyncGenerator.next === 'function');

  globalThis.heapDumpPromisesAsyncJobsSample = catalog;
})();
