# Current upstream against the proposal branch (12 Oct)

**upstream** = `upstream/main` at `7695859a` (12 Oct); **branch** = `pr/amd-decode-speed` (`5973146a`): that commit plus
the fork's engine changes (merged by hand where upstream had changed the same code: its own `hits_in_combine`, the
adaptive-tier wait, `STRATA_QFUSE_HC`); **branch + switches** = the same with `STRATA_HIP_POST_SPLIT=1
STRATA_HIP_FRONT_FUSE=1`. All three built as setup builds (`STRATA_PREFILL_MMQ=ON`, gfx1100). Swift 1.5 IQ3_S on
RX 7900 XTX: one GPU with setup's one-GPU config (`strata-swift-iq3_s-1gpu.json`), two GPUs with the fork's tuned
config without the two switches (`strata-swift-iq3_s-2gpu-base.json`; its other switches are upstream's). How it ran:
`p1.sh`.

| | one GPU | two GPUs |
|---|---|---|
| exact-protocol text (story / code, 2 reps each) | the same in all three (efc6d92f9a / 83404c01e8) | the same in all three (3e8eff97e5 / 7cb581aa1f) |
| decode, eight prompts x 3 rounds: upstream | 65.7 tok/s | 99.1 tok/s |
| branch | 67.3 (**+2.4%**; rounds 1.026 / 1.020 / 1.027; 20 of 24 prompts faster) | 107.1 (**+8.6%**; 1.035 / 1.144 / 1.079; 20 of 24) |
| branch + switches | 68.6 (**+4.4%**; 1.043 / 1.044 / 1.046; 23 of 24) | 110.2 (**+11.6%**; 1.117 / 1.138 / 1.094; 21 of 24) |
| prompt reading: upstream / branch / + switches | 1K 170 / 169 / 170; 4K 517 / 530 / 511; 16K 746 / 750 / 776 | 4K 1,024 / 1,041 / 1,037; 32K 2,444 / 2,561 / 2,530 |
| server smoke (7 checks), branch + switches | 7 of 7, no engine errors | 7 of 7, no engine errors |

On two GPUs upstream's second exact-protocol code run decoded at 78.3 against 112.7 in the first: the slow starts the
drafter-priority change removes (FORK.md: before it, about every other engine start on two cards was ~10% slower; here
-30% on that run). The branch's runs did not show it.
