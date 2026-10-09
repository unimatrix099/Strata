# Long-context suite: fused-quality (2026-10-09 18:11)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 1046.2 | 4.0 | 96.2 | ok (comp 0.587, rep8 1) |
| 16k | 15288 | 1798.5 | 8.5 | 101.2 | ok (comp 0.62, rep8 1) |
| 32k | 30072 | 2239.9 | 13.4 | 91.1 | ok (comp 0.603, rep8 1) |
| 64k | 62045 | 2695.1 | 23.0 | 90.6 | ok (comp 0.619, rep8 1) |
| 118k | 121504 | 2786.5 | 43.6 | 72.9 | ok (comp 0.604, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 7.2 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 5.2 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 5.2 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 14.0 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 14.0 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 9.2 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 22.8 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 19.1 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 13.9 |  |
| 128k@10 | True | glacier-falcon-670 | 126907 | 45.0 |  |
| 128k@50 | True | copper-lantern-529 | 126908 | 32.1 |  |
| 128k@90 | True | copper-willow-684 | 126907 | 36.4 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33608 | True | 7264 / Ilinca Moraru | 117.6 | ok (comp 0.483, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 125.1 | ok (comp 0.482, rep8 2) |
| 118k | 118921 | True | 4631 / Mirela Toma | 112.8 | ok (comp 0.474, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32225 | 13.2 | 2968 | 100.8 | [102.9, 101.2, 104.0, 95.2] | stop | ok (comp 0.506, rep8 1) |
| 64k | 66848 | 24.7 | 2422 | 93.6 | [91.0, 92.1, 94.3, 97.6] | stop | ok (comp 0.494, rep8 1) |
| 118k | 121617 | 42.5 | 2507 | 93.6 | [98.2, 92.3, 97.1, 88.1] | stop | ok (comp 0.512, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 12848 | 126.2 | [92.7, 99.4, 177.9, 192.3] | stop | BAD: an 8-word sequence repeated 17 times |
| story-t0.7 | 8295 | 87.5 | [85.4, 85.2, 90.5, 89.2] | stop | ok (comp 0.456, rep8 1) |
| code-t0.0 | 16000 | 150.8 | [144.4, 150.8, 155.9, 152.5] | length | ok (comp 0.214, rep8 6) |
| code-t0.7 | 14993 | 136.5 | [137.3, 138.9, 137.9, 132.2] | stop | ok (comp 0.175, rep8 6) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23304 | 23304 | 10.6 |  |  |  |
| part2 | 48272 | 24955 | 12.0 |  |  |  |
| part3 | 72393 | 24110 | 11.4 |  |  |  |
| part4 | 96179 | 23775 | 12.2 |  |  |  |
| recall | 96230 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata serve: suffix drafts: 1 windows, 4 of 4 drafts accepted
strata serve: prompt 72393 tokens = 48283 reused + 24110 read in 11433 ms (2108.8 tok/s), 12 generated in 116 ms (103.1 tok/s), drafts accepted 8 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.1% (7406 hits / 7470 lookups)
strata serve: KV streaming: 92.42% of 227994 block reads hit VRAM, 69.6 MiB read from RAM
strata serve: suffix drafts: 1 windows, 4 of 4 drafts accepted
strata serve: prompt 96179 tokens = 72404 reused + 23775 read in 12234 ms (1943.3 tok/s), 12 generated in 121 ms (99.3 tok/s), drafts accepted 8 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.3% (7449 hits / 7500 lookups)
strata serve: KV streaming: 93.25% of 308106 block reads hit VRAM, 83.8 MiB read from RAM
strata serve: suffix drafts: 1 windows, 4 of 4 drafts accepted
strata serve: prompt 96230 tokens = 96190 reused + 40 read in 214 ms (187.3 tok/s), 17 generated in 228 ms (74.7 tok/s), drafts accepted 9 of 11, 6 checkpoints
strata serve: decode expert cache hit rate: 98.9% (14152 hits / 14310 lookups)
strata serve: KV streaming: 94.89% of 554592 block reads hit VRAM, 114.2 MiB read from RAM
```
