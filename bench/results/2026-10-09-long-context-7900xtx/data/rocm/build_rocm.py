#!/usr/bin/env python3
"""Build the HIP engine against another ROCm (a folder), with setup.py's own flags, into build-hip-<tag>/ - the
installed engine/ is not touched.

  build_rocm.py ~/rocm-10.1/opt/rocm-10.1.0 r10 [gfx1100]
"""
import os, sys
from pathlib import Path

sys.path.insert(0, "/workspace")
os.chdir("/workspace")
import setup  # noqa: E402

root = Path(os.path.expanduser(sys.argv[1])).resolve()
tag = sys.argv[2]
archs = (sys.argv[3] if len(sys.argv) > 3 else "gfx1100").split(";")
bitcode = next((p for p in (root / "lib" / "llvm" / "amdgcn" / "bitcode", root / "amdgcn" / "bitcode") if p.is_dir()),
               root / "amdgcn" / "bitcode")
os.environ.update({"HIP_PLATFORM": "amd", "HIP_COMPILER": "clang", "HIP_RUNTIME": "rocclr", "ROCM_PATH": str(root),
                   "HIP_PATH": str(root)})
os.environ["LD_LIBRARY_PATH"] = os.pathsep.join([str(root / "lib"), os.environ.get("LD_LIBRARY_PATH", "")]).rstrip(os.pathsep)
os.environ["PATH"] = os.pathsep.join([str(root / "bin"), str(root / "llvm" / "bin"), os.environ.get("PATH", "")])
llama = setup.get_llama_cpp()
floor = setup.cpu_floor(setup.cpu_info()[1])
out = Path("/workspace") / f"build-hip-{tag}"
setup.cmake_build(Path("/workspace"), out, "strata",
                  ["-DSTRATA_ENABLE_HIP=ON", "-DSTRATA_ENABLE_CUDA=OFF", "-DSTRATA_BUILD_TESTS=OFF",
                   "-DSTRATA_PREFILL_MMQ=ON", "-DCMAKE_HIP_ARCHITECTURES=" + ";".join(archs),
                   f"-DCMAKE_HIP_COMPILER={root / 'llvm' / 'bin' / 'clang++'}", f"-DCMAKE_HIP_COMPILER_ROCM_ROOT={root}",
                   f"-DCMAKE_PREFIX_PATH={root}", f"-DCMAKE_HIP_FLAGS=--rocm-path={root} --rocm-device-lib-path={bitcode}",
                   f"-DSTRATA_GGML_DIR={llama}", *setup.isa_floor_defs(floor, out, {})], None, "")
print("built:", out / setup.EXE)
print("hipBLASLt:", setup.hipblaslt_version([str(root / "lib")]))
