// Heap-dump shape sample for builtin protector/property-cell invalidation paths.
//
// This borrows stable mutation patterns from V8's protector-cell mjsunit tests
// without using native syntax. Each mutation keeps the mutated owner, previous
// value, replacement, and resulting objects alive for heap dump inspection.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function recordDescriptor(name, owner, property) {
    keep(name, {
      owner,
      property,
      descriptor: Object.getOwnPropertyDescriptor(owner, property),
    });
  }

  recordDescriptor('arrayPrototypeConstructorBefore',
                   Array.prototype, 'constructor');
  const arrayConstructorReplacement = keep(
      'arrayConstructorReplacement',
      function HeapDumpArrayConstructorReplacement(...values) {
        return {kind: 'array-constructor-replacement', values};
      });
  Object.defineProperty(Array.prototype, 'constructor', {
    value: arrayConstructorReplacement,
    writable: true,
    configurable: true,
  });
  recordDescriptor('arrayPrototypeConstructorAfter',
                   Array.prototype, 'constructor');
  keep('arraySpeciesResult', [1, 2, 3].map((value) => value + 1));

  const promiseForConstructorMutation =
      keep('promiseForConstructorMutation', new Promise(() => {}));
  recordDescriptor('promiseConstructorBefore',
                   promiseForConstructorMutation, 'constructor');
  Object.defineProperty(promiseForConstructorMutation, 'constructor', {
    value: keep('promiseConstructorReplacement',
                function HeapDumpPromiseReplacement(executor) {
                  return new Promise(executor);
                }),
    writable: true,
    configurable: true,
  });
  recordDescriptor('promiseConstructorAfter',
                   promiseForConstructorMutation, 'constructor');
  keep('promiseThenAfterConstructorMutation',
       promiseForConstructorMutation.then(() => ({kind: 'promise-then'})));

  const regexpForConstructorMutation =
      keep('regexpForConstructorMutation', /protector-(?<cell>\w+)/g);
  recordDescriptor('regexpConstructorBefore',
                   regexpForConstructorMutation, 'constructor');
  Object.defineProperty(regexpForConstructorMutation, 'constructor', {
    value: keep('regexpConstructorReplacement',
                function HeapDumpRegExpReplacement(pattern, flags) {
                  return new RegExp(pattern, flags);
                }),
    writable: true,
    configurable: true,
  });
  recordDescriptor('regexpConstructorAfter',
                   regexpForConstructorMutation, 'constructor');
  regexpForConstructorMutation.lastIndex = 0;
  keep('regexpMatchAfterConstructorMutation',
       regexpForConstructorMutation.exec('protector-cell'));

  const typedArrayForConstructorMutation =
      keep('typedArrayForConstructorMutation', new Uint8Array([1, 2, 3, 4]));
  recordDescriptor('typedArrayConstructorBefore',
                   typedArrayForConstructorMutation, 'constructor');
  Object.defineProperty(typedArrayForConstructorMutation, 'constructor', {
    value: keep('typedArrayConstructorReplacement',
                function HeapDumpTypedArrayReplacement(values) {
                  return new Uint8Array(values);
                }),
    writable: true,
    configurable: true,
  });
  recordDescriptor('typedArrayConstructorAfter',
                   typedArrayForConstructorMutation, 'constructor');
  keep('typedArraySliceAfterConstructorMutation',
       typedArrayForConstructorMutation.slice(1, 3));

  const oldArrayIterator = keep(
      'oldArrayIterator', Array.prototype[Symbol.iterator]);
  const customArrayIteratorLog = keep('customArrayIteratorLog', []);
  Object.defineProperty(Array.prototype, Symbol.iterator, {
    value: function* heapDumpArrayIterator() {
      customArrayIteratorLog.push(this.length);
      for (let i = 0; i < this.length; ++i) {
        yield {kind: 'custom-array-iterator-value', value: this[i]};
      }
    },
    writable: true,
    configurable: true,
  });
  const customArrayIterator = keep('customArrayIterator', [5, 6][Symbol.iterator]());
  keep('customArrayIteratorFirst', customArrayIterator.next());

  const oldMapIteratorNext =
      keep('oldMapIteratorNext', MapIteratorPrototype().next);
  Object.defineProperty(MapIteratorPrototype(), 'next', {
    value: function heapDumpMapIteratorNext() {
      const result = oldMapIteratorNext.call(this);
      if (!result.done) {
        return {
          value: {kind: 'custom-map-iterator-result', entry: result.value},
          done: false,
        };
      }
      return result;
    },
    writable: true,
    configurable: true,
  });
  const mapIterator = keep(
      'mapIteratorAfterNextMutation',
      new Map([['a', {kind: 'map-value'}]]).entries());
  keep('mapIteratorMutatedNextResult', mapIterator.next());

  const oldSetIteratorNext =
      keep('oldSetIteratorNext', SetIteratorPrototype().next);
  Object.defineProperty(SetIteratorPrototype(), 'next', {
    value: function heapDumpSetIteratorNext() {
      const result = oldSetIteratorNext.call(this);
      if (!result.done) {
        return {
          value: {kind: 'custom-set-iterator-result', value: result.value},
          done: false,
        };
      }
      return result;
    },
    writable: true,
    configurable: true,
  });
  const setIterator = keep(
      'setIteratorAfterNextMutation',
      new Set([{kind: 'set-value'}]).values());
  keep('setIteratorMutatedNextResult', setIterator.next());

  assertEquals(2, catalog.arraySpeciesResult[0]);
  assertEquals('cell', catalog.regexpMatchAfterConstructorMutation.groups.cell);
  assertEquals(2, catalog.typedArraySliceAfterConstructorMutation.length);
  assertEquals('custom-array-iterator-value',
               catalog.customArrayIteratorFirst.value.kind);
  assertEquals('custom-map-iterator-result',
               catalog.mapIteratorMutatedNextResult.value.kind);
  assertEquals('custom-set-iterator-result',
               catalog.setIteratorMutatedNextResult.value.kind);

  globalThis.__heapDumpProtectorCellsDeepKeepalive = catalog;

  function MapIteratorPrototype() {
    return Object.getPrototypeOf(new Map().entries());
  }

  function SetIteratorPrototype() {
    return Object.getPrototypeOf(new Set().values());
  }
})();
