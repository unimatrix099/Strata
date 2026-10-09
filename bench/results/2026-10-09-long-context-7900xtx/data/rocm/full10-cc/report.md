# Long-context suite: full10-cc (2026-10-09 15:13)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 106; engine error lines: 1

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 990.8 | 4.3 | 87.8 | ok (comp 0.586, rep8 1) |
| 16k | 15291 | 1401.9 | 10.9 | 91.6 | ok (comp 0.616, rep8 1) |
| 32k | 30019 | 1093.5 | 27.5 | 90.1 | ok (comp 0.594, rep8 1) |
| 64k | 61978 | 1160.1 | 53.4 | 96.8 | ok (comp 0.607, rep8 1) |
| 118k | 121332 | 1211.4 | 100.2 | 73.2 | ok (comp 0.62, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 7.7 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 5.7 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 5.6 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 28.3 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 27.1 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 12.9 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 44.6 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 37.0 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 22.0 |  |
| 128k@10 | True | glacier-falcon-670 | 126913 | 103.7 |  |
| 128k@50 | True | copper-lantern-529 | 126914 | 69.4 |  |
| 128k@90 | True | copper-willow-684 | 126913 | 86.5 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33711 | True | 7264 / Ilinca Moraru | 117.3 | ok (comp 0.492, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 116.5 | ok (comp 0.488, rep8 2) |
| error |  |  |  |  |  |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32176 | 48.2 | 2456 | 98.1 | [97.8, 97.9, 99.2, 98.1] | stop | ok (comp 0.497, rep8 1) |
| 64k | 66751 | 45.5 | 2863 | 96.2 | [96.4, 95.9, 93.7, 99.5] | stop | ok (comp 0.502, rep8 1) |
| 118k | 121555 | 79.3 | 2788 | 96.4 | [95.9, 98.6, 97.3, 94.5] | stop | ok (comp 0.508, rep8 2) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 8229 | 94.3 | [88.3, 94.5, 95.8, 99.5] | stop | ok (comp 0.421, rep8 5) |
| story-t0.7 | 11415 | 93.0 | [86.8, 88.4, 94.4, 104.2] | stop | ok (comp 0.395, rep8 6) |
| code-t0.0 | 16000 | 147.8 | [145.6, 158.7, 144.5, 143.6] | length | ok (comp 0.188, rep8 6) |
| code-t0.7 | 16000 | 142.3 | [132.9, 147.6, 148.8, 141.1] | length | ok (comp 0.217, rep8 5) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23390 | 23390 | 18.2 |  |  |  |
| part2 | 48218 | 24816 | 19.6 |  |  |  |
| part3 | 72344 | 24114 | 19.2 |  |  |  |
| part4 | 96239 | 23883 | 21.0 |  |  |  |
| recall | 96290 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
Memory access fault by GPU node-1 (Agent handle: 0x57a3683f2890) on address 0x57a3e859d000. Reason: Page not present or supervisor privilege.
```
