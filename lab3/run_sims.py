#!/usr/bin/python3
import subprocess
import itertools
import os

outputs_dir = 'dummy_outputs'
if not os.path.exists(outputs_dir):
	os.mkdir(outputs_dir)

base_path = '../../base'
inputs_path = os.path.join(base_path, 'benchmarks', 'inputs')
binary_path = os.path.join(base_path, 'benchmarks', 'binaries')

blackscholes = ('blackscholes', os.path.join(binary_path, 'blackscholes'),
				'1',
				os.path.join(inputs_path, 'blackscholes', 'in_4K.txt'),
				os.path.join(outputs_dir, 'blackscholes.txt'))				

body_track = ('bodytrack', os.path.join(binary_path, 'bodytrack'),
			  os.path.join(inputs_path, 'bodytrack', 'simsmall', 'sequenceB_1'),
			  '4', '1', '1000', '5', '0', '1')

cholesky = ('cholesky', os.path.join(binary_path, 'cholesky'),
			'-p1',
			'<', os.path.join(inputs_path, 'cholesky', 'tk14.0'))

ferret = ('ferret', os.path.join(binary_path, 'ferret'),
		  os.path.join(inputs_path, 'ferret', 'simsmall', 'corel'),
		  'lsh',
		  os.path.join(inputs_path, 'ferret', 'simsmall', 'queries'),
		  '10', '20', '1',
		  os.path.join(outputs_dir, 'ferret.txt'))

fft = ('fft', os.path.join(binary_path, 'fft'),
	   '-m16', '-p1',
	   os.path.join(outputs_dir, 'fft.txt'))	

fluidanimate = ('fluidanimate', os.path.join(binary_path, 'fluidanimate'),
				'1', '5', 
				os.path.join(inputs_path, 'fluidanimate', 'in_35K.fluid'))

tests = (blackscholes, body_track, cholesky, ferret, fft, fluidanimate)

log_num_rows_opts = [str(i) for i in range(9, 17)]
log_block_size_opts = [str(i) for i in range(2, 8)]
associativity_opts = [str(i) for i in range(1, 8)]

configs = itertools.product(log_num_rows_opts,
							log_block_size_opts,
							associativity_opts)

def test():
	log_num_rows = '10'
	log_block_size = '5'
	associativity = '2'

	results_path = os.path.join('simulations', f'{log_num_rows}_{log_block_size}_{associativity}')
	os.makedirs(results_path)
	running_tests = []
	for test in tests:
		args = ['../../base/pin/pin', '-t', 'caches.so',
			'-o', os.path.join(results_path, f'{test[0]}.txt'),
			'-r', log_num_rows, '-b', log_block_size, '-a', associativity,
			'--', *test[1:]]	
		running_tests.append(subprocess.Popen(args))

	for rt in running_tests:
		rt.wait()

if __name__ == '__main__':
	if not os.path.exists('simulations'):
		os.mkdir('simulations')

	for log_num_rows, log_block_size, associativity in configs:
		results_path = os.path.join('simulations', f'{log_num_rows}_{log_block_size}_{associativity}')
		os.makedirs(results_path)
		running_tests = []
		for test in tests:
			args = ['../../base/pin/pin', '-t', 'caches.so',
				'-o', os.path.join(results_path, f'{test[0]}.txt'),
				'-r', log_num_rows, '-b', log_block_size, '-a', associativity,
				'--', *test[1:]]	
			running_tests.append(subprocess.Popen(args))

		for rt in running_tests:
			rt.wait()

