// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep PromiseCapability-style construction paths alive for heap dump smoke.
// This follows V8's Promise.withResolvers mjsunit patterns while retaining
// pending capabilities, resolver functions, reaction chains, subclass promises,
// Promise.try results, and representative error objects.

(function() {
  'use strict';

  const catalog = {};
  globalThis.__heapDumpPromiseCapabilityVariants = catalog;

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (e) {
      return keep(name, e);
    }
    return keep(name, null);
  }

  const descriptor = keep(
      'withResolversDescriptor',
      Object.getOwnPropertyDescriptor(Promise, 'withResolvers'));
  assertTrue(descriptor.configurable);
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);

  const pendingCapabilities = keep('pendingCapabilities', []);
  const reactionChains = keep('reactionChains', []);
  for (let i = 0; i < 12; ++i) {
    const capability = Promise.withResolvers();
    pendingCapabilities.push({
      label: 'pending-' + i,
      promise: capability.promise,
      resolve: capability.resolve,
      reject: capability.reject,
    });
    reactionChains.push({
      thenValue: capability.promise.then((value) => ({i, value})),
      catchReason: capability.promise.catch((reason) => ({i, reason})),
      finallyOnly: capability.promise.finally(() => ({i, finallyOnly: true})),
    });
  }

  const resolvedCapability = keep('resolvedCapability', Promise.withResolvers());
  const resolvedChain = keep('resolvedChain', {
    then: resolvedCapability.promise.then((value) => {
      keep('resolvedValue', value);
      return {value, handled: true};
    }),
    finally: resolvedCapability.promise.finally(() => {
      keep('resolvedFinallyMarker', {ran: true});
    }),
  });
  resolvedCapability.resolve({kind: 'with-resolvers-resolved'});

  const rejectedCapability = keep('rejectedCapability', Promise.withResolvers());
  const rejectionReason = keep(
      'rejectionReason', new Error('heap-dump-with-resolvers-reject'));
  const rejectedChain = keep('rejectedChain', {
    catch: rejectedCapability.promise.catch((reason) => {
      keep('caughtRejectionReason', reason);
      return {message: reason.message};
    }),
    thenRejected: rejectedCapability.promise.then(
        () => ({unexpected: true}),
        (reason) => ({handled: reason.message})),
  });
  rejectedCapability.reject(rejectionReason);

  class HeapDumpPromiseCapabilitySubclass extends Promise {
    static get [Symbol.species]() {
      return Promise;
    }
  }
  keep('promiseCapabilitySubclass', HeapDumpPromiseCapabilitySubclass);
  const subclassCapability = keep(
      'subclassCapability',
      Promise.withResolvers.call(HeapDumpPromiseCapabilitySubclass));
  const subclassChain = keep('subclassChain', {
    then: subclassCapability.promise.then((value) => ({subclassValue: value})),
    finally: subclassCapability.promise.finally(() => ({subclassFinally: true})),
  });

  const combinators = keep('combinators', {
    allPending: Promise.all(pendingCapabilities.slice(0, 4).map(
        (entry) => entry.promise)),
    racePending: Promise.race(pendingCapabilities.slice(4, 8).map(
        (entry) => entry.promise)),
    allSettledPending: Promise.allSettled(pendingCapabilities.slice(8).map(
        (entry) => entry.promise)),
  });

  const promiseTryResults = keep('promiseTryResults', []);
  if (typeof Promise.try === 'function') {
    const tryResolved = Promise.try(() => ({kind: 'try-resolved'}));
    const tryRejected = Promise.try(() => {
      throw new RangeError('heap-dump-promise-try');
    });
    promiseTryResults.push({
      tryResolved,
      tryResolvedThen: tryResolved.then((value) => ({value})),
      tryRejected,
      tryRejectedCatch: tryRejected.catch((error) => {
        keep('promiseTryError', error);
        return error.message;
      }),
    });
  }

  keep('withResolversBadReceiverError',
       keepError('withResolversBadReceiverError',
                 () => Promise.withResolvers.call({})));
  keep('withResolversNonConstructorError',
       keepError('withResolversNonConstructorError',
                 () => Promise.withResolvers.call(() => {})));
  keep('promiseTryBadReceiverError',
       keepError('promiseTryBadReceiverError',
                 () => Promise.try.call({}, () => 1)));

  assertTrue(pendingCapabilities[0].promise instanceof Promise);
  assertEquals('function', typeof pendingCapabilities[0].resolve);
  assertEquals('function', typeof pendingCapabilities[0].reject);
  assertTrue(resolvedChain.then instanceof Promise);
  assertTrue(rejectedChain.catch instanceof Promise);
  assertTrue(subclassCapability.promise instanceof HeapDumpPromiseCapabilitySubclass);
  assertTrue(subclassChain.then instanceof Promise);
  assertTrue(combinators.allPending instanceof Promise);
  assertTrue(catalog.withResolversBadReceiverError instanceof TypeError);

  %PerformMicrotaskCheckpoint();
})();
