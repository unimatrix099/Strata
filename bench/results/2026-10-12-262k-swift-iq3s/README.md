# The 262K context on Swift 1.5 IQ3_S (2x RX 7900 XTX, 11-12 Oct)

The production config with `--max-context 262144` (the model's own maximum; no rope scaling), `--kv-resident 32768`
(the newest 32,768 positions of each attention layer in VRAM, the rest in pinned RAM: the engine reports 1.55 GiB of
K/V per card, ~3.1 GiB in all). How it ran: `l1.sh` (stopped after the speed runs at 128K and 200K, the PC went to
sleep) and `l2.sh` (the rest); the tests: `longctx.py` (the 09 Oct long-context suite).

| test | result |
|---|---|
| prompt reading, 128K / 200K / 240K | 3,036 / 2,846 / 2,824 tok/s; the first token after 42 / 71 / 85 s |
| writing the answer after 128K / 200K / 240K | 84 / 75 / 76 tok/s (short answers, the pipeline's start included) |
| needles: 128K, 200K, 240K at 10%, 50%, 90% depth | **9 of 9 found** |
| facts (three planted notes; the answer needs two of them combined), 200K and 240K | **2 of 2** |
| a ~3,000-word report after 240K | 2,768 tokens at 88 tok/s, ended normally (`stop`), coherent from its first heading to its conclusion (`data/answers/longans-240k.txt`) |
| degeneration checks (repeats, loops, stray characters) on every answer | clean |

240K is the longest tested: a request's prompt and its answer must fit in 262,144 tokens together (`fit_max_tokens`
shortens an answer that would not). Not tested here: several 240K conversations at once, the conversation cache with
conversations this long, and anything past 262K (setup's rope-scaled 384K / 512K).
