# Long-context suite: full714-cc-2 (2026-10-09 16:56)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 106; engine error lines: 0

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 669.0 | 6.3 | 83.8 | ok (comp 0.586, rep8 1) |
| 16k | 15262 | 880.0 | 17.3 | 94.5 | ok (comp 0.614, rep8 1) |
| 32k | 30047 | 649.7 | 46.2 | 91.0 | ok (comp 0.607, rep8 1) |
| 64k | 62013 | 695.7 | 89.1 | 97.9 | ok (comp 0.605, rep8 1) |
| 118k | 121363 | 696.0 | 174.4 | 69.8 | ok (comp 0.597, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 11.5 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 9.6 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 9.6 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 45.2 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 44.6 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 22.7 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 87.4 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 71.7 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 44.2 |  |
| 128k@10 | True | glacier-falcon-670 | 126921 | 188.6 |  |
| 128k@50 | True | copper-lantern-529 | 126922 | 118.9 |  |
| 128k@90 | True | copper-willow-684 | 126921 | 149.7 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33643 | True | 7264 / Ilinca Moraru | 112.5 | ok (comp 0.477, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 113.9 | ok (comp 0.488, rep8 2) |
| 118k | 119066 | True | 4631 / Mirela Toma | 110.7 | ok (comp 0.481, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32210 | 42.1 | 2723 | 100.8 | [100.9, 101.0, 103.9, 98.0] | stop | ok (comp 0.501, rep8 1) |
| 64k | 66796 | 99.4 | 3206 | 101.1 | [101.9, 101.4, 101.3, 100.1] | stop | ok (comp 0.489, rep8 1) |
| 118k | 121552 | 181.1 | 2684 | 95.8 | [98.9, 94.0, 98.3, 93.4] | stop | ok (comp 0.507, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 6009 | 94.0 | [85.7, 93.5, 88.6, 112.3] | stop | ok (comp 0.429, rep8 2) |
| story-t0.7 | 8896 | 89.3 | [87.8, 87.6, 90.2, 91.9] | stop | ok (comp 0.457, rep8 2) |
| code-t0.0 | 16000 | 140.3 | [140.9, 144.4, 154.9, 124.5] | length | ok (comp 0.224, rep8 5) |
| code-t0.7 | 13951 | 140.9 | [141.3, 147.4, 144.0, 132.1] | stop | ok (comp 0.256, rep8 4) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23307 | 23307 | 33.8 |  |  |  |
| part2 | 48164 | 24844 | 31.5 |  |  |  |
| part3 | 72282 | 24107 | 34.6 |  |  |  |
| part4 | 96124 | 23831 | 36.5 |  |  |  |
| recall | 96175 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata serve: suffix drafts: 1 windows, 4 of 4 drafts accepted
strata serve: prompt 72282 tokens = 48175 reused + 24107 read in 34566 ms (697.4 tok/s), 12 generated in 114 ms (105.6 tok/s), drafts accepted 8 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 98.5% (7337 hits / 7450 lookups)
strata serve: KV streaming: 92.32% of 228000 block reads hit VRAM, 70.6 MiB read from RAM
strata serve: suffix drafts: 1 windows, 4 of 4 drafts accepted
strata serve: prompt 96124 tokens = 72293 reused + 23831 read in 36521 ms (652.5 tok/s), 12 generated in 116 ms (103.5 tok/s), drafts accepted 8 of 8, 6 checkpoints
strata serve: decode expert cache hit rate: 98.4% (7437 hits / 7559 lookups); 1 more read by the GPU over PCIe or from another GPU (0.0% of all 7560 routed)
strata serve: KV streaming: 93.20% of 308112 block reads hit VRAM, 84.4 MiB read from RAM
strata serve: suffix drafts: 1 windows, 4 of 4 drafts accepted
strata serve: prompt 96175 tokens = 96135 reused + 40 read in 210 ms (190.7 tok/s), 17 generated in 228 ms (74.6 tok/s), drafts accepted 9 of 10, 6 checkpoints
strata serve: decode expert cache hit rate: 98.3% (11785 hits / 11989 lookups); 1 more read by the GPU over PCIe or from another GPU (0.0% of all 11990 routed)
strata serve: KV streaming: 94.74% of 526854 block reads hit VRAM, 111.5 MiB read from RAM
```
