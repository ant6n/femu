import json
import base64
import re
import gzip
import glob
import sys
import os
import stat
from os import path
import subprocess
import contextlib
import argparse

TEST_DIR = path.join('test')
TEMP_DIR = TEST_DIR
ELF_FILE = path.join(TEMP_DIR, 'test-elf')
JSON_FILE = path.join(TEMP_DIR, 'test-elf.json')
FEMU_EXE = './femu'
TESTS_FOLDER = path.join(TEST_DIR, 'x86-unit-tests', 'x86-tests')
INJECT_ELF_FILE = path.join(TEST_DIR, 'test-emu-inject')



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


def execute_test(elf_file, mem_range, args):
    # execute and load result
    if args.create_extra_files:
        # TODO improve code...
        e = (' -e '+INJECT_ELF_FILE)
    cmd = f'{FEMU_EXE} -t {JSON_FILE} -m {mem_range} {elf_file}'
    if args.verbose: print(cmd)
    exec_result = subprocess.run(cmd, shell=True, capture_output=True)
    try:
        exec_result.check_returncode()
    except subprocess.CalledProcessError as e:
        if args.verbose: print(exec_result.stdout.decode('utf-8'))
        result = {
            'result_status_test_failed': True,
            'result_status': 'exit:' + exec_result.stderr.decode('utf-8'),
        }
    else:
        with open(JSON_FILE) as f:
            result = json.load(f)
    #print(exec_result.stdout.decode('utf-8'))
    return result

def run_test(test, args):
    failed_message= f"FAIL: '{test['test_name']}', testing '{test['test_code']}'"
    if not args.verbose: failed_message = "\n" + failed_message
    
    # write binary
    elf_binary = gzip.decompress(base64.b64decode(test['elf_gzip_base64']))
    with open(ELF_FILE, 'wb') as f:
        f.write(elf_binary)
    os.chmod(ELF_FILE, stat.S_IRWXU)
    mem_range = ",".join(test['result_memory'][k] for k in ['start', 'end'])

    # remove json
    with contextlib.suppress(FileNotFoundError):
        os.remove(JSON_FILE)
    
    result = execute_test(ELF_FILE, mem_range, args)
    #? print(result)
    
    # check failed status of result
    if result['result_status_test_failed']:
        print(failed_message)
        suffix = ''
        if result['result_status'] == 'EXIT_UNIMPLEMENTED_OPCODE':
            suffix = f" with opcode: {result['unimplemented_opcode']}'"
        print(f"  failed with status {result['result_status']}" + suffix)
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
        if args.verbose: print("PASS")
        return True


def run_test_for_file(file_name, args):
    print(f"=== run tests from '{file_name}' ===")
    with open(file_name) as f:
        data = json.load(f)
    tests = data['tests']
    num_pass = 0
    num_fail = 0
    for test in tests:
        if args.pattern is not None and not re.match(args.pattern, test['test_name']):
            continue
        if args.verbose: print("test", test['test_name'])
        if run_test(test, args):
            if not args.verbose: print(".", end="", flush=True)
            num_pass += 1
        else:
            num_fail += 1
        if args.verbose: print()
        if num_fail > 0 and args.fail_fast: break
    if args.verbose:
        print(f"ran {num_pass+num_fail} tests, {num_pass} passed, {num_fail} failed")
    return (num_pass, num_fail)


def run_tests(args):
    # find files to run tests on
    if len(args.files) == 0 or args.files == ['all']:
        file_names = glob.glob(path.join(TESTS_FOLDER, '*.json'))
    else:
        file_names = args.files

    # run tests
    num_pass, num_fail = (0, 0)
    for file_name in file_names:
        s,f = run_test_for_file(file_name, args)
        num_pass += s
        num_fail += f
        if num_fail > 0 and args.fail_fast: break
    print()
    print(f"ran {num_pass+num_fail} tests, {num_pass} passed, {num_fail} failed")



##### MAIN #########################################################################################
def main(argv=[]):
    parser = argparse.ArgumentParser(
        description='Running Femu Unit Tests',
    )
    
    parser.add_argument('-v', '--verbose', action="store_true", default=False)
    parser.add_argument('-e', '--create-extra-files', action="store_true", default=False)
    parser.add_argument('-p', '--pattern', action="store", dest="pattern",
                        help='only run tests whose name start with given regex pattern')
    parser.add_argument('-f', '--fail-fast', action="store_true", help='stop/exit on first failing test.')
    parser.add_argument(nargs="*", metavar='TESTFILE.json', dest='files',
                        help='only run tests from given json filenames (otherwise run all)')
    
    args = parser.parse_args()
    #print(args)
    
    run_tests(args)

if __name__ == "__main__":
    main(sys.argv)

