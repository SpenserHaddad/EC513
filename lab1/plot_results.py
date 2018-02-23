import sys
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt


def load_data(results_dir):
    results = []
    p = Path(results_dir)
    for result_file in p.glob('*.out'):
        with open(result_file) as f:
            csv_data = [int(r) for r in f.read().split(',') if r]
        data = np.array(csv_data, dtype='uint64')
        pct_data = data / data.sum()
        title = result_file.name.split('_')[0]
        results.append((title, pct_data))
    return results


full_results = load_data(sys.argv[1])
partial_results = load_data(sys.argv[2])

results_pairs = ((f, p) for f in full_results for p in partial_results
                 if f[0] == p[0])
points = np.arange(1, 101)


subplot_rows = round(len(full_results) / 2)
fig, axes = plt.subplots(nrows=2, ncols=subplot_rows)
axes = (a for ax in axes for a in ax)
for ax, ((full_title, full_data), (partial_title, partial_data)) in zip(axes, results_pairs):
    ax.semilogy(points, full_data, 'r', label='Full registers')
    ax.semilogy(points, partial_data, 'b', label='Partial registers')

    ax.grid()
    ax.set_xlabel('Dependence Distance')
    ax.set_ylabel('Percent of Dependencies')
    ax.legend()
    ax.set_title(full_title)

fig.suptitle('Dependency Spacings for the Test Suites')
fig.tight_layout()
plt.show()
