// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep String.prototype.replaceAll fast/slow replacement and override states
// alive for heap-dump smoke coverage. This follows stable portions of V8's
// harmony/string-replaceAll.js mjsunit test.

const heapDumpReplaceAllState = {
  subjects: [],
  searchValues: [],
  replacements: [],
  results: [],
  logs: [],
  errors: [],
  descriptors: [],
};

function keepReplaceAll(name, value) {
  heapDumpReplaceAllState[name] = value;
  return value;
}

(function basicStringAndRegExpResults() {
  const subject = "a+b+c+d";
  const regexSubject = "No Uppercase? NO UPPERCASE!";
  const emptySubject = "";
  const emptyPatternResult = "xxx".replaceAll("", "_");
  const overlappingResult = "xxx".replaceAll("xx", "y");
  const substitutionResult = "xx".replaceAll("xx", "$&$&");
  const escapedResult = ".+*$.+*$".replaceAll(".+*$", "i");
  const globalRegexResult = "abcd".replaceAll(/./g, "a");
  const stickyGlobalResult = regexSubject.replaceAll(/[A-Z]/gy, "");

  assertEquals("a-b-c-d", subject.replaceAll("+", "-"));
  assertEquals("aaaa", globalRegexResult);
  assertEquals("b", emptySubject.replaceAll("", "b"));
  assertEquals("_x_x_x_", emptyPatternResult);
  assertEquals("yx", overlappingResult);
  assertEquals("xxxx", substitutionResult);
  assertEquals("ii", escapedResult);
  assertEquals("o Uppercase? NO UPPERCASE!", stickyGlobalResult);

  heapDumpReplaceAllState.subjects.push(subject, regexSubject, emptySubject);
  heapDumpReplaceAllState.results.push(
      subject.replaceAll("+", "-"), globalRegexResult, emptyPatternResult,
      overlappingResult, substitutionResult, escapedResult, stickyGlobalResult);
})();

(function symbolReplaceSearchValue() {
  const overrideLog = [];
  const searchValue = {
    [Symbol.replace](string, replacer) {
      overrideLog.push(["replace", string, replacer]);
      return {kind: "override-result", string, replacer};
    },
    toString() {
      throw new Error("toString should not run for callable Symbol.replace");
    },
  };
  const result = "barbar".replaceAll(searchValue, "moo");
  assertEquals("override-result", result.kind);
  assertEquals("barbar", result.string);
  assertEquals("moo", result.replacer);

  const nonCallableReplace = {
    [Symbol.replace]: "not-callable",
    toString() {
      throw new Error("toString should not run for non-callable Symbol.replace");
    },
  };
  const nonCallableReplaceError = assertThrows(
      () => "barbar".replaceAll(nonCallableReplace, "moo"), TypeError);

  const toStringReplacement = {
    toString() {
      overrideLog.push(["replacement-toString"]);
      return "boo";
    },
  };
  const coercedReplacement =
      "moomoo".replaceAll("moo", toStringReplacement);
  assertEquals("booboo", coercedReplacement);

  heapDumpReplaceAllState.searchValues.push(searchValue, nonCallableReplace);
  heapDumpReplaceAllState.replacements.push(toStringReplacement);
  heapDumpReplaceAllState.results.push(result, coercedReplacement);
  heapDumpReplaceAllState.logs.push(overrideLog);
  heapDumpReplaceAllState.errors.push(nonCallableReplaceError);
})();

(function callbackReplacementRecords() {
  const positions = [];
  const callback = function(searchString, position, string) {
    positions.push({searchString, position, string, thisValue: this});
    return {toString() { return "b"; }};
  };
  const callbackResult = "aca".replaceAll("a", callback);
  assertEquals("bcb", callbackResult);
  assertEquals([0, 2], positions.map((record) => record.position));
  assertEquals("aca", positions[1].string);

  heapDumpReplaceAllState.replacements.push(callback);
  heapDumpReplaceAllState.results.push(callbackResult);
  heapDumpReplaceAllState.logs.push(positions);
})();

(function regexpFailureCasesAndDescriptors() {
  class RegExpNonCoercibleFlags extends RegExp {
    static get [Symbol.species]() {
      return RegExp;
    }
    get flags() { return null; }
  }

  const nonGlobalError = assertThrows(
      () => "ab".replaceAll(/./, "."), TypeError);
  const stickyOnlyError = assertThrows(
      () => "ab".replaceAll(/./y, "."), TypeError);
  const flagsError = assertThrows(
      () => "barbar".replaceAll(new RegExpNonCoercibleFlags(), "moo"),
      TypeError);
  const nullReceiverError = assertThrows(
      () => String.prototype.replaceAll.call(null, "a", "b"), TypeError);

  const descriptor =
      Object.getOwnPropertyDescriptor(String.prototype, "replaceAll");
  assertEquals(String.prototype.replaceAll, descriptor.value);
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);

  heapDumpReplaceAllState.errors.push(
      nonGlobalError, stickyOnlyError, flagsError, nullReceiverError);
  heapDumpReplaceAllState.descriptors.push(descriptor);
  heapDumpReplaceAllState.searchValues.push(new RegExpNonCoercibleFlags());
})();

(function consStringSlowPath() {
  const consNeedle = %ConstructConsString("abcdefghijklmn", "def");
  const shortSubject = "a";
  const exactSubject = "abcdefghijklmndef";
  const noMatchSubject = "aaaaaaaaaaaaaaaaaa";
  const shortResult = shortSubject.replaceAll(consNeedle, "b");
  const exactResult = exactSubject.replaceAll(consNeedle, "b");
  const noMatchResult = noMatchSubject.replaceAll(consNeedle, "b");
  assertEquals("a", shortResult);
  assertEquals("b", exactResult);
  assertEquals("aaaaaaaaaaaaaaaaaa", noMatchResult);

  heapDumpReplaceAllState.subjects.push(
      shortSubject, exactSubject, noMatchSubject);
  heapDumpReplaceAllState.searchValues.push(consNeedle);
  heapDumpReplaceAllState.results.push(shortResult, exactResult, noMatchResult);
})();

keepReplaceAll("summary", {
  subjectCount: heapDumpReplaceAllState.subjects.length,
  searchValueCount: heapDumpReplaceAllState.searchValues.length,
  replacementCount: heapDumpReplaceAllState.replacements.length,
  resultCount: heapDumpReplaceAllState.results.length,
  logCount: heapDumpReplaceAllState.logs.length,
  errorCount: heapDumpReplaceAllState.errors.length,
});
