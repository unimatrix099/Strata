# Long-context suite: f10-nocc (2026-10-09 14:14)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 106; engine error lines: 0

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 10872 | 93.9 | [89.9, 91.1, 98.0, 97.2] | stop | ok (comp 0.438, rep8 3) |
| story-t0.7 | 9470 | 92.4 | [90.5, 90.1, 92.2, 97.2] | stop | ok (comp 0.429, rep8 3) |
| code-t0.0 | 16000 | 151.9 | [148.7, 154.5, 158.9, 146.2] | length | ok (comp 0.216, rep8 8) |
| code-t0.7 | 16000 | 142.2 | [136.7, 139.0, 144.2, 149.6] | length | BAD: an 8-word sequence repeated 12 times |

## engine log (last lines)

```
strata serve: prompt 70 tokens = 63 reused + 7 read in 65 ms (107.2 tok/s), 9470 generated in 102464 ms (92.4 tok/s), drafts accepted 4089 of 5747, 1 checkpoints
strata serve: decode expert cache hit rate: 99.9% (6826479 hits / 6830639 lookups); 1 more read by the GPU over PCIe or from another GPU (0.0% of all 6830640 routed)
strata serve: KV streaming: 99.98% of 105441756 block reads hit VRAM, 66.1 MiB read from RAM
strata serve: suffix drafts: 32 windows, 46 of 83 drafts accepted
strata serve: prompt 82 tokens = 0 reused + 82 read in 698 ms (117.4 tok/s), 16000 generated in 105321 ms (151.9 tok/s), drafts accepted 9891 of 10965, 1 checkpoints
strata serve: decode expert cache hit rate: 99.5% (8823088 hits / 8869566 lookups); 44 more read by the GPU over PCIe or from another GPU (0.0% of all 8869610 routed)
strata serve: KV streaming: 99.96% of 58889466 block reads hit VRAM, 97.2 MiB read from RAM
strata serve: suffix drafts: 39 windows, 87 of 120 drafts accepted
strata serve: prompt 82 tokens = 75 reused + 7 read in 65 ms (107.1 tok/s), 16000 generated in 112517 ms (142.2 tok/s), drafts accepted 9692 of 11002, 1 checkpoints
strata serve: decode expert cache hit rate: 99.6% (9146676 hits / 9180346 lookups); 14 more read by the GPU over PCIe or from another GPU (0.0% of all 9180360 routed)
strata serve: KV streaming: 99.98% of 120765498 block reads hit VRAM, 97.2 MiB read from RAM
strata serve: suffix drafts: 111 windows, 327 of 397 drafts accepted
```
