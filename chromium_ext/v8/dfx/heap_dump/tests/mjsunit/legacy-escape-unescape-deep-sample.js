// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep legacy escape/unescape result strings alive for heap dump smoke. This
// follows V8's escape.js mjsunit while retaining escaped strings, malformed
// unescape remnants, round-trip outputs, wrappers, arrays, and property
// carriers.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const unescaped =
      keep('unescaped',
           'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789' +
               '@*_+-./');
  const escapedResults = keep('escapedResults', {
    unescaped,
    spaces: escape(' / '),
    nullChar: escape('\0' + '0'),
    unicode: escape(String.fromCharCode(
        0x41, 0x20, 0x42, 0x1234, 0, 0x20, 0x43)),
    paddedUnicode: escape(String.fromCharCode(0x123)),
    upperUnicode: escape(String.fromCharCode(0xabcd)),
    oneByte: escape(String.fromCharCode(0xab)),
    newline: escape('\n'),
  });

  const charChecks = keep('charChecks', []);
  for (let i = 0; i < 96; ++i) {
    const source = String.fromCharCode(i);
    charChecks.push({
      code: i,
      source,
      escaped: escape(source),
      stable: source === escape(source),
    });
  }

  const groupedRoundTrips = keep('groupedRoundTrips', []);
  for (let i = 0; i < 120; i += 10) {
    const source = String.fromCharCode(
        i, i + 1, i + 2, i + 3, i + 4, i + 5, i + 6, i + 7, i + 8, i + 9);
    groupedRoundTrips.push({
      source,
      escaped: escape(source),
      unescaped: unescape(escape(source)),
    });
  }

  const example = keep(
      'example',
      'Now is the time for all good men to come to the aid of the party.' +
          String.fromCharCode(267, 0x1234, 0x6667, 0xabcd) +
          ' The quick brown fox jumps over the lazy dog.' +
          String.fromCharCode(171, 172, 173, 174, 175, 176, 178, 179));
  const escapedExample = keep('escapedExample', escape(example));
  const roundTripExample = keep('roundTripExample', unescape(escapedExample));

  const malformedInputs = keep('malformedInputs', [
    '%',
    '%4',
    '%u',
    '%u4',
    '%u44',
    '%u444',
    '%4z',
    '%uzzzz',
    '%u4zzz',
    '%u44zz',
    '%u444z',
    '%4+',
    '%u++++',
    'foo%4+bar',
    'foo%u++++bar',
    '%%20',
    '%%%20',
  ]);
  const malformedResults = keep(
      'malformedResults',
      malformedInputs.map((input) => ({input, output: unescape(input)})));

  const wrappers = keep('wrappers', {
    escapedExample: new String(escapedExample),
    malformed: new String(malformedResults[0].output),
    unicode: new String(escapedResults.unicode),
  });
  wrappers.escapedExample.roundTrip = roundTripExample;
  wrappers.malformed.allResults = malformedResults;
  wrappers.unicode.source = escapedResults.unicode;

  const carrier = keep('carrier', {
    escapedResults,
    charChecks,
    groupedRoundTrips,
    malformedResults,
    wrappers,
    lookup: {},
  });
  carrier.lookup[escapedResults.paddedUnicode] = {kind: 'padded-unicode-key'};
  carrier.lookup[malformedResults[13].output] = {kind: 'malformed-key'};

  assertEquals(unescaped, escapedResults.unescaped);
  assertEquals('%20/%20', escapedResults.spaces);
  assertEquals('%000', escapedResults.nullChar);
  assertEquals('A%20B%u1234%00%20C', escapedResults.unicode);
  assertEquals('%u0123', escapedResults.paddedUnicode);
  assertEquals('%uABCD', escapedResults.upperUnicode);
  assertEquals('%AB', escapedResults.oneByte);
  assertEquals('%0A', escapedResults.newline);
  assertEquals('AJJ', unescape('%41%4A%4a'));
  assertEquals('%U1234', unescape('%U1234'));
  assertEquals(example, roundTripExample);
  assertEquals('%', malformedResults[0].output);
  assertEquals('%u++++', malformedResults[12].output);
  assertEquals('% ', malformedResults[15].output);
  assertEquals('%% ', malformedResults[16].output);
  assertEquals('padded-unicode-key',
               carrier.lookup[escapedResults.paddedUnicode].kind);
  assertEquals('malformed-key',
               carrier.lookup[malformedResults[13].output].kind);

  globalThis.heapDumpLegacyEscapeUnescapeDeepSample = catalog;
})();
