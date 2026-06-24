// Heap-dump module helper:
// This module is imported by module-namespace-sample.mjs and is also safe to
// run as a standalone mjsunit sample. Keeping it in the heap-dump corpus makes
// SourceTextModule/module context coverage visible even when no importer runs.

export let mutableCounter = 1;
export const exportedObject = { helper: true, nested: { value: 3 } };
export const exportedArray = [{ index: 0 }, { index: 1 }];
export function mutate(value) {
  mutableCounter = value;
  return mutableCounter;
}

export default {
  name: 'heap-dump-module-helper',
  exportedObject,
  exportedArray,
};

globalThis.heapDumpModuleNamespaceHelper = {
  exportedObject,
  exportedArray,
  mutate,
  defaultName: 'heap-dump-module-helper',
};
