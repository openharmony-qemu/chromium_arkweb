// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep toLocaleString result/error paths alive for heap dump smoke. This
// follows V8's Array.prototype.toLocaleString cycle/error mjsunit and a
// trimmed BigInt toLocaleString regression pattern while retaining the source
// arrays, callable/getter elements, result strings, and thrown errors.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, callback) {
    try {
      callback();
    } catch (error) {
      return keep(name, error);
    }
    throw new Error(name + ' did not throw');
  }

  const cycleArray = keep('cycleArray', []);
  const cycleElement = keep('cycleElement', {
    toLocaleString() {
      return [1, cycleArray];
    },
  });
  cycleArray[0] = cycleElement;
  keep('cycleFirstResult', cycleArray.toLocaleString());
  keep('cycleSecondResult', cycleArray.toLocaleString());

  function CallableThrowError() {}
  const callableThrowArray = keep('callableThrowArray', []);
  const callableThrowElement = keep('callableThrowElement', {
    toLocaleString() {
      throw new CallableThrowError();
    },
  });
  callableThrowArray[0] = callableThrowElement;
  keep('callableThrowError',
       keepError('callableThrowError',
                 () => callableThrowArray.toLocaleString()));
  callableThrowArray[0] = keep('callableRecoveryElement', {
    toLocaleString() {
      return 1;
    },
  });
  keep('callableRecoveryResult', callableThrowArray.toLocaleString());

  function GetterThrowError() {}
  const getterThrowArray = keep('getterThrowArray', []);
  const getterThrowElement = keep('getterThrowElement', {
    get toLocaleString() {
      throw new GetterThrowError();
    },
  });
  getterThrowArray[0] = getterThrowElement;
  keep('getterThrowError',
       keepError('getterThrowError', () => getterThrowArray.toLocaleString()));
  getterThrowArray[0] = keep('getterRecoveryElement', {
    toLocaleString() {
      return 2;
    },
  });
  keep('getterRecoveryResult', getterThrowArray.toLocaleString());

  const notCallableArray = keep('notCallableArray', []);
  notCallableArray[0] = keep('notCallableElement', {toLocaleString: 7});
  keep('notCallableError',
       keepError('notCallableError', () => notCallableArray.toLocaleString()));
  notCallableArray[0] = keep('notCallableRecoveryElement', {
    toLocaleString() {
      return 3;
    },
  });
  keep('notCallableRecoveryResult', notCallableArray.toLocaleString());

  const objectArray = keep('objectArray', [
    {
      toLocaleString() {
        return 'alpha';
      },
    },
    null,
    undefined,
    {
      toLocaleString() {
        return 'omega';
      },
    },
  ]);
  keep('objectArrayResult', objectArray.toLocaleString());

  const nestedArray = keep('nestedArray', [
    [1, 2],
    {
      toLocaleString() {
        return ['nested', cycleArray];
      },
    },
  ]);
  keep('nestedArrayResult', nestedArray.toLocaleString());

  const bigIntInputs = keep('bigIntInputs', [
    '1000000000000000000000000000000000000000000000'.repeat(2) + '0',
    '100000000000000000000000000000000000000000000'.repeat(2) + '0',
    ('1' + '0'.repeat(38)).repeat(4),
  ]);
  const bigInts = keep('bigInts', bigIntInputs.map((input) => BigInt(input)));
  keep('bigIntLocaleStrings',
       bigInts.map((value) => value.toLocaleString('en-US')));
  keep('bigIntPlainStrings', bigInts.map((value) => value.toString()));

  assertEquals('1,', catalog.cycleFirstResult);
  assertEquals('1,', catalog.cycleSecondResult);
  assertTrue(catalog.callableThrowError instanceof CallableThrowError);
  assertEquals('1', catalog.callableRecoveryResult);
  assertTrue(catalog.getterThrowError instanceof GetterThrowError);
  assertEquals('2', catalog.getterRecoveryResult);
  assertTrue(catalog.notCallableError instanceof TypeError);
  assertEquals('3', catalog.notCallableRecoveryResult);
  assertEquals('alpha,,,omega', catalog.objectArrayResult);
  assertTrue(catalog.nestedArrayResult.includes('nested'));
  assertEquals(bigIntInputs[0], catalog.bigIntPlainStrings[0]);
  if (catalog.bigIntLocaleStrings[0] !== catalog.bigIntPlainStrings[0]) {
    assertTrue(catalog.bigIntLocaleStrings[0].includes(','));
  }

  globalThis.heapDumpToLocaleStringVariantsDeepSample = catalog;
})();
