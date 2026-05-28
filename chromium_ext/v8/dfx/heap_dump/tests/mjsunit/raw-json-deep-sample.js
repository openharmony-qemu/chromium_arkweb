// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep JSRawJSON objects alive for heap dump smoke. This follows V8's
// json-parse-with-source rawJSON coverage while staying focused on heap shapes.

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const rawValues = keep('rawValues', [
    JSON.rawJSON(1),
    JSON.rawJSON(1.1),
    JSON.rawJSON(-1),
    JSON.rawJSON(1.1e1),
    JSON.rawJSON(1.1e-1),
    JSON.rawJSON(null),
    JSON.rawJSON(true),
    JSON.rawJSON(false),
    JSON.rawJSON('"heap-dump-raw-json"'),
    JSON.rawJSON('"' + '\u1234'.repeat(32) + '"'),
  ]);

  for (const raw of rawValues) {
    assertTrue(JSON.isRawJSON(raw));
    assertEquals(['rawJSON'], Object.getOwnPropertyNames(raw));
    assertEquals('string', typeof raw.rawJSON);
  }

  const nested = keep('nested', {
    number: rawValues[0],
    bools: [rawValues[6], rawValues[7]],
    text: {value: rawValues[8]},
    longText: rawValues[9],
  });
  keep('stringifiedNested', JSON.stringify(nested));
  assertEquals(
      '{"number":1,"bools":[true,false],"text":{"value":"heap-dump-raw-json"},"longText":"' +
          '\u1234'.repeat(32) + '"}',
      catalog.stringifiedNested);

  const replacementSource = keep('replacementSource', {
    precise: 9007199254740993n,
    safe: 42,
    nested: {big: -9007199254740995n},
  });
  keep('replacementJSON', JSON.stringify(replacementSource, (key, value) => {
    if (typeof value === 'bigint') return JSON.rawJSON(value);
    return value;
  }));
  assertEquals(
      '{"precise":9007199254740993,"safe":42,"nested":{"big":-9007199254740995}}',
      catalog.replacementJSON);

  const invalidErrors = keep('invalidErrors', []);
  for (const invalid of ['', '{}', '[]', Symbol('raw-json'), undefined, {}]) {
    try {
      JSON.rawJSON(invalid);
    } catch (error) {
      invalidErrors.push(error);
      assertTrue(error instanceof SyntaxError || error instanceof TypeError);
    }
  }
  assertEquals(6, invalidErrors.length);
  assertFalse(JSON.isRawJSON({rawJSON: '1'}));
  assertFalse(JSON.isRawJSON([]));

  keep('catalog', catalog);
})();
