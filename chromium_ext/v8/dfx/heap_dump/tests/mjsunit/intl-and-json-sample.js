// Heap-dump smoke sample for optional Intl and JSON/realm builtins.
//
// Keep every feature guarded: this file is part of the generic heap-dump
// mjsunit corpus and must run on no-i18n or feature-flag-limited builds.

(function() {
  'use strict';

  const catalog = {};

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function tryKeep(name, factory) {
    try {
      const value = factory();
      if (value !== undefined && value !== null) keep(name, value);
      return value;
    } catch (e) {
      keep(name + 'Error', e);
      return null;
    }
  }

  if (typeof Intl === 'object') {
    if (typeof Intl.Collator === 'function') {
      const collator = keep('collator', new Intl.Collator('en-US'));
      assertEquals(-1, Math.sign(collator.compare('a', 'b')));
      keep('collatorOptions', collator.resolvedOptions());
    }

    if (typeof Intl.DateTimeFormat === 'function') {
      const dateTimeFormat = keep(
          'dateTimeFormat',
          new Intl.DateTimeFormat('en-US', {
            dateStyle: 'full',
            timeStyle: 'long',
            timeZone: 'UTC',
          }));
      keep('dateTimeFormatParts',
           dateTimeFormat.formatToParts(new Date(Date.UTC(2026, 4, 25))));
      keep('dateTimeFormatOptions', dateTimeFormat.resolvedOptions());
    }

    if (typeof Intl.DisplayNames === 'function') {
      const displayNames = keep(
          'displayNames',
          new Intl.DisplayNames(['en-US'], {type: 'region'}));
      assertEquals('Germany', displayNames.of('DE'));
      keep('displayNamesOptions', displayNames.resolvedOptions());
    }

    if (typeof Intl.DurationFormat === 'function') {
      const durationFormat = keep(
          'durationFormat',
          new Intl.DurationFormat('en-US', {style: 'long'}));
      keep('durationFormatParts',
           durationFormat.formatToParts({hours: 1, minutes: 2, seconds: 3}));
      keep('durationFormatOptions', durationFormat.resolvedOptions());
    }

    if (typeof Intl.ListFormat === 'function') {
      const listFormat = keep(
          'listFormat',
          new Intl.ListFormat('en-US', {style: 'long', type: 'conjunction'}));
      assertEquals('a, b, and c', listFormat.format(['a', 'b', 'c']));
      keep('listFormatParts', listFormat.formatToParts(['a', 'b', 'c']));
    }

    if (typeof Intl.Locale === 'function') {
      const locale = keep(
          'locale',
          new Intl.Locale('en-Latn-US', {calendar: 'gregory', hourCycle: 'h23'}));
      keep('localeMaximized', locale.maximize());
      keep('localeMinimized', locale.minimize());
    }

    if (typeof Intl.NumberFormat === 'function') {
      const numberFormat = keep(
          'numberFormat',
          new Intl.NumberFormat('en-US', {
            style: 'currency',
            currency: 'USD',
            currencyDisplay: 'name',
          }));
      keep('numberFormatParts', numberFormat.formatToParts(1234.5));
      keep('numberFormatOptions', numberFormat.resolvedOptions());
    }

    if (typeof Intl.PluralRules === 'function') {
      const pluralRules = keep(
          'pluralRules',
          new Intl.PluralRules('en-US', {type: 'ordinal'}));
      assertEquals('one', pluralRules.select(1));
      keep('pluralRulesOptions', pluralRules.resolvedOptions());
    }

    if (typeof Intl.RelativeTimeFormat === 'function') {
      const relativeTimeFormat = keep(
          'relativeTimeFormat',
          new Intl.RelativeTimeFormat('en-US', {numeric: 'auto'}));
      keep('relativeTimeFormatParts',
           relativeTimeFormat.formatToParts(-1, 'day'));
      keep('relativeTimeFormatOptions', relativeTimeFormat.resolvedOptions());
    }

    if (typeof Intl.Segmenter === 'function') {
      const segmenter = keep(
          'segmenter',
          new Intl.Segmenter('en-US', {granularity: 'word'}));
      const segments = keep('segments', segmenter.segment('heap dump test'));
      const segmentIterator = keep('segmentIterator', segments[Symbol.iterator]());
      const first = segmentIterator.next();
      assertFalse(first.done);
      keep('firstSegment', first.value);
    }

    if (typeof Intl.supportedValuesOf === 'function') {
      keep('supportedCalendars', Intl.supportedValuesOf('calendar').slice(0, 8));
      keep('supportedTimeZones', Intl.supportedValuesOf('timeZone').slice(0, 8));
    }

    if (typeof Intl.v8BreakIterator === 'function') {
      const breakIterator = keep(
          'v8BreakIterator',
          new Intl.v8BreakIterator(['en-US'], {type: 'word'}));
      breakIterator.adoptText('heap dump');
      keep('v8BreakIteratorFirst', breakIterator.first());
      keep('v8BreakIteratorNext', breakIterator.next());
    }
  }

  if (typeof JSON.rawJSON === 'function' &&
      typeof JSON.isRawJSON === 'function') {
    const rawJSON = keep('rawJSON', JSON.rawJSON('42'));
    assertTrue(JSON.isRawJSON(rawJSON));
    assertEquals('{"payload":42}', JSON.stringify({payload: rawJSON}));
  }

  if (typeof ShadowRealm === 'function') {
    const shadowRealm = tryKeep('shadowRealm', () => new ShadowRealm());
    if (shadowRealm !== null) {
      keep('shadowRealmEvaluateResult', shadowRealm.evaluate('40 + 2'));
    }
  }

  globalThis.heapDumpIntlAndJsonSample = catalog;
})();
