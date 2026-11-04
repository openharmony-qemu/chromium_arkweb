export const pendingObject = {kind: "tla-pending-object"};
export const pendingArray = [pendingObject, {kind: "tla-pending-array-object"}];

globalThis.__heapDumpTlaPendingHelperStarted = {
  pendingObject,
  pendingArray,
};

await globalThis.__heapDumpTlaNeverResolve;

export const unreachable = {kind: "tla-pending-unreachable"};
