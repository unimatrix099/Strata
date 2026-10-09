# Long-context suite: solo2-64k (2026-10-09 10:57)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## par

| name | prompt_n | start_s | ttft_s | end_s | decode_n | decode_tps | want | ok_code | ok_name | turn2_ok | turn2_read | crosstalk | ok | decode_tps_total | all_ok | degeneration |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| c0 | 64939 | 0.0 | 119.7 | 134.6 | 1485 | 100.0 | 7857 / Teodora Vlasic | True | True | True | 66457 | [] | True |  |  | ok (comp 0.505, rep8 1) |
| c1 | 62150 | 0.0 | 53.9 | 68.6 | 1512 | 103.1 | 8228 / Ilinca Moraru | False | True | True | 63695 | [] | False |  |  | ok (comp 0.518, rep8 1) |
| all |  |  |  |  |  |  |  |  |  |  |  |  |  | 37.1 | False |  |

## engine log (last lines)

```
strata serve: KV streaming: 97.05% of 7526934 block reads hit VRAM, 895.4 MiB read from RAM
strata serve: suffix drafts: 5 windows, 13 of 17 drafts accepted
strata serve: prompt 64939 tokens = 0 reused + 64939 read in 51146 ms (1269.7 tok/s), 1485 generated in 14849 ms (100.0 tok/s), drafts accepted 715 of 913, 4 checkpoints
strata serve: decode expert cache hit rate: 99.8% (973089 hits / 975050 lookups)
strata serve: KV streaming: 96.85% of 7520652 block reads hit VRAM, 954.4 MiB read from RAM
strata serve: suffix drafts: 8 windows, 14 of 26 drafts accepted
strata serve: prompt 63695 tokens = 0 reused + 63695 read in 53065 ms (1200.3 tok/s), 4 generated in 63 ms (63.0 tok/s), drafts accepted 2 of 2, 5 checkpoints
strata serve: decode expert cache hit rate: 98.3% (2684 hits / 2730 lookups)
strata serve: KV streaming: 74.55% of 40056 block reads hit VRAM, 41.1 MiB read from RAM
strata serve: prompt 66457 tokens = 0 reused + 66457 read in 49111 ms (1353.2 tok/s), 4 generated in 77 ms (51.7 tok/s), drafts accepted 2 of 2, 5 checkpoints
strata serve: decode expert cache hit rate: 97.7% (2665 hits / 2728 lookups); 2 more read by the GPU over PCIe or from another GPU (0.1% of all 2730 routed)
strata serve: KV streaming: 74.28% of 40050 block reads hit VRAM, 41.5 MiB read from RAM
```
