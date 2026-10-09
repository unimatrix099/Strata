# Long-context suite: full79-cc (2026-10-09 14:42)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 910.6 | 4.6 | 93.6 | ok (comp 0.612, rep8 1) |
| 16k | 15291 | 1304.4 | 11.7 | 93.9 | ok (comp 0.607, rep8 1) |
| 32k | 30019 | 1177.2 | 25.5 | 88.2 | ok (comp 0.593, rep8 1) |
| 64k | 61978 | 1201.9 | 51.6 | 91.3 | ok (comp 0.614, rep8 1) |
| 118k | 121332 | 1200.1 | 101.1 | 78.0 | ok (comp 0.6, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 7.7 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 5.7 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 5.7 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 27.3 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 27.5 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 13.6 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 47.0 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 37.4 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 26.4 |  |
| 128k@10 | True | glacier-falcon-670 | 126913 | 116.1 |  |
| 128k@50 | True | copper-lantern-529 | 126914 | 74.4 |  |
| 128k@90 | True | copper-willow-684 | 126913 | 86.7 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33711 | True | 7264 / Ilinca Moraru | 111.4 | ok (comp 0.487, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 117.2 | ok (comp 0.498, rep8 2) |
| 118k | 119070 | True | 4631 / Mirela Toma | 111.9 | ok (comp 0.444, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32176 | 22.2 | 2784 | 103.9 | [105.6, 105.3, 101.0, 103.8] | stop | ok (comp 0.493, rep8 1) |
| 64k | 66751 | 56.5 | 2624 | 96.4 | [90.9, 99.3, 100.6, 96.3] | stop | ok (comp 0.5, rep8 1) |
| 118k | 121555 | 91.2 | 2794 | 96.3 | [96.9, 95.7, 96.2, 96.4] | stop | ok (comp 0.504, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 8632 | 104.1 | [91.0, 93.9, 98.3, 151.3] | stop | ok (comp 0.397, rep8 8) |
| story-t0.7 | 13032 | 92.5 | [86.9, 89.7, 92.3, 102.8] | stop | ok (comp 0.423, rep8 3) |
| code-t0.0 | 16000 | 152.8 | [150.2, 154.4, 153.8, 153.1] | length | ok (comp 0.196, rep8 6) |
| code-t0.7 | 16000 | 140.4 | [140.6, 154.6, 149.8, 121.6] | length | ok (comp 0.231, rep8 5) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23390 | 23390 | 18.9 |  |  |  |
| part2 | 48218 | 24816 | 21.9 |  |  |  |
| part3 | 72344 | 24114 | 19.8 |  |  |  |
| part4 | 96239 | 23883 | 20.5 |  |  |  |
| recall | 96290 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata serve: prompt 48218 tokens = 23402 reused + 24816 read in 21948 ms (1130.7 tok/s), 12 generated in 124 ms (96.5 tok/s), drafts accepted 8 of 8, 4 checkpoints
strata serve: decode expert cache hit rate: 98.5% (6617 hits / 6720 lookups)
strata serve: KV streaming: 89.89% of 141726 block reads hit VRAM, 57.7 MiB read from RAM
strata serve: prompt 72344 tokens = 48230 reused + 24114 read in 19805 ms (1217.6 tok/s), 12 generated in 127 ms (94.5 tok/s), drafts accepted 7 of 7, 6 checkpoints
strata serve: decode expert cache hit rate: 98.6% (6889 hits / 6990 lookups)
strata serve: KV streaming: 91.56% of 209508 block reads hit VRAM, 71.2 MiB read from RAM
strata serve: prompt 96239 tokens = 72356 reused + 23883 read in 20489 ms (1165.7 tok/s), 12 generated in 133 ms (90.2 tok/s), drafts accepted 7 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.2% (6997 hits / 7050 lookups)
strata serve: KV streaming: 92.54% of 277290 block reads hit VRAM, 83.3 MiB read from RAM
strata serve: prompt 96290 tokens = 96250 reused + 40 read in 214 ms (186.7 tok/s), 17 generated in 215 ms (79.1 tok/s), drafts accepted 8 of 9, 6 checkpoints
strata serve: decode expert cache hit rate: 98.8% (11460 hits / 11600 lookups)
strata serve: KV streaming: 94.48% of 499134 block reads hit VRAM, 110.9 MiB read from RAM
```
