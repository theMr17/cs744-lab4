#!/usr/bin/env python3
import argparse
import json
import os
import re
import signal
import subprocess
import time

SERVER_BIN = "./cflask"
SERVER_CPUS = "0,1"
CLIENT_CPUS = "2,3,4,5"
PORT = 8080

CONFIGS = [
    {"name": "single-threaded", "key": "s", "cmd": [SERVER_BIN, "s", str(PORT)]},
    {"name": "thread per request", "key": "m", "cmd": [SERVER_BIN, "m", str(PORT)]},
    {"name": "thread pool = 1", "key": "t1", "cmd": [SERVER_BIN, "t", str(PORT), "1"]},
    {"name": "thread pool = 2", "key": "t2", "cmd": [SERVER_BIN, "t", str(PORT), "2"]},
    {"name": "thread pool = 4", "key": "t4", "cmd": [SERVER_BIN, "t", str(PORT), "4"]},
    {"name": "thread pool = 8", "key": "t8", "cmd": [SERVER_BIN, "t", str(PORT), "8"]},
]

CONCURRENCIES = [1, 2, 4, 8, 16, 32, 64, 128]

TESTS = {
    1: {
        "name": "Prime Number Check (CPU-bound)",
        "path": "/arithmetic/prime?num=1000000007",
        "desc": "Tests a compute-heavy task calculating primality of 1000000007 using trial division.",
        "aim": "How does throughput and mean response time change with load for a CPU-bound request under single-threaded, thread-per-request, and varying thread pool sizes?",
        "hypothesis": "For CPU-heavy work, throughput is constrained by CPU cores. Single-threaded will peak around 1 core capacity (~6k req/s). Thread pool of 1 will match single-threaded. Thread pool of 2 will double throughput (~12k req/s) by saturating both cores (CPUs 0,1). Larger thread pools (4, 8) will not increase throughput further as only 2 CPU cores are available. Thread-per-request will suffer from repeated thread creation/destruction overhead and perform similarly to or worse than single-threaded.",
    },
    2: {
        "name": "Pingpong Echo (I/O-bound)",
        "path": "/pingpong?str=cs744",
        "desc": "Tests a lightweight string echo endpoint with minimal computation to evaluate network I/O and connection handling efficiency.",
        "aim": "How does throughput and response time scale for an I/O-bound lightweight request across server architectures under increasing concurrency?",
        "hypothesis": "With minimal CPU compute per request, throughput will reach high rates (60k-90k req/s). Thread pools of 2, 4, 8 will handle concurrent socket handoffs effectively. In thread-per-request mode, the kernel thread creation/destruction overhead per connection will become a dominant bottleneck compared to the trivial request execution time.",
    },
    3: {
        "name": "Square Calculation (Lightweight compute)",
        "path": "/square?num=12345",
        "desc": "Tests an endpoint with parameter parsing and basic integer multiplication.",
        "aim": "How do thread pools and concurrency affect throughput and latency for quick arithmetic operations?",
        "hypothesis": "Performance will sit between the CPU-heavy prime test and the pure I/O test. Adding workers up to the core count (2 workers) will scale throughput close to 2x single-threaded. Concurrency increases beyond saturation will lead to linearly rising response times in accordance with Little's Law.",
    },
}

def parse_wrk_output(output):
    """
    Extracts mean latency in ms and throughput in req/s from wrk output.
    """
    throughput = 0.0
    latency_ms = 0.0

    lat_match = re.search(r"Latency\s+([\d\.]+)(us|ms|s)", output)
    if lat_match:
        val = float(lat_match.group(1))
        unit = lat_match.group(2)
        if unit == "us":
            latency_ms = val / 1000.0
        elif unit == "ms":
            latency_ms = val
        elif unit == "s":
            latency_ms = val * 1000.0

    thr_match = re.search(r"Requests/sec:\s+([\d\.]+)([kKmM]?)", output)
    if thr_match:
        val = float(thr_match.group(1))
        suffix = thr_match.group(2).upper()
        if suffix == "K":
            val *= 1000.0
        elif suffix == "M":
            val *= 1000000.0
        throughput = val

    return throughput, latency_ms

def wait_for_port(port, timeout=5.0):
    start = time.time()
    while time.time() - start < timeout:
        try:
            res = subprocess.run(["curl", "-s", f"http://127.0.0.1:{port}/"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if res.returncode == 0:
                return True
        except Exception:
            pass
        time.sleep(0.05)
    return False

def stop_process(proc):
    if proc and proc.poll() is None:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass
        proc.wait()

def run_single_benchmark(cfg, concurrency, duration, url):
    threads = min(concurrency, 4)
    server_cmd = ["taskset", "-c", SERVER_CPUS] + cfg["cmd"]
    proc = subprocess.Popen(
        server_cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        preexec_fn=os.setsid
    )

    try:
        if not wait_for_port(PORT, timeout=4.0):
            raise RuntimeError(f"Server failed to start on port {PORT}: {' '.join(server_cmd)}")

        wrk_cmd = [
            "taskset", "-c", CLIENT_CPUS,
            "wrk",
            f"-t{threads}",
            f"-c{concurrency}",
            f"-d{duration}s",
            "-H", "Connection: close",
            url
        ]
        res = subprocess.run(wrk_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode != 0:
            print(f"Warning: wrk returned code {res.returncode}: {res.stderr}")

        throughput, latency_ms = parse_wrk_output(res.stdout)
        return throughput, latency_ms, " ".join(server_cmd), " ".join(wrk_cmd)
    finally:
        stop_process(proc)
        time.sleep(0.2)

def run_test(test_id, duration):
    info = TESTS[test_id]
    url = f"http://127.0.0.1:{PORT}{info['path']}"
    print(f"\n=======================================================")
    print(f"Running Load Test {test_id}: {info['name']}")
    print(f"Endpoint: {url}")
    print(f"Duration per point: {duration}s")
    print(f"=======================================================")

    results = {
        "test_id": test_id,
        "name": info["name"],
        "endpoint": info["path"],
        "description": info["desc"],
        "aim": info["aim"],
        "hypothesis": info["hypothesis"],
        "duration": duration,
        "server_cpus": SERVER_CPUS,
        "client_cpus": CLIENT_CPUS,
        "concurrencies": CONCURRENCIES,
        "configs": {}
    }

    commands_log = []

    for cfg in CONFIGS:
        key = cfg["key"]
        print(f"\n--> Server Configuration: {cfg['name']} ({key})")
        results["configs"][key] = {
            "name": cfg["name"],
            "throughputs": [],
            "latencies": []
        }

        for c in CONCURRENCIES:
            print(f"  Concurrency {c:3d}... ", end="", flush=True)
            thr, lat, s_cmd, w_cmd = run_single_benchmark(cfg, c, duration, url)
            print(f"Throughput: {thr:9.2f} req/s | Mean Latency: {lat:7.3f} ms")
            results["configs"][key]["throughputs"].append(thr)
            results["configs"][key]["latencies"].append(lat)
            commands_log.append(f"Config: {cfg['name']} (c={c}):\n  Server: {s_cmd}\n  Wrk:    {w_cmd}\n  Result: {thr:.2f} req/s, {lat:.3f} ms\n")

    json_path = f"plots/loadtest{test_id}.json"
    with open(json_path, "w") as f:
        json.dump(results, f, indent=2)

    data_path = f"plots/loadtest{test_id}.data"
    with open(data_path, "w") as f:
        f.write(f"================================================================================\n")
        f.write(f"CS744 Lab 4 - Load Test {test_id}: {info['name']}\n")
        f.write(f"================================================================================\n\n")
        f.write(f"1. AIM:\n   {info['aim']}\n\n")
        f.write(f"2. SETUP & EXECUTION:\n")
        f.write(f"   Machine: Intel(R) Core(TM) i5-11400H @ 2.70GHz (6 physical cores, 12 hyperthreads)\n")
        f.write(f"   Server core assignment: taskset -c {SERVER_CPUS} (Physical Cores 0 and 1)\n")
        f.write(f"   Load generator (wrk) assignment: taskset -c {CLIENT_CPUS} (Physical Cores 2, 3, 4, 5)\n")
        f.write(f"   Measurement duration: {duration}s per point\n")
        f.write(f"   Varying parameter: Concurrency -c over [1, 2, 4, 8, 16, 32, 64, 128]\n")
        f.write(f"   Client threads: -t = min(c, 4)\n")
        f.write(f"   Metrics: Throughput (requests/sec) and Mean Response Time (ms)\n\n")
        f.write(f"3. HYPOTHESIS:\n   {info['hypothesis']}\n\n")
        f.write(f"4. TABULATED RESULTS:\n\n")
        f.write(f"Throughput (requests/sec):\n")
        f.write(f"{'Concurrency':<12} | " + " | ".join(f"{cfg['key']:>10}" for cfg in CONFIGS) + "\n")
        f.write("-" * 85 + "\n")
        for i, c in enumerate(CONCURRENCIES):
            row = f"{c:<12} | " + " | ".join(f"{results['configs'][cfg['key']]['throughputs'][i]:>10.2f}" for cfg in CONFIGS)
            f.write(row + "\n")
        f.write("\n")

        f.write(f"Mean Latency (ms):\n")
        f.write(f"{'Concurrency':<12} | " + " | ".join(f"{cfg['key']:>10}" for cfg in CONFIGS) + "\n")
        f.write("-" * 85 + "\n")
        for i, c in enumerate(CONCURRENCIES):
            row = f"{c:<12} | " + " | ".join(f"{results['configs'][cfg['key']]['latencies'][i]:>10.3f}" for cfg in CONFIGS)
            f.write(row + "\n")
        f.write("\n")

        f.write(f"5. EXACT COMMANDS LOG:\n")
        for log_entry in commands_log:
            f.write(log_entry + "\n")

    print(f"\n--> Successfully saved results to {json_path} and {data_path}")

def main():
    parser = argparse.ArgumentParser(description="Run cflask load testing benchmarks.")
    parser.add_argument("--test", type=int, choices=[1, 2, 3], help="Specific test to run (1, 2, or 3). Runs all if omitted.")
    parser.add_argument("--duration", type=int, default=10, help="Duration for each wrk measurement in seconds (default: 10s)")
    args = parser.parse_args()

    tests_to_run = [args.test] if args.test else [1, 2, 3]
    for tid in tests_to_run:
        run_test(tid, args.duration)

if __name__ == "__main__":
    main()
