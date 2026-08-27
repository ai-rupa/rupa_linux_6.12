#!/usr/bin/env python3
"""Aggregate JSONL test results from multiple rockchip-test modules into a single report."""

import json
import sys
from datetime import datetime


def load_jsonl(filepath):
    """Load results from a JSONL file."""
    results = []
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        results.append(json.loads(line))
                    except json.JSONDecodeError:
                        pass
    except FileNotFoundError:
        pass
    return results


def aggregate(filepaths):
    """Aggregate results from multiple JSONL files."""
    all_results = []
    for fp in filepaths:
        all_results.extend(load_jsonl(fp))

    # Calculate summary
    total = len(all_results)
    pass_count = sum(1 for r in all_results if r.get('status') == 'PASS')
    fail_count = sum(1 for r in all_results if r.get('status') == 'FAIL')
    skip_count = sum(1 for r in all_results if r.get('status') == 'SKIP')

    # Detect platform from first result or environment
    platform = "unknown"
    for r in all_results:
        if 'platform' in r:
            platform = r['platform']
            break

    report = {
        "test_suite": "rockchip-test",
        "platform": platform,
        "timestamp": datetime.now().strftime("%Y-%m-%dT%H:%M:%S%z"),
        "summary": {
            "total": total,
            "pass": pass_count,
            "fail": fail_count,
            "skip": skip_count,
        },
        "results": all_results,
    }

    return report


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <results1.jsonl> [results2.jsonl ...]", file=sys.stderr)
        sys.exit(1)

    filepaths = sys.argv[1:]
    report = aggregate(filepaths)
    print(json.dumps(report, indent=2))

    # Exit with failure if any test failed
    if report["summary"]["fail"] > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
