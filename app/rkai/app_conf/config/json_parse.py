#!/usr/bin/env python3
import json
import sys
import os

def check_config(json_path):
    if not os.path.exists(json_path):
        print(f"Error: File '{json_path}' does not exist.")
        return False

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            cfg = json.load(f)
    except json.JSONDecodeError as e:
        print(f"JSON parsing error: {e}")
        return False

    # 验证关键字段
    required_fields = [
        ("model_path", str),
        ("max_context_len", int),
        ("max_new_tokens", int),
        ("top_k", int),
        ("skip_special_token", int),
        ("base_domain_id", int)
    ]

    success = True
    for key, expected_type in required_fields:
        if key not in cfg:
            print(f"Missing required field: {key}")
            success = False
        elif not isinstance(cfg[key], expected_type):
            print(f"Field '{key}' should be {expected_type.__name__}, but got {type(cfg[key]).__name__}")
            success = False

    if success:
        print("Config JSON is valid.\nParsed values:")
        for k, v in cfg.items():
            print(f"  {k}: {v}")
    else:
        print("Config JSON has errors. Please fix the issues above.")

    return success

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 check_rkai_config.py <config.json>")
        sys.exit(1)

    config_path = sys.argv[1]
    if not check_config(config_path):
        sys.exit(1)
