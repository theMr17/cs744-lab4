#!/usr/bin/env python3
import json
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

CONFIG_STYLES = [
    {"key": "s", "label": "single-threaded", "color": "#1f77b4", "linestyle": "--", "marker": "o"},
    {"key": "m", "label": "thread per request", "color": "#ff7f0e", "linestyle": "--", "marker": "s"},
    {"key": "t1", "label": "thread pool = 1", "color": "#2ca02c", "linestyle": "-", "marker": "^"},
    {"key": "t2", "label": "thread pool = 2", "color": "#d62728", "linestyle": "-", "marker": "D"},
    {"key": "t4", "label": "thread pool = 4", "color": "#9467bd", "linestyle": "-", "marker": "v"},
    {"key": "t8", "label": "thread pool = 8", "color": "#8c564b", "linestyle": "-", "marker": "p"},
]

def plot_test(test_id):
    json_path = f"plots/loadtest{test_id}.json"
    if not os.path.exists(json_path):
        print(f"Error: {json_path} does not exist.")
        return

    with open(json_path, "r") as f:
        data = json.load(f)

    concurrencies = data["concurrencies"]
    endpoint = data["endpoint"]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 11), sharex=True)

    # 1. Throughput Plot
    for style in CONFIG_STYLES:
        key = style["key"]
        if key in data["configs"]:
            y_vals = data["configs"][key]["throughputs"]
            ax1.plot(
                concurrencies,
                y_vals,
                label=style["label"],
                color=style["color"],
                linestyle=style["linestyle"],
                marker=style["marker"],
                linewidth=1.8,
                markersize=6
            )

    ax1.set_title(f"Throughput vs load: {endpoint}", fontsize=13, fontweight='bold', pad=10)
    ax1.set_ylabel("Throughput (requests/sec)", fontsize=11)
    ax1.set_ylim(bottom=0)
    ax1.set_xscale("log", base=2)
    ax1.grid(True, which="both", linestyle="--", alpha=0.5)
    ax1.legend(loc="lower right", framealpha=0.9, fontsize=9)

    # 2. Response Time Plot
    for style in CONFIG_STYLES:
        key = style["key"]
        if key in data["configs"]:
            y_vals = data["configs"][key]["latencies"]
            ax2.plot(
                concurrencies,
                y_vals,
                label=style["label"],
                color=style["color"],
                linestyle=style["linestyle"],
                marker=style["marker"],
                linewidth=1.8,
                markersize=6
            )

    ax2.set_title(f"Response time vs load: {endpoint}", fontsize=13, fontweight='bold', pad=10)
    ax2.set_xlabel("Concurrent connections (wrk -c)", fontsize=11)
    ax2.set_ylabel("Mean response time (ms)", fontsize=11)
    ax2.set_ylim(bottom=0)
    ax2.set_xscale("log", base=2)
    ax2.grid(True, which="both", linestyle="--", alpha=0.5)
    ax2.legend(loc="upper left", framealpha=0.9, fontsize=9)

    # Format x ticks: 1, 2, 4, 8, 16, 32, 64, 128 as 2^0, 2^1, ...
    ax2.set_xticks(concurrencies)
    ax2.set_xticklabels([f"$2^{{{i}}}$" for i in range(len(concurrencies))])

    plt.tight_layout()
    output_path = f"plots/loadtest{test_id}.jpg"
    plt.savefig(output_path, dpi=200, format="jpg")
    plt.close()
    print(f"--> Saved plot: {output_path}")

def main():
    for test_id in [1, 2, 3]:
        plot_test(test_id)

if __name__ == "__main__":
    main()
