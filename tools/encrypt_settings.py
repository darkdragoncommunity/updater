import json
import argparse
import os

def process_xor(data, key):
    return "".join(chr(ord(data[i]) ^ ord(key[i % len(key)])) for i in range(len(data)))

def to_hex(data):
    return data.encode().hex()

def build_social_urls(base_url):
    base = base_url.rstrip("/")
    return {
        "website": base,
        "discord": f"{base}/discord",
        "twitch": f"{base}/twitch",
        "facebook": f"{base}/facebook",
        "tiktok": f"{base}/tiktok",
        "youtube": f"{base}/youtube",
        "instagram": f"{base}/instagram",
    }

def encrypt_settings(patch_url, key, output_file, website_url=None, remote_debug_enabled=False, remote_debug_log_url=None, patch_mirror_urls=None):
    settings = {
        "patch_url": patch_url
    }
    if patch_mirror_urls:
        settings["patch_mirror_urls"] = patch_mirror_urls
    if website_url:
        settings["social_urls"] = build_social_urls(website_url)
    if remote_debug_enabled:
        settings["remote_debug_enabled"] = True
    if remote_debug_log_url:
        settings["remote_debug_log_url"] = remote_debug_log_url
    json_str = json.dumps(settings)
    encrypted = process_xor(json_str, key)
    # The C++ code expects a hex string of the XORed result
    # Actually my C++ code does: FromHex(hexData) -> ProcessXOR
    # So here: json_str -> ProcessXOR -> ToHex
    
    # Wait, my C++ code: decrypted = CryptoUtils::ProcessXOR(CryptoUtils::FromHex(hexData), m_encryptionKey);
    # So I need to:
    # 1. Take JSON string
    # 2. XOR it
    # 3. Convert XOR result to HEX
    
    # Re-implement XOR on raw bytes to be safe
    key_bytes = key.encode()
    json_bytes = json_str.encode()
    xor_result = bytes([json_bytes[i] ^ key_bytes[i % len(key_bytes)] for i in range(len(json_bytes))])
    hex_result = xor_result.hex()
    
    with open(output_file, "w") as f:
        f.write(hex_result)
    print(f"Settings encrypted to {output_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Encrypt launcher settings.")
    parser.add_argument("--config", help="Path to a JSON config file containing settings")
    parser.add_argument("--url", help="Patch server URL (overrides config)")
    parser.add_argument("--website-url", help="Base website URL used to build social links (overrides config)")
    parser.add_argument("--mirror-url", action="append", default=[], help="Additional patch mirror base URL. Can be repeated.")
    parser.add_argument("--enable-remote-debug", action="store_true", help="Enable remote launcher log uploads")
    parser.add_argument("--remote-debug-log-url", help="HTTP endpoint for log uploads")
    parser.add_argument("--key", default=os.getenv("LAUNCHER_KEY", "CHANGE_ME_DEFAULT_KEY_2026"), help="Encryption key (can be set via LAUNCHER_KEY env var)")
    parser.add_argument("--output", default="settings.bin", help="Output filename")
    
    args = parser.parse_args()

    patch_url = args.url
    website_url = args.website_url
    mirrors = args.mirror_url
    remote_debug = args.enable_remote_debug
    remote_debug_url = args.remote_debug_log_url

    if args.config and os.path.exists(args.config):
        with open(args.config, "r") as f:
            c = json.load(f)
            if not patch_url: patch_url = c.get("patch_url")
            if not website_url: website_url = c.get("website_url")
            if not mirrors: mirrors = c.get("patch_mirror_urls", [])
            if not remote_debug: remote_debug = c.get("remote_debug_enabled", False)
            if not remote_debug_url: remote_debug_url = c.get("remote_debug_log_url")

    if not patch_url:
        print("Error: patch_url is required (via --url or --config)")
        exit(1)

    encrypt_settings(patch_url, args.key, args.output, website_url, remote_debug, remote_debug_url, mirrors)
