#!/usr/bin/python3
from pathlib import Path
import sys
import re

pattern = re.compile(r'takenCorrect: (\d+)  takenIncorrect: (\d+) notTakenCorrect: (\d+) notTakenIncorrect: (\d+)')


def read_results_file(result_file):
	with open(result_file) as f:
		result_text = f.read()
	
	m = pattern.match(result_text)
	if not m:
		raise ValueError('Invalid results file')

	return [int(g) for g in m.groups()]
	

def print_results(results, title):

	total_branches = sum(results)
	total_correct = results[0] + results[2]
	correct_rate = 100 * (total_correct / total_branches)

	print(f'{title}:')
	print(f'\tTaken Correct = {results[0]}')
	print(f'\tTaken Incorrect = {results[1]}')
	print(f'\tNot Taken Correct = {results[2]}')
	print(f'\tNot Taken Incorrect = {results[3]}')
	print(f'\n\tResults: {total_correct} / {total_branches} ({correct_rate:0.3f}%)\n')


if __name__ == '__main__':
	if len(sys.argv) == 1:
		results = read_results_file('result.out')
		print_results(results, 'result.out')
		sys.exit()	
	
	result_path = Path(sys.argv[1])
	if result_path.is_dir():
		total_results = [0, 0, 0, 0]
		for result_file in result_path.glob('**/*.out'):
			short_name = result_file.name.split('_')[0]
			results = read_results_file(result_file)
			print_results(results, short_name)

			total_results = [t + r for t, r in zip(total_results, results)]

		print_results(total_results, 'Total')

	else:
		short_name = result_path.name.split('_')[0]
		results = read_results_file(result_path)
		print_results(results, short_name)

