#!/usr/bin/env python3
"""Unpack the ROCm folder (/opt/rocm*) of an AMD ROCm Docker image without Docker: the image's layers are streamed
from Docker Hub and only their opt/ members are written (layer whiteouts applied).

  pull_rocm.py rocm/dev-ubuntu-24.04 10.1.0-full ~/rocm-10.1
"""
import json, os, sys, tarfile, urllib.request

repo, tag, dest = sys.argv[1], sys.argv[2], os.path.expanduser(sys.argv[3])
os.makedirs(dest, exist_ok=True)


def get(url, headers=None):
    return urllib.request.urlopen(urllib.request.Request(url, headers=headers or {}), timeout=120)


tok = json.load(get(f"https://auth.docker.io/token?service=registry.docker.io&scope=repository:{repo}:pull"))["token"]
H = {"Authorization": f"Bearer {tok}",
     "Accept": ", ".join(["application/vnd.oci.image.index.v1+json", "application/vnd.docker.distribution.manifest.list.v2+json",
                          "application/vnd.oci.image.manifest.v1+json", "application/vnd.docker.distribution.manifest.v2+json"])}
m = json.load(get(f"https://registry-1.docker.io/v2/{repo}/manifests/{tag}", H))
if "manifests" in m:   # an index: the linux/amd64 image
    d = next(x for x in m["manifests"] if x.get("platform", {}).get("architecture") == "amd64" and x["platform"].get("os") == "linux")
    m = json.load(get(f"https://registry-1.docker.io/v2/{repo}/manifests/{d['digest']}", H))
layers = m["layers"]
print(f"{len(layers)} layers, {sum(l['size'] for l in layers) / 2**30:.1f} GiB compressed", flush=True)
for i, l in enumerate(layers):
    r = get(f"https://registry-1.docker.io/v2/{repo}/blobs/{l['digest']}", {"Authorization": f"Bearer {tok}"})
    n = 0
    with tarfile.open(fileobj=r, mode="r|*") as t:
        for mem in t:
            name = mem.name.lstrip("./")
            if not name.startswith("opt/"):
                continue
            base = os.path.basename(name)
            if base.startswith(".wh."):   # a whiteout: the earlier layers' file is gone
                gone = os.path.join(dest, os.path.dirname(name), base[4:])
                if base != ".wh..wh..opq" and os.path.lexists(gone):
                    os.system(f"rm -rf '{gone}'")
                continue
            mem.name = name
            try:
                t.extract(mem, dest, set_attrs=False, filter="fully_trusted")
            except TypeError:
                t.extract(mem, dest, set_attrs=False)
            n += 1
    print(f"layer {i + 1}/{len(layers)}: {l['size'] / 2**20:.0f} MiB, {n} files under opt/", flush=True)
print("done:", os.listdir(os.path.join(dest, "opt")) if os.path.isdir(os.path.join(dest, "opt")) else "no opt/")
