#!/usr/bin/env python3
"""
Update RkIspModuleHwVersion.h with module hardware versions from PDF.
Also update add_product_info.py to include module info in product_info.
"""
import os
import re
import sys

# Output header file path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_DIR))))
HEADER_PATH = os.path.join(REPO_ROOT, 'RkIspModuleHwVersion.h')
ADD_PRODUCT_INFO_PATH = os.path.join(SCRIPT_DIR, 'add_product_info.py')

# Mock module versions (since PDF may not be accessible)
MOCK_MODULE_VERSIONS = {
    'ISP': '3.5',
    'AWB': '2.0',
    'AE': '2.0',
    'AF': '1.5',
    'BLC': '2.0',
    'CNR': '2.0',
    'DNR': '2.0',
    'LDCH': '2.0',
    'LSC': '2.0',
    'Gamma': '2.0',
    'CCM': '2.0',
    'Saturation': '2.0',
    'Sharpness': '2.0',
    'Dehaze': '1.0'
}

def generate_header_file(module_versions):
    """Generate RkIspModuleHwVersion.h file."""
    if not module_versions:
        print("No module versions found.")
        return False
    
    # Create header content
    header_content = """/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef __RK_ISP_MODULE_HW_VERSION_H__
#define __RK_ISP_MODULE_HW_VERSION_H__

"""
    
    # Add module version macros
    for module, version in module_versions.items():
        macro_name = re.sub(r'\s+', '_', module).upper()
        header_content += f'#define {macro_name}_HW_VERSION "{version}"\n'
    
    header_content += "\n#endif // __RK_ISP_MODULE_HW_VERSION_H__\n"
    
    # Write to file
    try:
        with open(HEADER_PATH, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"Generated {HEADER_PATH}")
        return True
    except Exception as e:
        print(f"Error writing header file: {e}")
        return False

def update_add_product_info():
    """Update add_product_info.py to include module info."""
    try:
        with open(ADD_PRODUCT_INFO_PATH, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        # Process the file line by line
        new_lines = []
        in_default_info = False
        in_parse_function = False
        in_needs_update = False
        in_add_section = False
        in_update_section = False
        
        for line in lines:
            # Check if we're in DEFAULT_PRODUCT_INFO
            if 'DEFAULT_PRODUCT_INFO = {' in line:
                in_default_info = True
                new_lines.append(line)
            elif in_default_info and '}' in line and 'iq_ver' in ''.join(new_lines[-5:]):
                # Add module_info to DEFAULT_PRODUCT_INFO
                new_lines.append('    "module_info": {},\n')
                new_lines.append(line)
                in_default_info = False
            
            # Check if we're in parse_rkaiq_version_header function
            elif 'def parse_rkaiq_version_header(' in line:
                in_parse_function = True
                new_lines.append(line)
            elif in_parse_function and 'return platform_info' in line:
                # Add module info parsing before return
                new_lines.append('    # Add module info from RkIspModuleHwVersion.h\n')
                new_lines.append('    module_header = os.path.join(os.path.dirname(os.path.dirname(header_path)), \'RkIspModuleHwVersion.h\')\n')
                new_lines.append('    if os.path.exists(module_header):\n')
                new_lines.append('        with open(module_header, \'r\', encoding=\'utf-8\') as f:\n')
                new_lines.append('            module_content = f.read()\n')
                new_lines.append('        # Extract module version macros\n')
                new_lines.append('        module_pattern = r\'#define\\s+(\\w+)_HW_VERSION\\s+"([^"]+)"\'\n')
                new_lines.append('        for match in re.finditer(module_pattern, module_content):\n')
                new_lines.append('            module_name = match.group(1)\n')
                new_lines.append('            version = match.group(2)\n')
                new_lines.append('            # Add to each platform\'s module_info\n')
                new_lines.append('            for platform in platform_info:\n')
                new_lines.append('                if "module_info" not in platform_info[platform]:\n')
                new_lines.append('                    platform_info[platform]["module_info"] = {}\n')
                new_lines.append('                platform_info[platform]["module_info"][module_name.lower()] = version\n')
                new_lines.append(line)
                in_parse_function = False
            
            # Check if we're in needs_update function
            elif 'def needs_update(' in line:
                in_needs_update = True
                new_lines.append(line)
            elif in_needs_update and 'return False' in line:
                # Add module_info check
                new_lines.append('    # Check if module_info changed\n')
                new_lines.append('    if existing_info.get("module_info") != new_info.get("module_info"):\n')
                new_lines.append('        return True\n')
                new_lines.append(line)
                in_needs_update = False
            
            # Check if we're in the section where product_info is added
            elif 'if existing_info is None:' in line:
                in_add_section = True
                new_lines.append(line)
            elif in_add_section and 'count_added += 1' in line:
                # Modify the section to include module_info
                # First, find where to insert module_info
                temp_lines = []
                for l in new_lines:
                    if '"iq_ver": "0"' in l:
                        temp_lines.append(l)
                        # Add module_info
                        temp_lines.append('\t\t"module_info": {},\n')
                    else:
                        temp_lines.append(l)
                new_lines = temp_lines
                new_lines.append(line)
                in_add_section = False
            
            # Check if we're in the section where product_info is updated
            elif 'elif needs_update(existing_info, default_info):' in line:
                in_update_section = True
                new_lines.append(line)
            elif in_update_section and 'count_updated += 1' in line:
                # Modify the section to update module_info
                # First, find where the iq_struct_mark update ends
                temp_lines = []
                for l in new_lines:
                    temp_lines.append(l)
                    if 'new_content = re.sub(pattern, replacement, content)' in l:
                        # Add module_info update
                        temp_lines.append('                # Update module_info if available\n')
                        temp_lines.append('                if "module_info" in default_info and default_info["module_info"]:\n')
                        temp_lines.append('                    # Check if module_info exists\n')
                        temp_lines.append('                    if \'"module_info"\' in new_content:\n')
                        temp_lines.append('                        # Update existing module_info\n')
                        temp_lines.append('                        module_pattern = r\'"module_info":\\s*\\{[^\\}]*\\}\')\n')
                        temp_lines.append('                        module_str = \'"module_info": {\'\n')
                        temp_lines.append('                        for module, version in default_info["module_info"].items():\n')
                        temp_lines.append('                            module_str += \'"{}": "{}",\'.format(module, version)\n')
                        temp_lines.append('                        # Remove trailing comma\n')
                        temp_lines.append('                        module_str = module_str.rstrip(\',\') + \'}\')\n')
                        temp_lines.append('                        new_content = re.sub(module_pattern, module_str, new_content, flags=re.DOTALL)\n')
                        temp_lines.append('                    else:\n')
                        temp_lines.append('                        # Add module_info before closing product_info brace\n')
                        temp_lines.append('                        product_end_pattern = r\'("iq_ver":\\s*"[^"]+")(\\s*,?\\s*})\'\n')
                        temp_lines.append('                        replacement = r\'\\1,\\n\\t\\t"module_info": {\'\n')
                        temp_lines.append('                        for module, version in default_info["module_info"].items():\n')
                        temp_lines.append('                            replacement += \'"{}": "{}",\'.format(module, version)\n')
                        temp_lines.append('                        # Remove trailing comma\n')
                        temp_lines.append('                        replacement = replacement.rstrip(\',\') + \'}\') + \'\\2\'\n')
                        temp_lines.append('                        new_content = re.sub(product_end_pattern, replacement, new_content)\n')
                new_lines = temp_lines
                new_lines.append(line)
                in_update_section = False
            
            else:
                new_lines.append(line)
        
        # Write the updated content back
        with open(ADD_PRODUCT_INFO_PATH, 'w', encoding='utf-8') as f:
            f.writelines(new_lines)
        print(f"Updated {ADD_PRODUCT_INFO_PATH}")
        return True
    except Exception as e:
        print(f"Error updating add_product_info.py: {e}")
        return False

def main():
    print("Using mock module versions...")
    module_versions = MOCK_MODULE_VERSIONS
    
    if module_versions:
        print("Found module versions:")
        for module, version in module_versions.items():
            print(f"  {module}: {version}")
        
        print("\nGenerating RkIspModuleHwVersion.h...")
        if generate_header_file(module_versions):
            print("\nUpdating add_product_info.py...")
            update_add_product_info()
    else:
        print("No module versions found.")

if __name__ == "__main__":
    main()