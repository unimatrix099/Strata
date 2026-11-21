#!/bin/bash
cd /workspace/ar1120
while ps -eo args | grep -q "[e]xp.sh base-1"; do sleep 2; done
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh mtp0 $P "STRATA_MTP_STAGE0=1" "" 2 decode
./exp.sh g12dec $P "STRATA_ATTN_LANECELL=1 STRATA_ATTN_MERGE_V2=1 STRATA_TSUM=1 STRATA_EXPDEC=1 STRATA_SMALL_UNR=3 STRATA_GR_Q32_G=2 STRATA_GR_Q8_PFS=1" "" 2 decode
./exp.sh g12pf $P "STRATA_GDN_HEAD=1 STRATA_GDN_PP=2 STRATA_GDN_CONVL2=1 STRATA_GDN_NOY=1 STRATA_CVEC_FUSE=1 STRATA_TOPK_REG_MIN_BLOCKS=4096" "" 2 prefill
./exp.sh pff16 $P "STRATA_PF_F16=1 STRATA_PF_DM16=1" "" 2 prefill
./exp.sh pipe1 $P "" "--prefill-pipe 1" 2 prefill
./exp.sh pipe3000 $P "" "--prefill-pipe 3000" 2 prefill
./exp.sh help $P "STRATA_PREFILL_HELP=1 STRATA_PREFILL_HELP_FRAC=0.3" "" 2 prefill
./exp.sh routed $P "STRATA_PREFILL_ROUTED_STREAM=1" "" 2 prefill
./exp.sh base-2 $P "" "" 2 all
echo Q1 DONE
