"""Train the AstraMind anomaly model and export it as TorchScript.

Contract with astro_guardian_node (do not change without changing the C++):
  input : float32 (1, 10), order = position_x/y/z, velocity_x/y/z,
          battery_voltage, battery_current, battery_soc, temperature
  output: float32 (1, 1), anomaly score in [0, 1]

Method: an autoencoder is trained on NOMINAL telemetry only. The score is
1 - exp(-err / scale), where err is the reconstruction error and scale is the
99th-percentile error on held-out nominal data. Nominal rows therefore score
mostly below ~0.63, and the default inference_threshold of 0.8 sits at about
1.6x that p99 error. Normalization and calibration are baked into the model,
so the C++ node feeds raw telemetry unchanged.

Export with torch 2.7.x to match the LibTorch 2.7.1 the node links against:
  pip install "torch==2.7.*" --index-url https://download.pytorch.org/whl/cpu

Usage:
  python train_model.py --data nominal_telemetry.csv
  python train_model.py --synthetic 20000     # pipeline check only, NOT a real model
"""
import argparse
import math
import time

import numpy as np
import torch
from torch import nn

N_FEATURES = 10
DEFAULT_OUT = "astramind_ws/src/astramind_core/models/astramind_model.pt"


def synthetic_nominal(n, seed):
    """Mimics telem_ingest_node's mock generator, plus small sensor noise."""
    rng = np.random.default_rng(seed)
    t = rng.uniform(0, 5000, n)
    ph = t * 0.001
    cols = [
        7e6 * np.cos(ph), 7e6 * np.sin(ph), np.zeros(n),
        -7500 * np.sin(ph), 7500 * np.cos(ph), np.zeros(n),
        28 + rng.normal(0, 0.05, n), 1.5 + rng.normal(0, 0.03, n),
        90 - 0.001 * t + rng.normal(0, 0.05, n),
        20 + 5 * np.sin(0.01 * t) + rng.normal(0, 0.2, n),
    ]
    return np.stack(cols, axis=1).astype(np.float32)


def load_csv(path):
    """CSV of nominal telemetry: 10 numeric columns in contract order; a header row is skipped."""
    try:
        data = np.loadtxt(path, delimiter=",", dtype=np.float32)
    except ValueError:
        data = np.loadtxt(path, delimiter=",", dtype=np.float32, skiprows=1)
    if data.ndim != 2 or data.shape[1] != N_FEATURES:
        raise SystemExit(f"{path}: expected {N_FEATURES} columns, got shape {data.shape}")
    return data


class Scorer(nn.Module):
    def __init__(self, ae, mean, std, scale):
        super().__init__()
        self.ae = ae
        self.register_buffer("mean", mean)
        self.register_buffer("std", std)
        self.register_buffer("scale", torch.tensor(float(scale)))

    def forward(self, x):
        z = (x - self.mean) / self.std
        err = ((z - self.ae(z)) ** 2).mean(dim=1, keepdim=True)
        return 1.0 - torch.exp(-err / self.scale)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--data", help="CSV of nominal telemetry (10 columns)")
    src.add_argument("--synthetic", type=int, metavar="N", help="generate N mock rows (pipeline check only)")
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--epochs", type=int, default=300)
    ap.add_argument("--seed", type=int, default=0)
    a = ap.parse_args()

    torch.manual_seed(a.seed)
    raw = synthetic_nominal(a.synthetic, a.seed) if a.synthetic else load_csv(a.data)
    if len(raw) < 100:
        raise SystemExit("need at least 100 nominal rows")
    x = torch.from_numpy(raw)
    perm = torch.randperm(len(x))
    n_val = max(len(x) // 5, 20)
    val, tr = x[perm[:n_val]], x[perm[n_val:]]

    mean = tr.mean(0)
    # shortcut: floor on std keeps constant features (e.g. z axes) from exploding, tune if real data needs it
    std = tr.std(0).clamp_min(1e-3 * (mean.abs() + 1))
    ztr, zval = (tr - mean) / std, (val - mean) / std

    ae = nn.Sequential(nn.Linear(10, 16), nn.ReLU(), nn.Linear(16, 4), nn.ReLU(),
                       nn.Linear(4, 16), nn.ReLU(), nn.Linear(16, 10))
    opt = torch.optim.Adam(ae.parameters(), lr=1e-3)
    for epoch in range(a.epochs):
        for i in torch.randperm(len(ztr)).split(256):
            opt.zero_grad()
            loss = nn.functional.mse_loss(ae(ztr[i]), ztr[i])
            loss.backward()
            opt.step()
        if epoch % 50 == 0 or epoch == a.epochs - 1:
            print(f"epoch {epoch:4d}  train mse {loss.item():.5f}")

    with torch.no_grad():
        err = ((zval - ae(zval)) ** 2).mean(1)
    scale = float(np.percentile(err.numpy(), 99))
    print(f"calibration: p99 nominal reconstruction error = {scale:.5f}")

    model = Scorer(ae, mean, std, scale).eval()
    traced = torch.jit.trace(model, torch.from_numpy(raw[:1]))
    traced.save(a.out)

    # Self-check on the saved file, exactly as the C++ node will use it.
    loaded = torch.jit.load(a.out)
    nominal = torch.from_numpy(raw[:1])
    corrupt = nominal.clone()
    corrupt[0, 9] += 80.0  # +80 C on temperature
    with torch.no_grad():
        s_nom, s_bad = loaded(nominal), loaded(corrupt)
        t0 = time.perf_counter()
        loaded(nominal)
        ms = (time.perf_counter() - t0) * 1000
    for s in (s_nom, s_bad):
        assert s.shape == (1, 1) and 0.0 <= s.item() <= 1.0 and not math.isnan(s.item()), s
    print(f"saved {a.out}  (torch {torch.__version__})")
    print(f"nominal score {s_nom.item():.3f} | +80C temperature score {s_bad.item():.3f} | {ms:.2f} ms/frame")
    if s_bad.item() <= s_nom.item():
        print("WARNING: corrupted input did not score higher than nominal; check the data.")


if __name__ == "__main__":
    main()
