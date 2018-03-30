from collections import namedtuple
from pathlib import Path
import matplotlib.pyplot as plt

Results = namedtuple('Results', 'PIPT VIPT VIVT')


def load_results(results_file):
    results = {}
    with open(results_file) as f:
        result_text = f.readlines()
        for line in result_text:
            name, values = line.split(':')
            name = ''.join(n[0].upper() for n in name.split())
            r_cnt, w_cnt, r_hit, w_hit = [int(v) for v in values.strip().split(',')]
            total_miss_rate = 1 - ((r_hit + w_hit) / (r_cnt + w_cnt))
            results[name] = total_miss_rate
    return Results(**results)


class Simulation:
    def __init__(self, sim_dir):
        self.dir = sim_dir
        params = [int(i) for i in sim_dir.name.split('_')]
        self.log_num_rows, self.log_block_size, self.associativity = params
        self.runs = {}
        for s in sim_dir.iterdir():
            run_name = s.name.replace(s.suffix, '')
            self.runs[run_name] = load_results(s)

    @property
    def params(self):
        return self.log_num_rows, self.log_block_size, self.associativity

    def __repr__(self):
        return f'Simulation("{self.dir}")'


sim_root = Path(r'C:\Users\sahko\Downloads\simulations')
simulations = [Simulation(sim_dir) for sim_dir in sim_root.iterdir()]

# Get the PARSEC program names. All simulations have the same names
parsec_programs = tuple(simulations[0].runs.keys())


def plot_simulation(x, simulations, xlabel=None, ylabel=None, title=None):
    fig = plt.figure()
    ax = fig.subplots(1)
    for p in parsec_programs:
        data = [s.runs[p].PIPT for s in simulations]
        ax.plot(x, data, label=p)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(loc='upper right')
    ax.grid(True)
    return fig


if __name__ == '__main__':
    # Find results for PIPT based on associativity. Keep other params as default
    assoc_simulations = [s for s in simulations if
                         s.log_num_rows == 9 and s.log_block_size == 2]
    assoc_points = [a.associativity for a in assoc_simulations]
    assoc_fig = plot_simulation(assoc_points, assoc_simulations,
                                'Cache Associativity', 'Miss Rate',
                                'Cache miss rate vs. Associativity (Rows=512, Block Size=4 bytes)')
    assoc_fig.savefig(r'plots\associativity.png')

    # Find results for PIPT based on rows.
    row_simulations = [s for s in simulations
                       if s.associativity == 1 and s.log_block_size == 2]
    row_points = [2**r.log_num_rows for r in row_simulations]
    row_fig = plot_simulation(row_points, row_simulations,
                              'Number of Cache Rows', 'Miss Rate',
                              'Cache miss rate vs. Rows (Block Size=4 bytes, Associativity=1)')
    row_fig.savefig(r'plots\rows.png')

    # Find results for PIPT based on cache block size.
    block_simulations = [s for s in simulations
                         if s.associativity == 1 and s.log_num_rows == 9]
    block_points = [2**r.log_block_size for r in block_simulations]
    block_fig = plot_simulation(block_points, block_simulations,
                                'Cache Row Block Size (bytes)', 'Miss Rate',
                                'Cache miss rate vs. Block Size (Rows=512, Associativity=1)')
    block_fig.savefig(r'plots\block_size.png')

    # Compare different cache models
    display_opts = [(9, 2, 1), (9, 2, 4),
                    (12, 2, 1), (12, 4, 4),
                    (9, 4, 1), (9, 4, 4), (16, 7, 7)]
    data = {}
    o = display_opts[0]
    s = next(s for s in simulations if s.params == o)
    pipt_avg = sum(r.PIPT for r in s.runs.values()) / len(s.runs)
    vipt_avg = sum(r.VIPT for r in s.runs.values()) / len(s.runs)
    vivt_avg = sum(r.VIVT for r in s.runs.values()) / len(s.runs)
    print(pipt_avg, vipt_avg, vivt_avg)