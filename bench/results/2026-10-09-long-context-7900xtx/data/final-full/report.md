# Long-context suite: final-full (2026-10-09 19:17)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 1170.8 | 3.6 | 92.1 | ok (comp 0.621, rep8 1) |
| 16k | 15288 | 2152.5 | 7.1 | 94.1 | ok (comp 0.623, rep8 1) |
| 32k | 30072 | 2726.2 | 11.0 | 94.9 | ok (comp 0.594, rep8 1) |
| 64k | 62045 | 3361.7 | 18.5 | 92.6 | ok (comp 0.618, rep8 1) |
| 118k | 121504 | 3458.5 | 35.1 | 73.0 | ok (comp 0.604, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 6.3 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 4.3 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 4.3 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 10.8 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 10.9 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 7.2 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 18.1 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 15.5 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 11.2 |  |
| 128k@10 | True | glacier-falcon-670 | 126907 | 36.1 |  |
| 128k@50 | True | copper-lantern-529 | 126908 | 26.4 |  |
| 128k@90 | True | copper-willow-684 | 126907 | 29.7 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33608 | True | 7264 / Ilinca Moraru | 121.4 | ok (comp 0.483, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 112.0 | ok (comp 0.488, rep8 2) |
| 118k | 118921 | True | 4631 / Mirela Toma | 109.8 | ok (comp 0.421, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32225 | 10.7 | 3158 | 99.5 | [100.4, 99.3, 100.3, 98.5] | stop | ok (comp 0.501, rep8 1) |
| 64k | 66848 | 19.0 | 3056 | 93.6 | [94.8, 92.2, 97.3, 90.6] | stop | ok (comp 0.491, rep8 1) |
| 118k | 121617 | 33.9 | 2942 | 94.9 | [98.4, 95.6, 95.2, 91.4] | stop | ok (comp 0.506, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 8252 | 98.1 | [91.5, 94.3, 100.5, 107.5] | stop | ok (comp 0.385, rep8 3) |
| story-t0.7 | 8833 | 94.5 | [88.2, 90.5, 95.6, 105.5] | stop | ok (comp 0.389, rep8 3) |
| code-t0.0 | 16000 | 150.4 | [147.1, 155.3, 152.8, 146.6] | length | ok (comp 0.197, rep8 6) |
| code-t0.7 | 14915 | 140.4 | [138.7, 144.0, 145.3, 134.4] | stop | ok (comp 0.209, rep8 7) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23304 | 23304 | 8.7 |  |  |  |
| part2 | 48272 | 24956 | 9.6 |  |  |  |
| part3 | 72393 | 24109 | 9.3 |  |  |  |
| part4 | 96179 | 23775 | 10.2 |  |  |  |
| recall | 96230 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata serve: prompt 48272 tokens = 23316 reused + 24956 read in 9558 ms (2611.1 tok/s), 12 generated in 111 ms (108.5 tok/s), drafts accepted 8 of 8, 4 checkpoints
strata serve: decode expert cache hit rate: 96.7% (6730 hits / 6960 lookups)
strata serve: KV streaming: 90.01% of 138642 block reads hit VRAM, 55.8 MiB read from RAM
strata serve: prompt 72393 tokens = 48284 reused + 24109 read in 9328 ms (2584.6 tok/s), 12 generated in 130 ms (92.5 tok/s), drafts accepted 7 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.2% (6996 hits / 7050 lookups)
strata serve: KV streaming: 91.59% of 206424 block reads hit VRAM, 69.9 MiB read from RAM
strata serve: prompt 96179 tokens = 72404 reused + 23775 read in 10171 ms (2337.6 tok/s), 12 generated in 125 ms (96.1 tok/s), drafts accepted 7 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.3% (7001 hits / 7050 lookups)
strata serve: KV streaming: 92.50% of 274206 block reads hit VRAM, 82.8 MiB read from RAM
strata serve: prompt 96230 tokens = 96190 reused + 40 read in 214 ms (187.0 tok/s), 17 generated in 221 ms (76.8 tok/s), drafts accepted 9 of 12, 6 checkpoints
strata serve: decode expert cache hit rate: 98.8% (13155 hits / 13320 lookups)
strata serve: KV streaming: 94.56% of 505290 block reads hit VRAM, 110.7 MiB read from RAM
```
