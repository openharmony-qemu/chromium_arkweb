export const resolvedObject = {kind: "tla-resolved-object"};
export const resolvedArray = [1, 2, {kind: "tla-resolved-array-object"}];

const awaitedValue = await Promise.resolve({
  kind: "tla-awaited-value",
  timestamp: 20260525,
});

export const awaited = awaitedValue;
export function readAwaitedKind() {
  return awaitedValue.kind;
}

globalThis.__heapDumpTlaResolvedHelperKeepalive = {
  resolvedObject,
  resolvedArray,
  awaitedValue,
  readAwaitedKind,
};
