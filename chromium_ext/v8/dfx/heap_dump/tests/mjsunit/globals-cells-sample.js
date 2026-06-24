// Heap-dump shape sample for global bindings, script-context cells, and
// global-object property-cell churn.

var heapDumpGlobalVar = {kind: "var-binding"};
let heapDumpGlobalLet = {kind: "let-binding"};
const heapDumpGlobalConst = {kind: "const-binding"};

function heapDumpGlobalFunction(value) {
  return heapDumpGlobalLet.kind + ":" + value;
}

class HeapDumpGlobalClass {
  static staticObject = {kind: "static-field"};
  #secret = {kind: "private-field"};

  constructor(value) {
    this.value = value;
  }

  method() {
    return this.#secret.kind + ":" + this.value;
  }
}

const globalSymbolKey = Symbol("heap-dump-global-symbol-key");

globalThis.heapDumpDataCell = {state: "initial-object"};
globalThis.heapDumpDataCell = 17;
globalThis.heapDumpDataCell = 3.25;
globalThis.heapDumpDataCell = {state: "object-again"};

globalThis.heapDumpDeleteCell = {state: "before-delete"};
delete globalThis.heapDumpDeleteCell;
globalThis.heapDumpDeleteCell = {state: "after-delete"};

Object.defineProperty(globalThis, "heapDumpReadonlyCell", {
  value: {kind: "readonly-global-property"},
  writable: false,
  enumerable: true,
  configurable: true,
});

const accessorBacking = {value: {kind: "accessor-initial"}};
Object.defineProperty(globalThis, "heapDumpAccessorCell", {
  get() {
    return accessorBacking.value;
  },
  set(value) {
    accessorBacking.value = value;
  },
  enumerable: true,
  configurable: true,
});
globalThis.heapDumpAccessorCell = {kind: "accessor-updated"};

Object.defineProperty(globalThis, "heapDumpNonConfigurableCell", {
  value: {kind: "non-configurable"},
  writable: true,
  enumerable: true,
  configurable: false,
});

globalThis[globalSymbolKey] = {kind: "symbol-named-global"};

eval(`
  var heapDumpDirectEvalVar = {kind: "direct-eval-var"};
  function heapDumpDirectEvalFunction() {
    return heapDumpDirectEvalVar.kind;
  }
`);

(0, eval)(`
  var heapDumpIndirectEvalVar = {kind: "indirect-eval-var"};
  this.heapDumpIndirectEvalObject = {kind: "indirect-eval-object"};
`);

const dynamicName = "heapDumpDynamicGlobal_" + "name";
globalThis[dynamicName] = {kind: "dynamic-name-global"};

function makeGlobalClosure(tag) {
  let localCell = {kind: "closure-local", tag};
  return function globalClosure() {
    return [
      heapDumpGlobalVar,
      heapDumpGlobalLet,
      heapDumpGlobalConst,
      localCell,
      globalThis.heapDumpDataCell,
      globalThis.heapDumpAccessorCell,
    ];
  };
}

const globalClosure = makeGlobalClosure("kept");
const classInstance = new HeapDumpGlobalClass("instance");

assertEquals("let-binding:ok", heapDumpGlobalFunction("ok"));
assertEquals("private-field:instance", classInstance.method());
assertEquals("direct-eval-var", heapDumpDirectEvalFunction());
assertEquals("indirect-eval-var", heapDumpIndirectEvalVar.kind);
assertEquals("accessor-updated", globalThis.heapDumpAccessorCell.kind);
assertEquals("dynamic-name-global", globalThis[dynamicName].kind);

globalThis.__heapDumpGlobalsCellsKeepalive = {
  heapDumpGlobalVar,
  heapDumpGlobalLet,
  heapDumpGlobalConst,
  heapDumpGlobalFunction,
  HeapDumpGlobalClass,
  classInstance,
  staticObject: HeapDumpGlobalClass.staticObject,
  accessorBacking,
  globalClosure,
  closureValues: globalClosure(),
  directEvalVar: heapDumpDirectEvalVar,
  directEvalFunction: heapDumpDirectEvalFunction,
  indirectEvalVar: heapDumpIndirectEvalVar,
  indirectEvalObject: globalThis.heapDumpIndirectEvalObject,
  dataCell: globalThis.heapDumpDataCell,
  deleteCell: globalThis.heapDumpDeleteCell,
  readonlyCell: globalThis.heapDumpReadonlyCell,
  accessorCell: globalThis.heapDumpAccessorCell,
  nonConfigurableCell: globalThis.heapDumpNonConfigurableCell,
  symbolKey: globalSymbolKey,
  symbolGlobal: globalThis[globalSymbolKey],
  dynamicName,
  dynamicGlobal: globalThis[dynamicName],
};
