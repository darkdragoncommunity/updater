import os
import json
import hashlib
import argparse
import zlib
import boto3
from botocore.config import Config
from git import Repo

# Simple Internal Encryption for Settings (Same as C++ / Python helper)
def process_xor(data, key):
    key_bytes = key.encode()
    data_bytes = data.encode() if isinstance(data, str) else data
    return bytes([data_bytes[i] ^ key_bytes[i % len(key_bytes)] for i in range(len(data_bytes))])

def calculate_sha256(filepath):
    """Calculates the SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

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

class DeployTool:
    def __init__(self, config):
        self.config = config
        self.s3 = boto3.client(
            's3',
            endpoint_url=config['r2_endpoint'],
            aws_access_key_id=config['r2_access_key'],
            aws_secret_access_key=config['r2_secret_key'],
            config=Config(signature_version='s3v4'),
            region_name='auto'
        )
        self.bucket = config['r2_bucket']
        self.master_manifest = self.load_master_manifest()

    def load_master_manifest(self):
        if os.path.exists("master_manifest.json"):
            with open("master_manifest.json", "r") as f:
                return json.load(f)
        return {"files": []}

    def save_master_manifest(self):
        with open("master_manifest.json", "w") as f:
            json.dump(self.master_manifest, f, indent=2)

    def get_git_changes(self, repo_path):
        repo = Repo(repo_path)
        # Use diff to get changes between last commit and current state
        # Or you can specify a commit range
        changed = [item.a_path for item in repo.index.diff(None)]
        added = repo.untracked_files
        deleted = [item.a_path for item in repo.index.diff("HEAD") if item.deleted_file]
        return added + changed, deleted

    def upload_to_r2(self, local_path, remote_path):
        print(f"Uploading {local_path} to R2...")
        self.s3.upload_file(local_path, self.bucket, remote_path)

    def deploy(self, base_dir, patch_repo_path):
        dirty_files, deleted_files = self.get_git_changes(patch_repo_path)
        
        # 1. Update master manifest with additions/modifications
        manifest_files = {f["path"]: f for f in self.master_manifest["files"]}
        
        for rel_path in dirty_files:
            full_path = os.path.join(patch_repo_path, rel_path)
            if not os.path.isfile(full_path): continue
            
            print(f"Processing dirty file: {rel_path}")
            
            # Hash and Compress
            os.makedirs("temp_patch", exist_ok=True)
            gz_path = os.path.join("temp_patch", rel_path + ".gz")
            os.makedirs(os.path.dirname(gz_path), exist_ok=True)
            
            c_size, c_hash = compress_file(full_path, gz_path)
            u_size = os.path.getsize(full_path)
            u_hash = calculate_sha256(full_path)
            
            # Update entry
            manifest_files[rel_path] = {
                "path": rel_path,
                "size": u_size,
                "sha256": u_hash,
                "compressed_size": c_size,
                "compressed_sha256": c_hash,
                "action": "update",
                "mtime": os.path.getmtime(full_path)
            }
            
            # Upload to R2
            self.upload_to_r2(gz_path, "files/" + rel_path + ".gz")

        # 2. Handle Deletions
        for rel_path in deleted_files:
            print(f"Marking for deletion: {rel_path}")
            if rel_path in manifest_files:
                manifest_files[rel_path]["action"] = "delete"

        # 3. Save Unified Manifest
        self.master_manifest["files"] = list(manifest_files.values())
        self.save_master_manifest()
        
        # 4. Upload manifest to R2
        self.upload_to_r2("master_manifest.json", "manifest.json")
        print("Deployment complete.")

if __name__ == "__main__":
    # Example usage:
    # python deploy_tool.py --patch-repo ./patch --endpoint https://xxx.r2.cloudflarestorage.com --key x --secret x --bucket x
    parser = argparse.ArgumentParser()
    parser.add_argument("--patch-repo", required=True)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--secret", required=True)
    parser.add_argument("--bucket", required=True)
    
    args = parser.parse_args()
    
    config = {
        'r2_endpoint': args.endpoint,
        'r2_access_key': args.key,
        'r2_secret_key': args.secret,
        'r2_bucket': args.bucket
    }
    
    deployer = DeployTool(config)
    deployer.deploy(None, args.patch_repo)
