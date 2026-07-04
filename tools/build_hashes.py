import json
import sys
import os

def main():
    if len(sys.argv) != 4:
        print("Usage: build_hashes.py <input.json> <output.cpp> <output.h>")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_cpp = sys.argv[2]
    output_h = sys.argv[3]
    
    # Ensure output directories exist
    os.makedirs(os.path.dirname(output_cpp), exist_ok=True)
    os.makedirs(os.path.dirname(output_h), exist_ok=True)
    
    with open(input_file, 'r') as f:
        data = json.load(f)
        
    # Group hashes by absolute file path
    # e.g. path: "/vol/system/title/00050010/1000400a/code/nn_dlp.rpl" -> list of valid hashes
    regions = {
        "ALL": "Common",
        "JPN": "Jp",
        "USA": "Us",
        "EUR": "Eu"
    }

    file_hashes = {
        "Common": {},
        "Jp": {},
        "Us": {},
        "Eu": {}
    }
    
    for title in data:
        title_id = title['title_id'].lower()
        if len(title_id) != 16:
            continue
        high_id = title_id[:8]
        low_id = title_id[8:]
        
        if high_id == "00050010" and low_id.startswith("1004"):
            base_path = f"/vol/storage_mlc01/sys/title/{high_id}/{low_id}"
        else:
            base_path = f"/vol/system/title/{high_id}/{low_id}"
        
        region_key = regions.get(title.get('region', 'ALL'), 'Common')
        
        for rel_path, filehash in title['files'].items():
            full_path = f"{base_path}/{rel_path}"
            
            if full_path not in file_hashes[region_key]:
                file_hashes[region_key][full_path] = set()
            file_hashes[region_key][full_path].add(filehash)
            
    # Write .h
    h_content = """#pragma once
#include <stddef.h>

struct SystemFileHashes {
    const char* filepath;
    const char* const* valid_hashes;
    size_t num_hashes;
};

extern const SystemFileHashes g_systemFileHashesCommon[];
extern const size_t g_numSystemFileHashesCommon;

extern const SystemFileHashes g_systemFileHashesJp[];
extern const size_t g_numSystemFileHashesJp;

extern const SystemFileHashes g_systemFileHashesUs[];
extern const size_t g_numSystemFileHashesUs;

extern const SystemFileHashes g_systemFileHashesEu[];
extern const size_t g_numSystemFileHashesEu;
"""
    with open(output_h, 'w') as f:
        f.write(h_content)
        
    # Write .cpp
    cpp_content = """#include "system_hashes_data.h"

"""
    # Create arrays of hashes
    for region_name, hashes_dict in file_hashes.items():
        for i, (filepath, hashes) in enumerate(hashes_dict.items()):
            cpp_content += f"static const char* s_hashes_{region_name}_{i}[] = {{\n"
            for h in sorted(list(hashes)):
                cpp_content += f'    "{h}",\n'
            cpp_content += "};\n\n"
            
    for region_name, hashes_dict in file_hashes.items():
        if len(hashes_dict) == 0:
            cpp_content += f"const SystemFileHashes g_systemFileHashes{region_name}[] = {{}};\n\n"
            cpp_content += f"const size_t g_numSystemFileHashes{region_name} = 0;\n\n"
            continue
            
        cpp_content += f"const SystemFileHashes g_systemFileHashes{region_name}[] = {{\n"
        for i, (filepath, hashes) in enumerate(hashes_dict.items()):
            num_hashes = len(hashes)
            cpp_content += f'    {{"{filepath}", s_hashes_{region_name}_{i}, {num_hashes}}},\n'
        cpp_content += "};\n\n"
        cpp_content += f"const size_t g_numSystemFileHashes{region_name} = sizeof(g_systemFileHashes{region_name}) / sizeof(g_systemFileHashes{region_name}[0]);\n\n"
    
    with open(output_cpp, 'w') as f:
        f.write(cpp_content)

if __name__ == '__main__':
    main()
