# Long-context suite: r10-longctx (2026-10-09 13:57)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 106; engine error lines: 1

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 989.1 | 4.3 | 87.2 | ok (comp 0.586, rep8 1) |
| 16k | 15291 | 1271.2 | 12.0 | 93.6 | ok (comp 0.603, rep8 1) |
| 32k | 30019 | 1217.5 | 24.7 | 91.2 | ok (comp 0.594, rep8 1) |
| 64k | 61978 | 1409.0 | 44.0 | 97.7 | ok (comp 0.607, rep8 1) |
| 118k | 121332 | 1334.5 | 90.9 | 72.5 | ok (comp 0.621, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 9.0 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 5.7 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 5.6 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 25.9 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 23.8 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 12.4 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 49.3 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 34.6 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 23.8 |  |
| 128k@10 | True | glacier-falcon-670 | 126913 | 93.9 |  |
| 128k@50 | True | copper-lantern-529 | 126914 | 60.8 |  |
| 128k@90 | True | copper-willow-684 | 126913 | 73.3 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33711 | True | 7264 / Ilinca Moraru | 112.3 | ok (comp 0.481, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 116.5 | ok (comp 0.487, rep8 2) |
| 118k | 119070 | True | 4631 / Mirela Toma | 106.7 | ok (comp 0.484, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32176 | 22.5 | 2909 | 98.7 | [100.5, 95.3, 102.4, 97.2] | stop | ok (comp 0.495, rep8 2) |
| 64k | 66751 | 48.7 | 2791 | 96.1 | [91.8, 93.6, 102.5, 97.2] | stop | ok (comp 0.491, rep8 1) |
| 118k | 121555 | 83.6 | 3077 | 95.8 | [92.9, 96.8, 97.0, 96.7] | stop | ok (comp 0.501, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 12487 | 124.0 | [95.5, 113.1, 144.1, 166.1] | stop | BAD: a highly repetitive stretch (compresses to 0.098); an 8-word sequence repeated 22 times |
| story-t0.7 | 10396 | 91.3 | [87.5, 89.8, 91.7, 97.0] | stop | ok (comp 0.446, rep8 3) |
| code-t0.0 | None | None | [] | None | ok (comp None, rep8 None) |
| code-t0.7 | 16000 | 141.7 | [137.6, 144.1, 146.6, 139.1] | length | ok (comp 0.235, rep8 7) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23390 | 23390 | 16.2 |  |  |  |
| part2 | 48218 | 24816 | 19.6 |  |  |  |
| part3 | 72344 | 24114 | 19.6 |  |  |  |
| part4 | 96239 | 23884 | 19.4 |  |  |  |
| recall | 96290 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
Memory access fault by GPU node-1 (Agent handle: 0x5818d4649890) on address 0x58195a6ae000. Reason: Page not present or supervisor privilege.
```
