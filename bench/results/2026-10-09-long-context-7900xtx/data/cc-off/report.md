# Long-context suite: cc-off (2026-10-09 12:01)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## convcache

| name | total_tokens | prompt_n | ttft_s | wall_s | ok | want | answer | degeneration |
|---|---|---|---|---|---|---|---|---|
| c0-open |  | 64774 | 54.2 |  |  |  | The provided text is a comprehensive technical documentation guide for the Strat |  |
| c1-open |  | 63178 | 50.3 |  |  |  | The provided text is a comprehensive technical documentation for Strata, a local |  |
| c2-open |  | 66815 | 52.7 |  |  |  | The provided text is a collection of technical documentation and benchmark logs  |  |
| r0-c0-GAMMA | 64850 | 64850 | 52.49 | 52.62 | True | Teodora | Teodora Vlasic |  |
| r0-c1-DELTA | 63251 | 63251 | 53.58 | 53.72 | True | 462 | 462 |  |
| r0-c2-ALPHA | 66887 | 66887 | 58.25 | 58.38 | True | 1690 | 1690 |  |
| r1-c0-DELTA | 64889 | 64889 | 54.32 | 54.46 | True | 639 | 639 |  |
| r1-c1-ALPHA | 63287 | 63287 | 53.91 | 54.05 | True | 3575 | 3575 |  |
| r1-c2-EPSILON | 66924 | 66924 | 56.33 | 56.44 | True | indigo | indigo |  |
| r2-c0-ALPHA | 64925 | 64925 | 53.45 | 53.59 | True | 4889 | 4889 |  |
| r2-c1-EPSILON | 63324 | 63324 | 50.3 | 50.41 | True | saffron | saffron |  |
| r2-c2-GAMMA | 66960 | 66960 | 57.73 | 57.85 | True | Bogdan | Bogdan Serban |  |

## engine log (last lines)

```
strata serve: prompt 66924 tokens = 0 reused + 66924 read in 56334 ms (1188.0 tok/s), 3 generated in 64 ms (46.7 tok/s), drafts accepted 2 of 2, 6 checkpoints
strata serve: decode expert cache hit rate: 98.9% (2699 hits / 2730 lookups)
strata serve: KV streaming: 73.88% of 40056 block reads hit VRAM, 42.1 MiB read from RAM
strata serve: prompt 64925 tokens = 0 reused + 64925 read in 53450 ms (1214.7 tok/s), 5 generated in 93 ms (53.7 tok/s), drafts accepted 3 of 4, 5 checkpoints
strata serve: decode expert cache hit rate: 99.1% (4937 hits / 4980 lookups)
strata serve: KV streaming: 77.78% of 58536 block reads hit VRAM, 52.4 MiB read from RAM
strata serve: prompt 63324 tokens = 0 reused + 63324 read in 50300 ms (1258.9 tok/s), 4 generated in 65 ms (61.6 tok/s), drafts accepted 2 of 2, 5 checkpoints
strata serve: decode expert cache hit rate: 98.1% (2677 hits / 2730 lookups)
strata serve: KV streaming: 71.46% of 40056 block reads hit VRAM, 46.0 MiB read from RAM
strata serve: prompt 66960 tokens = 0 reused + 66960 read in 57730 ms (1159.9 tok/s), 6 generated in 86 ms (70.0 tok/s), drafts accepted 3 of 3, 6 checkpoints
strata serve: decode expert cache hit rate: 98.7% (4323 hits / 4380 lookups)
strata serve: KV streaming: 79.03% of 55452 block reads hit VRAM, 46.8 MiB read from RAM
```
