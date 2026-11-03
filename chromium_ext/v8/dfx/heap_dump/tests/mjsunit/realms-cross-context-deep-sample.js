// Heap-dump shape sample for cross-Realm/native-context object graphs.
//
// This uses the V8 mjsunit Realm helper when d8 exposes it. Keep the sample
// guarded so the heap-dump corpus remains usable on shells without Realm.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  if (typeof Realm === 'object' &&
      typeof Realm.createAllowCrossRealmAccess === 'function' &&
      typeof Realm.eval === 'function') {
    const realmId = keep('realmId', Realm.createAllowCrossRealmAccess());
    const cross = keep('crossRealmExports', Realm.eval(realmId, `
      class CrossRealmClass {
        constructor(value) {
          this.value = value;
          this.extra = {kind: 'cross-realm-class-extra'};
        }
        method() { return this.value + 1; }
      }

      const target = {kind: 'cross-realm-proxy-target', value: 7};
      const handler = {
        get(target, key, receiver) {
          if (key === 'marker') return 'cross-realm-proxy';
          return Reflect.get(target, key, receiver);
        },
      };

      const promise = Promise.resolve({kind: 'cross-realm-promise-result'});
      const rejected = Promise.reject(new Error('cross-realm-rejected'));
      rejected.catch(() => undefined);

      const exports = {
        array: [1, 2, {kind: 'cross-realm-array-object'}],
        map: new Map([[{kind: 'cross-realm-map-key'}, {kind: 'map-value'}]]),
        set: new Set([{kind: 'cross-realm-set-value'}]),
        weakKey: {kind: 'cross-realm-weak-key'},
        date: new Date(Date.UTC(2026, 4, 25)),
        regexp: /cross-(?<realm>realm)-(\\d+)/giu,
        error: new Error('cross-realm-error', {
          cause: new TypeError('cross-realm-cause'),
        }),
        aggregateError: new AggregateError(
            [new Error('inner-cross'), new RangeError('range-cross')],
            'cross aggregate'),
        arrayBuffer: new ArrayBuffer(32),
        typedArray: new Uint16Array([1, 2, 3, 4]),
        dataView: null,
        fn: function crossRealmFunction(value) {
          return {kind: 'cross-realm-function-result', value};
        },
        boundFn: null,
        instance: new CrossRealmClass(41),
        klass: CrossRealmClass,
        proxy: new Proxy(target, handler),
        promise,
        rejected,
        iterator: [10, 11, 12].values(),
        globalObject: globalThis,
        json: JSON.parse('{"kind":"cross-realm-json"}'),
      };
      exports.dataView = new DataView(exports.arrayBuffer, 4, 16);
      exports.boundFn = exports.fn.bind(null, 42);
      exports.weakMap = new WeakMap([[exports.weakKey, {kind: 'weak-value'}]]);
      exports.weakSet = new WeakSet([exports.weakKey]);
      globalThis.__heapDumpCrossRealmInnerKeepalive = exports;
      exports;
    `));

    const outerFunction = keep('outerFunction', function outerFunction(value) {
      return {kind: 'outer-function-result', value};
    });
    const crossCaller = keep('crossCaller', Realm.eval(realmId, `
      function callOuter(fn, value) {
        return fn(value);
      }
      callOuter;
    `));

    keep('crossArrayResult', cross.array.slice(1));
    keep('crossMapEntries', Array.from(cross.map.entries()));
    keep('crossSetValues', Array.from(cross.set.values()));
    keep('crossRegExpMatch', cross.regexp.exec('cross-realm-123'));
    keep('crossFunctionResult', cross.fn(9));
    keep('crossBoundFunctionResult', cross.boundFn());
    keep('outerCallFromCrossRealm', crossCaller(outerFunction, 17));
    keep('crossIteratorFirst', cross.iterator.next());
    keep('crossPromiseThen', cross.promise.then((value) => value));
    keep('crossRejectedCatch', cross.rejected.catch((error) => error));

    assertEquals(3, cross.array.length);
    assertEquals(42, cross.instance.method());
    assertEquals('cross-realm-proxy', cross.proxy.marker);
    assertEquals('realm', catalog.crossRegExpMatch.groups.realm);
    assertEquals(42, catalog.crossBoundFunctionResult.value);
    assertEquals(17, catalog.outerCallFromCrossRealm.value);
    assertEquals(16, cross.dataView.byteLength);
    assertEquals(32, cross.dataView.buffer.byteLength);
  } else {
    keep('realmUnavailable', true);
  }

  globalThis.__heapDumpRealmsCrossContextKeepalive = catalog;
})();
