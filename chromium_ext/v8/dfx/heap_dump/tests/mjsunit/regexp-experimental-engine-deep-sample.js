// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --enable-experimental-regexp-engine
// Flags: --default-to-experimental-regexp-engine
// Flags: --experimental-regexp-engine-capture-group-opt
// Flags: --no-force-slow-path

const retained = [];

function keep(name, value) {
  retained.push({ name, value });
  return value;
}

function testExperimental(regexp, subject, expected, expectedLastIndex) {
  assertEquals("EXPERIMENTAL", %RegexpTypeTag(regexp));
  const result = regexp.exec(subject);
  if (expected === null) {
    assertEquals(null, result);
  } else {
    assertArrayEquals(expected, result);
  }
  assertEquals(expectedLastIndex, regexp.lastIndex);
  keep("experimental:" + regexp.source + ":" + regexp.flags, {
    regexp,
    subject,
    result,
  });
}

testExperimental(new RegExp(""), "asdf", [""], 0);
testExperimental(/asdf1/, "123asdf1xyz", ["asdf1"], 0);
testExperimental(/\*\.\(\[\]\?/, "123*.([]?123", ["*.([]?"], 0);
testExperimental(/[abc]/, "123asdf", ["a"], 0);
testExperimental(/(?:asdf)|123/, "xyz123asdf", ["123"], 0);
testExperimental(/(123)|(xyz)/, "xyz", ["xyz", undefined, "xyz"], 0);
testExperimental(/(?:(a)|b){2,8}/, "aba", ["aba", "a"], 0);
testExperimental(/^a/m, "x\na", ["a"], 0);
testExperimental(/asdf.xyz/s, "asdf\nxyz", ["asdf\nxyz"], 0);
testExperimental(/ab(?<=a(?<=a)b)(c)/, "abc", ["abc", "c"], 0);

const globalExperimental = /asdf/g;
testExperimental(globalExperimental, "fjasdfkkasdf", ["asdf"], 6);

const stickyExperimental = /asdf/y;
stickyExperimental.lastIndex = 2;
testExperimental(stickyExperimental, "fjasdfkkasdf", ["asdf"], 6);

const linearPlain = keep("linear-plain", /asdf/l);
const linearAlternation = keep("linear-alternation", /123|asdf/l);
const linearNested = keep("linear-nested", /(a*)*x/l);
assertEquals("EXPERIMENTAL", %RegexpTypeTag(linearPlain));
assertEquals("EXPERIMENTAL", %RegexpTypeTag(linearAlternation));
assertEquals("EXPERIMENTAL", %RegexpTypeTag(linearNested));
assertTrue((/asdf/lymsg).linear);
assertFalse((/asdf/ymsg).linear);
assertThrows(() => /(a*)\1/l, SyntaxError);

keep("linear-results", {
  plain: linearPlain.exec("zzasdf"),
  alternation: linearAlternation.exec("xx123yy"),
  nested: linearNested.exec("aaaaax"),
});

const unsupportedGlobalLookbehind = keep("unsupported-global-lookbehind",
                                        /ab(?<=b)c/g);
const unsupportedStickyLookbehind = keep("unsupported-sticky-lookbehind",
                                        /ab(?<=b)c/y);
assertNotEquals("EXPERIMENTAL", %RegexpTypeTag(unsupportedGlobalLookbehind));
assertNotEquals("EXPERIMENTAL", %RegexpTypeTag(unsupportedStickyLookbehind));

const customLinearGetter = keep("custom-linear-getter", /./);
Object.defineProperty(customLinearGetter, "linear", {
  get() {
    return true;
  },
});
assertEquals("l", customLinearGetter.flags);

globalThis.__heap_dump_regexp_experimental_engine = retained;
