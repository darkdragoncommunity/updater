import os
import json
import hashlib
import argparse
import zlib
import fnmatch
from datetime import datetime

def calculate_sha256(filepath):
    """Calculates the SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def load_existing_manifest(manifest_path):
    """Loads existing manifest entries in order."""
    entries = []
    lookup = {}
    if not os.path.exists(manifest_path):
        return entries, lookup
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            for entry in data.get("files", []):
                path = entry.get("path", "")
                if not path:
                    continue
                entries.append(entry)
                lookup[path] = entry
    except Exception:
        pass
    return entries, lookup

def compress_file(src_path, dest_path):
    """Compresses a file using zlib (DEFLATE)."""
    with open(src_path, 'rb') as f_in:
        with open(dest_path, 'wb') as f_out:
            compressor = zlib.compressobj(zlib.Z_BEST_COMPRESSION, zlib.DEFLATED, -zlib.MAX_WBITS)
            for chunk in iter(lambda: f_in.read(4096), b''):
                f_out.write(compressor.compress(chunk))
            f_out.write(compressor.flush())

    sha = hashlib.sha256()
    with open(dest_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            sha.update(chunk)
    return os.path.getsize(dest_path), sha.hexdigest()

def normalize_pattern(pattern):
    return pattern.strip().replace("\\", "/").strip("/")

def should_exclude(relative_path, name, exclude_patterns):
    normalized_path = relative_path.replace("\\", "/").strip("/")
    normalized_name = name.strip("/")

    for pattern in exclude_patterns:
        normalized_pattern = normalize_pattern(pattern)
        if not normalized_pattern:
            continue

        if "/" in normalized_pattern:
            if fnmatch.fnmatch(normalized_path, normalized_pattern):
                return True
            if normalized_path == normalized_pattern or normalized_path.startswith(normalized_pattern + "/"):
                return True
        else:
            if fnmatch.fnmatch(normalized_name, normalized_pattern):
                return True
    return False

def load_exclude_file(path):
    patterns = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            entry = line.strip()
            if not entry or entry.startswith("#"):
                continue
            patterns.append(entry)
    return patterns

def load_json_file(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def normalize_customs_options(customs_data):
    if not customs_data:
        return {}
    options = {}
    if isinstance(customs_data, dict):
        options = customs_data.get("options", {}) if "options" in customs_data else {}
    return options


def normalize_customs_run_path(customs_data):
    if not customs_data:
        return ""
    return customs_data.get("run_path", "") if isinstance(customs_data, dict) else ""


def merge_custom_fields(customs_data, manifest_data):
    if not customs_data or not isinstance(customs_data, dict):
        return
    for key, value in customs_data.items():
        if key in {"files", "manifest_version", "options"}:
            continue
        manifest_data[key] = value


def build_manifest_path(prefix, relative_path):
    normalized = relative_path.replace("\\", "/").strip("./")
    if not normalized:
        return prefix
    if prefix:
        cleaned = prefix.rstrip("/\\")
        return f"{cleaned}/{normalized}"
    return normalized


def scan_directory(source_dir, prefix, updated_entries_order, updated_entries_map, existing_manifest, exclude_patterns, skip_if_exists_patterns, compress, patch_dir, verbose):
    if not os.path.isdir(source_dir):
        if verbose:
            print(f"Skipping missing directory: {source_dir}")
        return

    for root, dirs, files in os.walk(source_dir):
        current_rel_root = os.path.relpath(root, source_dir)
        if current_rel_root == ".":
            current_rel_root = ""

        dirs[:] = [d for d in dirs if not should_exclude(build_manifest_path(prefix, os.path.join(current_rel_root, d)), d, exclude_patterns)]
        dirs.sort()
        files.sort()

        for filename in files:
            filepath = os.path.join(root, filename)
            relative_path = os.path.relpath(filepath, source_dir)
            normalized_path = build_manifest_path(prefix, relative_path)
            if should_exclude(normalized_path, filename, exclude_patterns):
                if verbose:
                    print(f"Skipping excluded path: {normalized_path}")
                continue

            try:
                file_size = os.path.getsize(filepath)
                file_mtime = os.path.getmtime(filepath)

                if normalized_path in updated_entries_map:
                    continue

                existing_entry = existing_manifest.get(normalized_path)
                if existing_entry and existing_entry.get("size") == file_size and existing_entry.get("mtime") == file_mtime:
                    updated_entries_map[normalized_path] = existing_entry
                    updated_entries_order.append(normalized_path)
                    continue

                print(f"Processing: {normalized_path}")
                file_hash = calculate_sha256(filepath)

                entry = {
                    "path": normalized_path,
                    "size": file_size,
                    "sha256": file_hash,
                    "mtime": file_mtime
                }
                if should_exclude(normalized_path, filename, skip_if_exists_patterns):
                    entry["skip_if_exists"] = True

                if compress:
                    dest_path = os.path.join(patch_dir, normalized_path + ".gz")
                    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
                    c_size, c_hash = compress_file(filepath, dest_path)
                    entry["compressed_size"] = c_size
                    entry["compressed_sha256"] = c_hash
                    print(f"  -> Compressed: {normalized_path} -> {c_size} bytes")

                updated_entries_map[normalized_path] = entry
                updated_entries_order.append(normalized_path)
            except Exception as e:
                print(f"Error processing {filename}: {e}")


def generate_manifest(source_dir, output_file, exclude_patterns, skip_if_exists_patterns, compress=False, patch_dir="patch_files", run_path="", manifest_version="", customs=None, keep_missing=False, custom_folder="", custom_prefix="", verbose=False):
    """Generates a manifest and optionally pre-compresses files."""
    old_manifest_order, old_manifest_lookup = load_existing_manifest(output_file)
    manifest_data = {"files": []}

    if manifest_version:
        manifest_data["manifest_version"] = manifest_version

    custom_run_path = normalize_customs_run_path(customs)
    if custom_run_path:
        manifest_data["run_path"] = custom_run_path
    elif run_path:
        manifest_data["run_path"] = run_path

    if customs:
        customs_options = normalize_customs_options(customs)
        if customs_options:
            manifest_data["options"] = customs_options
        merge_custom_fields(customs, manifest_data)

    output_dir = os.path.dirname(output_file)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    if compress and not os.path.exists(patch_dir):
        os.makedirs(patch_dir)

    updated_entries_order = []
    updated_entries_map = {}
    scan_directory(source_dir, "", updated_entries_order, updated_entries_map, old_manifest_lookup, exclude_patterns, skip_if_exists_patterns, compress, patch_dir, verbose)

    if custom_folder:
        cleaned_prefix = custom_prefix.strip("/\\") if custom_prefix else ""
        if verbose:
            print(f"Scanning custom folder {custom_folder} (prefix='{cleaned_prefix}')")
        scan_directory(custom_folder, cleaned_prefix, updated_entries_order, updated_entries_map, old_manifest_lookup, exclude_patterns, skip_if_exists_patterns, compress, patch_dir, verbose)

    final_files = []
    seen_paths_final = set()
    for entry in old_manifest_order:
        path = entry.get("path", "")
        if not path:
            continue
        if path in updated_entries_map:
            final_files.append(updated_entries_map[path])
            seen_paths_final.add(path)
        elif keep_missing:
            final_files.append(entry)
            seen_paths_final.add(path)

    for path in updated_entries_order:
        if path not in seen_paths_final:
            final_files.append(updated_entries_map[path])
            seen_paths_final.add(path)

    manifest_data["files"] = final_files

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(manifest_data, f, indent=2)

    print(f"\nManifest saved: {output_file}")


def write_manifest_version_file(version, version_file, default_dir):
    if not version or not version_file:
        return
    target_path = version_file
    if not os.path.isabs(target_path):
        target_path = os.path.join(default_dir or ".", target_path)
    os.makedirs(os.path.dirname(target_path), exist_ok=True)
    with open(target_path, "w", encoding="utf-8") as vf:
        vf.write(version)
    print(f"Manifest version saved: {target_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate manifest and compress files.")
    parser.add_argument("source", help="Game directory.")
    parser.add_argument("--output", default="manifest.json")
    parser.add_argument("--run-path", default="", help="Path (relative to game root) that the launcher should run.")
    parser.add_argument("--version", default="", help="Override the manifest version token. Defaults to UTC timestamp.")
    parser.add_argument("--version-file", default="manifest_version.txt", help="File that should receive the manifest version token.")
    parser.add_argument("--customs", default="", help="Optional JSON file with additional manifest sections (run_path, options, etc.).")
    parser.add_argument("--custom-folder", default="", help="Additional directory whose files should be added to the manifest.")
    parser.add_argument("--custom-prefix", default="", help="Prefix to apply to paths imported from --custom-folder.")
    parser.add_argument("--no-delete", action="store_true", help="Keep entries from the previous manifest even if they are missing locally.")
    parser.add_argument("--debug", action="store_true", help="Print verbose diagnostics during manifest generation.")
    parser.add_argument("--compress", action="store_true", help="Pre-compress files for the server.")
    parser.add_argument("--patch-dir", default="patch_server", help="Where to save compressed files.")
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="Exclude a file, folder, or glob. Repeat as needed. Examples: --exclude logs --exclude '*.log' --exclude 'system/cheat/*'"
    )
    parser.add_argument(
        "--exclude-from",
        action="append",
        default=[],
        help="Read exclude patterns from a file, one per line. Lines starting with # are ignored."
    )
    parser.add_argument(
        "--skip-if-exists",
        action="append",
        default=[],
        help="Mark matching files with skip_if_exists in the manifest so existing local files are preserved during normal updates."
    )
    parser.add_argument(
        "--skip-if-exists-from",
        action="append",
        default=[],
        help="Read skip_if_exists patterns from a file, one per line. Lines starting with # are ignored."
    )

    args = parser.parse_args()
    exclude_patterns = ["manifest.json", ".DS_Store", ".gitattributes", "translate_l2", ".geminiignore"]
    exclude_patterns.extend(args.exclude)
    for exclude_file in args.exclude_from:
        exclude_patterns.extend(load_exclude_file(exclude_file))
    skip_if_exists_patterns = []
    skip_if_exists_patterns.extend(args.skip_if_exists)
    for skip_file in args.skip_if_exists_from:
        skip_if_exists_patterns.extend(load_exclude_file(skip_file))
    customs_data = None
    if args.customs:
        customs_data = load_json_file(args.customs)

    manifest_token = args.version.strip() if args.version else datetime.utcnow().strftime("%Y%m%d%H%M%S")
    if not manifest_token:
        manifest_token = datetime.utcnow().strftime("%Y%m%d%H%M%S")

    generate_manifest(
        args.source,
        args.output,
        exclude_patterns,
        skip_if_exists_patterns,
        args.compress,
        args.patch_dir,
        args.run_path,
        manifest_token,
        customs_data,
        keep_missing=args.no_delete,
        custom_folder=args.custom_folder,
        custom_prefix=args.custom_prefix,
        verbose=args.debug
    )
    write_manifest_version_file(manifest_token, args.version_file, os.path.dirname(args.output) or os.getcwd())
