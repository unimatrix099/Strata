# The merge of upstream 0.2.8 (branch `merge-upstream-0.2.8`), checked on 2x RX 7900 XTX, 30 Oct 2026

PC: 2x RX 7900 XTX (24 GB, PCIe 3.0 x8, card 0 drives the desktop), Ryzen 9 5950X, 125 GB RAM, ROCm 7.9 (system,
`/opt/rocm`), Swift 1.5 IQ3_S, the production config `strata-swift-iq3_s-tuned.json` (layer split auto = 26,
`--pipeline-windows 2`, `--ple-io ram`, 262K context, `--spec 3`). Engines: **installed** = `engine/strata-prod-1010`
(the fork's main, 10-12 Oct); **merged** = `build-hip/strata` from the branch (0.1.54); **upstream** =
`build-hip-up/strata`, upstream 870aedd9 built with the same options (plus the ROCm 7.9 `alignas` fix as a local edit).
Raw rows: `data/results.jsonl` (labels `x1030-*` exactness, `s1030-*` speed); engine logs in
`autoresearch/explore-261007-tg/logs/`. `bench.py` with `--reps 1 --env STRATA_IQ_MT_MIN=1 --pcie-frac 0
--adapt-every 0` for the text checks (the text then depends only on the engine and the resident expert set).

## Exactness (the text's SHA-1 over the story and code prompts, 512 greedy tokens)

| run | CUDA0 cache slots | code | story |
|---|---:|---|---|
| merged, production switches on (`STRATA_HIP_POST_SPLIT/FRONT_FUSE=1`, gather) | 10148 | bbd8e8c352 | 374e5a5b40 |
| merged, the fork's switches off (`POST_SPLIT=0 FRONT_FUSE=0 COMBINE_GATHER=0`) | 10148 | bbd8e8c352 | 374e5a5b40 |
| merged, `STRATA_SPLIT_OVERLAP=1` and serial (a no-op beside `--pipeline-windows 2`, the engine says so) | 10148 | bbd8e8c352 | 374e5a5b40 |
| **upstream 870aedd9**, the same flags | 10148 | bbd8e8c352 | 374e5a5b40 |
| merged, `--expert-cache 10285` (also with `STRATA_COMMIT_BATCH=0`, `STRATA_FUSE_HITS=0`, `STRATA_DEFAULTS=026`) | 10285 | ff83da1b9b | 3be34a5d2c |
| installed, auto (`--vram-reserve-mib 700`) | 10289 | c60e18dbc6 | 0e65068074 |
| installed, `--vram-reserve-mib 708` (the merged engine's resident set) | 10285 | f0e97eb5f1 | 0e65068074 |

- The merged engine and pure upstream write the **same text**, with the fork's re-applied code on or off: the fork's
  changes are exact against upstream.
- Against the installed engine the text differs, and not only through the resident set: at the same 10285 slots the
  two still differ. That change is upstream's (3,208 commits); `STRATA_DEFAULTS=026` and the two new decode
  switches above do not take it back. Its effect on answers: the known-answer suite below.
- The merged engine's automatic cache is 4 slots smaller than the installed one's (10285 against 10289 at the same
  reserve): the bigger binary (53 against 26 MB) leaves ~10 MiB less free VRAM at sizing time. In three starts the
  automatic sizing also gave 10148 (the desktop card's free memory moves); the installed engine got 10289 each time.

## Speed (`bench_many.py`, 8 prompts x 3 reps, 512 tokens, the production config and switches, alternating)

| round | installed tok/s (median of 8) | merged | ms per window | tokens per window |
|---|---:|---:|---|---|
| 1 | 108.9 | 116.2 | 17.49 / 16.61 | 1.89 / 1.95 |
| 2 | 112.8 | 107.6 | 16.70 / 17.56 | 1.91 / 1.89 |
| mean | 110.8 | 111.9 (+0.9%) | | |

Per prompt (mean of the two rounds): story +2.2%, code +9.1%, explain +1.5%, email +1.9%, sql -0.3%, list -1.1%,
math +0.4%, rust -2.5%. The round-to-round spread (installed 108.9 against 112.8) is larger than the difference:
**parity**. The first server starts after the sandbox restart were 10-15% slower (cold page cache); those single-rep
rows are in `data/results.jsonl` but not in this table.

## Server and batch

- `smoke.py` (7 checks, merged engine, production config with `STRATA_SPLIT_OVERLAP=1 STRATA_MTP_HIP_PRIO=1`): 7 of 7,
  0 error lines; 4.9K-token prompt read at 1,282 tok/s.
- `tools/batch_test.py --batch 4 --n 4 --extra "--layer-split 26"`: merged engine 3 of 4 slots identical to their solo
  runs (slot 3 differs at token 61 of 64); the installed engine 4 of 4 on the same day; with `--mt-min 1 --pcie-frac
  0 --adapt-every 0` the merged engine 4 of 4. FORK.md records the same small drift (2 of 4) for the 10 Oct engine.

## Known-answer quality (`qeval.py --set both`, 79 items, temperature 0)

| engine | math (63) | code (16) | tokens | time |
|---|---:|---:|---:|---:|
| merged (`build-hip/strata`, production switches + `STRATA_SPLIT_OVERLAP=1 STRATA_MTP_HIP_PRIO=1`) | **63/63** | 16/16 | 80,458 | 627 s |
| installed (`engine/strata-prod-1010`), same day | 61/63 | 16/16 | 86,419 | 679 s |

Per-item rows: `../2026-10-10-iq3s-vs-iq3xxs-7900xtx/data/qeval/merge1030-{new,old}-both.jsonl`. The merged engine
answers every item, with 7% fewer tokens; the two the installed engine missed are arithmetic slips of the kind the
10 Oct run also had (one). So upstream's numeric change costs nothing measurable here.

## Verdict

Builds, exact against upstream, parity in decode speed, 7/7 smoke, 79/79 known answers: the branch can replace
`main` and the engine can be installed (`engine/strata`, keep the old one beside it; add `STRATA_MTP_HIP_PRIO=1` to the
config's env - `STRATA_SPLIT_OVERLAP` is a no-op with `--pipeline-windows 2`).
