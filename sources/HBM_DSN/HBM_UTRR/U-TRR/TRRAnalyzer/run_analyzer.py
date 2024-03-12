#!/usr/bin/env python3

# This is the Python version of the sweepDummiesWhileHammering.sh bash script, which is no longer updated

ROOT_PROJDIR = "/home/hhasan/SoftMC_TRR/sources/apps/TRR"


import sys, os, subprocess
import re

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def colorPrint(msg, color):
    print(color + msg + bcolors.ENDC)

def run_analysis():

    # Run the reset_fpga.sh script to clean the potentially incorrect state from the previous experiment
    FNULL = open(os.devnull, 'w')
    subprocess.run(ROOT_PROJDIR + '/../ResetBoard/reset_fpga.sh', stdout=FNULL, stderr=subprocess.STDOUT)


    BIN_TRR_ANALYZER = "./trr_analyzer"

    EXPERIMENT_ROOT_DIR = "experiment_data"
    RET_PROFILER_RESULTS_DIR = "../RetentionProfiler/results_V1.2"

    MODULE_LABEL = os.environ['SOFTMC_MODULE_ID']
    TEMPERATURE = os.environ['SOFTMC_TEMPERATURE']

    ### EXPERIMENT CONFIGURATION ###

    # Main params, aggressors, and victims
    # NUM_ITERATIONS = 128
    NUM_ITERATIONS = 128
    # NUM_ITERATIONS = 524288 # 8K * 64
    NUM_WEAKS = 1
    ROW_LAYOUT = "RARAR"
    # ROW_LAYOUT = "UUUUU"

    # Hammering
    # LIST_HAMMERS = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "20", "50", "100", "200", "500", "1000", "2000"]
    LIST_HAMMERS = ["50 9"]
    LIST_HAMMER_DURATION = [0]
    LIST_HAMMERS_BEFORE_WAIT = ['']
    CASCADED_HAMMER = "TRUE"


    # Data initialization
    INIT_AGGRS_FIRST = "TRUE" # when true, the aggressors are initialized before the victims
    INIT_ONLY_VICTIMS = "TRUE" # when true, the aggressors are not initialized at the beginning of an iteration
    ONLY_FIRST_IT_AGGR_INIT_AND_HAMMER = "FALSE" # when true, only during the first iteration the aggressor rows can be initialized and hammered. During the other iteration the aggressors are never activated.

    # Refresh
    LIST_NUM_REFS = [1]
    LIST_REFS_PER_REFCYCLE = [1]

    # Dummy rows
    DUMMY_AGGRS_BANK = 1
    NUM_DUMMIES_BEFORE_ITERATION = 0
    # LIST_NUM_DUMMIES_WHILE_HAMMERING = [13, 14, 15, 16]
    LIST_NUM_DUMMIES_WHILE_HAMMERING = [0]
    # LIST_DUMMY_HAMMERS_PER_REF = [84, 83, 82, 81, 80, 79, 79]
    LIST_DUMMY_HAMMERS_PER_REF = [0]
    HAMMER_DUMMIES_FIRST = "FALSE" # When TRUE, the dummies are hammered before the aggressors
    HAMMER_DUMMIES_INDEPENDENTLY = "TRUE" # When TRUE, the dummies are hammered after all ACTs to the aggressors are issued even when using interleaved RowHammer. Basically, the dummy rows are treated as separate group of rows to hammer last when this option is TRUE.
    HAMMER_AGGRS_DUMMIES_EQUALLY = False # when true, the aggressors are hammered the same amount as the dummies
    ONLY_FIRST_IT_DUMMY_HAMMER = "FALSE" # when true, only during the first iteration the dummy rows can be hammered. During the other iteration the dummy rows are never activated.


    # Cleaning TRR state
    RESET_TRR = False # this is for one time TRR state reset. 64 dummies are hammered only once before the experiment but not in every iteration
    SATURATE_TRR_COUNTERS = False
    PERFORM_SELF_REFRESH_CYCLE = False
    REFS_BEFORE_ITERATION = 0
    ARG_NO_DUMMY_HAMMER_REFS = "FALSE" # when TRUE, issuing REFS_BEFORE_ITERATION REF cmds after completing the dummy row hammering and refreshing phase

    # Misc.
    RUN_AS_SINGLE_SOFTMC_PROG = "TRUE"
    VERSION = "0"
    # BANK0_HAMMER_PERIOD = "8192" #"4096"
    BANK0_HAMMER_PERIOD = "0" #"4096"
    # NUM_PRE_INIT_BANK0_HAMMERS = str(131072) # 128K
    NUM_PRE_INIT_BANK0_HAMMERS = str(0) # 128K
    PRE_INIT_BANK0_HAMMERS_INC = "0"

    ### END - EXPERIMENT CONFIGURATION ###


    # Find an appropriate RetentionProfiler results file
    # we look for a RetentionProfiler output that respects only the number R's and the distance between them
    search_pattern = ".*" + ROW_LAYOUT + ".*"
    chars_to_replace = [('A', '.'), ('-', '.'), ('U', 'R')] # each tuple contains (<substring_to_replace>, <new_substring>)
    for c, r in chars_to_replace:
        search_pattern = search_pattern.replace(c, r)

    # get a list of the profiled row layouts
    profiling_results_dir = RET_PROFILER_RESULTS_DIR + f'/{MODULE_LABEL}-{TEMPERATURE}/'
    profiled_row_layouts = {f.split('.')[1] : f for f in os.listdir(profiling_results_dir) if '-wr' not in f}

    # find the match that has the shortest row layout
    cur_match_lenght = -1
    matching_file = ''
    for layout, filename in profiled_row_layouts.items():
        match = re.search(search_pattern, layout)

        if match != None:
            if matching_file == '' or cur_match_lenght > len(layout):
                cur_match_lenght = len(layout)
                matching_file = filename

    if matching_file == '':
        colorPrint(f'ERROR: No RetentionProfiler result file found that matches the desired rowlayout: {ROW_LAYOUT}', bcolors.FAIL)
        sys.exit(-1)

    # Process cmd line flags
    flags = f'--append -w {NUM_WEAKS}'
    flags += f' --refs_after_init {REFS_BEFORE_ITERATION} --num_dummy_after_init {NUM_DUMMIES_BEFORE_ITERATION}'

    if "sam" in MODULE_LABEL or "gr" in MODULE_LABEL:
        flags += ' --log_phys_scheme 1'

    if CASCADED_HAMMER == "TRUE":
        flags += ' --cascaded_hammer'

    if ARG_NO_DUMMY_HAMMER_REFS == "TRUE":
        flags += ' --refs_after_init_no_dummy_hammer'

    if INIT_AGGRS_FIRST == "TRUE":
        flags += ' --init_aggrs_first'

    if ONLY_FIRST_IT_AGGR_INIT_AND_HAMMER == "TRUE":
        flags += ' --first_it_aggr_init_and_hammer'

    if ONLY_FIRST_IT_DUMMY_HAMMER == "TRUE":
        flags += ' --first_it_dummy_hammer'

    if HAMMER_DUMMIES_FIRST == "TRUE":
        flags += ' --hammer_dummies_first'

    if HAMMER_DUMMIES_INDEPENDENTLY == "TRUE":
        flags += ' --hammer_dummies_independently'

    if INIT_ONLY_VICTIMS == "TRUE":
        flags += ' --init_only_victims'

    if RUN_AS_SINGLE_SOFTMC_PROG == "TRUE":
        flags += ' --use_single_softmc_prog'


    # Analyze the profiled rows file and get hammerable rows to work with
    colorPrint(f'Analyzing the weak rows in file: {matching_file}', bcolors.OKBLUE)

    weaks_inds_fname = matching_file + f'.{NUM_WEAKS}-wr'

    fullpath_profiler_data = os.path.join(profiling_results_dir, matching_file)
    fullpath_weak_inds = os.path.join(profiling_results_dir, weaks_inds_fname)

    if not os.path.isfile(fullpath_weak_inds):
        colorPrint(f"A weak row indices file not found", bcolors.WARNING)
        colorPrint(f"Picking {NUM_WEAKS} weak rows from {matching_file}", bcolors.OKBLUE)

        cmd = ['./get_hammerable_weak_rows.sh', fullpath_profiler_data, str(NUM_WEAKS), ROW_LAYOUT]
        subprocess.run(cmd)

    if os.stat(fullpath_weak_inds).st_size == 0:
        colorPrint(f'File {weaks_inds_fname} is empty. Potentially the get_hammerable_weak_rows.sh script failed to find suitable weak rows.', bcolors.FAIL)

    with open(fullpath_weak_inds, 'r') as weak_inds_file:
        weak_row_inds = weak_inds_file.readline()
        weak_row_inds = weak_row_inds.strip()


    for num_ref_cycles in LIST_NUM_REFS:
        for refs_per_cycle in LIST_REFS_PER_REFCYCLE:
            for num_dummies in LIST_NUM_DUMMIES_WHILE_HAMMERING:
                for hammers_per_ref in LIST_HAMMERS:
                    for hammers_per_ref_before_wait in LIST_HAMMERS_BEFORE_WAIT:
                        for hammer_duration in LIST_HAMMER_DURATION:
                            for dummy_hammers_per_ref in LIST_DUMMY_HAMMERS_PER_REF:
                                
                                if HAMMER_AGGRS_DUMMIES_EQUALLY:
                                    num_aggrs = ROW_LAYOUT.count('A')

                                    hammers_per_ref = str(dummy_hammers_per_ref)
                                    for i in range(num_aggrs-1):
                                        hammers_per_ref += f" {dummy_hammers_per_ref}"

                                if SATURATE_TRR_COUNTERS:
                                    cmd = ['./saturate_TRR_counters.sh', fullpath_profiler_data, str(NUM_WEAKS), ROW_LAYOUT]
                                    cp = subprocess.run(cmd)

                                    if cp.returncode != 0:
                                        colorPrint(f'ERROR: Saturating the TRR counters failed with exit code: {cp.returncode}!', bcolors.FAIL)
                                        sys.exit(cp.returncode)
                                    colorPrint('Saturating the TRR counters was successful!', bcolors.OKGREEN)

                                if PERFORM_SELF_REFRESH_CYCLE:
                                    cmd = ['./perform_self_refresh_cycle.sh']
                                    cp = subprocess.run(cmd)

                                    if cp.returncode != 0:
                                        colorPrint(f'ERROR: Performing a SelfRefresh cycle failed with exit code: {cp.returncode}!', bcolors.FAIL)
                                        sys.exit(cp.returncode)
                                    colorPrint('A SelfRefresh cycle was successful!', bcolors.OKGREEN)

                                if RESET_TRR:
                                    cmd = ['./reset_TRR_state.sh', fullpath_profiler_data, NUM_WEAKS]
                                    cp = subprocess.run(cmd)

                                    if cp.returncode != 0:
                                        colorPrint(f'ERROR: Resetting the TRR state failed with exit code: {cp.returncode}!', bcolors.FAIL)
                                        sys.exit(cp.returncode)
                                    colorPrint('Resetting the TRR state was successful!', bcolors.OKGREEN)

                                working_dir = f"./{EXPERIMENT_ROOT_DIR}/ASSINGLESOFTMCPROG-{RUN_AS_SINGLE_SOFTMC_PROG}/CASCADED-{CASCADED_HAMMER}/TRRreset-{str(RESET_TRR).upper()}_countersaturation-{str(SATURATE_TRR_COUNTERS).upper()}_selfrefresh{str(PERFORM_SELF_REFRESH_CYCLE).upper()}/{ROW_LAYOUT}/weaks-{str(NUM_WEAKS)}_hammers-{hammers_per_ref.replace(' ', '-')}_PreItREFs-{str(REFS_BEFORE_ITERATION)}_PreItDummies-{str(NUM_DUMMIES_BEFORE_ITERATION)}/module-{MODULE_LABEL}_temp-{TEMPERATURE}"
                                os.makedirs(working_dir, exist_ok=True)

                                output_fname=f"DUMMIES-{num_dummies}_DUMMYHAMMERS-{dummy_hammers_per_ref}_DUMMYAGGRSBANK-{str(DUMMY_AGGRS_BANK)}_NODUMMYREF-{ARG_NO_DUMMY_HAMMER_REFS}_REF-{str(num_ref_cycles)}_RPC-{str(refs_per_cycle)}_initaggrsfirst-{INIT_AGGRS_FIRST}_firstitaggrhammer-{ONLY_FIRST_IT_AGGR_INIT_AND_HAMMER}_initonlyvictims-{INIT_ONLY_VICTIMS}_hammerdummiesfirst-{HAMMER_DUMMIES_FIRST}_hdi-{HAMMER_DUMMIES_INDEPENDENTLY}"
                                output_fname += f"_hd-{hammer_duration}"
                                output_fname += f"_fidh-{ONLY_FIRST_IT_DUMMY_HAMMER}"
                                output_fname += f"_b0hp-{BANK0_HAMMER_PERIOD}"
                                output_fname += f"_b0h-{NUM_PRE_INIT_BANK0_HAMMERS}"
                                output_fname += f"_b0hi-{PRE_INIT_BANK0_HAMMERS_INC}"
                                output_fname += f"_ver-{VERSION}"

                                if hammers_per_ref_before_wait != '':
                                    output_fname += f"_hbw-{hammers_per_ref_before_wait.split()[0]}"
                                # output_fname += f"_prerefdelay-${PRE_REF_DELAY}"
                                output_fname += f"_{matching_file}"
                                output_path = os.path.join(working_dir, output_fname)

                                cmd = ['sudo', BIN_TRR_ANALYZER]

                                args =  f'-f {fullpath_profiler_data}' \
                                        f' --wrs_type {ROW_LAYOUT}' \
                                        f' --num_refresh_cycles {num_ref_cycles}' \
                                        f' --num_iterations {NUM_ITERATIONS}' \
                                        f' --weak_rows {weak_row_inds}' \
                                        f' --hammers_per_ref {hammers_per_ref}' \
                                        f' --num_dummy_aggrs {num_dummies}' \
                                        f' --dummy_hammers_per_ref {dummy_hammers_per_ref}' \
                                        f' --dummy_aggrs_bank {DUMMY_AGGRS_BANK}' \
                                        f' --hammer_duration {hammer_duration}' \
                                        f' --refs_per_cycle {refs_per_cycle}' \
                                        f' --bank0_hammer_period {BANK0_HAMMER_PERIOD}' \
                                        f' --num_pre_init_bank0_hammers {NUM_PRE_INIT_BANK0_HAMMERS}' \
                                        f' --pre_init_bank0_hammers_increment {PRE_INIT_BANK0_HAMMERS_INC}' \
                                        f' -o {output_path}'

                                if hammers_per_ref_before_wait != '':
                                    args += f' --hammers_per_ref_before_wait {hammers_per_ref_before_wait}'

                                cmd += flags.split()
                                cmd += args.split()

                                colorPrint(f'Executing cmd (as arg list):', bcolors.OKBLUE)
                                print(f'{cmd}')
                                colorPrint(f'Executing cmd (as string):', bcolors.OKBLUE)
                                print(f'{" ".join(cmd)}')
                                # sys.exit(0)

                                # appending the command to the output file
                                with open(output_path, 'a+') as output_file:
                                    output_file.write("cmd: " + " ".join(cmd) + '\n')

                                subprocess.run(cmd)
                            
                        if HAMMER_AGGRS_DUMMIES_EQUALLY: # in LIST_HAMMERS loop
                            break

    colorPrint("Done.", bcolors.OKGREEN)

if __name__ == "__main__":
    run_analysis()