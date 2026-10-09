# Long-context suite: final-cc (2026-10-09 19:26)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## convcache

| name | total_tokens | prompt_n | ttft_s | wall_s | ok | want | answer | degeneration |
|---|---|---|---|---|---|---|---|---|
| c0-open |  | 64774 | 20.5 |  |  |  | The provided text is a comprehensive technical documentation for the Strata AI e |  |
| c1-open |  | 63150 | 18.6 |  |  |  | This text is a comprehensive technical documentation for Strata, a local AI infe |  |
| c2-open |  | 67051 | 19.9 |  |  |  | This text is a comprehensive technical documentation set for the Strata AI engin |  |
| r0-c0-GAMMA | 64851 | 33 | 1.31 | 1.45 | True | Teodora | Teodora Vlasic |  |
| r0-c1-DELTA | 63220 | 33 | 0.75 | 0.85 | True | 462 | 462 |  |
| r0-c2-ALPHA | 67129 | 32 | 0.75 | 0.89 | True | 1690 | 1690 |  |
| r1-c0-DELTA | 64890 | 33 | 0.82 | 0.95 | True | 639 | 639 |  |
| r1-c1-ALPHA | 63256 | 33 | 0.8 | 0.94 | True | 3575 | 3575 |  |
| r1-c2-EPSILON | 67166 | 31 | 0.82 | 0.92 | True | indigo | indigo |  |
| r2-c0-ALPHA | 64926 | 31 | 0.82 | 0.96 | True | 4889 | 4889 |  |
| r2-c1-EPSILON | 63293 | 32 | 0.84 | 0.95 | True | saffron | saffron |  |
| r2-c2-GAMMA | 67202 | 33 | 0.82 | 0.96 | True | Bogdan | Bogdan Serban |  |

## engine log (last lines)

```
strata serve: KV streaming: 83.94% of 135564 block reads hit VRAM, 87.7 MiB read from RAM
strata serve: suffix drafts: 1 windows, 1 of 1 drafts accepted
strata serve: conversation cache: parked 64930 tokens in 373.7 ms; parked=2 bytes=4666064776 evictions=0 snapshot_bytes=2510827952 reused_kv_bytes=990511488
strata serve: conversation cache: restored 63261 tokens (live) in 209.7 ms; parked=2 bytes=6313466544
strata serve: prompt 63293 tokens = 63261 reused + 32 read in 838 ms (38.2 tok/s), 4 generated in 63 ms (63.1 tok/s), drafts accepted 2 of 2, 6 checkpoints
strata serve: decode expert cache hit rate: 98.4% (2687 hits / 2730 lookups)
strata serve: KV streaming: 84.17% of 117078 block reads hit VRAM, 74.7 MiB read from RAM
strata serve: conversation cache: parked 63296 tokens in 372.7 ms; parked=2 bytes=4990308480 evictions=0 snapshot_bytes=2479480528 reused_kv_bytes=965600640
strata serve: conversation cache: restored 67169 tokens (live) in 194.4 ms; parked=2 bytes=6313323800
strata serve: prompt 67202 tokens = 67169 reused + 33 read in 825 ms (40.0 tok/s), 6 generated in 101 ms (59.6 tok/s), drafts accepted 3 of 3, 6 checkpoints
strata serve: decode expert cache hit rate: 99.0% (4454 hits / 4500 lookups)
strata serve: KV streaming: 86.20% of 135570 block reads hit VRAM, 75.4 MiB read from RAM
```
