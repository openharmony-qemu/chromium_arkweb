// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep proxy invariant errors and private-brand check shapes alive for heap
// dump smoke. This complements proxy-reflect-deep-sample.js and
// ic-super-private-deep-sample.js, which cover successful proxy/private paths.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, callback) {
    try {
      callback();
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  const trapLog = keep('trapLog', []);

  const fixedTarget = keep('fixedTarget', {});
  Object.defineProperty(fixedTarget, 'fixed', {
    configurable: false,
    enumerable: true,
    writable: false,
    value: {kind: 'fixed-value'},
  });
  Object.preventExtensions(fixedTarget);

  const invariantProxy = keep('invariantProxy', new Proxy(fixedTarget, {
    getOwnPropertyDescriptor(target, key) {
      trapLog.push(['invariantGetOwnPropertyDescriptor', key]);
      if (key === 'fixed') return undefined;
      return Reflect.getOwnPropertyDescriptor(target, key);
    },
    ownKeys(target) {
      trapLog.push(['invariantOwnKeys']);
      return [];
    },
    getPrototypeOf(target) {
      trapLog.push(['invariantGetPrototypeOf']);
      return {fakePrototype: true};
    },
    isExtensible(target) {
      trapLog.push(['invariantIsExtensible']);
      return true;
    },
  }));

  keepError('descriptorInvariantError',
            () => Object.getOwnPropertyDescriptor(invariantProxy, 'fixed'));
  keepError('ownKeysInvariantError', () => Reflect.ownKeys(invariantProxy));
  keepError('prototypeInvariantError',
            () => Object.getPrototypeOf(invariantProxy));
  keepError('isExtensibleInvariantError',
            () => Object.isExtensible(invariantProxy));

  const extensibleTarget = keep('extensibleTarget', {value: 1});
  const defineProxy = keep('defineProxy', new Proxy(extensibleTarget, {
    defineProperty(target, key, descriptor) {
      trapLog.push(['defineProperty', key, descriptor.configurable]);
      return false;
    },
    deleteProperty(target, key) {
      trapLog.push(['deleteProperty', key]);
      return false;
    },
    preventExtensions(target) {
      trapLog.push(['preventExtensions']);
      return true;
    },
    setPrototypeOf(target, prototype) {
      trapLog.push(['setPrototypeOf', prototype && prototype.kind]);
      return false;
    },
  }));
  Object.defineProperty(extensibleTarget, 'locked', {
    configurable: false,
    value: {kind: 'locked'},
  });

  keepError('definePropertyFalseError', () => Object.defineProperty(
      defineProxy, 'newProperty', {value: {kind: 'new'}}));
  keepError('deletePropertyFalseError', () => {
    delete defineProxy.locked;
  });
  keepError('preventExtensionsInvariantError',
            () => Object.preventExtensions(defineProxy));
  keepError('setPrototypeFalseError',
            () => Object.setPrototypeOf(defineProxy, {kind: 'proto'}));

  function callableTarget(value) {
    return value + 1;
  }
  class ConstructedTarget {
    constructor(value) {
      this.value = value;
    }
  }

  const callableErrorProxy = keep('callableErrorProxy', new Proxy(callableTarget, {
    apply(target, receiver, args) {
      trapLog.push(['applyThrow', args.length]);
      throw new RangeError('heap-dump-proxy-apply');
    },
    construct(target, args, newTarget) {
      trapLog.push(['constructNonObject', args.length]);
      return 17;
    },
  }));
  const constructErrorProxy = keep(
      'constructErrorProxy',
      new Proxy(ConstructedTarget, {
        construct(target, args, newTarget) {
          trapLog.push(['constructThrow', args.length]);
          throw new SyntaxError('heap-dump-proxy-construct');
        },
      }));

  keepError('applyThrowError', () => callableErrorProxy(5));
  keepError('constructNonObjectError', () => new callableErrorProxy(6));
  keepError('constructThrowError', () => new constructErrorProxy(7));

  const revokedPairs = keep('revokedPairs', []);
  for (let i = 0; i < 6; ++i) {
    const pair = Proxy.revocable({index: i}, {
      get(target, key, receiver) {
        trapLog.push(['revokedGetBeforeRevoke', i, key]);
        return Reflect.get(target, key, receiver);
      },
    });
    keep('revokedBeforeValue' + i, pair.proxy.index);
    pair.revoke();
    keepError('revokedGetError' + i, () => pair.proxy.index);
    keepError('revokedSetError' + i, () => {
      pair.proxy.extra = i;
    });
    keepError('revokedOwnKeysError' + i, () => Reflect.ownKeys(pair.proxy));
    revokedPairs.push(pair);
  }

  class BrandCarrier {
    #field;
    static #staticField = {kind: 'static-private'};

    constructor(value) {
      this.#field = {kind: 'private-field', value};
    }

    static hasBrand(value) {
      return #field in value;
    }

    static readStatic() {
      return this.#staticField;
    }

    readField() {
      return this.#field;
    }

    writeField(value) {
      this.#field = {kind: 'private-field-written', value};
      return this.#field;
    }

    #privateMethod(delta) {
      return this.#field.value + delta;
    }

    callPrivateMethod(delta) {
      return this.#privateMethod(delta);
    }
  }

  const brandObjects = keep('brandObjects', []);
  const brandResults = keep('brandResults', []);
  for (let i = 0; i < 16; ++i) {
    const carrier = new BrandCarrier(i);
    const proxy = new Proxy(carrier, {
      get(target, key, receiver) {
        trapLog.push(['brandProxyGet', key]);
        return Reflect.get(target, key, receiver);
      },
    });
    brandObjects.push({carrier, proxy, plain: {i}});
    brandResults.push({
      hasCarrier: BrandCarrier.hasBrand(carrier),
      hasProxy: BrandCarrier.hasBrand(proxy),
      hasPlain: BrandCarrier.hasBrand({i}),
      read: carrier.readField(),
      write: carrier.writeField(i + 100),
      method: carrier.callPrivateMethod(1),
    });
  }

  keepError('privateReadWrongReceiverError',
            () => BrandCarrier.prototype.readField.call({}));
  keepError('privateWriteWrongReceiverError',
            () => BrandCarrier.prototype.writeField.call({}, 1));
  keepError('privateMethodWrongReceiverError',
            () => BrandCarrier.prototype.callPrivateMethod.call({}, 1));
  keepError('privateStaticWrongReceiverError',
            () => BrandCarrier.readStatic.call(function NotBrandCarrier() {}));

  keep('BrandCarrier', BrandCarrier);
  keep('staticPrivateValue', BrandCarrier.readStatic());
  keep('errorNames', Object.keys(catalog).filter((key) => /Error\d*$/.test(key)));

  assertTrue(catalog.descriptorInvariantError instanceof TypeError);
  assertTrue(catalog.ownKeysInvariantError instanceof TypeError);
  assertTrue(catalog.applyThrowError instanceof RangeError);
  assertTrue(catalog.constructThrowError instanceof SyntaxError);
  assertEquals(6, revokedPairs.length);
  assertTrue(catalog.revokedGetError0 instanceof TypeError);
  assertTrue(brandResults[0].hasCarrier);
  assertFalse(brandResults[0].hasProxy);
  assertFalse(brandResults[0].hasPlain);
  assertEquals('private-field', brandResults[0].read.kind);
  assertEquals('private-field-written', brandResults[0].write.kind);
  assertEquals(101, brandResults[0].method);
  assertTrue(catalog.privateReadWrongReceiverError instanceof TypeError);
  assertTrue(catalog.privateStaticWrongReceiverError instanceof TypeError);

  globalThis.heapDumpProxyPrivateBrandErrorsDeepSample = catalog;
})();
