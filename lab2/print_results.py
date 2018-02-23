#!/usr/bin/python3
import sys
import re

if len(sys.argv) > 1:
	result_file = sys.argv[1]
else:
	result_file = 'result.out'

with open(result_file) as f:
	result_text = f.read()

pattern = re.compile(r'takenCorrect: (\d+)  takenIncorrect: (\d+) notTakenCorrect: (\d+) notTakenIncorrect: (\d+)')

m = pattern.match(result_text)
if not m:
	raise ValueError("Invalid results file")

results = [int(g) for g in m.groups()]
total_branches = sum(results)
total_correct = results[0] + results[2]
correct_rate = 100 * (total_correct / total_branches)

print(f'Taken Correct = {results[0]}')
print(f'Taken Incorrect = {results[1]}')
print(f'Not Taken Correct = {results[2]}')
print(f'Not Taken Incorrect = {results[3]}')
print(f'\nResults: {total_correct} / {total_branches} ({correct_rate:0.3f}%)')

