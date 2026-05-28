// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-shadow-realm

// Keep ShadowRealm and wrapped-function shapes alive for heap dump smoke. This
// follows V8's ShadowRealm mjsunit patterns while preserving the realm object,
// wrapped callables, bound/proxy wrappers, and copied error objects.

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const shadowRealm = keep('shadowRealm', new ShadowRealm());
  keep('realmGlobalMarker',
       shadowRealm.evaluate('globalThis.heapDumpShadowRealmMarker = 42'));
  assertEquals(42, shadowRealm.evaluate('globalThis.heapDumpShadowRealmMarker'));

  const wrappedClosure = keep('wrappedClosure', shadowRealm.evaluate(`
    (function() {
      let counter = 0;
      return function realmClosure(delta) {
        counter += delta;
        return counter;
      };
    })()
  `));
  assertEquals(3, wrappedClosure(3));
  assertEquals(8, wrappedClosure(5));
  assertEquals('realmClosure', wrappedClosure.name);
  assertEquals(1, wrappedClosure.length);

  const boundWrapped = keep('boundWrapped', wrappedClosure.bind(undefined, 7));
  assertEquals(15, boundWrapped());
  assertEquals('bound realmClosure', boundWrapped.name);

  const proxyWrapped = keep('proxyWrapped', new Proxy(wrappedClosure, {
    apply(target, receiver, args) {
      return Reflect.apply(target, receiver, args);
    },
  }));
  assertEquals(17, proxyWrapped(2));

  const wrappedBuiltin = keep(
      'wrappedBuiltin', shadowRealm.evaluate('String.prototype.substring'));
  assertEquals('adow', wrappedBuiltin.call('shadow', 2));

  const wrappedThrower = keep('wrappedThrower', shadowRealm.evaluate(`() => {
    throw new Error('heap-dump-shadowrealm');
  }`));
  const copiedErrors = keep('copiedErrors', []);
  try {
    wrappedThrower();
  } catch (error) {
    copiedErrors.push(error);
    assertInstanceof(error, TypeError);
    assertEquals(
        'WrappedFunction threw (Error: heap-dump-shadowrealm)', error.message);
  }
  assertEquals(1, copiedErrors.length);

  const evaluateErrors = keep('evaluateErrors', []);
  try {
    shadowRealm.evaluate(`
      const err = new Error('evaluate-copy');
      Object.defineProperty(err, 'message', {
        get() { return 'hidden-message'; }
      });
      throw err;
    `);
  } catch (error) {
    evaluateErrors.push(error);
    assertInstanceof(error, TypeError);
    assertEquals('ShadowRealm evaluate threw (Error)', error.message);
  }
  assertEquals(1, evaluateErrors.length);

  keep('catalog', catalog);
})();
