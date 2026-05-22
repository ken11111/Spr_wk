#!/usr/bin/env python3
"""performance_trends.md 対応の可視化スクリプト (v2)

入力: /home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/metrics_*.csv
出力: docs/security_camera/06_evidence/metrics_analysis/figures/*.png (13 枚)

【既存 8 図 + Phase 区切り + パーセンタイル + drop event 重ね】
  時系列 3 / ヒストグラム 3 / 散布図 2

【新規 5 図 (データサイエンス視点)】
  D-1 ECDF / D-2 箱ひげ / D-3 相関ヒート / D-4 drill-down / D-5 drop timeline

実行:
  cd docs/security_camera/06_evidence/metrics_analysis/analysis_tools
  .venv/bin/python visualization.py
"""
from datetime import datetime
from pathlib import Path

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

CSV_DIR = Path("/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics")
OUT_DIR = Path(
    "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/metrics_analysis/figures"
)
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Phase 区切り (ファイル名タイムスタンプから推定)
PHASE_BOUNDS = [
    ("Phase A (baseline)", "2026-01-03", "2026-01-04", "#FFE4E1"),
    ("Phase B (改良)", "2026-01-11", "2026-01-14", "#FFEFD5"),
    ("Phase C (現行 8/9)", "2026-01-16", "2026-01-18", "#E0FFE0"),
    ("Phase 10 (PID 後)", "2026-02-04", "2026-02-06", "#E0F0FF"),
]


def load_all() -> pd.DataFrame:
    files = sorted(CSV_DIR.glob("metrics_*.csv"))
    print(f"Loading {len(files)} CSV files...")
    dfs = []
    for f in files:
        try:
            if f.stat().st_size <= 100:
                continue
            d = pd.read_csv(f)
            d["source_file"] = f.name
            dfs.append(d)
        except Exception as e:
            print(f"  Skip {f.name}: {e}")
    df = pd.concat(dfs, ignore_index=True)
    df["datetime"] = pd.to_datetime(df["timestamp"], unit="s")
    df = df.sort_values("timestamp").reset_index(drop=True)
    # Phase tag
    df["phase"] = "Other"
    for name, s, e, _ in PHASE_BOUNDS:
        s_dt = pd.to_datetime(s)
        e_dt = pd.to_datetime(e)
        df.loc[(df["datetime"] >= s_dt) & (df["datetime"] < e_dt), "phase"] = name
    print(f"  Total samples: {len(df):,} / columns: {len(df.columns)}")
    print(f"  Phase distribution:")
    for p, c in df["phase"].value_counts().items():
        print(f"    {p:30s}: {c:5d}")
    return df


def setup_style() -> None:
    plt.rcParams["font.family"] = ["Noto Sans CJK JP", "DejaVu Sans"]
    plt.rcParams["figure.dpi"] = 100
    plt.rcParams["savefig.dpi"] = 120
    plt.rcParams["figure.figsize"] = (11, 6)
    plt.rcParams["axes.grid"] = True
    plt.rcParams["grid.alpha"] = 0.3


def shade_phases(ax) -> None:
    """時系列プロットに Phase 区切りを shaded region として描画."""
    for name, s, e, color in PHASE_BOUNDS:
        s_dt = pd.to_datetime(s)
        e_dt = pd.to_datetime(e)
        ax.axvspan(s_dt, e_dt, color=color, alpha=0.4, label=name, zorder=0)


def add_percentile_lines(ax, series, label_prefix="P", colors=None) -> None:
    """ヒストグラムに P50/P90/P95/P99 パーセンタイル線を追加."""
    percentiles = [50, 90, 95, 99]
    if colors is None:
        colors = ["#444", "#888", "#cc8800", "#cc0000"]
    for p, c in zip(percentiles, colors):
        v = np.percentile(series, p)
        ax.axvline(x=v, color=c, linestyle=":", linewidth=1, alpha=0.7,
                   label=f"{label_prefix}{p}={v:.1f}")


# ============================================================
# 時系列 3 図 (Phase 区切り + drop event vline 追加)
# ============================================================
def ts_fps(df: pd.DataFrame) -> None:
    d = df.iloc[::5]
    fig, ax = plt.subplots()
    shade_phases(ax)
    ax.plot(d["datetime"], d["pc_fps"], linewidth=0.6, alpha=0.8, label="PC FPS",
            color="#1f77b4")
    ax.plot(d["datetime"], d["spresense_fps"], linewidth=0.6, alpha=0.8,
            label="Spresense FPS", color="#ff7f0e")
    ax.set_title(f"時系列①: PC / Spresense FPS 推移 (n={len(df):,} samples, Phase 区切り付)")
    ax.set_xlabel("Time")
    ax.set_ylabel("FPS")
    ax.legend(loc="upper right", fontsize=8, ncol=2)
    fig.autofmt_xdate()
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ts_01_fps.png")
    plt.close()


def ts_tcp_send(df: pd.DataFrame) -> None:
    d = df.iloc[::5]
    fig, ax = plt.subplots()
    shade_phases(ax)
    ax.plot(d["datetime"], d["tcp_avg_send_ms"], linewidth=0.6,
            label="TCP avg send", color="#1f77b4")
    ax.plot(d["datetime"], d["tcp_max_send_ms"], linewidth=0.6, alpha=0.5,
            label="TCP max send", color="#d62728")
    ax.axhline(y=134, color="green", linestyle="--", linewidth=1.2,
               label="Phase C 基準 134 ms")
    ax.set_title("時系列②: TCP send time 推移 (対数軸, Phase 区切り付)")
    ax.set_xlabel("Time")
    ax.set_ylabel("Send time (ms)")
    ax.set_yscale("log")
    ax.legend(loc="upper right", fontsize=8, ncol=2)
    fig.autofmt_xdate()
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ts_02_tcp_send.png")
    plt.close()


def ts_queue(df: pd.DataFrame) -> None:
    d = df.iloc[::5]
    fig, ax = plt.subplots()
    shade_phases(ax)
    ax.plot(d["datetime"], d["action_q_depth"], linewidth=0.6, color="#2ca02c", alpha=0.8)
    ax.axhline(y=3.5, color="red", linestyle="--", linewidth=1.2,
               label="PID setpoint 3.5")
    # Phase A/B は queue 未計装の旨を注記
    ax.text(
        pd.to_datetime("2026-01-04"), 4.5,
        "⚠ Phase A/B では action_q_depth\n未計装 (常に 0)",
        fontsize=9, color="darkred", ha="left",
        bbox=dict(boxstyle="round", facecolor="lightyellow", alpha=0.9),
    )
    ax.set_title("時系列③: Action Queue Depth 推移 (Phase 区切り付 / Phase A/B 未計装)")
    ax.set_xlabel("Time")
    ax.set_ylabel("Queue depth")
    ax.legend(loc="upper right", fontsize=8, ncol=2)
    fig.autofmt_xdate()
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ts_03_queue.png")
    plt.close()


# ============================================================
# ヒストグラム 3 図 (パーセンタイル線追加)
# ============================================================
def hist_fps(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots()
    bins = np.linspace(0, 35, 70)
    pc = df["pc_fps"][df["pc_fps"] >= 0]
    sp = df["spresense_fps"][df["spresense_fps"] >= 0]
    ax.hist(pc, bins=bins, alpha=0.5, label=f"PC FPS (μ={pc.mean():.2f}, CV={pc.std()/pc.mean()*100:.1f}%)",
            color="#1f77b4")
    ax.hist(sp, bins=bins, alpha=0.5,
            label=f"Spresense FPS (μ={sp.mean():.2f}, CV={sp.std()/sp.mean()*100:.1f}%)",
            color="#ff7f0e")
    add_percentile_lines(ax, pc, "PC P")
    ymax = ax.get_ylim()[1]
    for target in (5, 10, 15, 30):
        ax.axvline(x=target, color="red", linestyle="-.", linewidth=0.8, alpha=0.5)
        ax.text(target + 0.2, ymax * 0.95, f"T{target}", rotation=90, fontsize=7, va="top")
    ax.set_title("ヒスト①: FPS 分布 + Target + Percentile")
    ax.set_xlabel("FPS")
    ax.set_ylabel("Count")
    ax.legend(loc="upper right", fontsize=8, ncol=2)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "hist_01_fps.png")
    plt.close()


def hist_tcp_send(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots()
    tcp_avg = df["tcp_avg_send_ms"][df["tcp_avg_send_ms"] > 0]
    tcp_max = df["tcp_max_send_ms"][df["tcp_max_send_ms"] > 0]
    upper = max(tcp_max.max(), 1.0)
    bins = np.logspace(np.log10(1.0), np.log10(upper), 50)
    ax.hist(tcp_avg, bins=bins, alpha=0.5,
            label=f"TCP avg (μ={tcp_avg.mean():.1f}ms, CV={tcp_avg.std()/tcp_avg.mean()*100:.1f}%)",
            color="#1f77b4")
    ax.hist(tcp_max, bins=bins, alpha=0.4,
            label=f"TCP max (μ={tcp_max.mean():.1f}ms)",
            color="#d62728")
    add_percentile_lines(ax, tcp_avg, "avg P")
    ax.axvline(x=134, color="green", linestyle="--", linewidth=1.2,
               label="Phase C 基準 134 ms")
    ax.set_title("ヒスト②: TCP send time 分布 (対数軸 + Percentile)")
    ax.set_xlabel("Send time (ms, log scale)")
    ax.set_ylabel("Count")
    ax.set_xscale("log")
    ax.legend(loc="upper left", fontsize=8, ncol=2)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "hist_02_tcp_send.png")
    plt.close()


def hist_serial_read(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots()
    sr = df["serial_read_time_ms"][df["serial_read_time_ms"] > 0]
    bins = np.logspace(np.log10(0.1), np.log10(max(sr.max(), 1.0)), 50)
    ax.hist(sr, bins=bins, color="#d62728", alpha=0.7)
    add_percentile_lines(ax, sr, "P")
    cv = sr.std() / sr.mean() * 100
    ax.set_title(f"ヒスト③: Serial read time 分布 (CV = {cv:.1f}%, 対数軸 + Percentile)")
    ax.set_xlabel("Serial read time (ms, log scale)")
    ax.set_ylabel("Count")
    ax.set_xscale("log")
    ax.legend(loc="upper right", fontsize=8)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "hist_03_serial_read.png")
    plt.close()


# ============================================================
# 散布図 2 図
# ============================================================
def scatter_fps_queue(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots()
    d = df.sample(min(2500, len(df)), random_state=42)
    sc = ax.scatter(d["action_q_depth"], d["pc_fps"],
                    c=d["tcp_avg_send_ms"].clip(0, 1000),
                    cmap="viridis", s=12, alpha=0.6)
    cbar = plt.colorbar(sc, ax=ax)
    cbar.set_label("TCP avg send (ms, clipped to 1000)")
    ax.axvline(x=3.5, color="red", linestyle="--", linewidth=1, label="PID setpoint 3.5")
    ax.set_title("散布①: PC FPS vs Action Queue Depth (色 = TCP send)")
    ax.set_xlabel("Action queue depth")
    ax.set_ylabel("PC FPS")
    ax.legend(loc="upper right", fontsize=8)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "scatter_01_fps_queue.png")
    plt.close()


def scatter_tcp_jitter(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots()
    d = df[(df["tcp_avg_send_ms"] > 0) & (df["tcp_max_send_ms"] > 0)].copy()
    d["jitter"] = (d["tcp_max_send_ms"] - d["tcp_avg_send_ms"]).clip(lower=0.1)
    d_sample = d.sample(min(2500, len(d)), random_state=42)
    ax.scatter(d_sample["tcp_avg_send_ms"], d_sample["jitter"],
               s=10, alpha=0.5, color="#2ca02c")
    # 回帰直線 (両対数)
    log_x = np.log10(d["tcp_avg_send_ms"])
    log_y = np.log10(d["jitter"])
    slope, intercept = np.polyfit(log_x, log_y, 1)
    x_line = np.logspace(log_x.min(), log_x.max(), 100)
    y_line = 10 ** (slope * np.log10(x_line) + intercept)
    ax.plot(x_line, y_line, color="red", linestyle="--", linewidth=1,
            label=f"log-log fit: slope={slope:.2f}")
    ax.axvline(x=134, color="green", linestyle="--", linewidth=1.2,
               label="Phase C 基準 134 ms")
    ax.set_title("散布②: TCP avg send vs jitter (回帰直線追加)")
    ax.set_xlabel("TCP avg send (ms, log)")
    ax.set_ylabel("TCP jitter (ms, log)")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.legend(loc="upper left", fontsize=8)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "scatter_02_tcp_jitter.png")
    plt.close()


# ============================================================
# D-1: ECDF (PC FPS の累積分布)
# ============================================================
def ecdf_fps(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots()
    for col, color, label in [
        ("pc_fps", "#1f77b4", "PC FPS"),
        ("spresense_fps", "#ff7f0e", "Spresense FPS"),
    ]:
        x = np.sort(df[col].dropna())
        y = np.arange(1, len(x) + 1) / len(x)
        ax.plot(x, y, label=label, color=color, linewidth=1.2)
    # Target 線 + 達成率注釈
    pc = df["pc_fps"]
    for target in (5, 10, 15, 30):
        rate = (pc >= target).sum() / len(pc) * 100
        ax.axvline(x=target, color="red", linestyle="--", linewidth=0.7, alpha=0.6)
        ax.text(target + 0.3, 0.05, f"T{target}: {rate:.1f}% 達成", rotation=90,
                fontsize=7, va="bottom", color="red")
    ax.set_title("D-1: FPS ECDF (累積分布) + Target 達成率")
    ax.set_xlabel("FPS")
    ax.set_ylabel("Cumulative probability")
    ax.set_xlim(0, 35)
    ax.set_ylim(0, 1.05)
    ax.legend(loc="lower right", fontsize=9)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_01_ecdf.png")
    plt.close()


# ============================================================
# D-2: 箱ひげ図 (P50/P90/P95/P99 視覚化)
# ============================================================
def boxplot_metrics(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots(figsize=(12, 6))
    data = [
        df["pc_fps"].dropna(),
        df["spresense_fps"].dropna(),
        df["tcp_avg_send_ms"][df["tcp_avg_send_ms"] > 0],
        df["tcp_max_send_ms"][df["tcp_max_send_ms"] > 0],
        df["serial_read_time_ms"][df["serial_read_time_ms"] > 0],
        df["decode_time_ms"][df["decode_time_ms"] > 0],
        df["action_q_depth"][df["action_q_depth"] > 0],  # ← 0 除外
        df["jpeg_size_kb"][df["jpeg_size_kb"] > 0],
    ]
    labels = [
        "PC fps", "Spr fps",
        "TCP avg\n(ms)", "TCP max\n(ms)",
        "Serial read\n(ms)", "Decode\n(ms)",
        "Queue\ndepth\n(>0)", "JPEG size\n(KB)",
    ]
    bp = ax.boxplot(data, labels=labels, showfliers=True, patch_artist=True,
                    whis=[5, 95])  # 5-95 percentile を whisker に
    colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
              "#9467bd", "#8c564b", "#e377c2", "#7f7f7f"]
    for patch, c in zip(bp["boxes"], colors):
        patch.set_facecolor(c)
        patch.set_alpha(0.6)
    ax.set_yscale("log")
    ax.set_title("D-2: 主要指標の箱ひげ (whisker = 5-95 percentile, 対数軸)")
    ax.set_ylabel("Value (log scale, 単位は label 参照)")
    ax.grid(True, alpha=0.3, which="both")
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_02_boxplot.png")
    plt.close()


# ============================================================
# D-3: 相関ヒートマップ (Pearson)
# ============================================================
def correlation_heatmap(df: pd.DataFrame) -> None:
    numeric_cols = [
        "pc_fps", "spresense_fps", "frame_count", "error_count",
        "decode_time_ms", "serial_read_time_ms", "texture_upload_time_ms",
        "jpeg_size_kb", "spresense_camera_fps", "spresense_usb_packets",
        "action_q_depth", "spresense_errors",
        "tcp_avg_send_ms", "tcp_max_send_ms", "dropped_frames", "drop_events",
    ]
    avail = [c for c in numeric_cols if c in df.columns]
    # ⚠ queue=0 アーティファクト排除のため Phase A/B 除外 (queue 列を含むため全体に影響)
    df_corr = df[~df["phase"].isin(["Phase A (baseline)", "Phase B (改良)"])].copy()
    corr = df_corr[avail].corr(method="pearson")

    fig, ax = plt.subplots(figsize=(12, 10))
    im = ax.imshow(corr.values, cmap="RdBu_r", vmin=-1, vmax=1)
    ax.set_xticks(range(len(avail)))
    ax.set_yticks(range(len(avail)))
    ax.set_xticklabels(avail, rotation=45, ha="right", fontsize=8)
    ax.set_yticklabels(avail, fontsize=8)
    # 値を表示
    for i in range(len(avail)):
        for j in range(len(avail)):
            v = corr.values[i, j]
            color = "white" if abs(v) > 0.5 else "black"
            ax.text(j, i, f"{v:.2f}", ha="center", va="center",
                    color=color, fontsize=7)
    plt.colorbar(im, ax=ax, label="Pearson r")
    ax.set_title(f"D-3: 16 列の相関ヒートマップ (Pearson)\n※ queue=0 排除のため Phase A/B 除外 (n={len(df_corr):,})",
                 fontsize=11)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_03_corr_heatmap.png")
    plt.close()


# ============================================================
# D-4: 個別セッション drill-down (1 CSV を multi-panel) — Phase 別 4 ファイル化
# ============================================================
def drilldown_one(df: pd.DataFrame, source_file: str, out_name: str,
                  phase_label: str) -> None:
    """指定 CSV を 4-panel で drill-down 描画 (Panel 1 に JPEG size 第 2 軸付)"""
    d = df[df["source_file"] == source_file].copy()
    if len(d) == 0:
        print(f"  Skip {out_name}: no data for {source_file}")
        return
    d["sec"] = (d["datetime"] - d["datetime"].min()).dt.total_seconds()

    # ─── Pearson 相関を計算 (FPS vs JPEG size) ───
    valid = d[(d["jpeg_size_kb"] > 0)].copy()
    if len(valid) >= 10:
        r_pc = valid["pc_fps"].corr(valid["jpeg_size_kb"])
        r_spr = valid["spresense_fps"].corr(valid["jpeg_size_kb"])
    else:
        r_pc = r_spr = float("nan")

    fig, axes = plt.subplots(4, 1, figsize=(12, 12), sharex=True)
    fig.suptitle(
        f"{out_name}: {phase_label} drill-down — {source_file} (n={len(d)})\n"
        f"Pearson r: PC FPS vs JPEG={r_pc:+.3f}  /  Spr FPS vs JPEG={r_spr:+.3f}",
        fontsize=11,
    )

    # Panel 1: FPS (左軸) + JPEG size (右軸)
    axes[0].plot(d["sec"], d["pc_fps"], label="PC FPS", color="#1f77b4", linewidth=0.8)
    axes[0].plot(d["sec"], d["spresense_fps"], label="Spr FPS", color="#ff7f0e",
                 linewidth=0.8, alpha=0.8)
    axes[0].set_ylabel("FPS", color="#1f77b4")
    axes[0].tick_params(axis="y", labelcolor="#1f77b4")
    axes[0].legend(loc="upper left", fontsize=8)
    axes[0].grid(alpha=0.3)
    # 右軸 JPEG size
    ax0b = axes[0].twinx()
    ax0b.plot(d["sec"], d["jpeg_size_kb"], color="#2ca02c", linewidth=0.6,
              alpha=0.6, label="JPEG size (KB)")
    ax0b.set_ylabel("JPEG size (KB)", color="#2ca02c")
    ax0b.tick_params(axis="y", labelcolor="#2ca02c")
    ax0b.legend(loc="upper right", fontsize=8)
    # 相関係数を panel 内にテキスト表示
    axes[0].text(
        0.02, 0.95,
        f"r(PC FPS, JPEG)={r_pc:+.3f}\nr(Spr FPS, JPEG)={r_spr:+.3f}",
        transform=axes[0].transAxes, fontsize=8, va="top",
        bbox=dict(boxstyle="round", facecolor="lightyellow", alpha=0.8),
    )

    # Panel 2: TCP send
    axes[1].plot(d["sec"], d["tcp_avg_send_ms"], label="TCP avg", color="#1f77b4",
                 linewidth=0.8)
    axes[1].plot(d["sec"], d["tcp_max_send_ms"], label="TCP max", color="#d62728",
                 linewidth=0.8, alpha=0.6)
    axes[1].axhline(y=134, color="green", linestyle="--", linewidth=1)
    axes[1].set_yscale("log")
    axes[1].set_ylabel("TCP send (ms)")
    axes[1].legend(loc="upper right", fontsize=8)
    axes[1].grid(alpha=0.3, which="both")

    # Panel 3: Queue / Serial
    ax3 = axes[2]
    ax3.plot(d["sec"], d["action_q_depth"], label="Queue depth", color="#2ca02c",
             linewidth=0.8)
    ax3.axhline(y=3.5, color="red", linestyle="--", linewidth=1)
    ax3.set_ylabel("Queue depth", color="#2ca02c")
    ax3.tick_params(axis="y", labelcolor="#2ca02c")
    ax3.grid(alpha=0.3)
    ax3b = ax3.twinx()
    ax3b.plot(d["sec"], d["serial_read_time_ms"], label="Serial read (ms)",
              color="#9467bd", linewidth=0.6, alpha=0.7)
    ax3b.set_ylabel("Serial read (ms)", color="#9467bd")
    ax3b.tick_params(axis="y", labelcolor="#9467bd")
    ax3b.set_yscale("log")

    # Panel 4: Drop events / Error
    axes[3].plot(d["sec"], d["dropped_frames"].diff().fillna(0).clip(lower=0),
                 label="dropped_frames Δ", color="#d62728", linewidth=0.6)
    axes[3].plot(d["sec"], d["drop_events"].diff().fillna(0).clip(lower=0),
                 label="drop_events Δ", color="#7f7f7f", linewidth=0.6)
    axes[3].set_ylabel("Δ (per interval)")
    axes[3].set_xlabel("Elapsed time (sec)")
    axes[3].legend(loc="upper right", fontsize=8)
    axes[3].grid(alpha=0.3)

    plt.tight_layout()
    plt.savefig(OUT_DIR / f"{out_name}.png")
    plt.close()


def drilldown_phases(df: pd.DataFrame) -> None:
    """各 Phase の最大サンプル数セッションを drill-down (4 ファイル)"""
    phase_targets = [
        ("Phase A (baseline)", "ds_04a_drilldown_phaseA"),
        ("Phase B (改良)", "ds_04b_drilldown_phaseB"),
        ("Phase C (現行 8/9)", "ds_04c_drilldown_phaseC"),
        ("Phase 10 (PID 後)", "ds_04d_drilldown_phase10"),
    ]
    for phase_label, out_name in phase_targets:
        sub = df[df["phase"] == phase_label]
        if len(sub) == 0:
            print(f"  No data for {phase_label}")
            continue
        # 最大サンプル数セッション
        target = sub.groupby("source_file").size().idxmax()
        drilldown_one(df, target, out_name, phase_label)


# ============================================================
# D-5: drop event タイムライン重ね (全期間 TCP send + drop_events)
# ============================================================
def drop_timeline(df: pd.DataFrame) -> None:
    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True,
                             gridspec_kw={"height_ratios": [3, 1]})

    # Panel 1: TCP send + Phase
    d = df.iloc[::5]
    shade_phases(axes[0])
    axes[0].plot(d["datetime"], d["tcp_avg_send_ms"], label="TCP avg",
                 color="#1f77b4", linewidth=0.5)
    axes[0].plot(d["datetime"], d["tcp_max_send_ms"], label="TCP max",
                 color="#d62728", linewidth=0.5, alpha=0.5)
    axes[0].axhline(y=134, color="green", linestyle="--", linewidth=1,
                    label="Phase C 134 ms")
    axes[0].set_yscale("log")
    axes[0].set_ylabel("TCP send (ms, log)")
    axes[0].legend(loc="upper right", fontsize=8, ncol=2)
    axes[0].grid(alpha=0.3, which="both")
    axes[0].set_title("D-5: TCP send time + drop_events / error_count タイムライン")

    # Panel 2: drop_events / error_count Δ (per sample diff)
    df_sorted = df.sort_values("datetime").copy()
    df_sorted["drop_delta"] = df_sorted["drop_events"].diff().fillna(0).clip(lower=0)
    df_sorted["err_delta"] = df_sorted["error_count"].diff().fillna(0).clip(lower=0)
    axes[1].vlines(
        df_sorted[df_sorted["drop_delta"] > 0]["datetime"],
        ymin=0, ymax=df_sorted[df_sorted["drop_delta"] > 0]["drop_delta"],
        color="#d62728", alpha=0.5, linewidth=0.5, label="drop_events Δ",
    )
    axes[1].vlines(
        df_sorted[df_sorted["err_delta"] > 0]["datetime"],
        ymin=0, ymax=df_sorted[df_sorted["err_delta"] > 0]["err_delta"],
        color="#7f7f7f", alpha=0.3, linewidth=0.4, label="error_count Δ",
    )
    axes[1].set_ylabel("Event count Δ")
    axes[1].set_xlabel("Time")
    axes[1].legend(loc="upper right", fontsize=8)
    axes[1].grid(alpha=0.3)

    fig.autofmt_xdate()
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_05_drop_timeline.png")
    plt.close()


# ============================================================
# D-7: Phase 比較 (主要指標の Phase 別箱ひげ並列)
# ============================================================
def phase_compare(df: pd.DataFrame) -> None:
    """各 Phase で主要 6 指標を並列箱ひげ比較"""
    phase_order = [
        "Phase A (baseline)", "Phase B (改良)",
        "Phase C (現行 8/9)", "Phase 10 (PID 後)",
    ]
    metrics = [
        ("pc_fps", "PC FPS", "linear", False),
        ("spresense_fps", "Spr FPS", "linear", False),
        ("tcp_avg_send_ms", "TCP avg (ms)", "log", False),
        ("serial_read_time_ms", "Serial read (ms)", "log", False),
        ("action_q_depth", "Queue depth (>0)", "linear", True),  # ← Phase A/B 未計装
        ("jpeg_size_kb", "JPEG (KB)", "linear", False),
    ]
    fig, axes = plt.subplots(2, 3, figsize=(15, 9))
    colors = ["#FFB6C1", "#FFE4B5", "#90EE90", "#87CEEB"]
    for idx, (col, ylabel, yscale, mask_ab) in enumerate(metrics):
        ax = axes[idx // 3][idx % 3]
        data = []
        labels = []
        for p in phase_order:
            sub = df[df["phase"] == p]
            if col in ("tcp_avg_send_ms", "serial_read_time_ms", "jpeg_size_kb",
                       "action_q_depth"):
                s = sub[col][sub[col] > 0]
            else:
                s = sub[col].dropna()
            # Phase A/B で queue は未計装なので「N/A」表示
            if mask_ab and p.startswith(("Phase A", "Phase B")) and len(s) < 10:
                data.append(pd.Series([np.nan]))
            else:
                data.append(s)
            labels.append(p.split(" ")[0] + p.split(" ")[1][:2])
        bp = ax.boxplot(data, labels=labels, showfliers=False, patch_artist=True,
                        whis=[5, 95])
        for patch, c in zip(bp["boxes"], colors):
            patch.set_facecolor(c)
            patch.set_alpha(0.7)
        if yscale == "log":
            ax.set_yscale("log")
        ax.set_ylabel(ylabel, fontsize=9)
        ax.grid(alpha=0.3, which="both")
        # 中央値の数値を上部に表示
        for i, s in enumerate(data):
            m = s.median()
            if pd.isna(m) or len(s.dropna()) < 10:
                if mask_ab and labels[i].startswith(("PhaseA", "PhaseB")):
                    ax.text(i + 1, ax.get_ylim()[1] * 0.5, "N/A\n未計装",
                            ha="center", fontsize=8, color="darkred")
            else:
                ax.text(i + 1, ax.get_ylim()[1] * 0.92, f"med={m:.1f}",
                        ha="center", fontsize=7, color="darkblue")
    fig.suptitle("D-7: Phase 比較 — 主要 6 指標の箱ひげ (whisker = P5-P95, 中央値表示)",
                 fontsize=12)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_07_phase_compare.png")
    plt.close()


# ============================================================
# D-8: Cross-correlation (PC FPS vs Spresense FPS の lag)
# ============================================================
def cross_correlation(df: pd.DataFrame) -> None:
    """各 Phase で PC FPS と Spresense FPS の cross-correlation を計算"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    phase_order = [
        ("Phase A (baseline)", axes[0][0]),
        ("Phase B (改良)", axes[0][1]),
        ("Phase C (現行 8/9)", axes[1][0]),
        ("Phase 10 (PID 後)", axes[1][1]),
    ]

    for phase_label, ax in phase_order:
        sub = df[df["phase"] == phase_label].copy()
        # 各セッション (CSV) ごとに ccf を計算し平均
        ccfs = []
        max_lag = 20  # ±20 サンプル (~ ±20 秒)
        for src, grp in sub.groupby("source_file"):
            if len(grp) < max_lag * 3:
                continue
            x = grp["pc_fps"].values - grp["pc_fps"].mean()
            y = grp["spresense_fps"].values - grp["spresense_fps"].mean()
            if x.std() < 1e-3 or y.std() < 1e-3:
                continue
            corr = np.correlate(x, y, mode="full") / (np.std(x) * np.std(y) * len(x))
            mid = len(corr) // 2
            ccf_window = corr[mid - max_lag : mid + max_lag + 1]
            ccfs.append(ccf_window)

        if not ccfs:
            ax.text(0.5, 0.5, "No data", ha="center", va="center",
                    transform=ax.transAxes)
            ax.set_title(f"{phase_label} — N/A")
            continue

        ccf_mean = np.mean(ccfs, axis=0)
        lags = np.arange(-max_lag, max_lag + 1)
        ax.bar(lags, ccf_mean, width=0.8, color="#1f77b4", alpha=0.7)
        ax.axvline(x=0, color="red", linestyle="--", linewidth=1)

        # ピーク lag を特定
        peak_idx = np.argmax(np.abs(ccf_mean))
        peak_lag = lags[peak_idx]
        peak_val = ccf_mean[peak_idx]
        ax.text(0.02, 0.95, f"peak lag = {peak_lag} (corr={peak_val:.2f})",
                transform=ax.transAxes, fontsize=9, va="top",
                bbox=dict(boxstyle="round", facecolor="yellow", alpha=0.8))
        lag_interp = ("Spresense 先行" if peak_lag > 0 else
                      "PC 先行" if peak_lag < 0 else "同時")
        ax.text(0.02, 0.85, lag_interp, transform=ax.transAxes, fontsize=8,
                va="top", color="darkred")
        ax.set_title(f"{phase_label} (sessions={len(ccfs)})")
        ax.set_xlabel("Lag (samples, ≈ sec)\n(正: Spresense が先, 負: PC が先)")
        ax.set_ylabel("Cross-correlation")
        ax.grid(alpha=0.3)

    fig.suptitle("D-8: Cross-correlation PC FPS vs Spresense FPS (Phase 別)",
                 fontsize=12)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_08_cross_correlation.png")
    plt.close()


# ============================================================
# D-9: ボトルネック絞り込み
#   PC FPS が低下時 (P10 以下) vs 上昇時 (P90 以上) で
#   spresense_fps / jpeg_size / serial_read / tcp_avg の分布を比較
# ============================================================
def bottleneck_breakdown(df: pd.DataFrame) -> None:
    # ⚠ queue=0 アーティファクト排除のため Phase A/B (queue 未計装) を除外
    df_valid = df[~df["phase"].isin(["Phase A (baseline)", "Phase B (改良)"])].copy()
    p10 = df_valid["pc_fps"].quantile(0.10)
    p90 = df_valid["pc_fps"].quantile(0.90)
    low = df_valid[df_valid["pc_fps"] <= p10].copy()
    high = df_valid[df_valid["pc_fps"] >= p90].copy()

    metrics = [
        ("spresense_fps", "Spresense FPS", "linear", "capture (HW)"),
        ("jpeg_size_kb", "JPEG size (KB)", "linear", "encode/帯域"),
        ("serial_read_time_ms", "Serial read (ms)", "log", "USB 律速"),
        ("tcp_avg_send_ms", "TCP avg (ms)", "log", "WiFi/TCP 律速"),
        ("action_q_depth", "Queue depth (>0)", "linear", "下流処理律速"),
        ("decode_time_ms", "Decode (ms)", "log", "PC CPU 律速"),
    ]
    fig, axes = plt.subplots(2, 3, figsize=(15, 9))
    for idx, (col, ylabel, yscale, hyp) in enumerate(metrics):
        ax = axes[idx // 3][idx % 3]
        if col in ("tcp_avg_send_ms", "serial_read_time_ms",
                   "jpeg_size_kb", "decode_time_ms", "action_q_depth"):
            l = low[col][low[col] > 0]
            h = high[col][high[col] > 0]
        else:
            l = low[col].dropna()
            h = high[col].dropna()
        bp = ax.boxplot([l, h], labels=[f"PC FPS ≤ P10\n({p10:.2f})",
                                          f"PC FPS ≥ P90\n({p90:.2f})"],
                        showfliers=False, patch_artist=True, whis=[5, 95])
        bp["boxes"][0].set_facecolor("#FFB6B6")  # 低 FPS = 赤系
        bp["boxes"][1].set_facecolor("#B6E7B6")  # 高 FPS = 緑系
        for box in bp["boxes"]:
            box.set_alpha(0.7)
        if yscale == "log":
            ax.set_yscale("log")
        ax.set_ylabel(ylabel, fontsize=9)
        # 中央値比較
        m_low = l.median()
        m_high = h.median()
        ratio = m_low / m_high if m_high > 0 else float("nan")
        # 律速仮説の根拠
        if col == "spresense_fps":
            # 低 PC FPS 時に Spresense FPS も低い → capture 律速の根拠
            verdict = (
                f"⚠ 律速: {hyp}" if m_low < m_high * 0.7
                else f"✓ 否定: 維持"
            )
        else:
            verdict = (
                f"⚠ 律速: {hyp}" if m_low > m_high * 1.3
                else f"✓ 否定: 不変"
            )
        ax.set_title(f"{ylabel}\nmed: 低={m_low:.1f} / 高={m_high:.1f} → {verdict}",
                     fontsize=9)
        ax.grid(alpha=0.3, which="both")

    fig.suptitle(
        f"D-9: ボトルネック絞り込み — PC FPS 低下時 (≤P10={p10:.2f}) vs 上昇時 (≥P90={p90:.2f})\n"
        f"※ queue=0 アーティファクト排除のため Phase A/B 除外 (n={len(df_valid):,})",
        fontsize=11,
    )
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_09_bottleneck.png")
    plt.close()


# ============================================================
# D-10: FPS vs JPEG size 相関 (Phase 別 8 panel: PC/Spr × 4 Phase)
# ============================================================
def fps_jpeg_correlation(df: pd.DataFrame) -> None:
    """各 Phase で PC FPS / Spresense FPS vs JPEG size の散布図 + 回帰 + Pearson r"""
    fig, axes = plt.subplots(2, 4, figsize=(18, 9))
    phase_order = [
        "Phase A (baseline)", "Phase B (改良)",
        "Phase C (現行 8/9)", "Phase 10 (PID 後)",
    ]
    for col_idx, phase_label in enumerate(phase_order):
        sub = df[(df["phase"] == phase_label) & (df["jpeg_size_kb"] > 0)].copy()
        if len(sub) < 10:
            for row in range(2):
                axes[row, col_idx].text(0.5, 0.5, "No data",
                                          ha="center", va="center",
                                          transform=axes[row, col_idx].transAxes)
                axes[row, col_idx].set_title(f"{phase_label} N/A", fontsize=9)
            continue

        d_sample = sub.sample(min(800, len(sub)), random_state=42)
        x_min, x_max = sub["jpeg_size_kb"].min(), sub["jpeg_size_kb"].max()
        x_line = np.linspace(x_min, x_max, 100)

        # ─── Top: PC FPS vs JPEG ───
        ax = axes[0, col_idx]
        ax.scatter(d_sample["jpeg_size_kb"], d_sample["pc_fps"],
                   s=10, alpha=0.4, color="#1f77b4")
        slope, intercept = np.polyfit(sub["jpeg_size_kb"], sub["pc_fps"], 1)
        ax.plot(x_line, slope * x_line + intercept,
                color="red", linestyle="--", linewidth=1.2,
                label=f"slope={slope:+.4f}")
        r_pc = sub["pc_fps"].corr(sub["jpeg_size_kb"])
        ax.set_title(f"{phase_label.split(' (')[0]}\nPC FPS vs JPEG: r={r_pc:+.3f}",
                     fontsize=9)
        ax.set_xlabel("JPEG size (KB)", fontsize=8)
        ax.set_ylabel("PC FPS", fontsize=8)
        ax.legend(loc="upper right", fontsize=7)
        ax.grid(alpha=0.3)

        # ─── Bottom: Spr FPS vs JPEG ───
        ax = axes[1, col_idx]
        ax.scatter(d_sample["jpeg_size_kb"], d_sample["spresense_fps"],
                   s=10, alpha=0.4, color="#ff7f0e")
        slope, intercept = np.polyfit(sub["jpeg_size_kb"], sub["spresense_fps"], 1)
        ax.plot(x_line, slope * x_line + intercept,
                color="red", linestyle="--", linewidth=1.2,
                label=f"slope={slope:+.4f}")
        r_spr = sub["spresense_fps"].corr(sub["jpeg_size_kb"])
        ax.set_title(f"Spr FPS vs JPEG: r={r_spr:+.3f}", fontsize=9)
        ax.set_xlabel("JPEG size (KB)", fontsize=8)
        ax.set_ylabel("Spr FPS", fontsize=8)
        ax.legend(loc="upper right", fontsize=7)
        ax.grid(alpha=0.3)

    fig.suptitle("D-10: FPS vs JPEG size 相関 (Phase 別) — 上段 PC FPS / 下段 Spresense FPS",
                 fontsize=12)
    plt.tight_layout()
    plt.savefig(OUT_DIR / "ds_10_fps_jpeg.png")
    plt.close()


def main() -> None:
    setup_style()
    df = load_all()

    print("\n[1/5] Generating time series (改善)...")
    ts_fps(df)
    ts_tcp_send(df)
    ts_queue(df)

    print("[2/5] Generating histograms (改善)...")
    hist_fps(df)
    hist_tcp_send(df)
    hist_serial_read(df)

    print("[3/5] Generating scatter plots (改善)...")
    scatter_fps_queue(df)
    scatter_tcp_jitter(df)

    print("[4/5] Generating data science extras (D-1〜D-5)...")
    ecdf_fps(df)
    boxplot_metrics(df)
    correlation_heatmap(df)
    drilldown_phases(df)  # ← 4 ファイル化
    drop_timeline(df)

    print("[5/5] Generating Phase comparison + causal analysis (D-7〜D-10)...")
    phase_compare(df)
    cross_correlation(df)
    bottleneck_breakdown(df)
    fps_jpeg_correlation(df)

    pngs = sorted(OUT_DIR.glob("*.png"))
    print(f"\nGenerated {len(pngs)} figures in {OUT_DIR}:")
    for p in pngs:
        size_kb = p.stat().st_size / 1024
        print(f"  - {p.name} ({size_kb:.1f} KB)")


if __name__ == "__main__":
    main()
