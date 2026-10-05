#!/usr/bin/env python3
"""Compare optimized ordinary AVM LLVM IR with the pre-instrumentation sources.

This is a development verification tool, not part of either executable. Pass
the revision before adding hooks; after this milestone is committed, HEAD is
no longer the appropriate baseline. No SDK installation is performed.
"""
import argparse
import difflib
import hashlib
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    args.output.mkdir(parents=True, exist_ok=True)
    compiler = args.sdk_root / "bin" / ("avm-clang.exe" if __import__("os").name == "nt" else "avm-clang")
    report = []
    for name in ("state", "combat", "items", "world_gen"):
        baseline = args.output / f"{name}-baseline.cpp"
        baseline.write_bytes(subprocess.check_output(
            ["git", "-c", f"safe.directory={root.as_posix()}", "show", f"{args.baseline}:src/{name}.cpp"], cwd=root))
        ir = []
        for tag, source in (("before", baseline), ("after", root / "src" / f"{name}.cpp")):
            target = args.output / f"{name}-{tag}.ll"
            subprocess.run([str(compiler), "--target=avm-unknown-arduboyfx", "-std=c++17", "-O2",
                "-ffreestanding", "-fno-exceptions", "-fno-rtti", "-S", "-emit-llvm",
                "-isystem", str(args.sdk_root / "sysroot" / "include"), "-I", str(root / "src"),
                str(source), "-o", str(target)], check=True)
            # Only compilation-unit paths differ; retain every instruction,
            # global, attribute and constant in the comparison.
            normalized = "\n".join(line for line in target.read_text().splitlines()
                if not line.startswith(("; ModuleID =", "source_filename ="))) + "\n"
            if "_ZN3sim" in normalized or "ARDUROGUE2_SIM" in normalized:
                raise RuntimeError(f"simulator leaked into ordinary IR: {name}")
            ir.append(normalized)
        if ir[0] != ir[1]:
            diff = "\n".join(difflib.unified_diff(ir[0].splitlines(), ir[1].splitlines()))
            (args.output / f"{name}.diff").write_text(diff)
            raise RuntimeError(f"ordinary optimized IR changed: {name}; see {name}.diff")
        report.append(f"{name}.cpp: identical optimized AVM IR; sha256={hashlib.sha256(ir[0].encode()).hexdigest()}")
    report.append("Game/model.hpp: " + ("unchanged" if subprocess.check_output(
        ["git", "-c", f"safe.directory={root.as_posix()}", "diff", args.baseline, "--", "src/model.hpp"], cwd=root) == b"" else "CHANGED"))
    text = "\n".join(report) + "\n"
    (args.output / "proof.txt").write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
