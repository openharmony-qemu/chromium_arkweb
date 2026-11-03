// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Intl enumeration and locale-canonicalization result arrays alive for
// heap dump smoke. This follows V8 intl enumeration/getCanonicalLocales tests
// and retains supported-values arrays, canonical-locale arrays, Locale objects,
// resolved options, and RangeError objects.

(function() {
  'use strict';

  const catalog = {
    supportedValues: {},
    canonicalLocales: {},
    localeObjects: [],
    errors: [],
  };
  globalThis.__heapDumpIntlEnumerationLocale = catalog;

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

  if (typeof Intl !== 'object') {
    keep('unavailable', true);
    return;
  }

  if (typeof Intl.supportedValuesOf === 'function') {
    const keys = keep('supportedValueKeys', [
      'calendar',
      'collation',
      'currency',
      'numberingSystem',
      'timeZone',
      'unit',
    ]);
    for (const key of keys) {
      const values = Intl.supportedValuesOf(key);
      catalog.supportedValues[key] = {
        values,
        firstValues: values.slice(0, 12),
        lastValues: values.slice(Math.max(0, values.length - 4)),
        length: values.length,
      };
      assertTrue(Array.isArray(values));
    }

    const invalidKeys = keep('invalidSupportedValueKeys', [
      'calendars',
      'collations',
      'currencies',
      'numberingSystems',
      'timeZones',
      'units',
      1,
      0.3,
      true,
      false,
      {},
      [],
    ]);
    for (let i = 0; i < invalidKeys.length; ++i) {
      keepError('supportedValuesInvalidKeyError' + i,
                () => Intl.supportedValuesOf(invalidKeys[i]));
    }
  }

  const canonicalInputs = keep('canonicalInputs', [
    'foobar-foobar',
    'en-a-ca-Chinese-u-ca-Chinese',
    'en-u-ca-gregory-ca-chinese',
    'in',
    'iw',
    'ji',
    'jw',
    'mo',
    'scc',
    'sh',
    'cnr',
    'no',
    'tl',
    ['EN-us', 'zh-hant-tw', 'sr-cyrl-rs-u-ca-gregory'],
  ]);
  for (const input of canonicalInputs) {
    const result = Intl.getCanonicalLocales(input);
    catalog.canonicalLocales[JSON.stringify(input)] = result;
    assertTrue(Array.isArray(result));
  }
  assertEquals('en-u-ca-gregory',
               catalog.canonicalLocales['"en-u-ca-gregory-ca-chinese"'][0]);
  assertEquals('id', catalog.canonicalLocales['"in"'][0]);
  assertEquals('he', catalog.canonicalLocales['"iw"'][0]);

  const invalidLocales = keep('invalidLocales', [
    'en-foobar-foobar',
    'arcdefghl-Latn',
    'fil-Latn-kxx',
    'fr-Latn-CAK',
    'en-Latin-US',
    'en-a-foo-9charlong',
    'en-a-b',
  ]);
  for (let i = 0; i < invalidLocales.length; ++i) {
    keepError('invalidCanonicalLocaleError' + i,
              () => Intl.getCanonicalLocales(invalidLocales[i]));
    if (typeof Intl.NumberFormat === 'function') {
      keepError('invalidNumberFormatLocaleError' + i,
                () => new Intl.NumberFormat(invalidLocales[i]));
    }
  }

  if (typeof Intl.Locale === 'function') {
    const localeSources = keep('localeSources', [
      'sr-Cyrl-RS-u-ca-gregory-hc-h23',
      'zh-Hant-TW-u-nu-latn',
      'en-Latn-US',
      'fr-FR-u-ca-buddhist',
    ]);
    for (const source of localeSources) {
      const locale = new Intl.Locale(source);
      catalog.localeObjects.push({
        source,
        locale,
        maximized: locale.maximize(),
        minimized: locale.minimize(),
        baseName: locale.baseName,
        calendar: locale.calendar,
        numberingSystem: locale.numberingSystem,
        hourCycle: locale.hourCycle,
      });
    }
  }

  if (typeof Intl.NumberFormat === 'function') {
    const longLocale =
        'bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu-cab-cbs-avnllubs-avnihq-zcu';
    keep('longLocaleNumberFormat', new Intl.NumberFormat(longLocale));
    keep('longLocaleOptions', catalog.longLocaleNumberFormat.resolvedOptions());
    assertEquals('latn', catalog.longLocaleOptions.numberingSystem);
  }

  assertTrue(Object.keys(catalog.canonicalLocales).length >= 10);
  if (catalog.supportedValues.calendar) {
    assertTrue(catalog.supportedValues.calendar.length > 0);
  }
  assertTrue(catalog.errors.every((error) => error instanceof RangeError));
})();
