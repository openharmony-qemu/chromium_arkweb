// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep JSON.parse source-context, reviver holder, JSON.stringify replacer
// holder, typed-array stringify, RawJSON, and Error.isError shapes alive for
// heap dump smoke. This follows stable V8 JSON mjsunit patterns while retaining
// the intermediate context/holder records as graph roots.

(function() {
  'use strict';

  const catalog = {
    parseRecords: [],
    stringifyRecords: [],
    errors: [],
  };
  globalThis.__heapDumpJsonParseSource = catalog;

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (e) {
      catalog.errors.push(e);
      return keep(name, e);
    }
    return keep(name, null);
  }

  const preciseInteger = keep(
      'preciseInteger', BigInt(Number.MAX_SAFE_INTEGER) + 2n);
  const parsedPrecise = keep(
      'parsedPrecise',
      JSON.parse(String(preciseInteger), (key, value, context) => {
        catalog.parseRecords.push({key, value, context});
        if (typeof value === 'number' && Number.isInteger(value)) {
          return BigInt(context.source);
        }
        return value;
      }));
  assertEquals(preciseInteger, parsedPrecise);

  const nestedText = keep(
      'nestedText',
      '{"number":1.1e+2,"text":"heap","array":[true,null,-7],"object":{"x":3}}');
  const parsedNested = keep(
      'parsedNested',
      JSON.parse(nestedText, function jsonSourceReviver(key, value, context) {
        const record = {holder: this, key, value, context};
        catalog.parseRecords.push(record);
        if (context !== undefined && Object.hasOwn(context, 'source')) {
          record.sourceDescriptor =
              Object.getOwnPropertyDescriptor(context, 'source');
          record.sourceLength = context.source.length;
        }
        return value;
      }));
  assertEquals(110, parsedNested.number);
  assertEquals('heap', parsedNested.text);

  const rawCatalog = keep('rawCatalog', {
    integer: JSON.rawJSON(1),
    decimal: JSON.rawJSON(1.25),
    bool: JSON.rawJSON(true),
    string: JSON.rawJSON('"json-source"'),
    longString: JSON.rawJSON('"' + '\u1234'.repeat(16) + '"'),
  });
  const rawContainer = keep('rawContainer', {
    values: [rawCatalog.integer, rawCatalog.decimal, rawCatalog.bool],
    nested: {string: rawCatalog.string, longString: rawCatalog.longString},
  });
  keep('rawStringified', JSON.stringify(rawContainer));

  const stringifyInput = keep('stringifyInput', {
    a: false,
    nested: {
      toJSON() {
        return keep('nestedToJSONResult', {nestedToJSONd: true});
      },
    },
    toJSON() {
      return keep('rootToJSONResult', {
        a: false,
        replaced: true,
        nested: this.nested,
      });
    },
  });
  const stringifyResult = keep(
      'stringifyResult',
      JSON.stringify(stringifyInput, function jsonHolderReplacer(key, value) {
        catalog.stringifyRecords.push({holder: this, key, value});
        if (key === 'a') return true;
        return value;
      }));
  assertEquals('{"a":true,"replaced":true,"nested":{"nestedToJSONd":true}}',
               stringifyResult);

  const typedArrayStringify = keep('typedArrayStringify', []);
  const typedArrayConstructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
  ];
  if (typeof Float16Array === 'function') {
    typedArrayConstructors.push(Float16Array);
  }
  for (const Ctor of typedArrayConstructors) {
    const tiny = new Ctor(2).fill(7);
    const empty = new Ctor(0);
    typedArrayStringify.push({
      ctorName: Ctor.name,
      tiny,
      empty,
      tinyJSON: JSON.stringify(tiny),
      emptyJSON: JSON.stringify(empty),
    });
  }

  const syntaxError = keepError(
      'parseSyntaxError', () => JSON.parse('{"unterminated":'));
  const rawSyntaxError = keepError(
      'rawJsonSyntaxError', () => JSON.rawJSON('{}'));
  const rawTypeError = keepError(
      'rawJsonTypeError', () => JSON.rawJSON(Symbol('json-source')));

  keep('errorIsErrorResults', {
    syntaxError: Error.isError(syntaxError),
    rawSyntaxError: Error.isError(rawSyntaxError),
    rawTypeError: Error.isError(rawTypeError),
    plainObject: Error.isError({message: 'not-error'}),
  });

  assertTrue(catalog.parseRecords.length >= 8);
  assertTrue(catalog.parseRecords.some((record) =>
      record.context && record.context.source === '1.1e+2'));
  assertTrue(catalog.stringifyRecords.length >= 4);
  assertTrue(JSON.isRawJSON(rawCatalog.integer));
  assertEquals('{"0":7,"1":7}', typedArrayStringify[0].tinyJSON);
  assertEquals('{}', typedArrayStringify[0].emptyJSON);
  assertTrue(catalog.errorIsErrorResults.syntaxError);
  assertTrue(catalog.errorIsErrorResults.rawSyntaxError);
  assertTrue(catalog.errorIsErrorResults.rawTypeError);
  assertFalse(catalog.errorIsErrorResults.plainObject);
})();
