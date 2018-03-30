#! /usr/bin/python3
import sys
from pathlib import Path

if len(sys.argv) > 1:
    results_file = sys.argv[1]
else:
    results_file = 'results.out'


def load_results(results_file):
    with open(results_file) as f:
        result_text = f.readlines()

    results = {}
    for line in result_text:
        name, values = line.split(':')
        r_cnt, w_cnt, r_hit, w_hit = [int(v) for v in values.strip().split(',')]
        results[name] = (r_cnt, r_hit, w_cnt, w_hit)
    return results


def print_results(results, title):
    print(f'{title}:')
    for name, values in results.items():
        r_cnt, r_hit, w_cnt, w_hit = values
        r_miss_rate = (r_cnt - r_hit) / r_cnt * 100 if r_cnt > 0 else 0
        w_miss_rate = (w_cnt - w_hit) / w_cnt * 100 if w_cnt > 0 else 0
        if (r_cnt + w_cnt) > 0:
            total_rate = ((r_cnt + w_cnt) - (r_hit + w_hit)) / (r_cnt + w_cnt) * 100
        else:
            total_rate = 0
        name_str = f'\t{name}:'.ljust(30)
        read_stats = f'Read={r_hit}/{r_cnt} ({r_miss_rate:0.3f}%)'.ljust(35)
        write_stats = f'Write={w_hit}/{w_cnt} ({w_miss_rate:0.3f}%)'.ljust(35)
        total_stats = f'Total={total_rate:0.3f}%'
        print(f'{name_str}{read_stats}{write_stats}{total_stats}')


if __name__ == '__main__':
    if len(sys.argv) == 1:
        results = load_results('results.out')
        print_results(results, 'results.out')
        sys.exit()

    result_path = Path(sys.argv[1])
    if result_path.is_dir():
        dir_results = {}
        max_inst = 0
        for result_file in result_path.glob('**/*.out'):
            short_name = result_file.name.split('_')[0]
            results = load_results(result_file)
            max_inst = max(max_inst, results['physical index physical tag'][0] +
                           results['physical index physical tag'][2])
            print_results(results, short_name)
        print(f'Max # accesses: {max_inst}')
    else:
        short_name = result_path.name.split('_')[0]
        results = load_results(result_path)
        print_results(results, short_name)
