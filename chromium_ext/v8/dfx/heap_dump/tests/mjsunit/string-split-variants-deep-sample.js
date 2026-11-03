// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep String.prototype.split RegExp, coercion, sliced-string, and
// Symbol.split override states alive for heap-dump smoke coverage. This follows
// stable portions of V8's string-split.js, es6/string-split.js, and
// regexp-override-symbol-split.js mjsunit tests.

const heapDumpStringSplitState = {
  subjects: [],
  separators: [],
  limits: [],
  results: [],
  logs: [],
  errors: [],
};

function keepStringSplit(name, value) {
  heapDumpStringSplitState[name] = value;
  return value;
}

(function regexpCapturesAndZeroWidth() {
  const htmlSubject = "A<B>bold</B>and<CODE>coded</CODE>";
  const htmlRegExp = /<(\/)?([^<>]+)>/;
  const htmlResult = htmlSubject.split(htmlRegExp);
  assertArrayEquals([
    "A", undefined, "B", "bold", "/", "B", "and", undefined, "CODE",
    "coded", "/", "CODE", ""
  ], htmlResult);

  const lazyStar = "ab".split(/a*?/);
  const greedyStar = "ab".split(/a*/);
  const lookaheadHash = "div#id.class".split(/(?=[#.])/);
  const lookaheadChars = "abc".split(/(?=.)/);
  const groupedLookahead = "ab".split(/((?=.))/);
  const emptyLookahead = "ab".split(/(?=)/);
  assertArrayEquals(["a", "b"], lazyStar);
  assertArrayEquals(["", "b"], greedyStar);
  assertArrayEquals(["div", "#id", ".class"], lookaheadHash);
  assertArrayEquals(["a", "b", "c"], lookaheadChars);
  assertArrayEquals(["a", "", "b"], groupedLookahead);
  assertArrayEquals(["a", "b"], emptyLookahead);

  heapDumpStringSplitState.subjects.push(htmlSubject);
  heapDumpStringSplitState.separators.push(htmlRegExp);
  heapDumpStringSplitState.results.push(
      htmlResult, lazyStar, greedyStar, lookaheadHash, lookaheadChars,
      groupedLookahead, emptyLookahead);
})();

(function stringSeparatorsLimitsAndUnicode() {
  const asciiSubject = "asdfasdfasdf";
  const germanSubject = "Wenige sind auserwählt.";
  const commaSubject = "a,b,c,d,e,f";
  const allAsciiCodes = [];
  for (let i = 0; i < 128; i++) allAsciiCodes[i] = i;
  const allAsciiString = String.fromCharCode.apply(String, allAsciiCodes);

  const splitD = asciiSubject.split("d");
  const splitLimit = asciiSubject.split("d", 2);
  const splitZeroLimit = asciiSubject.split("d", 0);
  const splitSpace = germanSubject.split(" ", 3);
  const splitTwoByte = germanSubject.split("ä");
  const splitWord = germanSubject.split("auserwählt");
  const commaLimit = commaSubject.split(/,/, -4294967293);
  const asciiChars = allAsciiString.split("");
  const cyrillicChars = "\u0427\u0427".split("", 0xffffffff);
  assertArrayEquals(["as", "fas", "fas", "f"], splitD);
  assertArrayEquals(["as", "fas"], splitLimit);
  assertArrayEquals([], splitZeroLimit);
  assertArrayEquals(["Wenige", "sind", "auserwählt."], splitSpace);
  assertArrayEquals(["Wenige sind auserw", "hlt."], splitTwoByte);
  assertArrayEquals(["Wenige sind ", "."], splitWord);
  assertArrayEquals(["a", "b", "c"], commaLimit);
  assertEquals(128, asciiChars.length);
  assertArrayEquals(["\u0427", "\u0427"], cyrillicChars);

  heapDumpStringSplitState.subjects.push(
      asciiSubject, germanSubject, commaSubject, allAsciiString);
  heapDumpStringSplitState.results.push(
      splitD, splitLimit, splitZeroLimit, splitSpace, splitTwoByte, splitWord,
      commaLimit, asciiChars, cyrillicChars);
})();

(function emptyAndSlicedStrings() {
  const emptyDefault = "".split();
  const emptyDot = "".split(/./);
  const emptyOptional = "".split(/.?/);
  const emptyCaptures = "".split(/()()/);
  assertArrayEquals([""], emptyDefault);
  assertArrayEquals([""], emptyDot);
  assertArrayEquals([], emptyOptional);
  assertArrayEquals([], emptyCaptures);

  function numberObj(num) {
    return {valueOf() { return num; }};
  }
  const limitedChars = "abc".split("", numberObj(2));
  assertArrayEquals(["a", "b"], limitedChars);

  const slicedString = %ConstructSlicedString("abcdefghijklmnopqrstuvwxyz", 13);
  const slicedFirst = slicedString.split("");
  const slicedSecond = slicedString.split("");
  assertArrayEquals("nopqrstuvwxyz".split(""), slicedFirst);
  assertArrayEquals(slicedFirst, slicedSecond);

  heapDumpStringSplitState.subjects.push("", slicedString);
  heapDumpStringSplitState.limits.push(numberObj(2));
  heapDumpStringSplitState.results.push(
      emptyDefault, emptyDot, emptyOptional, emptyCaptures, limitedChars,
      slicedFirst, slicedSecond);
})();

(function coercionOrderAndSymbolSplitOverride() {
  const coercionLog = [];
  const subject = {
    toString() {
      coercionLog.push("subject-toString");
      return "abc";
    },
  };
  const separator = {
    toString() {
      coercionLog.push("separator-toString");
      return "b";
    },
  };
  const coercionResult = String.prototype.split.call(subject, separator);
  assertArrayEquals(["a", "c"], coercionResult);
  assertArrayEquals(["subject-toString", "separator-toString"], coercionLog);

  const limitZeroSeparatorLog = [];
  const limitZeroSeparator = {
    toString() {
      limitZeroSeparatorLog.push("separator-toString");
      return "b";
    },
  };
  const limitZeroResult = "abc".split(limitZeroSeparator, 0);
  assertArrayEquals([], limitZeroResult);
  assertArrayEquals(["separator-toString"], limitZeroSeparatorLog);

  const splitPattern = {
    toString() { throw new Error("toString should not run for Symbol.split"); },
    [Symbol.split](string, limit) {
      return {kind: "symbol-split-result", string, limitValue: limit.value};
    },
  };
  const limit = {value: 3};
  const symbolResult = "abcde".split(splitPattern, limit);
  assertEquals("symbol-split-result", symbolResult.kind);
  assertEquals("abcde", symbolResult.string);
  assertEquals(3, symbolResult.limitValue);

  const nonCallablePattern = {
    [Symbol.split]: "not-callable",
  };
  const nonCallableError = assertThrows(
      () => "abcde".split(nonCallablePattern, limit), TypeError);
  const nullReceiverError = assertThrows(
      () => String.prototype.split.call(null, splitPattern, limit), TypeError);

  const nullOverridePattern = {
    toString() { return ""; },
    [Symbol.split]: null,
  };
  const nullOverrideResult = "abcde".split(nullOverridePattern);
  assertArrayEquals(["a", "b", "c", "d", "e"], nullOverrideResult);

  heapDumpStringSplitState.subjects.push(subject);
  heapDumpStringSplitState.separators.push(
      separator, limitZeroSeparator, splitPattern, nonCallablePattern,
      nullOverridePattern);
  heapDumpStringSplitState.limits.push(limit);
  heapDumpStringSplitState.results.push(
      coercionResult, limitZeroResult, symbolResult, nullOverrideResult);
  heapDumpStringSplitState.logs.push(coercionLog, limitZeroSeparatorLog);
  heapDumpStringSplitState.errors.push(nonCallableError, nullReceiverError);
})();

keepStringSplit("summary", {
  subjectCount: heapDumpStringSplitState.subjects.length,
  separatorCount: heapDumpStringSplitState.separators.length,
  limitCount: heapDumpStringSplitState.limits.length,
  resultCount: heapDumpStringSplitState.results.length,
  logCount: heapDumpStringSplitState.logs.length,
  errorCount: heapDumpStringSplitState.errors.length,
});
