#!/usr/bin/env python3
"""The ROCm image's settings from env vars, written into the Strata config on every container start
(docker/rocm-entrypoint.sh): change a value in the .env file, restart the container, done - no reinstall.

An env var that is unset or empty leaves the config as it is; "off" removes an engine flag.

  engine flags:  PIPELINE_WINDOWS SPEC SPEC_MIN_P VRAM_RESERVE_MIB VRAM_RESERVE_LATER_MIB KV_RESIDENT
                 CONVERSATION_CACHE_MIB CONVERSATION_CACHE_SLOTS, and EXTRA_ARGS (more flags, as on a command line)
  config keys:   HOST API_KEY PARALLEL DRAFT_VOCAB
  engine env:    every STRATA_* variable (the engine's switches, e.g. STRATA_QFUSE=1), except STRATA_DATA(_DIR)

  python docker/rocm_overlay.py /data/config/strata-iq3_xxs.json
"""
import json
import os
import shlex
import sys

FLAGS = {
    "PIPELINE_WINDOWS": "--pipeline-windows",
    "SPEC": "--spec",
    "SPEC_MIN_P": "--spec-min-p",
    "VRAM_RESERVE_MIB": "--vram-reserve-mib",
    "VRAM_RESERVE_LATER_MIB": "--vram-reserve-later-mib",
    "KV_RESIDENT": "--kv-resident",
    "CONVERSATION_CACHE_MIB": "--conversation-cache-mib",
    "CONVERSATION_CACHE_SLOTS": "--conversation-cache-slots",
}


def set_flag(args, flag, value):
    """Replace flag's value (or append the pair); value "off" removes the flag."""
    while flag in args:
        i = args.index(flag)
        has_val = i + 1 < len(args) and not args[i + 1].startswith("--")
        del args[i:i + (2 if has_val else 1)]
    if value != "off":
        args += [flag, value]


def overlay(cfg, env):
    args = list(cfg.get("args", []))
    changed = []
    for name, flag in FLAGS.items():
        v = (env.get(name) or "").strip()
        if v:
            set_flag(args, flag, v)
            changed.append(f"{flag} {v}")
    # EXTRA_ARGS: the previous start's extras are removed first, so editing the variable never stacks them
    for flag in cfg.pop("docker_extra_flags", []):
        set_flag(args, flag, "off")
    extra = shlex.split(env.get("EXTRA_ARGS") or "")
    flags = []
    i = 0
    while i < len(extra):
        if extra[i].startswith("--"):
            flag = extra[i]
            val = extra[i + 1] if i + 1 < len(extra) and not extra[i + 1].startswith("--") else None
            set_flag(args, flag, "off")
            args += [flag] + ([val] if val is not None else [])
            flags.append(flag)
            i += 2 if val is not None else 1
        else:
            i += 1
    if flags:
        cfg["docker_extra_flags"] = flags
        changed.append("EXTRA_ARGS " + " ".join(extra))
    cfg["args"] = args
    if (env.get("HOST") or "").strip():
        cfg["host"] = env["HOST"].strip()
    if "API_KEY" in env and env["API_KEY"].strip():
        cfg["api_key"] = env["API_KEY"].strip()
    if (env.get("PARALLEL") or "").strip():
        cfg["parallel"] = int(env["PARALLEL"])
        changed.append(f"parallel {cfg['parallel']}")
    if (env.get("DRAFT_VOCAB") or "").strip():
        cfg["draft_vocab"] = env["DRAFT_VOCAB"].strip()
        changed.append(f"draft_vocab {cfg['draft_vocab']}")
    sw = {k: v for k, v in env.items()
          if k.startswith("STRATA_") and k not in ("STRATA_DATA", "STRATA_DATA_DIR") and v != ""}
    if sw:
        cfg["env"] = {**cfg.get("env", {}), **sw}
        changed.append("env " + " ".join(f"{k}={v}" for k, v in sorted(sw.items())))
    return cfg, changed


def main():
    path = sys.argv[1]
    cfg = json.load(open(path))
    cfg, changed = overlay(cfg, dict(os.environ))
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(cfg, f, indent=1)
    os.replace(tmp, path)
    print("Settings from the environment: " + ("; ".join(changed) if changed else "none"))


if __name__ == "__main__":
    main()
