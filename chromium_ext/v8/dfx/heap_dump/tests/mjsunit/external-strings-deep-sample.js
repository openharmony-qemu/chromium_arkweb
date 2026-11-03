// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --expose-gc

// Keep externalized string shapes alive for heap dump smoke. External strings
// are a no-content/no-need-dump family in dfx-visitor-list.h, so this sample
// feeds the rawheap -> translate path with naturally externalized one-byte and
// two-byte strings plus slices that still participate in normal JS operations.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const oneByteCons = keep('oneByteCons', '0123456789' + 'ABCDEFGHIJ');
  const twoByteCons = keep('twoByteCons', '0123456789\u1234' + 'ABCDEFGHIJ');
  gc();
  gc();
  externalizeString(oneByteCons);
  externalizeString(twoByteCons);

  const oneByteSlice = keep('oneByteSlice', oneByteCons.slice(1, -1));
  const twoByteSlice = keep('twoByteSlice', twoByteCons.slice(1, -1));
  const oneByteMatch = keep('oneByteMatch', /(B)/.exec(oneByteSlice));
  const twoByteMatch = keep('twoByteMatch', /(B)/.exec(twoByteSlice));

  const emojiString = keep(
      'emojiString', 'external string turned into two byte \u{1f913}');
  const emojiSlice = keep('emojiSlice', emojiString.substring(1));
  externalizeString(emojiString);
  keep('emojiLower', emojiSlice.toLowerCase());
  keep('emojiUpper', emojiString.toUpperCase());

  const longOneByte = keep(
      'longOneByte',
      'heap-dump-external-one-byte-' + 'abcdefghijklmnopqrstuvwxyz'.repeat(8));
  const longTwoByte = keep(
      'longTwoByte',
      'heap-dump-external-two-byte-\u0100-\u2603-' +
          '\u{1f680}'.repeat(8));

  const externalStringWrapper = keep('externalStringWrapper', new String(longOneByte));
  externalStringWrapper.extra = {kind: 'external-wrapper-extra'};

  const propertyCarrier = keep('propertyCarrier', {
    oneByteCons,
    twoByteCons,
    oneByteSlice,
    twoByteSlice,
    emojiString,
    emojiSlice,
    longOneByte,
    longTwoByte,
    nested: {
      wrapper: externalStringWrapper,
      array: [oneByteCons, twoByteCons, longOneByte, longTwoByte],
    },
  });

  assertEquals('B', oneByteMatch[1]);
  assertEquals('B', twoByteMatch[1]);
  assertTrue(emojiSlice.includes('xternal'));
  assertTrue(catalog.emojiLower.includes('xternal'));
  assertTrue(catalog.emojiUpper.includes('EXTERNAL'));
  assertTrue(longOneByte.includes('abcdefghijklmnopqrstuvwxyz'));
  assertTrue(longTwoByte.includes('\u0100'));
  assertEquals('external-wrapper-extra', externalStringWrapper.extra.kind);
  assertEquals(longTwoByte, propertyCarrier.nested.array[3]);

  globalThis.heapDumpExternalStringsDeepSample = catalog;
})();
