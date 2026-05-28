// Heap-dump shape sample for JSDate and Temporal-family objects.

const keepalive = {};

class HeapDumpDateSubclass extends Date {
  constructor(value, label) {
    super(value);
    this.label = label;
    this.extra = {kind: "date-subclass-extra", label};
  }

  getLabel() {
    return this.label;
  }
}

const validDate = new Date(Date.UTC(2026, 4, 25, 12, 34, 56, 789));
const zeroDate = new Date(0);
const negativeDate = new Date(-8640000000000000);
const maxDate = new Date(8640000000000000);
const invalidDate = new Date(NaN);
const parsedDate = new Date("2026-05-25T12:34:56.789Z");
const mutatedDate = new Date(1);
mutatedDate.setUTCFullYear(2030, 6, 15);
mutatedDate.setUTCHours(1, 2, 3, 4);
const subclassDate = new HeapDumpDateSubclass(Date.UTC(2024, 1, 2), "sub");

const dateArray = [
  validDate,
  zeroDate,
  negativeDate,
  maxDate,
  invalidDate,
  parsedDate,
  mutatedDate,
  subclassDate,
];

const dateFormatter = typeof Intl === "object" &&
    typeof Intl.DateTimeFormat === "function"
    ? new Intl.DateTimeFormat("en-US", {
        calendar: "gregory",
        dateStyle: "full",
        timeStyle: "long",
        timeZone: "UTC",
      })
    : undefined;
const dateFormatParts = dateFormatter ?
    dateFormatter.formatToParts(validDate) : [];

const temporalObjects = {};
if (typeof Temporal === "object") {
  temporalObjects.duration =
      new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  temporalObjects.instant =
      Temporal.Instant.fromEpochNanoseconds(1234567890123456789n);
  temporalObjects.plainDate =
      new Temporal.PlainDate(2026, 5, 25, "gregory");
  temporalObjects.plainDateTime =
      new Temporal.PlainDateTime(2026, 5, 25, 12, 34, 56, 987, 654, 321);
  temporalObjects.plainMonthDay =
      new Temporal.PlainMonthDay(5, 25, "gregory", 1972);
  temporalObjects.plainTime =
      new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
  temporalObjects.plainYearMonth =
      new Temporal.PlainYearMonth(2026, 5, "gregory", 25);
  if (typeof Temporal.TimeZone === "function") {
    temporalObjects.timeZone = Temporal.TimeZone.from("UTC");
  }
  if (typeof Temporal.Calendar === "function") {
    temporalObjects.calendar = Temporal.Calendar.from("gregory");
  }
  if (typeof Temporal.ZonedDateTime === "function") {
    temporalObjects.zonedDateTime = new Temporal.ZonedDateTime(
        1234567890123456789n, temporalObjects.timeZone || "UTC",
        temporalObjects.calendar || "gregory");
  }
  temporalObjects.nowInstant = Temporal.Now.instant();
  temporalObjects.dateFrom = Temporal.PlainDate.from("2026-05-25");
  temporalObjects.durationFrom = Temporal.Duration.from("P1Y2M3DT4H5M6.007S");
}

assertEquals(2026, validDate.getUTCFullYear());
assertEquals(0, zeroDate.getTime());
assertTrue(Number.isNaN(invalidDate.getTime()));
assertEquals("sub", subclassDate.getLabel());
assertEquals(2030, mutatedDate.getUTCFullYear());
if (dateFormatter) {
  assertTrue(dateFormatParts.length > 0);
}
if (temporalObjects.plainDate) {
  assertEquals(2026, temporalObjects.plainDate.year);
  assertEquals(5, temporalObjects.plainDate.month);
  assertEquals(25, temporalObjects.plainDate.day);
  assertEquals("12:34:56.987654321",
               temporalObjects.plainTime.toString());
}

keepalive.dateArray = dateArray;
keepalive.validDate = validDate;
keepalive.zeroDate = zeroDate;
keepalive.negativeDate = negativeDate;
keepalive.maxDate = maxDate;
keepalive.invalidDate = invalidDate;
keepalive.parsedDate = parsedDate;
keepalive.mutatedDate = mutatedDate;
keepalive.subclassDate = subclassDate;
keepalive.dateFormatter = dateFormatter;
keepalive.dateFormatParts = dateFormatParts;
keepalive.temporalObjects = temporalObjects;

globalThis.__heapDumpDateTemporalKeepalive = keepalive;
