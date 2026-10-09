# Long-context suite: f79-cc (2026-10-09 14:06)

GTT peak per card (MiB, before -> peak): 30 -> 30, 100 -> 100; engine error lines: 0

## longgen

| name | decode_n | decode_tps | decode_by_quarter | finish | degeneration |
|---|---|---|---|---|---|
| story-t0.0 | 8626 | 99.1 | [91.0, 91.6, 99.0, 119.9] | stop | ok (comp 0.406, rep8 4) |
| story-t0.7 | 11944 | 93.2 | [88.7, 92.5, 95.9, 96.2] | stop | ok (comp 0.39, rep8 3) |
| code-t0.0 | 16000 | 153.5 | [147.7, 154.5, 163.1, 149.8] | length | ok (comp 0.214, rep8 6) |
| code-t0.7 | 16000 | 140.9 | [135.9, 146.0, 144.8, 137.9] | length | BAD: an 8-word sequence repeated 22 times |

## engine log (last lines)

```
strata serve: suffix drafts: 51 windows, 45 of 112 drafts accepted
strata serve: conversation cache: dropped 1 superseded copy of this conversation; parked=0
strata serve: conversation cache: parked 12015 tokens in 169.6 ms; parked=1 bytes=420377192 evictions=0 snapshot_bytes=420377192 reused_kv_bytes=0
strata serve: prompt 82 tokens = 0 reused + 82 read in 872 ms (94.0 tok/s), 16000 generated in 104238 ms (153.5 tok/s), drafts accepted 9891 of 10925, 1 checkpoints
strata serve: decode expert cache hit rate: 99.5% (8801620 hits / 8847793 lookups); 47 more read by the GPU over PCIe or from another GPU (0.0% of all 8847840 routed)
strata serve: KV streaming: 99.96% of 58445364 block reads hit VRAM, 97.2 MiB read from RAM
strata serve: suffix drafts: 29 windows, 73 of 94 drafts accepted
strata serve: conversation cache: parked 16083 tokens in 253.1 ms; parked=2 bytes=902880976 evictions=0 snapshot_bytes=482503784 reused_kv_bytes=0
strata serve: prompt 82 tokens = 75 reused + 7 read in 320 ms (21.9 tok/s), 16000 generated in 113517 ms (140.9 tok/s), drafts accepted 9728 of 11004, 1 checkpoints
strata serve: decode expert cache hit rate: 99.6% (9111646 hits / 9146420 lookups); 10 more read by the GPU over PCIe or from another GPU (0.0% of all 9146430 routed)
strata serve: KV streaming: 99.98% of 119892738 block reads hit VRAM, 97.2 MiB read from RAM
strata serve: suffix drafts: 80 windows, 245 of 284 drafts accepted
```
