import hashlib
import sys

def generate_license(hwid: str) -> str:
    md5_hash = hashlib.md5(hwid.encode()).digest()
    reversed_md5 = md5_hash[::-1]
    
    license = ''.join(f'{byte:02x}' for byte in reversed_md5)
    return license

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <HWID>")
        sys.exit(1)
    
    hwid = sys.argv[1]
    key = generate_license(hwid)
    print(f"License key:\n{key}")
