# Long-context suite: s4-wmma-quality (2026-10-09 18:26)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 32k@10 | True | meadow-copper-504 | 33502 | 14.0 |  |
| 32k@50 | True | falcon-quartz-940 | 33502 | 12.2 |  |
| 32k@90 | True | willow-cobalt-696 | 33503 | 8.2 |  |
| 128k@10 | True | falcon-saffron-138 | 126909 | 38.0 |  |
| 128k@50 | True | quartz-tundra-528 | 126907 | 35.6 |  |
| 128k@90 | True | quartz-glacier-192 | 126907 | 35.2 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 64k | 67021 | True | 9015 / Ilinca Moraru | 121.1 | ok (comp 0.462, rep8 2) |
| 118k | 121430 | True | 3938 / Teodora Vlasic | 108.6 | ok (comp 0.506, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 118k | 121077 | 36.1 | 2000 | 91.1 | [89.0, 86.2, 94.8, 95.9] | length | ok (comp 0.5, rep8 1) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23181 | 23181 | 9.5 |  |  |  |
| part2 | 48257 | 25064 | 9.9 |  |  |  |
| part3 | 72237 | 23968 | 9.4 |  |  |  |
| part4 | 96069 | 23820 | 10.4 |  |  |  |
| recall | 96120 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata serve: prompt 48257 tokens = 23193 reused + 25064 read in 9925 ms (2525.2 tok/s), 12 generated in 110 ms (108.9 tok/s), drafts accepted 8 of 8, 4 checkpoints
strata serve: decode expert cache hit rate: 99.3% (6671 hits / 6720 lookups)
strata serve: KV streaming: 89.60% of 138636 block reads hit VRAM, 58.1 MiB read from RAM
strata serve: prompt 72237 tokens = 48269 reused + 23968 read in 9419 ms (2544.6 tok/s), 12 generated in 123 ms (97.7 tok/s), drafts accepted 7 of 7, 6 checkpoints
strata serve: decode expert cache hit rate: 99.5% (6924 hits / 6960 lookups)
strata serve: KV streaming: 91.16% of 206412 block reads hit VRAM, 73.5 MiB read from RAM
strata serve: prompt 96069 tokens = 72249 reused + 23820 read in 10419 ms (2286.2 tok/s), 12 generated in 125 ms (95.7 tok/s), drafts accepted 7 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.6% (7022 hits / 7050 lookups)
strata serve: KV streaming: 92.25% of 274194 block reads hit VRAM, 85.6 MiB read from RAM
strata serve: prompt 96120 tokens = 96080 reused + 40 read in 280 ms (142.8 tok/s), 17 generated in 186 ms (91.6 tok/s), drafts accepted 10 of 10, 6 checkpoints
strata serve: decode expert cache hit rate: 99.3% (11163 hits / 11240 lookups)
strata serve: KV streaming: 94.33% of 483690 block reads hit VRAM, 110.5 MiB read from RAM
```
