# Security audit of sangharshadhyeta/Strata-HotRod (HEAD d7f9dc4, 21 Nov 2026)

Read, not run, before any build. Result: no malware found.
- ~97 commits; owner plus two small outside contributions (both reviewed, harmless).
- setup.sh / update.sh / docker-entrypoint.sh / START-HERE.bat: Python via apt/dnf/pacman or the pinned python-build-standalone / python.org installer; nothing else.
- setup.py downloads only from Hugging Face, ModelScope, GitHub (llama.cpp pinned to commit 3cf0325) and AMD's ROCm wheel index; models and prebuilt files are SHA-256 checked. HotRod's own predictor download is off by default (HOTROD_PREDICTOR_DOWNLOAD=1) and pinned by hash.
- No eval/exec of fetched data, no base64 payloads, no bidi or zero-width characters; subprocess use is the build/run steps; the only execv runs the generated run script (SYCL path).
- Binaries in the repo: GGUF/expert-profile/draft-vocab data, benchmark logs and images. Server listens on 127.0.0.1 by default.
- HotRod does not build for HIP as published: see hotrod-hip-port.patch (applied for every HotRod number here).
