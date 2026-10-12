# The fork's changes on one GPU, and how each is gated (12 Oct)

For the upstream proposal: how the fork's changes behave on a single GPU, whether anything breaks, and which are AMD-
or RDNA3-specific. Hardware: one RX 7900 XTX (HIP device 1, the card without the desktop), Ryzen 9 5950X, 128 GB,
ROCm 7.9; Swift 1.5 IQ3_S with setup's own one-GPU config (`strata-swift-iq3_s-1gpu.json`: spec 4, min-p 0.5, KV
streaming, 128K; none of the 2-card tuning). How it ran: `o3.sh` (and `o2.sh`, the bisection below).

Three engines from the same source tree: **base** = the fork's upstream base commit `fb58e0d`, **fork** = the fork's
`main` with its defaults, **fork + switches** = the same with `STRATA_HIP_POST_SPLIT=1 STRATA_HIP_FRONT_FUSE=1`. All
three built the way setup builds (`STRATA_PREFILL_MMQ=ON`, the same HIP flags).

## Results (one GPU)

| | base | fork (defaults) | fork + switches |
|---|---|---|---|
| exact-protocol text (story / code, 2 reps each) | efc6d92f9a / 83404c01e8 | **the same** | **the same** |
| decode, eight prompts, 3 rounds (mean tok/s) | 65.1 | 67.3 (**+3.3%**, 22 of 24 prompts faster) | 68.5 (**+5.1%**, 24 of 24) |
| prompt reading 1K / 4K / 16K (tok/s) | 168 / 529 / 751 | 168 / 531 / 739 | 168 / 530 / 756 (all within +-2%) |
| server smoke (7 checks) | - | - | 7 of 7, no engine errors |

A first run compared against a base built without `STRATA_PREFILL_MMQ` (a fresh cmake configure leaves it off; setup
turns it on): the text differed and prompts looked 7-18% faster for the fork. Both were the build option, not the
fork's code - the bisection with the fork's own switches (`STRATA_COMBINE_GATHER=0`, `STRATA_MTP_PRIORITY=0`) kept the
fork's text, and the base rebuilt with the option gives exactly the fork's text and prompt speed.

## Compile checks

| backend / architectures | result |
|---|---|
| HIP (wave32): gfx1030, gfx1101, gfx1102, gfx1151, gfx1200, gfx1201, gfx1012 in one build (`strata`, `ffn_half_bench`) | builds, no errors |
| gfx906 compat backend (`STRATA_HIP_GFX906=ON`: the "CUDA" code paths compiled through HIP, without `STRATA_USE_HIP`) | builds, no errors |
| CUDA (nvcc) | **not checked**: no CUDA toolkit here |
| SYCL (Intel) | unaffected: it compiles its own ported copies of every changed file (`sycl/src/...`) |

GPU parity tests of the refactored kernels (one RX 7900 XTX): `iq_multi_parity`, `native_grouped_parity` (bitwise equal
to v1), `shared_expert_parity`, `router_top10_parity`, `elementwise_parity`, `bf16_gemv_parity` - 0 failures each.

## How each change is gated

| change | default | backends | gate | specific to a GPU? |
|---|---|---|---|---|
| the drafter's stream at the highest priority (mtp.cpp) | **on** | CUDA (as before) + HIP (new) | `STRATA_MTP_PRIORITY=0` turns it off; not gfx906 | no; measured on 2x RX 7900 XTX (half the starts were ~10% slower without it); one GPU: same text |
| asynchronous commit + early launch on a layer split (generate.cpp, verify.cpp) | **on** | CUDA + HIP | multi-GPU layer splits only; `STRATA_SPLIT_COMMIT_SYNC=1`, `STRATA_SPLIT_EARLY_LAUNCH=0` | no; measured on 2x RX 7900 XTX only (+36% decode there) - CUDA multi-GPU not measured |
| the merged combine `combine_gather` (verify.cpp, native_moe.cu) | **on** | CUDA + HIP | the doorbell path; `STRATA_COMBINE_GATHER=0` | no; bit-exact (one GPU: same text on and off); CUDA not measured |
| the PCIe copy kernel at 96 blocks (verify_kernels.cu) | **on** | HIP only (`STRATA_USE_HIP`; gfx906 and CUDA keep 384) | none | the 14-16 us cliff was measured at 384 = 48 CUs x 8 on gfx1100; 96 blocks are enough for a PCIe copy on any card (6.9 GB/s from 16 to 384 blocks) |
| `STRATA_HIP_POST_SPLIT` (persistent head / tail of post()) | off | HIP only (CUDA / gfx906: a stub that says "not supported", the kernels run) | env; and only for the native gate/up types 16/17/18/21/22 with down 20/42, without the AMD layouts, V1, old kernels | grid size clamped to what the device keeps resident for that kernel (12 Oct: a small GPU or a heavier instantiation could otherwise hang at the grid barrier); measured on gfx1100 only |
| `STRATA_HIP_FRONT_FUSE` (route + doorbell, gate + scale + head) | off | engine path HIP only (needs the split post(); its kernels compile for every backend) | env; the engine refuses a window where the front ran without the split post() | measured on gfx1100 only |
| `combine_gather_body`, `VBlock` bodies of gu / down / swiglu / bf16 GEMV / router row / sigmoid scale | - | all (the kernels are one-line wrappers) | - | no; the parity tests above, same text |
| `STRATA_MTP_TOP2`, `STRATA_PL_LAUNCH_TIMING`, `NATIVE_EXPERT_BENCH_GY` | off | - | env | diagnostics |
| setup.py: Swift 1.5 IQ3_S | - | - | `--family swift --model IQ3_S` | no |

**For the upstream proposal**: the four defaults above change behaviour for every user of their backend. Bit-exact or
measured here, but the multi-GPU ones (async commit, early launch) and the merged combine were measured on AMD only, so
for CUDA they are best proposed as opt-in until someone measures them there. The 96-block copy is HIP-only and safe on
any AMD card. The two switches are opt-in, HIP-only and guarded.
