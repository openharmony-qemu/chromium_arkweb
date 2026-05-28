// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep Array.prototype.join, Array.prototype.toString, and
// Array.prototype.toLocaleString side-effect paths alive for heap-dump smoke
// coverage. This follows stable portions of V8's array-join.js,
// array-join-*-side-effects.js, and array-tostring.js mjsunit tests.

const heapDumpArrayJoinState = {
  arrays: [],
  objects: [],
  proxies: [],
  logs: [],
  errors: [],
  results: [],
};

function keepJoin(name, value) {
  heapDumpArrayJoinState[name] = value;
  return value;
}

function keepResult(value) {
  heapDumpArrayJoinState.results.push(value);
  return value;
}

(function basicNestedHoleyAndCycleJoin() {
  const objectElement = {
    toString() {
      return 'obj';
    },
  };
  const holey = [1, 1.5, objectElement, , null, undefined];
  assertSame('1|1.5|obj|||', keepResult(holey.join('|')));

  const nested = [[1, 2], 3, 4, [5, 6]];
  assertEquals('1,2*3*4*5,6', keepResult(nested.join('*')));
  nested.push(nested);
  assertEquals('1,2*3*4*5,6*', keepResult(nested.join('*')));

  heapDumpArrayJoinState.arrays.push(holey, nested);
  heapDumpArrayJoinState.objects.push(objectElement);
})();

(function separatorToStringSideEffects() {
  class SeparatorError {}
  const throwingSeparatorArray = [1, 2];
  let throwingCallCount = 0;
  const throwingSeparator = {
    toString() {
      throwingCallCount++;
      throw new SeparatorError();
    },
  };
  const error = assertThrows(
      () => throwingSeparatorArray.join(throwingSeparator), SeparatorError);
  assertSame(1, throwingCallCount);
  assertSame('1,2', throwingSeparatorArray.join());

  const recursive = [1, 2, 3];
  let recursiveCallCount = 0;
  const recursiveSeparator = {
    toString() {
      recursiveCallCount++;
      return recursive.join('-');
    },
  };
  assertSame('11-2-321-2-33',
             keepResult(recursive.join(recursiveSeparator)));
  assertSame(1, recursiveCallCount);

  const growing = [1, 2, 3];
  let growingCallCount = 0;
  const growingSeparator = {
    toString() {
      growingCallCount++;
      growing.push(4);
      return '|';
    },
  };
  assertSame('1|2|3', keepResult(growing.join(growingSeparator)));
  assertSame('1|2|3|4', keepResult(growing.join('|')));
  assertSame(1, growingCallCount);

  heapDumpArrayJoinState.arrays.push(
      throwingSeparatorArray, recursive, growing);
  heapDumpArrayJoinState.objects.push(
      throwingSeparator, recursiveSeparator, growingSeparator);
  heapDumpArrayJoinState.errors.push(error);
})();

(function elementToStringSideEffects() {
  const mutatedObjects = [];
  const objectElements = [
    {
      toString() {
        objectElements[1] = 'two';
        objectElements[2] = {tail: true};
        mutatedObjects.push(objectElements[2]);
        return 'one';
      },
    },
    2,
    3,
  ];
  assertSame('one,two,[object Object]', keepResult(objectElements.join()));
  assertSame('one,two,[object Object]', keepResult(objectElements.join()));

  const dictionary = [];
  dictionary[8191] = {
    toString() {
      dictionary[8192] = 'tail';
      return 'head';
    },
  };
  dictionary[8192] = 'old-tail';
  assertTrue(%HasDictionaryElements(dictionary));
  assertSame('headtail', keepResult(dictionary.join('')));

  heapDumpArrayJoinState.arrays.push(objectElements, dictionary);
  heapDumpArrayJoinState.objects.push(...mutatedObjects, dictionary[8191]);
})();

(function proxyArrayLikeAccessOrder() {
  const log = [];
  const proxy = new Proxy({length: 3, 0: 'a', 1: 'b'}, {
    get(target, key, receiver) {
      log.push(key);
      return Reflect.get(target, key, receiver);
    },
  });
  assertEquals('a,b,', keepResult(Array.prototype.join.call(proxy)));
  assertEquals(['length', '0', '1', '2'], log);

  heapDumpArrayJoinState.proxies.push(proxy);
  heapDumpArrayJoinState.logs.push(log);
})();

(function arrayToStringAndLocaleString() {
  const customJoinArray = [1, 2, 3];
  customJoinArray.join = function() {
    return 'custom-join';
  };
  assertSame('custom-join', keepResult(customJoinArray.toString()));

  const nonCallableJoinArray = [1, 2, 3];
  nonCallableJoinArray.join = 'not callable';
  assertSame('[object Array]', keepResult(nonCallableJoinArray.toString()));

  const arrayLike = {
    length: 3,
    0: 1,
    1: 2,
    2: 3,
    toString: Array.prototype.toString,
    join: Array.prototype.join,
  };
  assertSame('1,2,3', keepResult(arrayLike.toString()));

  const localeElement = {
    toLocaleString() {
      return 'XX';
    },
  };
  const localeArray = [1, localeElement, null, undefined, , 5];
  assertSame('1,XX,,,,5', keepResult(localeArray.toLocaleString()));
  const localeErrorArray = [1, {toLocaleString: 'not callable'}, 3];
  const localeError = assertThrows(
      () => localeErrorArray.toLocaleString(), TypeError);

  heapDumpArrayJoinState.arrays.push(
      customJoinArray, nonCallableJoinArray, localeArray, localeErrorArray);
  heapDumpArrayJoinState.objects.push(arrayLike, localeElement);
  heapDumpArrayJoinState.errors.push(localeError);
})();

keepJoin('summary', {
  arrayCount: heapDumpArrayJoinState.arrays.length,
  objectCount: heapDumpArrayJoinState.objects.length,
  proxyCount: heapDumpArrayJoinState.proxies.length,
  logCount: heapDumpArrayJoinState.logs.length,
  errorCount: heapDumpArrayJoinState.errors.length,
  resultCount: heapDumpArrayJoinState.results.length,
});
