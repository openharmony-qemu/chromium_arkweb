// Heap-dump shape sample for Temporal operation results and errors.

(function() {
  'use strict';

  assertEquals('object', typeof Temporal);

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

  const plainDate = keep(
      'plainDate', Temporal.PlainDate.from('2026-05-25'));
  const changedDate = keep(
      'changedDate', plainDate.with({month: 6, day: 1}));
  const addedDate = keep(
      'addedDate', plainDate.add({years: 1, months: 2, days: 3}));
  const dateUntil = keep(
      'dateUntil', plainDate.until(changedDate));
  const dateSince = keep(
      'dateSince', changedDate.since(plainDate));

  const plainTime = keep(
      'plainTime', Temporal.PlainTime.from('12:34:56.987654321'));
  const roundedTime = keep(
      'roundedTime',
      plainTime.round({smallestUnit: 'minute', roundingMode: 'halfExpand'}));
  const timeWith = keep(
      'timeWith', plainTime.with({hour: 23, minute: 45}));

  const plainDateTime = keep(
      'plainDateTime',
      Temporal.PlainDateTime.from('2026-05-25T12:34:56.987654321'));
  const dateTimeAdded = keep(
      'dateTimeAdded', plainDateTime.add({days: 2, hours: 3}));
  const dateTimeRounded = keep(
      'dateTimeRounded',
      plainDateTime.round({smallestUnit: 'second', roundingMode: 'ceil'}));

  const instant = keep(
      'instant',
      Temporal.Instant.fromEpochNanoseconds(1234567890123456789n));
  const instantRounded = keep(
      'instantRounded',
      instant.round({smallestUnit: 'millisecond', roundingMode: 'floor'}));
  const instantSince = keep(
      'instantSince',
      instant.since(Temporal.Instant.fromEpochMilliseconds(0)));

  const duration = keep(
      'duration', Temporal.Duration.from('P1Y2M3DT4H5M6.007008009S'));
  const durationRounded = keep(
      'durationRounded',
      Temporal.Duration.from('P1DT25H').round({
        largestUnit: 'day',
        relativeTo: plainDate,
      }));
  const durationTotal = keep(
      'durationTotal',
      Temporal.Duration.from({hours: 49, minutes: 30}).total('hour'));

  const yearMonth = keep(
      'yearMonth', Temporal.PlainYearMonth.from('2026-05'));
  const yearMonthAdded = keep(
      'yearMonthAdded', yearMonth.add({months: 8}));
  const monthDay = keep(
      'monthDay', Temporal.PlainMonthDay.from('05-25'));
  const monthDayDate = keep(
      'monthDayDate', monthDay.toPlainDate({year: 2026}));

  const zonedDateTime = keep(
      'zonedDateTime',
      Temporal.ZonedDateTime.from('2026-05-25T12:34:56+00:00[UTC]'));
  const zonedAdded = keep(
      'zonedAdded', zonedDateTime.add({days: 1, hours: 2}));
  const zonedWithTimeZone = keep(
      'zonedWithTimeZone', zonedDateTime.withTimeZone('Europe/Berlin'));
  const zonedStartOfDay = keep(
      'zonedStartOfDay', zonedDateTime.startOfDay());

  const dateInstant = keep(
      'dateInstant', new Date(0).toTemporalInstant());
  const compareResults = keep('compareResults', {
    date: Temporal.PlainDate.compare(plainDate, changedDate),
    time: Temporal.PlainTime.compare(plainTime, timeWith),
    instant: Temporal.Instant.compare(instant, instantRounded),
    duration: Temporal.Duration.compare(
        Temporal.Duration.from('PT1H'), Temporal.Duration.from('PT2H')),
  });
  const equalityResults = keep('equalityResults', {
    date: plainDate.equals(Temporal.PlainDate.from('2026-05-25')),
    instant: instant.equals(
        Temporal.Instant.fromEpochNanoseconds(1234567890123456789n)),
    zoned: zonedDateTime.equals(
        Temporal.ZonedDateTime.from('2026-05-25T12:34:56+00:00[UTC]')),
  });

  keepError(
      'badPlainDateError',
      () => Temporal.PlainDate.from('2026-02-30', {overflow: 'reject'}));
  keepError(
      'badDurationRoundError',
      () => Temporal.Duration.from('P1M').round({smallestUnit: 'day'}));
  keepError(
      'badZonedDateTimeError',
      () => Temporal.ZonedDateTime.from('2026-05-25T12:34:56[No/Such_Zone]'));

  assertEquals('2026-06-01', changedDate.toString());
  assertEquals('P7D', dateUntil.toString());
  assertEquals('P7D', dateSince.toString());
  assertEquals('12:35:00', roundedTime.toString());
  assertEquals('2026-05-25T12:34:57', dateTimeRounded.toString());
  assertEquals('P2DT1H', durationRounded.toString());
  assertEquals(49.5, durationTotal);
  assertEquals('2027-01', yearMonthAdded.toString());
  assertEquals('2026-05-25', monthDayDate.toString());
  assertTrue(zonedWithTimeZone.toString().includes('[Europe/Berlin]'));
  assertEquals('1970-01-01T00:00:00Z', dateInstant.toString());
  assertEquals(-1, compareResults.date);
  assertTrue(equalityResults.date);
  assertTrue(equalityResults.instant);
  assertTrue(equalityResults.zoned);

  globalThis.__heapDumpTemporalOperationsKeepalive = keepalive;
})();
