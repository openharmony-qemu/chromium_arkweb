// Heap-dump shape sample for Intl.DurationFormat.

(function() {
  'use strict';

  const keepalive = {};

  function keep(name, value) {
    keepalive[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
      assertUnreachable(name);
    } catch (e) {
      keep(name, e);
    }
  }

  if (typeof Intl !== 'object' ||
      typeof Intl.DurationFormat !== 'function') {
    keep('durationFormatUnavailable', {
      intlType: typeof Intl,
      durationFormatType:
          typeof Intl === 'object' ? typeof Intl.DurationFormat : 'missing',
    });
    globalThis.__heapDumpIntlDurationFormatKeepalive = keepalive;
    return;
  }

  const fullDuration = Object.freeze({
    years: 1,
    months: 2,
    weeks: 3,
    days: 4,
    hours: 5,
    minutes: 6,
    seconds: 7,
    milliseconds: 8,
    microseconds: 9,
    nanoseconds: 10,
  });
  const clockDuration = Object.freeze({
    hours: 1,
    minutes: 2,
    seconds: 3,
    milliseconds: 4,
  });

  const longFormatter = keep(
      'longFormatter',
      new Intl.DurationFormat('en-US', {
        style: 'long',
        yearsDisplay: 'always',
        monthsDisplay: 'always',
        weeksDisplay: 'always',
        daysDisplay: 'always',
      }));
  const shortFormatter = keep(
      'shortFormatter',
      new Intl.DurationFormat(['fr-FR', 'en-US'], {style: 'short'}));
  const narrowFormatter = keep(
      'narrowFormatter',
      new Intl.DurationFormat('de-DE-u-nu-latn', {style: 'narrow'}));
  const digitalFormatter = keep(
      'digitalFormatter',
      new Intl.DurationFormat('en-GB', {
        style: 'digital',
        hours: 'numeric',
        minutes: '2-digit',
        seconds: '2-digit',
        milliseconds: 'numeric',
      }));

  const longString = keep('longString', longFormatter.format(fullDuration));
  const shortString = keep('shortString', shortFormatter.format(clockDuration));
  const narrowString = keep('narrowString', narrowFormatter.format(clockDuration));
  const digitalString = keep(
      'digitalString', digitalFormatter.format(clockDuration));

  const longParts = keep(
      'longParts', longFormatter.formatToParts(fullDuration));
  const digitalParts = keep(
      'digitalParts', digitalFormatter.formatToParts(clockDuration));

  const longOptions = keep('longOptions', longFormatter.resolvedOptions());
  const digitalOptions = keep(
      'digitalOptions', digitalFormatter.resolvedOptions());
  const supportedLocales = keep(
      'supportedLocales',
      Intl.DurationFormat.supportedLocalesOf(['en-US', 'fr-FR', 'invalid']));

  keepError('badLocaleError', () => new Intl.DurationFormat('not_a_locale'));
  keepError(
      'badStyleError', () => new Intl.DurationFormat('en-US', {style: 'huge'}));
  keepError(
      'badDurationError',
      () => longFormatter.format({hours: Number.POSITIVE_INFINITY}));

  assertTrue(longString.includes('year'));
  assertTrue(shortString.length > 0);
  assertTrue(narrowString.length > 0);
  assertTrue(digitalString.includes(':'));
  assertTrue(longParts.length > 0);
  assertTrue(digitalParts.some(part => part.type === 'integer'));
  assertEquals('en-US', longOptions.locale);
  assertEquals('long', longOptions.style);
  assertEquals('digital', digitalOptions.style);
  assertTrue(supportedLocales.includes('en-US'));

  keepalive.fullDuration = fullDuration;
  keepalive.clockDuration = clockDuration;
  globalThis.__heapDumpIntlDurationFormatKeepalive = keepalive;
})();
