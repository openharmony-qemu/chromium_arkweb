// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep RegExp override and string-regexp builtin result shapes alive for heap
// dump smoke. This follows V8 regexp override mjsunit patterns while retaining
// the overridden functions, custom exec result objects, matchAll iterators, and
// representative error objects.

(function() {
  'use strict';

  const catalog = {};
  globalThis.__heapDumpRegExpOverrides = catalog;

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (e) {
      return keep(name, e);
    }
    return keep(name, null);
  }

  const originalExec = RegExp.prototype.exec;
  const originalMatch = RegExp.prototype[Symbol.match];
  const originalReplace = RegExp.prototype[Symbol.replace];
  const originalSplit = RegExp.prototype[Symbol.split];
  const originalMatchAll = RegExp.prototype[Symbol.matchAll];

  const subject = keep('subject', 'baa heap-dump-123 heap-dump-456');
  const baseRegExps = keep('baseRegExps', {
    search: /a/,
    match: /a./,
    split: /a/,
    replace: /heap-(dump)-(\d+)/g,
    matchAll: /heap-(?<kind>dump)-(?<id>\d+)/dg,
  });

  try {
    keep('baseline', {
      search: 'baa'.search(baseRegExps.search),
      match: 'baa'.match(baseRegExps.match),
      split: 'baa'.split(baseRegExps.split),
      replace: subject.replace(baseRegExps.replace, '$1:$2'),
      matchAllIterator: subject.matchAll(baseRegExps.matchAll),
    });
    keep('baselineMatchAllResults',
         Array.from(catalog.baseline.matchAllIterator));

    const customExecResult = keep('customExecResult', {
      index: 3,
      input: subject,
      0: 'custom-match',
      1: 'custom-capture',
      groups: {kind: 'custom'},
    });
    const execOverrideLog = keep('execOverrideLog', []);
    const execOverride = keep('execOverride', function execOverride() {
      execOverrideLog.push({receiver: this, lastIndex: this.lastIndex});
      return customExecResult;
    });
    RegExp.prototype.exec = execOverride;
    keep('execOverrideResults', {
      search: subject.search(/heap/),
      match: subject.match(/heap-(dump)/),
      replace: subject.replace(/heap-(dump)/, 'overridden'),
    });

    RegExp.prototype.exec = keep('nullExecOverride', function nullExecOverride() {
      execOverrideLog.push({receiver: this, nullResult: true});
      return null;
    });
    keep('nullExecSplitResult', 'baa'.split(/a/));

    RegExp.prototype.exec = originalExec;
    const symbolOverrideLog = keep('symbolOverrideLog', []);
    RegExp.prototype[Symbol.match] = keep(
        'symbolMatchOverride',
        function symbolMatchOverride(string) {
          symbolOverrideLog.push(['match', this, string]);
          return {kind: 'symbol-match-result', string};
        });
    RegExp.prototype[Symbol.replace] = keep(
        'symbolReplaceOverride',
        function symbolReplaceOverride(string, replacement) {
          symbolOverrideLog.push(['replace', this, string, replacement]);
          return {kind: 'symbol-replace-result', replacement};
        });
    RegExp.prototype[Symbol.split] = keep(
        'symbolSplitOverride',
        function symbolSplitOverride(string, limit) {
          symbolOverrideLog.push(['split', this, string, limit]);
          return {kind: 'symbol-split-result', limit};
        });
    RegExp.prototype[Symbol.matchAll] = keep(
        'symbolMatchAllOverride',
        function symbolMatchAllOverride(string) {
          symbolOverrideLog.push(['matchAll', this, string]);
          return [{kind: 'symbol-match-all-result', string}][Symbol.iterator]();
        });

    keep('symbolOverrideResults', {
      match: subject.match(/heap/),
      replace: subject.replace(/heap/, 'replacement'),
      split: subject.split(/heap/, 2),
      matchAll: Array.from(subject.matchAll(/heap/g)),
    });

    class HeapDumpRegExpSubclass extends RegExp {
      exec(string) {
        const result = super.exec(string);
        if (result !== null) {
          result.subclassMarker = {kind: 'regexp-subclass-result'};
        }
        return result;
      }
    }
    const subclassRegExp = keep(
        'subclassRegExp',
        new HeapDumpRegExpSubclass('heap-(dump)-(\\d+)', 'dg'));
    keep('subclassExecResult', subclassRegExp.exec(subject));
    subclassRegExp.lastIndex = 0;
    keep('subclassMatchAllResults', Array.from(subject.matchAll(subclassRegExp)));

    RegExp.prototype[Symbol.match] = originalMatch;
    RegExp.prototype[Symbol.replace] = originalReplace;
    RegExp.prototype[Symbol.split] = originalSplit;
    RegExp.prototype[Symbol.matchAll] = originalMatchAll;

    keepError('badExecReturnError', () => {
      const bad = /heap/;
      bad.exec = function badExec() {
        return 17;
      };
      subject.match(bad);
    });
    keepError('badMatchAllReturnError', () => {
      RegExp.prototype[Symbol.matchAll] = function badMatchAll() {
        return 17;
      };
      subject.matchAll(/heap/g).next();
    });
  } finally {
    RegExp.prototype.exec = originalExec;
    RegExp.prototype[Symbol.match] = originalMatch;
    RegExp.prototype[Symbol.replace] = originalReplace;
    RegExp.prototype[Symbol.split] = originalSplit;
    RegExp.prototype[Symbol.matchAll] = originalMatchAll;
  }

  assertEquals(1, catalog.baseline.search);
  assertEquals('aa', catalog.baseline.match[0]);
  assertEquals('custom-match', catalog.execOverrideResults.match[0]);
  assertEquals('symbol-match-result', catalog.symbolOverrideResults.match.kind);
  assertEquals('symbol-replace-result',
               catalog.symbolOverrideResults.replace.kind);
  assertEquals('symbol-split-result', catalog.symbolOverrideResults.split.kind);
  assertEquals('symbol-match-all-result',
               catalog.symbolOverrideResults.matchAll[0].kind);
  assertEquals('regexp-subclass-result',
               catalog.subclassExecResult.subclassMarker.kind);
  assertTrue(catalog.badExecReturnError instanceof TypeError);
  assertTrue(catalog.badMatchAllReturnError instanceof TypeError);
})();
