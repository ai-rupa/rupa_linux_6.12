#!/usr/bin/env python3
"""
Add or update product_info in IQ JSON files.
Reads platform version info from RkAiqVersion.h (chip_name, iq_struct_mark)
and RkIspModuleHwVersion.h (module_info).
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
    'isp351s': 'ISP_HW_V351s',  # Note: 's' suffix in header
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


def parse_chip_info(header_path):
    """Parse RkAiqVersion.h to extract CHIP_NAME and IQ_STRUCT_MARK for each platform."""
    chip_info = {}
    
    if not os.path.exists(header_path):
        print(f"Warning: {header_path} not found")
        return None
    
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find all ISP_HW definitions
    isp_hw_pattern = r'#(?:if|elif)\s+defined\(([^)]+)\)'
    matches = list(re.finditer(isp_hw_pattern, content))
    print(f"DEBUG: Found {len(matches)} ISP_HW sections in chip header")
    
    for i, match in enumerate(matches):
        # Handle OR conditions like "ISP_HW_V32 || defined(ISP_HW_V32_LITE)"
        defines = match.group(1).split('||')
        for define in defines:
            define = define.strip()
            if not define.startswith('ISP_HW_'):
                continue
            
            isp_hw = define.replace('defined(', '').replace(')', '')
            
            # Find section boundaries
            # Section starts at match end (after the "#if defined(...)" or "#elif defined(...)")
            section_start = match.end()
            
            # Find the end of this section (next #elif, #else, or #endif)
            remaining = content[section_start:]
            section_match = re.search(r'#(?:elif|else|endif)\b', remaining)
            if section_match:
                section = remaining[:section_match.start()]
            else:
                section = remaining
            
            # Extract chip_name and iq_struct_mark from this section
            chip_match = re.search(r'#define\s+CHIP_NAME\s+"([^"]+)"', section)
            mark_match = re.search(r'#define\s+IQ_STRUCT_MARK\s+"([^"]+)"', section)
            
            if chip_match and mark_match:
                chip_info[isp_hw] = {
                    "chip_name": chip_match.group(1),
                    "iq_struct_mark": mark_match.group(1)
                }
                print(f"  {isp_hw}: chip={chip_match.group(1)}, struct={mark_match.group(1)}")
    
    return chip_info


def parse_module_info(header_path):
    """Parse RkIspModuleHwVersion.h to extract module versions for each platform."""
    module_info = {}
    
    if not os.path.exists(header_path):
        print(f"Warning: {header_path} not found")
        return None
    
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Match both #ifdef ISP_HW_Vxx and #elif defined(ISP_HW_Vxx) patterns
    isp_hw_pattern = r'#(?:ifdef|elif)\s+(?:defined\s*\()?((?:ISP_HW_V)\w+)'
    
    # Find all ISP_HW definitions and their sections
    matches = list(re.finditer(isp_hw_pattern, content))
    print(f"DEBUG: Found {len(matches)} ISP_HW sections in module header")
    
    # Module definitions to extract
    module_defs = [
        ("awbStats", r'#define\s+AWBSTATS_HW_VERSION\s+"([^"]+)"'),
        ("aeStats", r'#define\s+AESTATS_HW_VERSION\s+"([^"]+)"'),
        ("afStats", r'#define\s+AFSTATS_HW_VERSION\s+"([^"]+)"'),
        ("blc", r'#define\s+BLC_HW_VERSION\s+"([^"]+)"'),
        ("btnr", r'#define\s+BTNR_HW_VERSION\s+"([^"]+)"'),
        ("bnr", r'#define\s+BNR_HW_VERSION\s+"([^"]+)"'),
        ("cnr", r'#define\s+CNR_HW_VERSION\s+"([^"]+)"'),
        ("dm", r'#define\s+DM_HW_VERSION\s+"([^"]+)"'),
        ("dpc", r'#define\s+DPC_HW_VERSION\s+"([^"]+)"'),
        ("drc", r'#define\s+DRC_HW_VERSION\s+"([^"]+)"'),
        ("gamma", r'#define\s+GAMMA_HW_VERSION\s+"([^"]+)"'),
        ("ynr", r'#define\s+YNR_HW_VERSION\s+"([^"]+)"'),
        ("ytnr", r'#define\s+YTNR_HW_VERSION\s+"([^"]+)"'),
        ("sharp", r'#define\s+SHARP_HW_VERSION\s+"([^"]+)"'),
        ("lsc", r'#define\s+LSC_HW_VERSION\s+"([^"]+)"'),
        ("ccm", r'#define\s+CCM_HW_VERSION\s+"([^"]+)"'),
        ("csm", r'#define\s+CSM_HW_VERSION\s+"([^"]+)"'),
        ("dehaze", r'#define\s+DEHAZE_HW_VERSION\s+"([^"]+)"'),
        ("hdrmge", r'#define\s+HDRMGE_HW_VERSION\s+"([^"]+)"'),
        ("hsv", r'#define\s+HSV_HW_VERSION\s+"([^"]+)"'),
        ("ldcv", r'#define\s+LDCV_HW_VERSION\s+"([^"]+)"'),
        ("ldch", r'#define\s+LDCH_HW_VERSION\s+"([^"]+)"'),
        ("fpn", r'#define\s+FPN_HW_VERSION\s+"([^"]+)"'),
        ("gic", r'#define\s+GIC_HW_VERSION\s+"([^"]+)"'),
        ("lut3d", r'#define\s+LUT3D_HW_VERSION\s+"([^"]+)"'),
        ("rgbir", r'#define\s+RGBIR_HW_VERSION\s+"([^"]+)"'),
        ("yuvme", r'#define\s+YUVME_HW_VERSION\s+"([^"]+)"'),
        ("cac", r'#define\s+CAC_HW_VERSION\s+"([^"]+)"'),
        ("degm", r'#define\s+DEGM_HW_VERSION\s+"([^"]+)"'),
        ("enhance", r'#define\s+ENHANCE_HW_VERSION\s+"([^"]+)"'),
        ("aibnr", r'#define\s+AIBNR_HW_VERSION\s+"([^"]+)"'),
        ("airemosaic", r'#define\s+AIREMOSAIC_HW_VERSION\s+"([^"]+)"'),
        ("histeq", r'#define\s+HISTEQ_HW_VERSION\s+"([^"]+)"'),
    ]
    
    for i, match in enumerate(matches):
        isp_hw = match.group(1)
        
        # Find section boundaries
        # Section starts at match end (after the "#ifdef" or "#elif")
        section_start = match.end()
        
        # Find the end of this section (next #elif, #else, or #endif)
        remaining = content[section_start:]
        section_match = re.search(r'#(?:elif|else|endif)\b', remaining)
        if section_match:
            section = remaining[:section_match.start()]
        else:
            section = remaining
        
        print(f"  {isp_hw}: {len(section)} bytes")
        
        # Extract module versions (include all modules, even "no")
        modules = {}
        for module_name, pattern in module_defs:
            module_match = re.search(pattern, section)
            if module_match:
                version = module_match.group(1)
                modules[module_name] = version
        
        if modules:
            module_info[isp_hw] = modules
    
    return module_info


def merge_platform_info(chip_info, module_info):
    """Merge chip info and module info into a single platform_info dict."""
    platform_info = {}
    
    # Start with all chip_info entries
    if chip_info:
        for isp_hw, chip in chip_info.items():
            platform_info[isp_hw] = {
                "chip_name": chip["chip_name"],
                "iq_struct_mark": chip["iq_struct_mark"],
                "module_info": {}
            }
    
    # Merge module info
    if module_info:
        for isp_hw, modules in module_info.items():
            if isp_hw in platform_info:
                platform_info[isp_hw]["module_info"] = modules
            else:
                # Module info exists but no chip info for this platform
                platform_info[isp_hw] = {
                    "chip_name": "unknown",
                    "iq_struct_mark": "unknown",
                    "module_info": modules
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
    Check if iq_struct_mark or module_info changed."""
    if not existing_info:
        return True
    
    # Check if iq_struct_mark changed
    if existing_info.get("iq_struct_mark") != new_info.get("iq_struct_mark"):
        return True
    
    # Check if module_info needs update
    # If existing has no module_info but new has, needs update
    has_existing = "module_info" in existing_info and existing_info["module_info"]
    has_new = "module_info" in new_info and new_info["module_info"]
    
    if not has_existing and has_new:
        return True
    if has_existing and has_new and existing_info["module_info"] != new_info["module_info"]:
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
                        '\t\t"iq_ver": "0"'
                    )
                    
                    # Add module_info fields with proper indentation
                    if "module_info" in default_info and default_info["module_info"]:
                        product_info_str += ',' + newline
                        product_info_str += '\t\t"module_info": {' + newline
                        for i, (module, version) in enumerate(default_info["module_info"].items()):
                            if i > 0:
                                product_info_str += ',' + newline
                            product_info_str += '\t\t\t"{}": "{}"'.format(module, version)
                        product_info_str += newline + '\t\t}'
                    
                    product_info_str += newline + '\t\t}' + ',' + newline
                    
                    # Insert at the beginning
                    # Original content: "{\n\t\"key\": ..." or just "{\"key\": ..."
                    # content[1:] = "\n\t\"sensor_calib\"..." (with leading newline and tab)
                    # content[2:] = "\t\"sensor_calib\"..." (without leading newline, keeps tab)
                    
                    if content[1:].startswith(newline):
                        # content[1:] starts with "\n", so content[2:] removes the newline but keeps rest
                        rest = content[2:]
                    else:
                        rest = content[1:]
                    
                    new_content = '{\n' + product_info_str + rest
                    
                    with open(path, "w", encoding="utf-8", newline='') as fp:
                        fp.write(new_content)
                    count_added += 1
                    print(f"Added: {path}")
                
                elif needs_update(existing_info, default_info):
                    # product_info exists, check if need to update iq_struct_mark and/or module_info
                    new_content = content
                    
                    # Update iq_struct_mark
                    pattern = r'("iq_struct_mark":\s*")([^"]+)(")'
                    replacement = r'"iq_struct_mark": "{}"'.format(default_info["iq_struct_mark"])
                    new_content = re.sub(pattern, replacement, new_content)
                    
                    # Check if module_info exists in existing content
                    has_new_module = "module_info" in default_info and default_info["module_info"]
                    
                    # Initialize pi_close_match to None
                    pi_close_match = None
                    
                    if has_new_module:
                        # Check if module_info exists INSIDE product_info
                        # We need to find the product_info section first
                        # Extract product_info section
                        product_info_match = re.search(r'"product_info":\s*\{', new_content)
                        if product_info_match:
                            # Find the closing } of product_info
                            # Look for pattern: "product_info": {...}
                            prod_start = product_info_match.end()
                            depth = 1
                            pos = prod_start
                            while pos < len(new_content) and depth > 0:
                                if new_content[pos] == '{':
                                    depth += 1
                                elif new_content[pos] == '}':
                                    depth -= 1
                                pos += 1
                            product_section = new_content[product_info_match.start():pos]
                            
                            # Now check if module_info is inside this section
                            module_pattern_inside = r'"module_info":\s*\{[\s\S]*?\}'
                            if re.search(module_pattern_inside, product_section):
                                # Replace existing module_info inside product_info
                                module_str = '"module_info": {\n'
                                for i, (module, version) in enumerate(default_info["module_info"].items()):
                                    if i > 0:
                                        module_str += ',\n'
                                    module_str += '\t\t\t"{}": "{}"'.format(module, version)
                                module_str += '\n\t\t}'
                                # Replace in product_section
                                new_product_section = re.sub(module_pattern_inside, module_str, product_section, flags=re.DOTALL)
                                new_content = new_content[:product_info_match.start()] + new_product_section + new_content[pos:]
                            else:
                                # module_info doesn't exist inside product_info
                                # First, remove any top-level module_info manually
                                if '\n\t"module_info":' in new_content:
                                    start_idx = new_content.find('\n\t"module_info":')
                                    # Find the end - look for the next top-level key (sensor_calib, etc.)
                                    end_idx = new_content.find('\n\t"sensor_calib":', start_idx)
                                    if end_idx == -1:
                                        end_idx = new_content.find('\n\t"module_calib":', start_idx)
                                    if end_idx == -1:
                                        end_idx = new_content.find('\n\t"main_scene":', start_idx)
                                    if end_idx != -1:
                                        # Check what's between start and end
                                        block = new_content[start_idx:end_idx]
                                        # Block may end with "}" or "},"
                                        stripped = block.rstrip()
                                        if stripped.endswith('}') or stripped.endswith('},'):
                                            # Remove the block (including leading newline)
                                            new_content = new_content[:start_idx] + new_content[end_idx:]
                            
                                # Now add module_info inside product_info after iq_ver
                                # Find the pattern: "iq_ver": "..." followed by newline and closing }
                                pi_close_pattern = r'("iq_ver":\s*"[^"]*")(\s*\n\s*\})'
                                pi_close_match = re.search(pi_close_pattern, new_content)
                            
                            # Initialize pi_close_match if not set
                            if 'pi_close_match' not in locals():
                                pi_close_match = None
                            
                            if pi_close_match:
                                # Add comma after iq_ver and insert module_info before closing }
                                iq_ver_end = pi_close_match.start(2)
                                module_str = ',\n\t\t"module_info": {\n'
                                for i, (module, version) in enumerate(default_info["module_info"].items()):
                                    if i > 0:
                                        module_str += ',\n'
                                    module_str += '\t\t\t"{}": "{}"'.format(module, version)
                                module_str += '\n\t\t}'
                                new_content = new_content[:iq_ver_end] + module_str + new_content[iq_ver_end:]
                            else:
                                # Fallback: just add after iq_ver
                                ver_match = re.search(r'("iq_ver":\s*"[^"]*")', new_content)
                                if ver_match:
                                    insert_pos = ver_match.end()
                                    module_str = ',\n\t\t"module_info": {\n'
                                    for i, (module, version) in enumerate(default_info["module_info"].items()):
                                        if i > 0:
                                            module_str += ',\n'
                                        module_str += '\t\t\t"{}": "{}"'.format(module, version)
                                    module_str += '\n\t\t}'
                                    new_content = new_content[:insert_pos] + module_str + new_content[insert_pos:]
                    
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
    # script is at rk_aiq/tools/iq_check/script/, go up 3 levels to reach rk_aiq
    rk_aiq_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    repo_root = os.path.dirname(rk_aiq_dir)
    iqfiles_base = os.path.join(rk_aiq_dir, "iqfiles")
    chip_header = os.path.join(rk_aiq_dir, "RkAiqVersion.h")
    module_header = os.path.join(rk_aiq_dir, "RkIspModuleHwVersion.h")
    
    # Parse chip info (CHIP_NAME, IQ_STRUCT_MARK)
    print("Parsing RkAiqVersion.h for chip info...")
    chip_info = parse_chip_info(chip_header)
    
    # Parse module info (module versions)
    print("\nParsing RkIspModuleHwVersion.h for module info...")
    module_info = parse_module_info(module_header)
    
    # Merge both into platform_info
    platform_info = merge_platform_info(chip_info, module_info)
    
    print("\nMerged platform info:")
    for k, v in platform_info.items():
        module_count = len(v.get('module_info', {}))
        print(f"  {k}: chip={v['chip_name']}, struct={v['iq_struct_mark']}, modules={module_count}")
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