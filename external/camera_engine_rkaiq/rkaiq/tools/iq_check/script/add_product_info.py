#!/usr/bin/env python3
"""
Add or update product_info in IQ JSON files.
Reads platform version info from RkAiqVersion.h.
"""
import json
import os
import re
import sys

# Map isp directory names to their platform definitions
ISP_TO_PLATFORM = {
    'isp20': 'ISP_HW_V21',
    'isp21': 'ISP_HW_V21',
    'isp30': 'ISP_HW_V30',
    'isp3x': 'ISP_HW_V30',
    'isp32': 'ISP_HW_V32',
    'isp32_lite': 'ISP_HW_V32',
    'isp33': 'ISP_HW_V33',
    'isp35': 'ISP_HW_V35',
    'isp351s': 'ISP_HW_V351',
    'isp39': 'ISP_HW_V39',
}

# Default values (from RkAiqCalibDbTypesV2.h M4_DEFAULT comments)
# - chip_name: from CHIP_NAME macro in RkAiqVersion.h
# - iq_struct_mark: from IQ_STRUCT_MARK macro in RkAiqVersion.h
# - author: M4_DEFAULT(RK)
# - date: M4_DEFAULT(0) -> default "0"
# - iq_ver: M4_DEFAULT(0) -> default "0"
DEFAULT_PRODUCT_INFO = {
    "chip_name": "RV1126B",
    "author": "RK",
    "date": "0",
    "iq_struct_mark": "1.0.0",
    "iq_ver": "0"
}


def parse_rkaiq_version_header(header_path):
    """Parse RkAiqVersion.h to extract version info for each platform."""
    platform_info = {}
    
    if not os.path.exists(header_path):
        print(f"Warning: {header_path} not found, using defaults")
        return None
    
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Extract main version only (for iq_struct_mark)
    version_match = re.search(r'#define\s+RK_AIQ_VERSION_REAL_V\s+"([^"]+)"', content)
    main_version = version_match.group(1) if version_match else "unknown"
    
    # Find the section between RK_AIQ_RELEASE_DATE and #else or #endif
    section_match = re.search(
        r'#define\s+RK_AIQ_RELEASE_DATE.*?(?=#else|#endif)',
        content, re.DOTALL
    )
    
    if section_match:
        section = section_match.group(0)
        
        # Match both #if and #elif patterns
        isp_hw_pattern = r'#(?:if|elif)\s+defined\((ISP_HW_V\d+)'
        
        for match in re.finditer(isp_hw_pattern, section):
            isp_hw = match.group(1)
            
            # Find the CHIP_NAME that follows this if/elif
            pos = match.end()
            chip_match = re.search(r'#define\s+CHIP_NAME\s+"([^"]+)"', section[pos:pos+500])
            mark_match = re.search(r'#define\s+IQ_STRUCT_MARK\s+"([^"]+)"', section[pos:pos+500])
            
            if chip_match and mark_match:
                platform_info[isp_hw] = {
                    "chip_name": chip_match.group(1),
                    "iq_struct_mark": mark_match.group(1)
                }
    
    return platform_info


def get_isp_dir_name(base_dir):
    """Extract isp directory name from base_dir path."""
    # base_dir like /path/to/iqfiles/isp35
    return os.path.basename(base_dir)


def detect_newline(content):
    """Detect the newline style of the content."""
    if '\r\n' in content:
        return '\r\n'  # Windows CRLF
    return '\n'  # Unix LF


def extract_product_info(content):
    """Extract product_info from existing JSON content."""
    # Try to find and parse product_info section
    match = re.search(r'"product_info"\s*:\s*\{[^}]+\}', content, re.DOTALL)
    if match:
        try:
            # Find the full JSON object starting from {
            start = content.find('{', match.start())
            if start != -1:
                # Find matching closing brace at the same level
                depth = 0
                end = start
                in_string = False
                escape = False
                for i, c in enumerate(content[start:], start):
                    if escape:
                        escape = False
                        continue
                    if c == '\\':
                        escape = True
                        continue
                    if c == '"' and not escape:
                        in_string = not in_string
                    if not in_string:
                        if c == '{':
                            depth += 1
                        elif c == '}':
                            depth -= 1
                            if depth == 0:
                                end = i + 1
                                break
                
                product_str = content[start:end]
                # Fix JSON to be valid (add missing quotes if needed)
                product_str = re.sub(r'(\w+):', r'"\1":', product_str)
                return json.loads(product_str)
        except (json.JSONDecodeError, Exception) as e:
            print(f"Warning: Failed to parse existing product_info: {e}")
    return None


def needs_update(existing_info, new_info):
    """Check if product_info needs to be updated.
    Only check if iq_struct_mark changed."""
    if not existing_info:
        return True
    
    # Only check if iq_struct_mark changed
    if existing_info.get("iq_struct_mark") != new_info.get("iq_struct_mark"):
        return True
    
    return False


def add_or_update_product_info(base_dir, platform_info):
    """Add or update product_info in IQ JSON files."""
    isp_dir = get_isp_dir_name(base_dir)
    isp_macro = ISP_TO_PLATFORM.get(isp_dir, None)
    
    if isp_macro and platform_info and isp_macro in platform_info:
        default_info = platform_info[isp_macro]
    else:
        print(f"Warning: No specific platform info for {isp_dir}, using defaults")
        default_info = DEFAULT_PRODUCT_INFO.copy()
    
    count_added = 0
    count_updated = 0
    count_skipped = 0
    
    for root, dirs, files in os.walk(base_dir):
        for f in files:
            if not f.endswith(".json"):
                continue
            
            # Skip symlinks
            path = os.path.join(root, f)
            if os.path.islink(path):
                count_skipped += 1
                continue
            
            try:
                with open(path, "rb") as fp:
                    raw_content = fp.read()
                
                # Detect original newline style
                content = raw_content.decode('utf-8')
                newline = detect_newline(content)
                
                # Check if product_info exists
                existing_info = extract_product_info(content)
                
                if existing_info is None:
                    # product_info doesn't exist, add it with default values
                    # date and iq_ver should be "0" (M4_DEFAULT(0) in RkAiqCalibDbTypesV2.h)
                    # chip_name and iq_struct_mark come from platform info
                    product_info_str = (
                        '\t"product_info": {' + newline +
                        '\t\t"chip_name": "{}",'.format(default_info["chip_name"]) + newline +
                        '\t\t"author": "{}",'.format(default_info.get("author", "RK")) + newline +
                        '\t\t"date": "0",' + newline +
                        '\t\t"iq_struct_mark": "{}",'.format(default_info["iq_struct_mark"]) + newline +
                        '\t\t"iq_ver": "0"' + newline +
                        '\t},'
                    )
                    
                    # Insert at the beginning
                    new_content = '{' + newline + product_info_str + content[1:]
                    
                    with open(path, "w", encoding="utf-8", newline='') as fp:
                        fp.write(new_content)
                    count_added += 1
                    print(f"Added: {path}")
                
                elif needs_update(existing_info, default_info):
                    # product_info exists but only iq_struct_mark needs update
                    # Keep existing date, iq_ver, chip_name, author; only update iq_struct_mark
                    product_info_str = (
                        '\t"product_info": {' + newline +
                        '\t\t"chip_name": "{}",'.format(existing_info.get("chip_name", default_info.get("chip_name", ""))) + newline +
                        '\t\t"author": "{}",'.format(existing_info.get("author", "RK")) + newline +
                        '\t\t"date": "{}",'.format(existing_info.get("date", "0")) + newline +
                        '\t\t"iq_struct_mark": "{}",'.format(default_info["iq_struct_mark"]) + newline +
                        '\t\t"iq_ver": "{}"'.format(existing_info.get("iq_ver", "0")) + newline +
                        '\t},'
                    )
                    
                    # Replace existing product_info
                    pattern = r'\t"product_info": \{[^}]+\},?\s*\n?'
                    new_content = re.sub(pattern, product_info_str, content)
                    
                    with open(path, "w", encoding="utf-8", newline='') as fp:
                        fp.write(new_content)
                    count_updated += 1
                    print(f"Updated: {path}")
                else:
                    count_skipped += 1
                    
            except Exception as e:
                print(f"Error: {path} - {e}")
    
    print(f"\nSummary for {isp_dir}: Added={count_added}, Updated={count_updated}, Skipped={count_skipped}")
    return count_added + count_updated


def main():
    # Base IQ files directory (use relative path from script location)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # script is at rk_aiq/tools/iq_check/script/, go up 4 levels to reach repo root
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(script_dir))))
    iqfiles_base = os.path.join(repo_root, "rk_aiq", "iqfiles")
    version_header = os.path.join(repo_root, "rk_aiq", "RkAiqVersion.h")
    
    # Parse version header
    print("Parsing RkAiqVersion.h...")
    platform_info = parse_rkaiq_version_header(version_header)
    
    if platform_info:
        print("Found platform info:")
        for k, v in platform_info.items():
            print(f"  {k}: chip={v['chip_name']}, struct={v['iq_struct_mark']}")
    print()
    
    # Process each ispXX directory
    total_added = 0
    total_updated = 0
    
    if len(sys.argv) > 1:
        # Process specific directory
        target_dirs = [sys.argv[1]]
    else:
        # Process all isp directories
        target_dirs = sorted(ISP_TO_PLATFORM.keys())
    
    for isp_dir in target_dirs:
        dir_path = os.path.join(iqfiles_base, isp_dir)
        if os.path.isdir(dir_path):
            print(f"Processing {isp_dir}...")
            count = add_or_update_product_info(dir_path, platform_info)
            total_added += count
        else:
            print(f"Skipping {isp_dir}: directory not found")
    
    print(f"\nTotal files modified: {total_added}")


if __name__ == "__main__":
    main()
