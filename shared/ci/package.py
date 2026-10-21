#!/usr/bin/env python3
"""Package JUCE build artefacts: package.py <build_dir> <name> <out_dir>.

Produces <name>-vst3.zip and <name>-standalone.zip (tar.gz is not used so the
same format works on every platform). Fails if either artefact is missing.
"""
import os
import shutil
import sys
import tempfile


def find_artefact(build, suffix_dir, extensions):
    root = os.path.join(build, "TabPlayer_artefacts")
    for dirpath, dirnames, filenames in os.walk(root):
        if os.path.basename(dirpath) != suffix_dir:
            continue
        for entry in dirnames + filenames:
            if entry.lower().endswith(extensions) or entry == "TabPlayer":
                return os.path.join(dirpath, entry)
    return None


def package(path, out_zip_base):
    with tempfile.TemporaryDirectory() as tmp:
        dest = os.path.join(tmp, os.path.basename(path))
        if os.path.isdir(path):
            shutil.copytree(path, dest, symlinks=True)
        else:
            shutil.copy2(path, dest)
        shutil.copy2("README.md", tmp)
        shutil.make_archive(out_zip_base, "zip", tmp)


def main():
    build, name, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    vst3 = find_artefact(build, "VST3", (".vst3",))
    standalone = find_artefact(build, "Standalone", (".app", ".exe"))
    if not vst3 or not standalone:
        print(f"missing artefact: vst3={vst3} standalone={standalone}", file=sys.stderr)
        sys.exit(1)
    package(vst3, os.path.join(out, f"{name}-vst3"))
    package(standalone, os.path.join(out, f"{name}-standalone"))
    print("packaged", vst3, standalone)


main()
