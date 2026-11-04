// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep RegExpData variants alive for heap dump smoke. The sample separates
// atom-like patterns from complex IrRegExp-style patterns and drives exec,
// test, matchAll, split, replace, unicode, indices, sticky, and global paths.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeUnicodePropertyRegExp(flags) {
    try {
      return new RegExp('\\p{Script=Greek}+', flags);
    } catch (e) {
      keep('unicodePropertyRegExpError_' + flags, e);
      return new RegExp('[\\u0370-\\u03ff]+', flags);
    }
  }

  const atomRegexps = keep('atomRegexps', [
    /heapdump/,
    /heapdump/g,
    /heapdump/y,
    new RegExp('heapdump'),
    new RegExp('HEAPDUMP', 'i'),
    new RegExp('heap-dump-literal'),
  ]);

  const complexRegexps = keep('complexRegexps', [
    /(?<kind>heap)-(?<name>dump)-(\d+)/g,
    /(?<=heap-)dump(?=-\d+)/g,
    /(["'])(?<quoted>.*?)\1/g,
    makeUnicodePropertyRegExp('u'),
    /heap(?:-|\s+)dump/iu,
    /^heap.*dump$/ms,
    /(?:heap|dump){2,4}/y,
  ]);

  try {
    complexRegexps.push(new RegExp('(?<word>heap)-(dump)', 'dg'));
  } catch (e) {
    keep('indicesRegExpError', e);
  }

  const subjects = keep('subjects', [
    'heapdump heap-dump-literal heap-dump-1 heap-dump-22',
    '"quoted-one" \'quoted-two\' \u03b1\u03b2\u03b3 heap dump',
    'heap\nmiddle\ndump',
    'heapdumpheapdump',
  ]);

  const atomResults = keep('atomResults', []);
  for (let i = 0; i < 48; ++i) {
    const regexp = atomRegexps[i % atomRegexps.length];
    regexp.lastIndex = 0;
    atomResults.push({
      source: regexp.source,
      flags: regexp.flags,
      test: regexp.test(subjects[0]),
      exec: regexp.exec(subjects[0]),
      lastIndex: regexp.lastIndex,
    });
  }

  const complexResults = keep('complexResults', []);
  for (const regexp of complexRegexps) {
    regexp.lastIndex = 0;
    const subject = regexp.unicode ? subjects[1] : subjects[0] + ' ' + subjects[1];
    const result = {
      source: regexp.source,
      flags: regexp.flags,
      exec: regexp.exec(subject),
      split: subject.split(regexp).slice(0, 6),
      replace: subject.replace(regexp, 'REGEXP'),
      lastIndexAfterExec: regexp.lastIndex,
    };
    regexp.lastIndex = 0;
    if (regexp.global) {
      result.matchAll = Array.from(subject.matchAll(regexp)).slice(0, 4);
    }
    complexResults.push(result);
  }

  const repeatedConstructed = keep('repeatedConstructed', []);
  for (let i = 0; i < 64; ++i) {
    const pattern = i % 2 === 0 ?
        'heap-dump-(?<index>\\d+)' :
        '(?:heap|dump)-variant-' + (i % 8);
    const flags = i % 3 === 0 ? 'giu' : (i % 3 === 1 ? 'g' : 'i');
    const regexp = new RegExp(pattern, flags);
    const subject = 'heap-dump-' + i + ' heap-variant-' + (i % 8);
    regexp.lastIndex = 0;
    repeatedConstructed.push({
      regexp,
      result: regexp.exec(subject),
      subject,
    });
  }

  const sticky = keep('sticky', /heap|dump/y);
  const stickyResults = keep('stickyResults', []);
  sticky.lastIndex = 0;
  stickyResults.push(sticky.exec('heapdump'));
  stickyResults.push(sticky.exec('heapdump'));
  sticky.lastIndex = 4;
  stickyResults.push(sticky.exec('heapdump'));

  const dotAll = keep('dotAll', /heap.*dump/s);
  const unicodeWord = keep('unicodeWord', (() => {
    try {
      return new RegExp('\\p{Letter}+', 'gu');
    } catch (e) {
      keep('unicodeLetterRegExpError', e);
      return /[A-Za-z\u0370-\u03ff]+/gu;
    }
  })());
  const iteratorResults = keep('iteratorResults', [
    Array.from('heap\n\n dump'.matchAll(/heap|dump/g)),
    Array.from('\u03b1\u03b2 heap \u03b3\u03b4'.matchAll(unicodeWord)),
  ]);

  assertTrue(atomResults[0].test);
  assertEquals('heapdump', atomResults[0].exec[0]);
  assertEquals('dump', complexResults[0].exec.groups.name);
  assertEquals('quoted-one', complexResults[2].exec.groups.quoted);
  assertEquals('\u03b1\u03b2\u03b3', complexResults[3].exec[0]);
  assertEquals('heap', stickyResults[0][0]);
  assertEquals('dump', stickyResults[2][0]);
  assertEquals('heap\n\n dump', dotAll.exec('heap\n\n dump')[0]);
  assertTrue(repeatedConstructed[0].result.groups.index === '0');
  assertEquals('heap', iteratorResults[0][0][0]);

  globalThis.heapDumpRegExpDataVariantsDeepSample = catalog;
})();
