#!/usr/bin/env python3
"""
parse_cpu_log.py — Spresense syslog から [CPU] 行を抽出して集計する。

Usage:
  # stdin から
  cat screenlog.0 | python3 parse_cpu_log.py

  # ファイル指定
  python3 parse_cpu_log.py measurement_baseline.log
  python3 parse_cpu_log.py --csv out.csv measurement_baseline.log

期待する syslog 行 (perf_thread_cpu_log() が出力):
  [CPU] camera: cur=42.3% avg=41.5% max=58.1% (n=58)
  [CPU] usb: cur=27.8% avg=28.4% max=49.5% (n=58)
  [CPU] control: cur=0.4% avg=0.5% max=1.2% (n=58)

X-6 P1-A 実測手段の確立 (PENDING_NFR_WORK.md / CPU_MEASUREMENT_GUIDE.md §4)
"""
import argparse
import csv
import re
import sys
from collections import defaultdict
from typing import Dict, List, Tuple


CPU_LINE_RE = re.compile(
    r"\[CPU\]\s+(?P<name>\S+):\s+"
    r"cur=(?P<cur>[-\d.]+)%\s+"
    r"avg=(?P<avg>[-\d.]+)%\s+"
    r"max=(?P<max>[-\d.]+)%\s+"
    r"\(n=(?P<n>\d+)\)"
)


def parse_lines(lines) -> Dict[str, List[Tuple[float, float, float, int]]]:
    """Return dict[thread_name] -> list of (cur, avg, max, n) tuples."""
    samples: Dict[str, List[Tuple[float, float, float, int]]] = defaultdict(list)
    for line in lines:
        match = CPU_LINE_RE.search(line)
        if not match:
            continue
        name = match.group("name")
        cur = float(match.group("cur"))
        avg = float(match.group("avg"))
        max_ = float(match.group("max"))
        n = int(match.group("n"))
        samples[name].append((cur, avg, max_, n))
    return samples


def summarize(samples: Dict[str, List[Tuple[float, float, float, int]]]) -> None:
    if not samples:
        print("⚠ [CPU] 行が見つかりませんでした。")
        print("  - screenlog の取得タイミング、CONFIG_DEBUG_ENABLE=1 を確認")
        print("  - CPU_MEASUREMENT_GUIDE.md §6 (トラブルシューティング) 参照")
        return

    print("=== CPU 利用率サマリ ===")
    total_avg_sum = 0.0
    total_max_sum = 0.0
    for name, recs in samples.items():
        if not recs:
            continue
        # 最後のサンプルの avg / max がアプリ側計算済みの累計値
        last_cur, last_avg, last_max, last_n = recs[-1]
        # cur の平均 (本スクリプトで再集計)
        cur_mean = sum(r[0] for r in recs) / len(recs)
        cur_max_obs = max(r[0] for r in recs)
        print(
            f"{name:8s}: cur 平均={cur_mean:5.1f}% / 最大={cur_max_obs:5.1f}% "
            f"(n={last_n}, samples={len(recs)})"
        )
        print(
            f"          [app-side] avg={last_avg:5.1f}% max={last_max:5.1f}%"
        )
        total_avg_sum += cur_mean
        total_max_sum += cur_max_obs

    print()
    print(f"合計     : 平均={total_avg_sum:5.1f}% / 最大={total_max_sum:5.1f}%")
    print()
    print("[備考]")
    print("- 単一コア (CONFIG_SMP=n) のため合計は ~100% が物理上限")
    print("- 100% 超はサンプリング誤差 (CPU time が wall 跨ぎで計上される)")
    print("- gs2200m driver task (kernel) は計測対象外")
    print("  → SPRESENSE_TCP_CONSTRAINTS.md と組み合わせて分析")
    print()
    print("更新先:")
    print("- docs/security_camera/02_specifications/quality/CPU_BANDWIDTH_BUDGET.md §2")
    print("- docs/security_camera/02_specifications/quality/QUALITY_REQUIREMENTS.md §2.2")


def write_csv(
    samples: Dict[str, List[Tuple[float, float, float, int]]], path: str
) -> None:
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["thread", "sample_idx", "cur_percent", "avg_percent", "max_percent", "n"])
        for name, recs in samples.items():
            for i, (cur, avg, max_, n) in enumerate(recs):
                w.writerow([name, i, cur, avg, max_, n])
    print(f"CSV 出力: {path}")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("logfile", nargs="?", help="syslog ファイル (省略時は stdin)")
    p.add_argument("--csv", help="CSV 出力先ファイル")
    args = p.parse_args()

    if args.logfile:
        with open(args.logfile, encoding="utf-8", errors="replace") as f:
            samples = parse_lines(f)
    else:
        samples = parse_lines(sys.stdin)

    summarize(samples)

    if args.csv:
        write_csv(samples, args.csv)

    return 0


if __name__ == "__main__":
    sys.exit(main())
