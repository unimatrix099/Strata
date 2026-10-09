# Long-context suite: cc-on (2026-10-09 12:04)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## convcache

| name | total_tokens | prompt_n | ttft_s | wall_s | ok | want | answer | degeneration |
|---|---|---|---|---|---|---|---|---|
| c0-open |  | 64774 | 56.5 |  |  |  | The provided text is a comprehensive technical documentation guide for the Strat |  |
| c1-open |  | 63178 | 53.9 |  |  |  | The provided text is a comprehensive technical documentation for Strata, a local |  |
| c2-open |  | 66815 | 59.8 |  |  |  | The provided text is a collection of technical documentation and benchmark logs  |  |
| r0-c0-GAMMA | 64850 | 33 | 1.29 | 1.43 | True | Teodora | Teodora Vlasic |  |
| r0-c1-DELTA | 63251 | 32 | 0.73 | 0.87 | True | 462 | 462 |  |
| r0-c2-ALPHA | 66890 | 31 | 0.73 | 0.85 | True | 1690 | 1690 |  |
| r1-c0-DELTA | 64889 | 33 | 0.8 | 0.94 | True | 639 | 639 |  |
| r1-c1-ALPHA | 63287 | 31 | 0.78 | 0.93 | True | 3575 | 3575 |  |
| r1-c2-EPSILON | 66927 | 31 | 0.79 | 0.91 | True | indigo | indigo |  |
| r2-c0-ALPHA | 64925 | 31 | 0.8 | 0.94 | True | 4889 | 4889 |  |
| r2-c1-EPSILON | 63324 | 32 | 0.82 | 0.93 | True | saffron | saffron |  |
| r2-c2-GAMMA | 66963 | 33 | 0.8 | 0.93 | True | Bogdan | Bogdan Serban |  |

## engine log (last lines)

```
strata serve: decode expert cache hit rate: 99.0% (4930 hits / 4980 lookups)
strata serve: KV streaming: 84.84% of 132480 block reads hit VRAM, 80.9 MiB read from RAM
strata serve: conversation cache: parked 64930 tokens in 369.6 ms; parked=2 bytes=4655804328 evictions=0 snapshot_bytes=2510827928 reused_kv_bytes=990511488
strata serve: conversation cache: restored 63292 tokens (live) in 201.6 ms; parked=2 bytes=6303819472
strata serve: prompt 63324 tokens = 63292 reused + 32 read in 818 ms (39.1 tok/s), 4 generated in 63 ms (63.8 tok/s), drafts accepted 2 of 2, 6 checkpoints
strata serve: decode expert cache hit rate: 97.7% (2667 hits / 2730 lookups)
strata serve: KV streaming: 83.83% of 117084 block reads hit VRAM, 76.3 MiB read from RAM
strata serve: conversation cache: parked 63327 tokens in 370.4 ms; parked=2 bytes=4990923048 evictions=0 snapshot_bytes=2480095120 reused_kv_bytes=966084864
strata serve: conversation cache: restored 66930 tokens (live) in 175.4 ms; parked=2 bytes=6303685568
strata serve: prompt 66963 tokens = 66930 reused + 33 read in 802 ms (41.1 tok/s), 6 generated in 86 ms (69.6 tok/s), drafts accepted 3 of 3, 6 checkpoints
strata serve: decode expert cache hit rate: 98.7% (4322 hits / 4380 lookups)
strata serve: KV streaming: 86.64% of 135558 block reads hit VRAM, 72.9 MiB read from RAM
```
