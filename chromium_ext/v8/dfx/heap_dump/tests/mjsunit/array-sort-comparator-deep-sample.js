// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep Array.prototype.sort comparator, default ToString, sparse, generic, and
// accessor paths alive for heap-dump smoke coverage. This follows stable parts
// of V8's array-sort.js and array-to-sorted.js tests while retaining the object
// graph after sorting.

const heapDumpArraySortState = {
  arrays: [],
  objects: [],
  comparators: [],
  logs: [],
  errors: [],
  results: [],
};

function keepSort(name, value) {
  heapDumpArraySortState[name] = value;
  return value;
}

function keepResult(value) {
  heapDumpArraySortState.results.push(value);
  return value;
}

function makeComparator(label, compare) {
  const log = [];
  const comparator = function(left, right) {
    log.push({label, left, right});
    return compare(left, right);
  };
  comparator.log = log;
  heapDumpArraySortState.comparators.push(comparator);
  heapDumpArraySortState.logs.push(log);
  return comparator;
}

(function numberStringAndObjectSorts() {
  const numericDefault = keepResult([200, 45, 7].sort());
  assertArrayEquals([200, 45, 7], numericDefault);

  const numeric = [200, 45, 7, 1000000000, 9];
  numeric.sort(makeComparator('numeric', (left, right) => left - right));
  assertArrayEquals([7, 9, 45, 200, 1000000000], numeric);

  const strings = ['cc', 'c', 'aa', 'a', 'bb', 'b', 'ab', 'ac'];
  strings.sort();
  assertArrayEquals(['a', 'aa', 'ab', 'ac', 'b', 'bb', 'c', 'cc'], strings);

  const objectLog = [];
  const objects = [
    {rank: 3, toString() { objectLog.push('c'); return 'c'; }},
    {rank: 1, toString() { objectLog.push('a'); return 'a'; }},
    {rank: 2, toString() { objectLog.push('b'); return 'b'; }},
  ];
  objects.sort();
  assertEquals([1, 2, 3], objects.map((object) => object.rank));

  heapDumpArraySortState.arrays.push(numericDefault, numeric, strings, objects);
  heapDumpArraySortState.objects.push(...objects);
  heapDumpArraySortState.logs.push(objectLog);
})();

(function sparseUndefinedAndDictionarySorts() {
  const sparse = [];
  sparse[4] = '18';
  sparse[10] = '12';
  sparse.sort();
  assertEquals(11, sparse.length);
  assertEquals('12', sparse[0]);
  assertEquals('18', sparse[1]);
  assertFalse(2 in sparse);

  const undefinedValues = [3, void 0, 2, , 1];
  undefinedValues.sort();
  assertArrayEquals([1, 2, 3, void 0, ,], undefinedValues);

  const dictionary = [];
  dictionary[9000] = 9;
  dictionary[10] = 2;
  dictionary[100] = undefined;
  assertTrue(%HasDictionaryElements(dictionary));
  dictionary.sort(makeComparator('dictionary', (left, right) => left - right));
  assertEquals(9001, dictionary.length);
  assertEquals(2, dictionary[0]);
  assertEquals(9, dictionary[1]);
  assertEquals(undefined, dictionary[2]);
  assertTrue(2 in dictionary);
  assertFalse(3 in dictionary);

  heapDumpArraySortState.arrays.push(sparse, undefinedValues, dictionary);
})();

(function genericAccessorsAndPrototypeElements() {
  const generic = {length: 1000, 0: 42, 10: 37, 100: undefined, 999: null};
  Array.prototype.sort.call(generic);
  assertEquals(1000, generic.length);
  assertEquals(37, generic[0]);
  assertEquals(42, generic[1]);
  assertEquals(null, generic[2]);
  assertEquals(undefined, generic[3]);
  assertTrue(3 in generic);
  assertFalse(4 in generic);

  const accessorLog = [];
  const accessorObject = {length: 4, 0: 42, 3: 37};
  Object.defineProperty(accessorObject, '1', {
    get() {
      accessorLog.push('get-1');
      return this.foo;
    },
    set(value) {
      accessorLog.push('set-1');
      this.foo = value;
    },
    configurable: true,
  });
  accessorObject[1] = 49;
  Array.prototype.sort.call(accessorObject);
  assertEquals(37, accessorObject[0]);
  assertEquals(42, accessorObject[1]);
  assertEquals(49, accessorObject[2]);
  assertFalse(3 in accessorObject);

  const proto = {2: 'proto'};
  const prototypeArray = new Array(4);
  prototypeArray[3] = 'own';
  prototypeArray.__proto__ = proto;
  Array.prototype.sort.call(prototypeArray);
  assertEquals('own', prototypeArray[0]);
  assertEquals('proto', prototypeArray[1]);

  heapDumpArraySortState.objects.push(generic, accessorObject, proto);
  heapDumpArraySortState.arrays.push(prototypeArray);
  heapDumpArraySortState.logs.push(accessorLog);
})();

(function comparatorMutationAndErrors() {
  const mutating = [3, 1, 4, 2];
  const mutatingComparator = makeComparator('mutating', (left, right) => {
    mutating.pop();
    return left - right;
  });
  mutating.sort(mutatingComparator);
  assertArrayEquals([1, 2, 3, 4], mutating);
  assertTrue(mutatingComparator.log.length > 0);

  class SortComparatorError extends Error {}
  const errorArray = [3, 2, 1];
  const error = assertThrows(() => errorArray.sort(makeComparator(
      'throwing', (left, right) => {
        if (left === 2 || right === 2) {
          throw new SortComparatorError('heap dump sort comparator');
        }
        return left - right;
      })), SortComparatorError);
  assertEquals(3, errorArray.length);

  const typeError = assertThrows(() => [1, 2].sort({notCallable: true}),
                                TypeError);

  heapDumpArraySortState.arrays.push(mutating, errorArray);
  heapDumpArraySortState.errors.push(error, typeError);
})();

keepSort('summary', {
  arrayCount: heapDumpArraySortState.arrays.length,
  objectCount: heapDumpArraySortState.objects.length,
  comparatorCount: heapDumpArraySortState.comparators.length,
  logCount: heapDumpArraySortState.logs.length,
  errorCount: heapDumpArraySortState.errors.length,
  resultCount: heapDumpArraySortState.results.length,
});
