import gzip
import struct
import os
import sys

# Constants
INSTR_STRUCT_FORMAT = "<QBB4B2Q4Q"  # Matches trace_instr_format_t
INSTR_SIZE = struct.calcsize(INSTR_STRUCT_FORMAT)

def split_trace_file_gz(input_filename, cutpoint):
    if not input_filename.endswith(".out.gz"):
        print("Expected input file with '.out.gz' extension.")
        return

    base_name = input_filename.replace(".out.gz", "")
    count = 0
    instr_count = 0
    output_file = None

    with gzip.open(input_filename, "rb") as f:
        while True:
            data = f.read(INSTR_SIZE)
            if not data or len(data) < INSTR_SIZE:
                break

            if instr_count % cutpoint == 0:
                if output_file:
                    output_file.close()
                output_filename = f"{base_name}_{count:03}.out.gz"
                output_file = gzip.open(output_filename, "wb")
                count += 1

            output_file.write(data)
            instr_count += 1

    if output_file:
        output_file.close()

    print(f"Done. Total instructions: {instr_count}. Files created: {count}")

# Entry point
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 cut_champsim_trace.py <trace_file.out.gz> <cutpoint>")
        print("Example: python3 cut_champsim_trace.py mytrace.out.gz 150000000")
        sys.exit(1)

    input_filename = sys.argv[1]
    try:
        cutpoint = int(sys.argv[2])
    except ValueError:
        print("Cutpoint must be an integer.")
        sys.exit(1)

    split_trace_file_gz(input_filename, cutpoint)
