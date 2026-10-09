# Long-context suite: prod-2026-10-09 (2026-10-09 10:00)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 1

## speed

| name | prompt_n | prompt_tps | ttft_s | decode_tps | degeneration |
|---|---|---|---|---|---|
| 4k | 4224 | 915.4 | 4.6 | 88.6 | ok (comp 0.607, rep8 1) |
| 16k | 15058 | 1100.0 | 13.7 | 100.3 | ok (comp 0.603, rep8 1) |
| 32k | 29896 | 1097.1 | 27.2 | 94.6 | ok (comp 0.625, rep8 1) |
| 64k | 61937 | 1204.8 | 51.4 | 91.5 | ok (comp 0.612, rep8 1) |
| 118k | 120941 | 1118.5 | 108.1 | 73.7 | ok (comp 0.597, rep8 1) |

## needle

| name | found | answer | prompt_tokens | seconds | degeneration |
|---|---|---|---|---|---|
| 8k@10 | True | meadow-copper-504 | 8064 | 6.4 |  |
| 8k@50 | True | falcon-quartz-940 | 8064 | 5.6 |  |
| 8k@90 | True | willow-cobalt-696 | 8065 | 5.5 |  |
| 32k@10 | True | falcon-saffron-138 | 33503 | 28.1 |  |
| 32k@50 | True | quartz-tundra-528 | 33501 | 24.2 |  |
| 32k@90 | True | quartz-glacier-192 | 33501 | 13.7 |  |
| 64k@10 | True | tundra-falcon-946 | 63836 | 49.9 |  |
| 64k@50 | True | willow-glacier-745 | 63836 | 55.3 |  |
| 64k@90 | True | falcon-juniper-150 | 63837 | 26.9 |  |
| 128k@10 | True | glacier-falcon-670 | 126981 | 111.3 |  |
| 128k@50 | True | copper-lantern-529 | 126982 | 108.7 |  |
| 128k@90 | True | copper-willow-684 | 126981 | 99.0 |  |

## facts

| name | prompt_n | ok | want | decode_tps | degeneration |
|---|---|---|---|---|---|
| 32k | 33649 | True | 7264 / Ilinca Moraru | 119.5 | ok (comp 0.488, rep8 2) |
| 64k | 63315 | True | 7700 / Bogdan Serban | 112.0 | ok (comp 0.489, rep8 2) |
| 118k | 118645 | True | 4631 / Mirela Toma | 107.4 | ok (comp 0.446, rep8 2) |

## longans

| name | prompt_n | ttft_s | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|---|---|
| 32k | 32150 | 27.7 | 2683 | 101.3 | [104.8, 105.9, 100.2, 95.3] | stop | ok (comp 0.497, rep8 1) |
| 64k | 66704 | 58.4 | 2803 | 94.9 | [95.0, 94.6, 95.2, 95.2] | stop | ok (comp 0.495, rep8 1) |
| 118k | 121722 | 98.7 | 2960 | 92.2 | [88.5, 91.3, 91.7, 98.4] | stop | ok (comp 0.512, rep8 1) |

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 9055 | 101.3 | [93.1, 99.3, 101.2, 113.7] | stop | ok (comp 0.385, rep8 3) |
| story-t0.7 | 11073 | 105.1 | [87.7, 90.5, 117.5, 140.8] | stop | BAD: an 8-word sequence repeated 11 times |
| code-t0.0 | 16000 | 152.8 | [147.1, 155.3, 155.3, 154.0] | length | BAD: an 8-word sequence repeated 24 times; the end loops on '============' |
| code-t0.7 | 14469 | 139.7 | [133.1, 141.2, 140.3, 144.8] | stop | ok (comp 0.234, rep8 6) |

## turns

| name | total_tokens | prompt_n | ttft_s | ok | found | degeneration |
|---|---|---|---|---|---|---|
| part1 | 23262 | 23262 | 17.2 |  |  |  |
| part2 | 48226 | 24952 | 19.7 |  |  |  |
| part3 | 72103 | 23866 | 18.0 |  |  |  |
| part4 | 96157 | 24043 | 23.1 |  |  |  |
| recall | 96208 | 40 |  | True | ['HELIOTROPE', 'BASALT', 'MARZIPAN', 'KESTREL'] | ok (comp None, rep8 None) |

## engine log (last lines)

```
strata generate: PCIe probe: 7.1 GB/s host->device (best of 7.1 7.1 7.1 7.1) -> pcie_frac 0.19 (default 0.55)
```
