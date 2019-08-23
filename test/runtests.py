import json
import base64
import gzip
import glob
import sys
import os
import stat
from os import path
import subprocess
import collections
import contextlib

TEST_DIR = path.join('test')
TEMP_DIR = TEST_DIR
ELF_FILE = path.join(TEMP_DIR, 'test-elf')
JSON_FILE = path.join(TEMP_DIR, 'test-elf.json')
FEMU_EXE = './femu'
TESTS_FOLDER = path.join(TEST_DIR, 'x86-unit-tests')


def execute(cmd, verbose=False):
    if verbose: print('run:', cmd)
    result = subprocess.run(cmd, shell=True, capture_output=True)
    try:
        result.check_returncode()
    except subprocess.CalledProcessError as e:
        print(result.stdout)
        print(result.stderr)
        raise e
    return result


# given two dictionaries, returns a new dictionary (key, a-value, b-value)
# of all mismatching values. If a given key only appears in one dict, uses
# the default value
def dict_diff(a, b, default=None):
    keys = a.keys() | b.keys()
    result = {}
    for k in keys:
        if not(k in a and k in b and a[k] == b[k]):
            result[k] = (a.get(k, default), b.get(k, default))
    return result


def run_test(test, verbose=False):
    failed_message= f"FAIL: '{test['test_name']}', testing '{test['test_code']}'"
    if not verbose: failed_message = "\n" + failed_message
    
    # write binary
    elf_binary = gzip.decompress(base64.b64decode(test['elf_gzip_base64']))
    with open(ELF_FILE, 'wb') as f:
        f.write(elf_binary)
    os.chmod(ELF_FILE, stat.S_IRWXU)
    mem_range = ",".join(test['result_memory'][k] for k in ['start', 'end'])

    # remove json
    with contextlib.suppress(FileNotFoundError):
        os.remove(JSON_FILE)
    
    # execute and load result
    exec_result = execute(f'{FEMU_EXE} -t {JSON_FILE} -m {mem_range} {ELF_FILE}', verbose=verbose)
    with open(JSON_FILE) as f:
        result = json.load(f)
    # ?print(exec_result.stdout.decode('utf-8'))

    # check failed status  of result
    if result['result_status_test_failed']:
        print(failed_message)
        print(f"  failed with status {result['result_status']}")
        return False
    
    
    # compare result
    reg_diff = dict_diff(test['result_registers'], result['result_registers'], '-')
    mem_diff = dict_diff(test['result_memory']['nonzeros'], result['result_memory']['nonzeros'],
                         '00 00 00 00 00 00 00 00')
    mem_range_diff = dict_diff(
        *[{ 'mem_' + k : d['result_memory'][k]
            for k in ['start', 'end']}
          for d in [test, result]]
    )
    
    # combine diff, sort and print
    reg_order = ['eip', 'eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi', 'eflags']
    reg_sort_key = lambda t : ((reg_order.index(t[0]),'')
                               if t[0] in reg_order else
                               (len(reg_order), t[0]))
    all_diff = (
        sorted(reg_diff.items(), key=reg_sort_key) +
        sorted(mem_range_diff.items(), reverse=True) +
        sorted(mem_diff.items())
    )
    if len(all_diff) > 0:
        print(failed_message)
        print("  key|" + "|".join(f"{'['+k+']':>23}" if k.startswith('0x') else f'{k:>10}'
                                for k,_ in all_diff))
        print("  exp|" + "|".join(f'{v:>10}' for _,(v,_) in all_diff))
        print("  got|" + "|".join(f'{v:>10}' for _,(_,v) in all_diff))
        
        return False
    else:
        if verbose: print("PASS")
        return True


def run_test_for_file(file_name, verbose=False):
    print(f"=== run tests from '{file_name}' ===")
    with open(file_name) as f:
        data = json.load(f)
    tests = data['tests']
    num_pass = 0
    num_fail = 0
    for test in tests:
        if verbose: print("test", test['test_name'])
        if run_test(test, verbose=verbose):
            if not verbose: print(".", end="", flush=True)
            num_pass += 1
        else:
            num_fail += 1
        if verbose: print()
    if verbose:
        print(f"ran {num_pass+num_fail} tests, {num_pass} passed, {num_fail} failured")
    return (num_pass, num_fail)


def run_tests(file_names, verbose=False):
    num_pass, num_fail = (0, 0)
    for file_name in file_names:
        s,f = run_test_for_file(file_name, verbose=verbose)
        num_pass += s
        num_fail += f
    print()
    print(f"ran {num_pass+num_fail} tests, {num_pass} passed, {num_fail} failured")



##### MAIN #########################################################################################
def main(argv=[], verbose=False):
    # remove -v/--verbose
    if len(argv) > 0 and argv[0] in {'-v', '--verbose'}:
        argv = argv[1:]
        verbose = True
    # collect filenames as necessary
    if len(argv) == 0 or argv == ['all']:
        file_names = glob.glob(path.join(TESTS_FOLDER, '*.json'))
    else:
        file_names = argv
    #runtests
    run_tests(file_names, verbose=verbose)

if __name__ == "__main__":
    main(sys.argv[1:], verbose=False)

