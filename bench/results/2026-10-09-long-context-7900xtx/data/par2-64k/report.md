# Long-context suite: par2-64k (2026-10-09 10:45)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## par

| name | prompt_n | start_s | ttft_s | end_s | decode_n | decode_tps | want | ok_code | ok_name | turn2_ok | turn2_read | crosstalk | ok | decode_tps_total | all_ok | degeneration |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| c0 | 64939 | 0.0 | 112.9 | 141.8 | 1415 | 47.4 | 7857 / Teodora Vlasic | True | True | True | 34 | [] | True |  |  | ok (comp 0.5, rep8 1) |
| c1 | 62150 | 0.0 | 55.7 | 146.3 | 1435 | 55.0 | 8228 / Ilinca Moraru | False | True | True | 33 | [] | False |  |  | ok (comp 0.515, rep8 1) |
| all |  |  |  |  |  |  |  |  |  |  |  |  |  | 31.5 | False |  |

## engine log (last lines)

```
strata batch (pipelined, 2 groups of 1): 8 group-steps, 8 rows in 1973 ms = 4.1 rows/s (admissions included)
strata batch: slot 0 gave back 63574 tokens of this conversation (all it holds) in 697.0 ms
strata verify: capturing the 3-token window (502 MiB of VRAM free)
strata verify: captured the 3-token window (upload no error, sync no error)
strata verify: capturing the 3-token window (2882 MiB of VRAM free)
strata verify: captured the 3-token window (upload no error, sync no error)
strata serve: prompt 63575 tokens = 63574 reused + 1 read in 702 ms (1.4 tok/s), 10 generated in 182 ms (55.1 tok/s), drafts accepted 5 of 5, 0 checkpoints
strata serve: decode expert cache hit rate: 98.6% (5205 hits / 5280 lookups)
strata serve: KV streaming: 70.64% of 33888 block reads hit VRAM, 40.1 MiB read from RAM
strata serve: prompt 63618 tokens = 63585 reused + 33 read in 344 ms (95.9 tok/s), 4 generated in 84 ms (47.7 tok/s), drafts accepted 3 of 4, 1 checkpoints
strata serve: decode expert cache hit rate: 97.5% (3276 hits / 3360 lookups)
strata serve: KV streaming: 85.26% of 154050 block reads hit VRAM, 91.5 MiB read from RAM
```
