#!/bin/bash
cd /workspace/ar1120
while pgrep -f "q4.sh" > /dev/null; do sleep 2; done
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh res2048 $P "" "--vram-reserve-later-mib 2048" 2 all
./exp.sh res1536 $P "" "--vram-reserve-later-mib 1536" 2 all
echo Q5 DONE
