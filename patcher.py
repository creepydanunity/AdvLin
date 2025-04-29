import sys

def patch_binary(file_path, find_bytes, replace_bytes):
    with open(file_path, 'rb') as f:
        data = f.read()

    index = data.find(find_bytes)
    if index == -1:
        print("Not found")
        return False

    print(f"Pattern found at offset 0x{index:X}")
  
    patched_data = data[:index] + replace_bytes + data[index + len(find_bytes):]

    with open(file_path, 'wb') as f:
        f.write(patched_data)

    print("File patched")
    return True

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <binary_file>")
        sys.exit(1)

    file_path = sys.argv[1]
    
    find_bytes = bytes.fromhex('75 07')
    replace_bytes = bytes.fromhex('90 90')

    patch_binary(file_path, find_bytes, replace_bytes)

if __name__ == '__main__':
    main()
