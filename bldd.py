import os
import sys
import subprocess
import argparse
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas

def is_executable(file_path):
    try:
        output = subprocess.check_output(['file', file_path], text=True)
        return 'ELF' in output and 'executable' in output
    except subprocess.CalledProcessError:
        return False

def get_architecture(file_path):
    try:
        output = subprocess.check_output(['file', file_path], text=True)
        if 'x86-64' in output:
            return 'x86-64'
        elif 'Intel 80386' in output:
            return 'i386'
        elif 'ARM' in output and '32-bit' in output:
            return 'armv7'
        elif 'ARM aarch64' in output or 'ARM64' in output:
            return 'aarch64'
        else:
            return 'unknown'
    except subprocess.CalledProcessError:
        return 'unknown'

def get_libraries_readelf(file_path):
    try:
        output = subprocess.check_output(['readelf', '-d', file_path], text=True)
        libs = []
        for line in output.splitlines():
            if '(NEEDED)' in line:
                parts = line.split('[', 1)
                if len(parts) > 1:
                    lib = parts[1].rstrip(']')
                    libs.append(lib)
        return libs
    except subprocess.CalledProcessError:
        return []

def process_executable(path):
    if is_executable(path):
        arch = get_architecture(path)
        libs = get_libraries_readelf(path)
        return path, arch, libs
    return None

def scan_directory_parallel(directory):
    results = defaultdict(lambda: defaultdict(list))
    files_to_process = []

    for root, _, files in os.walk(directory):
        for name in files:
            path = os.path.join(root, name)
            files_to_process.append(path)

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
        futures = {executor.submit(process_executable, path): path for path in files_to_process}

        for future in as_completed(futures):
            result = future.result()
            if result:
                path, arch, libs = result
                for lib in libs:
                    results[arch][lib].append(path)

    return results

def generate_text_report(results, output_file):
    with open(output_file, 'w') as f:
        f.write("Report on dynamic used libraries by ELF executables\n\n")
        for arch in results:
            f.write(f"--------- {arch} ---------\n\n")
            sorted_libs = sorted(results[arch].items(), key=lambda item: len(item[1]), reverse=True)
            for lib, execs in sorted_libs:
                f.write(f"{lib} ({len(execs)} execs)\n")
                for exe in execs:
                    f.write(f"  -> {exe}\n")
                f.write("\n")

def generate_pdf_report(results, output_file):
    c = canvas.Canvas(output_file, pagesize=letter)
    width, height = letter
    c.setFont("Helvetica", 10)

    y = height - 40
    c.drawString(30, y, "Report on dynamic used libraries by ELF executables")
    y -= 30

    for arch in results:
        if y < 100:
            c.showPage()
            c.setFont("Helvetica", 10)
            y = height - 40
        c.drawString(30, y, f"--------- {arch} ---------")
        y -= 20

        sorted_libs = sorted(results[arch].items(), key=lambda item: len(item[1]), reverse=True)
        for lib, execs in sorted_libs:
            if y < 100:
                c.showPage()
                c.setFont("Helvetica", 10)
                y = height - 40
            c.drawString(40, y, f"{lib} ({len(execs)} execs)")
            y -= 20
            for exe in execs:
                if y < 100:
                    c.showPage()
                    c.setFont("Helvetica", 10)
                    y = height - 40
                c.drawString(60, y, f"-> {exe}")
                y -= 15
            y -= 10

    c.save()

def main():
    parser = argparse.ArgumentParser(description='bldd - backward ldd tool (readelf-based)')
    parser.add_argument('directory', help='Directory to scan')
    parser.add_argument('-o', '--output', default='report.txt', help='Output file (default: report.txt)')
    parser.add_argument('--pdf', action='store_true', help='Generate PDF report instead of text')
    args = parser.parse_args()

    results = scan_directory_parallel(args.directory)

    if args.pdf:
        if not args.output.endswith('.pdf'):
            print("When using --pdf, please specify proper output filename!")
            sys.exit(1)
        generate_pdf_report(results, args.output)
    else:
        generate_text_report(results, args.output)

    print(f"Report: {args.output}")

if __name__ == '__main__':
    main()
