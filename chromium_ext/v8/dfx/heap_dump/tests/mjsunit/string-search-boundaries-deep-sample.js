// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep string search boundary result shapes alive for heap dump smoke. This
// follows V8's string-lastindexof.js and ES6 string includes/startsWith/endsWith
// mjsunit paths while retaining result records, two-byte inputs, coercion logs,
// wrapper objects, and representative TypeError objects.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (error) {
      keep(name, error);
      return error;
    }
    return null;
  }

  const base = keep('base', 'test test test');
  const maxDouble = 1.7976931348623157e+308;
  const minDouble = -maxDouble;
  const maxSmi = Math.pow(2, 30) - 1;
  const minSmi = -Math.pow(2, 30);
  const positions = keep('positions', [
    Infinity,
    maxDouble,
    maxSmi,
    base.length * 2,
    15,
    14,
    10,
    9,
    6,
    5,
    4,
    0,
    -1,
    -base.length,
    minSmi,
    minDouble,
    -Infinity,
    NaN,
  ]);
  const lastIndexResults = keep(
      'lastIndexResults',
      positions.map((position) => ({
        position,
        result: base.lastIndexOf('test', position),
        empty: base.lastIndexOf('', position),
      })));

  const searchStrings = keep('searchStrings', {
    longNonMatch: 'overlong string that does not match',
    longAlmostMatch: 'test test test!',
    nonInitialMatch: 'est',
    regexpText: 'asdf[a-z]+(asdf)?',
  });
  const boundaryResults = keep('boundaryResults', {
    missing: base.lastIndexOf('notpresent'),
    undefinedSearch: base.lastIndexOf(),
    longNonMatch: base.lastIndexOf(searchStrings.longNonMatch),
    longAlmostMatch: base.lastIndexOf(searchStrings.longAlmostMatch),
    nonInitialAtZero: base.lastIndexOf(searchStrings.nonInitialMatch, 0),
    regexpTextClass: searchStrings.regexpText.lastIndexOf('[a-z]+'),
    regexpTextOptional: searchStrings.regexpText.lastIndexOf('(asdf)?'),
  });

  const twoByte = keep('twoByte', '\u039a\u0391\u03a3\u03a3\u0395');
  const georgian = keep(
      'georgian', '\u10D0\u10D1\u10D2\u10D3\u10D4\u10D5\u10D6\u10D7');
  const predicateResults = keep('predicateResults', {
    includesSigma: twoByte.includes('\u03a3'),
    includesSecondSigma: twoByte.includes('\u03a3', 3),
    includesMissingBeta: twoByte.includes('\u0392'),
    startsWithGeorgianPrefix: georgian.startsWith('\u10D0\u10D1\u10D2'),
    startsWithMissingGeorgian: georgian.startsWith('\u10D8'),
    endsWithGeorgianSuffix: georgian.endsWith('\u10D4\u10D5\u10D6\u10D7'),
    endsWithMissingGeorgian: georgian.endsWith('\u10D0'),
    includesNegativeInfinity: 'abc'.includes('ab', -Infinity),
    includesInfinity: 'abc'.includes('a', Infinity),
    startsInfinity: 'abc'.startsWith('a', Infinity),
    endsAtLength: 'abc'.endsWith('bc', 3),
  });

  const coercionLog = keep('coercionLog', []);
  const coercibleReceiver = keep('coercibleReceiver', {
    toString() {
      coercionLog.push('receiver');
      return 'coercion-target';
    },
    includes: String.prototype.includes,
    startsWith: String.prototype.startsWith,
    endsWith: String.prototype.endsWith,
  });
  const coercibleSearch = keep('coercibleSearch', {
    toString() {
      coercionLog.push('search');
      return 'target';
    },
  });
  const coerciblePosition = keep('coerciblePosition', {
    valueOf() {
      coercionLog.push('position');
      return 9;
    },
  });
  const coercionResults = keep('coercionResults', {
    includes: coercibleReceiver.includes(coercibleSearch, coerciblePosition),
    startsWith: coercibleReceiver.startsWith('coercion'),
    endsWith: coercibleReceiver.endsWith('target'),
  });

  const wrappers = keep('wrappers', {
    base: new String(base),
    twoByte: new String(twoByte),
    georgian: new String(georgian),
  });
  wrappers.base.lastIndexResults = lastIndexResults;
  wrappers.twoByte.predicateResults = predicateResults;
  wrappers.georgian.coercionResults = coercionResults;

  keepError('includesNullReceiverError',
            () => String.prototype.includes.call(null, 'test'));
  keepError('startsUndefinedReceiverError',
            () => String.prototype.startsWith.call(undefined, 'test'));
  keepError('endsNullReceiverError',
            () => String.prototype.endsWith.call(null, 'test'));
  keepError('includesRegExpError', () => 'foo[a-z]+'.includes(/[a-z]+/));
  keepError('startsWithRegExpError', () => 'foo[a-z]+'.startsWith(/[a-z]+/));
  keepError('endsWithRegExpError', () => 'foo[a-z]+'.endsWith(/[a-z]+/));

  assertEquals(10, lastIndexResults[0].result);
  assertEquals(0, lastIndexResults[12].result);
  assertEquals(-1, boundaryResults.missing);
  assertEquals(-1, boundaryResults.longAlmostMatch);
  assertEquals(4, boundaryResults.regexpTextClass);
  assertTrue(predicateResults.includesSigma);
  assertFalse(predicateResults.includesMissingBeta);
  assertTrue(predicateResults.startsWithGeorgianPrefix);
  assertTrue(predicateResults.endsWithGeorgianSuffix);
  assertTrue(coercionResults.includes);
  assertTrue(coercionLog.includes('receiver'));
  assertTrue(coercionLog.includes('search'));
  assertTrue(coercionLog.includes('position'));
  assertTrue(catalog.includesNullReceiverError instanceof TypeError);
  assertTrue(catalog.startsUndefinedReceiverError instanceof TypeError);
  assertTrue(catalog.endsNullReceiverError instanceof TypeError);
  assertTrue(catalog.includesRegExpError instanceof TypeError);
  assertTrue(catalog.startsWithRegExpError instanceof TypeError);
  assertTrue(catalog.endsWithRegExpError instanceof TypeError);

  globalThis.heapDumpStringSearchBoundariesDeepSample = catalog;
})();
