#!/usr/bin/env python3
"""Synthetic OFDM quantization study; no RF/ADC/AGC hardware model."""
import json
from pathlib import Path
import numpy as np

def quantize(x, bits):
    peak = (1 << (bits - 1)) - 1
    clipped = (np.abs(x.real) > 1) | (np.abs(x.imag) > 1)
    y = np.clip(np.rint(x.real * peak), -peak, peak) / peak
    y = y + 1j * np.clip(np.rint(x.imag * peak), -peak, peak) / peak
    return y, float(clipped.mean())

def run(seed=20261030):
    rng = np.random.default_rng(seed)
    nfft, symbols = 64, 4096
    bins = np.r_[np.arange(1, 27), np.arange(38, 64)]
    payload = (rng.choice([-1., 1.], (symbols, len(bins)))
               + 1j * rng.choice([-1., 1.], (symbols, len(bins)))) / np.sqrt(2)
    grid = np.zeros((symbols, nfft), complex)
    grid[:, bins] = payload
    wanted = np.fft.ifft(grid, axis=1)
    wanted_rms = np.sqrt(np.mean(np.abs(wanted) ** 2))
    fixed_gain = .8 / max(np.abs(wanted.real).max(), np.abs(wanted.imag).max())
    noise = (rng.standard_normal(wanted.shape) + 1j * rng.standard_normal(wanted.shape))
    noise *= wanted_rms * 10 ** (-35 / 20) / np.sqrt(2)
    tone = np.exp(2j * np.pi * 30 * np.arange(nfft) / nfft)[None, :]
    rows = []
    for blocker_db in [None, 0, 20, 40]:
        composite = wanted + noise
        if blocker_db is not None:
            composite = composite + wanted_rms * 10 ** (blocker_db / 20) * tone
        for mode in ['fixed', 'ideal_peak_rescale']:
            gain = fixed_gain if mode == 'fixed' else .8 / max(
                np.abs(composite.real).max(), np.abs(composite.imag).max())
            for bits in [12, 8]:
                output, clipped = quantize(composite * gain, bits)
                recovered = np.fft.fft(output / gain, axis=1)[:, bins]
                evm = np.sqrt(np.mean(np.abs(recovered - payload) ** 2))
                rows.append(dict(blocker_dbc=blocker_db, gain_mode=mode, bits=bits,
                                 gain=float(gain), clipped_fraction=clipped,
                                 evm_db=float(20 * np.log10(evm))))
    # Algorithm sanity checks, not RF acceptance tests.
    for bits in [8, 12]:
        y, clip = quantize(np.array([0, .25+.125j]), bits)
        assert clip == 0 and np.max(np.abs(y - [0, .25+.125j])) < .01
    assert all(r['clipped_fraction'] == 0 for r in rows if r['gain_mode'] == 'ideal_peak_rescale')
    return dict(schema='synthetic-quantization-v1', seed=seed, nfft=nfft,
                symbols=symbols, active_bins=bins.tolist(), noise_db_relative_wanted=-35,
                blocker_bin=30, status='SIMULATION_ONLY',
                limitations=['Not a full IEEE802.11 modem or measured AD936x model',
                             'No analog compression, AGC timing, phase noise, clock jitter or PSU ripple model',
                             'Ideal FFT synchronization; no channel or packet FEC; no CP needed in this ideal channel',
                             'ideal_peak_rescale knows the whole capture peak; not a realizable AGC claim',
                             'Strong tone lies outside active bins but inside the sampled band'],
                results=rows)

if __name__ == '__main__':
    data = run()
    path = Path(__file__).with_name('quantization_results.json')
    path.write_text(json.dumps(data, indent=2) + '\n')
    print('SIMULATION_ONLY', path)
    for r in data['results']:
        print(r)
