# Long-context suite: par4-64k (2026-10-09 10:52)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## par

| name | prompt_n | start_s | ttft_s | end_s | decode_n | decode_tps | want | ok_code | ok_name | turn2_ok | turn2_read | crosstalk | ok | decode_tps_total | all_ok | degeneration |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| c0 | 64939 | 0.0 | 229.8 | 402.0 | 1535 | 54.0 | 7857 / Teodora Vlasic | True | True | True | 32 | [] | True |  |  | ok (comp 0.513, rep8 1) |
| c1 | 62150 | 0.0 | 53.0 | 334.4 | 1421 | 44.2 | 8228 / Ilinca Moraru | False | True | True | 63604 | [] | False |  |  | ok (comp 0.531, rep8 1) |
| c2 | 66593 | 0.0 | 174.7 | 396.4 | 1458 | 46.1 | 7408 / Bogdan Serban | True | True | True | 34 | [] | True |  |  | ok (comp 0.51, rep8 1) |
| c3 | 67075 | 0.0 | 113.3 | 266.4 | 1392 | 47.2 | 4381 / Mirela Toma | True | True | True | 68500 | [] | True |  |  | ok (comp 0.51, rep8 1) |
| all |  |  |  |  |  |  |  |  |  |  |  |  |  | 16.6 | False |  |

## engine log (last lines)

```
strata batch (pipelined, 2 groups of 2): 8 group-steps, 8 rows in 1998 ms = 4.0 rows/s (admissions included)
strata batch: slot 0 gave back 66404 tokens of this conversation (all it holds) in 706.5 ms
strata verify: capturing the 3-token window (494 MiB of VRAM free)
strata verify: captured the 3-token window (upload no error, sync no error)
strata verify: capturing the 3-token window (2874 MiB of VRAM free)
strata verify: captured the 3-token window (upload no error, sync no error)
strata serve: prompt 66405 tokens = 66404 reused + 1 read in 712 ms (1.4 tok/s), 69 generated in 1277 ms (54.0 tok/s), drafts accepted 22 of 36, 0 checkpoints
strata serve: decode expert cache hit rate: 99.4% (40570 hits / 40800 lookups)
strata serve: KV streaming: 90.34% of 261858 block reads hit VRAM, 101.9 MiB read from RAM
strata serve: prompt 66507 tokens = 66475 reused + 32 read in 318 ms (100.5 tok/s), 4 generated in 68 ms (59.0 tok/s), drafts accepted 2 of 2, 1 checkpoints
strata serve: decode expert cache hit rate: 97.2% (1866 hits / 1920 lookups)
strata serve: KV streaming: 90.99% of 369696 block reads hit VRAM, 134.2 MiB read from RAM
```
