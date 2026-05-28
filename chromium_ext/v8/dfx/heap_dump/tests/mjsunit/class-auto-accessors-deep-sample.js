// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators

// Keep class auto-accessor, private-name, computed-name, and reparsing error
// shapes alive for heap dump smoke. This follows V8 decorators mjsunit
// coverage, but retains the classes, descriptors, closures, instances, and
// thrown error objects as graph roots.

(function() {
  'use strict';

  const catalog = {};
  globalThis.__heapDumpClassAutoAccessors = catalog;

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

  const computedKey = keep('computedKey', Symbol.for('heap-dump-accessor'));
  const initializerPayload = keep('initializerPayload', {
    publicValue() {
      return 'public-value';
    },
    privateValue() {
      return 'private-value';
    },
    staticValue() {
      return 'static-value';
    },
    computedValue() {
      return 'computed-value';
    },
  });

  class AutoAccessorBase {
    accessor publicSlot = initializerPayload.publicValue;
    accessor #privateSlot = initializerPayload.privateValue;
    accessor [computedKey] = initializerPayload.computedValue;
    static accessor staticSlot = initializerPayload.staticValue;

    readPrivate() {
      return this.#privateSlot;
    }

    writePrivate(value) {
      this.#privateSlot = value;
      return this.#privateSlot;
    }
  }

  class AutoAccessorDerived extends AutoAccessorBase {
    accessor derivedSlot = function derivedInitializer() {
      return 'derived-value';
    };

    getAll() {
      return {
        publicSlot: this.publicSlot,
        privateSlot: this.readPrivate(),
        computedSlot: this[computedKey],
        derivedSlot: this.derivedSlot,
      };
    }
  }

  const baseInstance = keep('baseInstance', new AutoAccessorBase());
  const derivedInstance = keep('derivedInstance', new AutoAccessorDerived());
  keep('autoAccessorBaseClass', AutoAccessorBase);
  keep('autoAccessorDerivedClass', AutoAccessorDerived);
  keep('prototypeDescriptors', {
    publicSlot:
        Object.getOwnPropertyDescriptor(AutoAccessorBase.prototype, 'publicSlot'),
    computedSlot:
        Object.getOwnPropertyDescriptor(AutoAccessorBase.prototype, computedKey),
    derivedSlot: Object.getOwnPropertyDescriptor(
        AutoAccessorDerived.prototype, 'derivedSlot'),
  });
  keep('staticDescriptor',
       Object.getOwnPropertyDescriptor(AutoAccessorBase, 'staticSlot'));

  baseInstance.publicSlot = keep('mutatedPublicFunction', function mutatedPublic() {
    return 'mutated-public';
  });
  baseInstance[computedKey] = keep('mutatedComputedFunction',
                                  function mutatedComputed() {
                                    return 'mutated-computed';
                                  });
  keep('privateWriteResult',
       baseInstance.writePrivate(function mutatedPrivate() {
         return 'mutated-private';
       }));
  AutoAccessorBase.staticSlot = keep(
      'mutatedStaticFunction',
      function mutatedStatic() {
        return 'mutated-static';
      });

  const wrapperClass = keep('wrapperClass', ((arg = (function wrapper() {
    class WrappedAutoAccessor {
      accessor #wrappedPrivate = function wrappedPrivateFunction() {};
      accessor [Symbol.for('heap-dump-wrapped-computed')] =
          function wrappedComputedFunction() {};

      readWrappedPrivate() {
        return this.#wrappedPrivate;
      }
    }
    return WrappedAutoAccessor;
  })()) => arg)());
  const wrapperInstance = keep('wrapperInstance', new wrapperClass());

  const reparsingError = keepError('autoAccessorReparsingError', () => {
    const spreadSource = null;
    class ReparsingAutoAccessor {
      accessor value = (() => {})('force-reparse', ...spreadSource);
    }
    return new ReparsingAutoAccessor();
  });

  const publicDescriptor = catalog.prototypeDescriptors.publicSlot;
  const computedDescriptor = catalog.prototypeDescriptors.computedSlot;
  const staticDescriptor = catalog.staticDescriptor;

  assertEquals('function', typeof publicDescriptor.get);
  assertEquals('function', typeof publicDescriptor.set);
  assertEquals('function', typeof computedDescriptor.get);
  assertEquals('function', typeof staticDescriptor.get);
  assertEquals('mutated-public', baseInstance.publicSlot());
  assertEquals('mutated-computed', baseInstance[computedKey]());
  assertEquals('mutated-private', baseInstance.readPrivate()());
  assertEquals('mutated-static', AutoAccessorBase.staticSlot());
  assertEquals('derived-value', derivedInstance.getAll().derivedSlot());
  assertEquals('function', typeof wrapperInstance.readWrappedPrivate());
  assertTrue(reparsingError instanceof TypeError);
})();
