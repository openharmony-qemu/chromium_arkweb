// Heap-dump smoke sample for execution and language metadata objects.
//
// This collects stable patterns already used throughout V8 mjsunit tests:
// structured error stacks, private/static class state, revocable proxies,
// tagged templates, promise combinators, and derived builtins.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  class BaseForHeapDump {
    constructor(label) {
      this.baseLabel = label;
    }

    baseMethod() {
      return this.baseLabel;
    }
  }

  class DerivedForHeapDump extends BaseForHeapDump {
    #secret;
    #privateData = {private: true};
    static #staticSecret = 'static-secret';
    static staticField = {staticField: true};
    static {
      this.staticBlockValue = this.#staticSecret + '-block';
    }

    constructor(label, value) {
      super(label);
      this.publicField = {value};
      this.#secret = value;
    }

    get privateData() {
      return this.#privateData;
    }

    #privateMethod() {
      return this.#secret + 1;
    }

    readPrivate() {
      return this.#privateMethod();
    }

    static readStaticPrivate() {
      return this.#staticSecret;
    }
  }

  const derived = keep('derivedInstance',
                       new DerivedForHeapDump('base-label', 41));
  assertEquals('base-label', derived.baseMethod());
  assertEquals(42, derived.readPrivate());
  assertEquals('static-secret', DerivedForHeapDump.readStaticPrivate());
  keep('derivedConstructor', DerivedForHeapDump);
  keep('derivedPrototype', DerivedForHeapDump.prototype);
  keep('derivedPrivateData', derived.privateData);

  function tagForHeapDump(strings, ...values) {
    return {strings, raw: strings.raw, values};
  }
  const tagValue = 'tag-value';
  keep('taggedTemplate', tagForHeapDump`head-${tagValue}-tail`);

  const revokedPair = Proxy.revocable(
      function proxyTarget(a, b) { return a + b; },
      {
        apply(target, receiver, args) {
          return Reflect.apply(target, receiver, args) + 1;
        },
        get(target, key, receiver) {
          if (key === 'heapDumpProxyMarker') return true;
          return Reflect.get(target, key, receiver);
        },
      });
  assertEquals(4, revokedPair.proxy(1, 2));
  assertTrue(revokedPair.proxy.heapDumpProxyMarker);
  keep('revocableProxyPair', revokedPair);
  keep('revocableProxy', revokedPair.proxy);
  keep('revocableRevoke', revokedPair.revoke);

  const callSiteState = {};
  const oldPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = function(error, frames) {
    callSiteState.error = error;
    callSiteState.frames = frames;
    callSiteState.firstFrame = frames[0];
    return frames;
  };
  try {
    function captureStackOuter() {
      return captureStackInner();
    }
    function captureStackInner() {
      const error = new Error('heap-dump structured stack');
      return error.stack;
    }
    keep('structuredStackFrames', captureStackOuter());
    keep('structuredStackState', callSiteState);
  } finally {
    Error.prepareStackTrace = oldPrepareStackTrace;
  }
  assertTrue(Array.isArray(callSiteState.frames));
  assertTrue(callSiteState.frames.length > 0);

  const aggregateError = keep(
      'aggregateError',
      new AggregateError([new Error('inner-one'), new TypeError('inner-two')],
                         'heap-dump aggregate'));
  assertEquals(2, aggregateError.errors.length);

  keep('promiseAll', Promise.all([Promise.resolve(1), Promise.resolve(2)]));
  keep('promiseRace', Promise.race([Promise.resolve('race')]));
  if (typeof Promise.any === 'function') {
    keep('promiseAnyReject',
         Promise.any([Promise.reject(new Error('any-reject'))])
             .catch((error) => error));
  }
  if (typeof Promise.allSettled === 'function') {
    keep('promiseAllSettled',
         Promise.allSettled([Promise.resolve('ok'), Promise.reject('bad')]));
  }

  class HeapDumpArraySubclass extends Array {
    static get [Symbol.species]() {
      return Array;
    }
  }
  const subclassArray = keep(
      'subclassArray',
      new HeapDumpArraySubclass(1, 2, 3));
  keep('subclassArrayMapped', subclassArray.map((value) => value * 2));

  class HeapDumpUint8ArraySubclass extends Uint8Array {}
  const typedSubclass = keep(
      'typedSubclass',
      new HeapDumpUint8ArraySubclass([1, 2, 3, 4]));
  keep('typedSubclassSliced', typedSubclass.slice(1, 3));

  const catchContextValue = (function makeCatchContext() {
    try {
      throw new Error('heap-dump catch context');
    } catch (caught) {
      return function readCaught() {
        return caught.message;
      };
    }
  })();
  assertEquals('heap-dump catch context', catchContextValue());
  keep('catchContextClosure', catchContextValue);

  globalThis.heapDumpExecutionDeepSample = catalog;
})();
