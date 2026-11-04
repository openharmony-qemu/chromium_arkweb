// Heap-dump shape sample for RegExp boilerplates, RegExpData variants, match
// info arrays, template objects, and nested literal boilerplates.

const keepalive = {};

function keep(name, value) {
  keepalive[name] = value;
  return value;
}

function tagForHeapDump(strings, ...values) {
  return {
    strings,
    raw: strings.raw,
    values,
    joined: strings.join("|") + ":" + values.join("|"),
  };
}

function makeUnicodePropertyRegExp() {
  try {
    return new RegExp("\\p{Script=Greek}+", "u");
  } catch (e) {
    keep("unicodePropertyRegExpError", e);
    return /[\u0370-\u03ff]+/u;
  }
}

function makeLiteralCatalog(seed) {
  const atomLiteral = /heapdump/;
  const globalAtomLiteral = /heapdump/g;
  const complexLiteral = /(?<prefix>heap)-(?<name>dump)-(\d+)/giu;
  const stickyLiteral = /(?:heap|dump)+/y;
  const backrefLiteral = /(["'])(.*?)\1/g;
  const unicodePropertyLiteral = makeUnicodePropertyRegExp();
  const lookaroundLiteral = /(?<=heap-)dump(?=-\d+)/g;

  const subject = `heapdump heap-dump-${seed} "quoted-${seed}" ` +
      `\u03b1\u03b2 heap-dump-${seed + 1}`;
  const literalResults = {
    atom: atomLiteral.exec(subject),
    globalAtom: [globalAtomLiteral.exec(subject), globalAtomLiteral.exec(subject)],
    complex: Array.from(subject.matchAll(complexLiteral)),
    sticky: stickyLiteral.exec("heapdump"),
    backref: Array.from(subject.matchAll(backrefLiteral)),
    unicodeProperty: unicodePropertyLiteral.exec(subject),
    lookaround: Array.from(subject.matchAll(lookaroundLiteral)),
  };

  const constructed = {
    atom: new RegExp("heapdump"),
    escaped: new RegExp("\\\\heap\\\\dump"),
    named: new RegExp("(?<kind>heap)-(?<name>dump)-(\\d+)", "giu"),
    multiline: new RegExp("^heap", "m"),
    dotAll: new RegExp("heap.*dump", "s"),
  };
  const constructedResults = {
    atom: constructed.atom.exec(subject),
    escaped: constructed.escaped.exec("\\heap\\dump"),
    named: Array.from(subject.matchAll(constructed.named)),
    multiline: constructed.multiline.exec("miss\nheap"),
    dotAll: constructed.dotAll.exec("heap\n\n dump"),
  };

  const literalObject = {
    seed,
    atomLiteral,
    globalAtomLiteral,
    complexLiteral,
    stickyLiteral,
    backrefLiteral,
    unicodePropertyLiteral,
    lookaroundLiteral,
    nested: {
      constructed,
      literalResults,
      constructedResults,
    },
  };
  const literalArray = [
    atomLiteral,
    complexLiteral,
    constructed.named,
    literalResults.complex,
    constructedResults.named,
    {seed, subject},
  ];
  const templateResult = tagForHeapDump`heap-${seed}-dump-${subject}`;

  return {
    subject,
    literalObject,
    literalArray,
    templateResult,
  };
}

const catalogs = keep("catalogs", [
  makeLiteralCatalog(7),
  makeLiteralCatalog(8),
  makeLiteralCatalog(9),
]);

const sharedLiteralFactoryResults = keep("sharedLiteralFactoryResults", []);
function sharedLiteralFactory(value) {
  return {
    regexp: /shared-(?<value>\d+)/g,
    template: tagForHeapDump`shared-${value}`,
    objectLiteral: {
      value,
      nested: {regexp: /nested-shared/},
      array: [/array-shared/, {value}],
    },
  };
}
for (let i = 0; i < 8; ++i) {
  const entry = sharedLiteralFactory(i);
  entry.regexp.exec(`shared-${i}`);
  sharedLiteralFactoryResults.push(entry);
}

assertEquals("heapdump", catalogs[0].literalObject.nested.literalResults.atom[0]);
assertEquals("dump",
             catalogs[0].literalObject.nested.literalResults.complex[0].groups.name);
assertEquals("quoted-7",
             catalogs[0].literalObject.nested.literalResults.backref[0][2]);
assertEquals("\u03b1\u03b2",
             catalogs[0].literalObject.nested.literalResults.unicodeProperty[0]);
assertEquals("dump",
             catalogs[0].literalObject.nested.literalResults.lookaround[0][0]);
assertEquals("heap", catalogs[0].literalObject.nested.constructedResults.named[0].groups.kind);
sharedLiteralFactoryResults[7].regexp.lastIndex = 0;
assertEquals("7", sharedLiteralFactoryResults[7].regexp.exec("shared-7")[1]);

globalThis.__heapDumpRegExpBoilerplateDeepKeepalive = keepalive;
