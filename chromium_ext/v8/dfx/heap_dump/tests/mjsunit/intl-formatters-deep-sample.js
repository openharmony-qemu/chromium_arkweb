// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Intl formatter variants alive for heap dump smoke. This complements the
// minimal intl-and-json sample and the dedicated DurationFormat sample with
// option-heavy formatter, Locale, parts-array, iterator, and error objects.

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
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  function keepConstructors(name, ctor, locales, optionSets, exercise) {
    if (typeof ctor !== 'function') return;
    const entries = keep(name, []);
    for (let i = 0; i < optionSets.length; ++i) {
      const formatter = new ctor(locales[i % locales.length], optionSets[i]);
      entries.push({
        formatter,
        options: formatter.resolvedOptions(),
        result: exercise(formatter, i),
      });
    }
    if (typeof ctor.supportedLocalesOf === 'function') {
      keep(name + 'SupportedLocales',
           ctor.supportedLocalesOf(locales, {localeMatcher: 'lookup'}));
    }
  }

  if (typeof Intl !== 'object') {
    globalThis.heapDumpIntlFormattersDeepSample = catalog;
    return;
  }

  const localeInputs = keep('localeInputs', [
    'en-US',
    'de-DE-u-co-phonebk',
    'zh-Hant-TW-u-nu-thai',
    'fr-FR',
  ]);

  if (typeof Intl.Locale === 'function') {
    class HeapDumpLocale extends Intl.Locale {}
    const locale = keep(
        'locale',
        new Intl.Locale('sr-Cyrl-RS-u-ca-gregory-hc-h23', {
          calendar: 'gregory',
          caseFirst: 'upper',
          collation: 'search',
          hourCycle: 'h23',
          numberingSystem: 'latn',
        }));
    keep('localeMaximized', locale.maximize());
    keep('localeMinimized', locale.minimize());
    keep('localeSubclass', new HeapDumpLocale('zh-Hant-TW-u-nu-thai'));
    keep('canonicalLocales', Intl.getCanonicalLocales([locale, 'fr-FR']));
    keepError('invalidLocaleError', () => new Intl.Locale('bad_locale'));
  }

  keepConstructors(
      'collators',
      Intl.Collator,
      localeInputs,
      [
        {usage: 'sort', sensitivity: 'base'},
        {usage: 'search', sensitivity: 'variant', ignorePunctuation: true},
        {numeric: true, caseFirst: 'upper'},
        {collation: 'emoji'},
      ],
      (collator) => ({
        comparePair: collator.compare('file2', 'file10'),
        sorted: ['b', 'a', 'c'].sort(collator.compare),
      }));

  keepConstructors(
      'numberFormats',
      Intl.NumberFormat,
      localeInputs,
      [
        {style: 'currency', currency: 'USD', currencyDisplay: 'name'},
        {style: 'unit', unit: 'meter-per-second', unitDisplay: 'long'},
        {notation: 'compact', compactDisplay: 'short'},
        {signDisplay: 'always', roundingMode: 'ceil', maximumFractionDigits: 2},
      ],
      (numberFormat, index) => ({
        formatted: numberFormat.format(index === 3 ? -1234.567 : 123456.789),
        parts: numberFormat.formatToParts(123456.789),
      }));

  if (typeof Intl.DateTimeFormat === 'function') {
    const date = new Date(Date.UTC(2026, 4, 25, 12, 34, 56));
    const endDate = new Date(Date.UTC(2026, 4, 28, 12, 34, 56));
    keepConstructors(
        'dateTimeFormats',
        Intl.DateTimeFormat,
        localeInputs,
        [
          {dateStyle: 'full', timeStyle: 'long', timeZone: 'UTC'},
          {
            calendar: 'gregory',
            weekday: 'long',
            era: 'short',
            year: 'numeric',
            month: 'long',
            day: '2-digit',
            timeZone: 'UTC',
          },
          {hour: 'numeric', minute: '2-digit', hourCycle: 'h23', timeZone: 'UTC'},
          {timeZoneName: 'longOffset', timeZone: 'UTC'},
        ],
        (dateTimeFormat) => {
          const result = {
            formatted: dateTimeFormat.format(date),
            parts: dateTimeFormat.formatToParts(date),
          };
          if (typeof dateTimeFormat.formatRange === 'function') {
            result.range = dateTimeFormat.formatRange(date, endDate);
            result.rangeParts = dateTimeFormat.formatRangeToParts(date, endDate);
          }
          return result;
        });
  }

  keepConstructors(
      'displayNames',
      Intl.DisplayNames,
      localeInputs,
      [
        {type: 'language'},
        {type: 'region'},
        {type: 'script'},
        {type: 'currency', fallback: 'code'},
      ],
      (displayNames, index) => {
        const codes = ['de', 'US', 'Latn', 'JPY'];
        return displayNames.of(codes[index % codes.length]);
      });

  keepConstructors(
      'listFormats',
      Intl.ListFormat,
      localeInputs,
      [
        {style: 'long', type: 'conjunction'},
        {style: 'short', type: 'disjunction'},
        {style: 'narrow', type: 'unit'},
      ],
      (listFormat) => ({
        formatted: listFormat.format(['alpha', 'beta', 'gamma']),
        parts: listFormat.formatToParts(['alpha', 'beta', 'gamma']),
      }));

  keepConstructors(
      'pluralRules',
      Intl.PluralRules,
      localeInputs,
      [
        {type: 'cardinal'},
        {type: 'ordinal'},
        {minimumFractionDigits: 1, maximumFractionDigits: 2},
      ],
      (pluralRules) => {
        const result = {
          one: pluralRules.select(1),
          many: pluralRules.select(5),
        };
        if (typeof pluralRules.selectRange === 'function') {
          result.range = pluralRules.selectRange(1, 3);
        }
        return result;
      });

  keepConstructors(
      'relativeTimeFormats',
      Intl.RelativeTimeFormat,
      localeInputs,
      [
        {numeric: 'auto', style: 'long'},
        {numeric: 'always', style: 'short'},
        {numeric: 'always', style: 'narrow'},
      ],
      (relativeTimeFormat) => ({
        day: relativeTimeFormat.format(-1, 'day'),
        secondParts: relativeTimeFormat.formatToParts(-0, 'second'),
        quarterParts: relativeTimeFormat.formatToParts(3, 'quarter'),
      }));

  if (typeof Intl.Segmenter === 'function') {
    const segmenters = keep('segmenters', [
      new Intl.Segmenter('en', {granularity: 'grapheme'}),
      new Intl.Segmenter('en', {granularity: 'word'}),
      new Intl.Segmenter('en', {granularity: 'sentence'}),
    ]);
    const segmentInputs = ['a\u0301b', 'heap dump test', 'First. Second.'];
    keep('segments', segmenters.map((segmenter, index) => {
      const segments = segmenter.segment(segmentInputs[index]);
      const iterator = segments[Symbol.iterator]();
      return {
        segmenter,
        segments,
        iterator,
        first: iterator.next(),
        containing: segments.containing(0),
        options: segmenter.resolvedOptions(),
      };
    }));
    keepError('segmenterSymbolError',
              () => segmenters[1].segment(Symbol('heap-dump')));
  }

  keepError('badCollatorOptionError',
            () => new Intl.Collator('en', {usage: 'invalid'}));
  keepError('badNumberUnitError',
            () => new Intl.NumberFormat('en', {style: 'unit', unit: 'bad'}));
  keepError('badDateTimeZoneError',
            () => new Intl.DateTimeFormat('en', {timeZone: 'Not/AZone'}));
  keepError('badDisplayNamesTypeError',
            () => new Intl.DisplayNames('en', {type: 'invalid'}));
  keepError('badListFormatTypeError',
            () => new Intl.ListFormat('en', {type: 'invalid'}));
  keepError('badPluralRulesTypeError',
            () => new Intl.PluralRules('en', {type: 'invalid'}));
  keepError('badRelativeTimeUnitError',
            () => new Intl.RelativeTimeFormat('en').formatToParts(1, 'decade'));

  assertTrue(Object.keys(catalog).length > 0);
  if (catalog.numberFormats) {
    assertTrue(Array.isArray(catalog.numberFormats[0].result.parts));
  }
  if (catalog.segmenters) {
    assertFalse(catalog.segments[0].first.done);
  }

  globalThis.heapDumpIntlFormattersDeepSample = catalog;
})();
