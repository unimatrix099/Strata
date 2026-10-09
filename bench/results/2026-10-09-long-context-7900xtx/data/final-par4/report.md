# Long-context suite: final-par4 (2026-10-09 19:25)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## par

| name | prompt_n | start_s | ttft_s | end_s | decode_n | decode_tps | want | ok_code | ok_name | turn2_ok | turn2_read | crosstalk | ok | decode_tps_total | all_ok | degeneration |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| c0 | 64939 | 0.1 | 87.9 | 125.2 | 1407 | 46.3 | 7857 / Teodora Vlasic | True | True | True | 34 | [] | True |  |  | ok (comp 0.507, rep8 1) |
| c1 | 62015 | 0.1 | 21.2 | 131.0 | 1445 | 43.9 | 8228 / Ilinca Moraru | True | True | True | 63493 | [] | True |  |  | ok (comp 0.524, rep8 1) |
| c2 | 66494 | 0.1 | 66.4 | 157.8 | 1539 | 66.7 | 7408 / Bogdan Serban | True | True | True | 32 | [] | True |  |  | ok (comp 0.52, rep8 1) |
| c3 | 67035 | 0.1 | 44.3 | 128.2 | 1418 | 46.1 | 4381 / Mirela Toma | True | True | True | 34 | [] | True |  |  | ok (comp 0.52, rep8 1) |
| all |  |  |  |  |  |  |  |  |  |  |  |  |  | 42.6 | True |  |

## engine log (last lines)

```
strata serve: conversation cache: parked 63493 tokens in 511.2 ms; parked=4 bytes=5372638192 evictions=4 snapshot_bytes=1682099072 reused_kv_bytes=0
strata batch: slot 0 gave back 67944 tokens of this conversation (all it holds) in 628.5 ms
strata verify: capturing the 3-token window (490 MiB of VRAM free)
strata verify: captured the 3-token window (upload no error, sync no error)
strata verify: capturing the 3-token window (2864 MiB of VRAM free)
strata verify: captured the 3-token window (upload no error, sync no error)
strata serve: prompt 67945 tokens = 67944 reused + 1 read in 1196 ms (0.8 tok/s), 88 generated in 1319 ms (66.7 tok/s), drafts accepted 42 of 46, 0 checkpoints
strata serve: decode expert cache hit rate: 99.4% (44834 hits / 45120 lookups)
strata serve: KV streaming: 91.37% of 289614 block reads hit VRAM, 100.7 MiB read from RAM
strata serve: prompt 68066 tokens = 68034 reused + 32 read in 316 ms (101.2 tok/s), 4 generated in 92 ms (43.5 tok/s), drafts accepted 3 of 4, 1 checkpoints
strata serve: decode expert cache hit rate: 97.8% (3285 hits / 3360 lookups)
strata serve: KV streaming: 92.00% of 406692 block reads hit VRAM, 131.1 MiB read from RAM
```
