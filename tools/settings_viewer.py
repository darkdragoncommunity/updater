import json
import argparse
import os

def decrypt_settings(input_file, key):
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found.")
        return

    with open(input_file, "r") as f:
        hex_data = f.read().strip()

    try:
        # 1. Convert HEX string back to bytes
        xor_bytes = bytes.fromhex(hex_data)
        
        # 2. XOR bytes with the key
        key_bytes = key.encode()
        decrypted_bytes = bytes([xor_bytes[i] ^ key_bytes[i % len(key_bytes)] for i in range(len(xor_bytes))])
        
        # 3. Decode JSON string
        json_str = decrypted_bytes.decode('utf-8')
        settings = json.loads(json_str)
        
        # 4. Print pretty-formatted JSON
        print("\n--- Decrypted settings.bin Contents ---")
        print(json.dumps(settings, indent=4))
        print("----------------------------------------\n")
        
    except Exception as e:
        print(f"Error decrypting settings: {e}")
        print("Make sure the encryption key matches the one used to create the file.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="View decrypted contents of settings.bin.")
    parser.add_argument("--input", default="settings.bin", help="Input filename (default: settings.bin)")
    parser.add_argument("--key", default=os.getenv("LAUNCHER_KEY", "CHANGE_ME_DEFAULT_KEY_2026"), help="Encryption key (can be set via LAUNCHER_KEY env var)")
    
    args = parser.parse_args()
    
    # Check if settings.bin is in the current directory or parent directory
    input_path = args.input
    if not os.path.exists(input_path):
        # Try root directory if called from tools/
        parent_path = os.path.join("..", args.input)
        if os.path.exists(parent_path):
            input_path = parent_path
            
    decrypt_settings(input_path, args.key)
