# Long-context suite: full714-cc-1 (2026-10-09 16:16)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 106; engine error lines: 0

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 668.3 | 6.3 | 85.8 | ok (comp 0.586, rep8 1) |
| 16k | 15262 | 771.3 | 19.8 | 92.4 | ok (comp 0.614, rep8 1) |
| 32k | 30047 | 717.7 | 41.9 | 88.7 | ok (comp 0.607, rep8 1) |
| 64k | 62013 | 672.5 | 92.2 | 96.1 | ok (comp 0.605, rep8 1) |
| 118k | 121363 | 646.3 | 187.8 | 68.6 | ok (comp 0.597, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 11.5 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 9.6 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 9.6 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 47.1 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 47.6 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 24.9 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 90.4 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 71.1 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 44.7 |  |
| 128k@10 | True | glacier-falcon-670 | 126921 | 190.6 |  |
| 128k@50 | True | copper-lantern-529 | 126922 | 113.2 |  |
| 128k@90 | True | copper-willow-684 | 126921 | 148.6 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33643 | True | 7264 / Ilinca Moraru | 114.7 | ok (comp 0.484, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 108.5 | ok (comp 0.488, rep8 2) |
| 118k | 119066 | True | 4631 / Mirela Toma | 111.3 | ok (comp 0.453, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32210 | 42.5 | 2914 | 103.8 | [102.8, 108.8, 100.9, 103.4] | stop | ok (comp 0.502, rep8 1) |
| 64k | 66796 | 97.9 | 3586 | 101.7 | [96.2, 100.4, 103.6, 107.8] | stop | ok (comp 0.493, rep8 2) |
| 118k | 121552 | 178.9 | 2551 | 93.4 | [93.8, 94.8, 93.2, 92.7] | stop | ok (comp 0.487, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 14453 | 117.9 | [90.1, 103.4, 134.8, 174.1] | stop | BAD: an 8-word sequence repeated 60 times |
| story-t0.7 | 10919 | 91.2 | [86.7, 89.5, 92.3, 97.2] | stop | ok (comp 0.423, rep8 3) |
| code-t0.0 | 16000 | 147.0 | [138.4, 147.9, 151.3, 151.0] | length | ok (comp 0.244, rep8 5) |
| code-t0.7 | 16000 | 135.4 | [138.2, 143.3, 129.6, 131.6] | length | ok (comp 0.196, rep8 7) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23307 | 23307 | 32.8 |  |  |  |
| part2 | 48164 | 24845 | 31.6 |  |  |  |
| part3 | 72282 | 24106 | 35.7 |  |  |  |
| part4 | 96124 | 23831 | 33.0 |  |  |  |
| recall | 96175 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata serve: prompt 48164 tokens = 23319 reused + 24845 read in 31588 ms (786.5 tok/s), 12 generated in 101 ms (119.2 tok/s), drafts accepted 8 of 8, 4 checkpoints
strata serve: decode expert cache hit rate: 98.5% (6651 hits / 6750 lookups)
strata serve: KV streaming: 89.93% of 138648 block reads hit VRAM, 56.2 MiB read from RAM
strata serve: prompt 72282 tokens = 48176 reused + 24106 read in 35683 ms (675.6 tok/s), 12 generated in 135 ms (89.2 tok/s), drafts accepted 7 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 98.3% (6928 hits / 7049 lookups); 1 more read by the GPU over PCIe or from another GPU (0.0% of all 7050 routed)
strata serve: KV streaming: 91.50% of 206424 block reads hit VRAM, 70.7 MiB read from RAM
strata serve: prompt 96124 tokens = 72293 reused + 23831 read in 33004 ms (722.1 tok/s), 12 generated in 127 ms (94.8 tok/s), drafts accepted 7 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 99.4% (6977 hits / 7020 lookups)
strata serve: KV streaming: 92.46% of 274212 block reads hit VRAM, 83.2 MiB read from RAM
strata serve: prompt 96175 tokens = 96135 reused + 40 read in 208 ms (192.2 tok/s), 17 generated in 226 ms (75.3 tok/s), drafts accepted 9 of 12, 6 checkpoints
strata serve: decode expert cache hit rate: 98.8% (12790 hits / 12950 lookups)
strata serve: KV streaming: 94.45% of 499122 block reads hit VRAM, 111.6 MiB read from RAM
```
