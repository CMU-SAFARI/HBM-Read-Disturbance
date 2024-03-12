import os

os.system("make")

# num_row_groups_list = ["2", "2", "2", "2", "2", "2", "2", "2", "2"]
# hammers_per_round_list = ["1 1000", "250 1000", "500 1000", "750 1000", "1250 1000", "1500 1000", "1750 1000", "2000 1000", "3000 1000"]
# hammers_per_round_list = ["1000 1", "1000 250", "1000 500", "1000 750", "1000 1250", "1000 1500", "1000 1750", "1000 2000", "1000 3000"]

# num_row_groups_list = ["1", "2", "2", "2", "3", "3", "3", "3"]
# hammers_per_round_list = ["1000", "1000 1000", "100 1000", "1000 100", "1000 1000 1000", "100 1000 100", "1000 100 100", "100 100 1000"]

# num_row_groups_list = ["1", "2", "2", "2", "3", "3", "3", "3"]
# hammers_per_round_list = ["500", "500 500", "100 500", "500 100", "500 500 500", "100 500 100", "500 100 100", "100 100 500"]
refs_per_round = "17"
hammers_per_round_list = [
    # "1 1",
    # "1 2",
    # "1 2 1"
    # "10 1",
    "100 1 253 150",
    "100 1 258 150",

    # "1000 1",
    # "10000 1",

]

refs_after_init = 0

dummy_hammers_per_round_list = [0]
num_dummy_aggrs_list = [0]

# dummy_hammers_per_round_list = [50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000]
# num_dummy_aggrs_list = [1, 2, 3, 4, 5, 6, 7, 8, 15, 16]

# for num_row_groups, hammers_per_round in zip(num_row_groups_list, hammers_per_round_list):
for dummy_hammers_per_round in dummy_hammers_per_round_list:
    for num_dummy_aggrs in num_dummy_aggrs_list:
        for hammers_per_round in hammers_per_round_list:
            num_row_groups = str(hammers_per_round.count(" ") + 1)

            out_filename = "results/" + \
                    "numdummy_" + str(num_dummy_aggrs) + \
                    "-" + str(dummy_hammers_per_round) + \
                    "/rg_" + num_row_groups + \
                    "-ham" + hammers_per_round.replace(" ", "_") + \
                    "-ref" + refs_per_round + \
                    "-refinit_" + str(refs_after_init) + \
                    ".txt"
            os.system("mkdir -p " + out_filename[:-len(out_filename.split("/")[-1])] + "/")
            # out_filename = "results_1/fast.txt"
            log_filename = out_filename[:-4] + ".log"

            indices = [str(i+3) for i in range(int(num_row_groups))]
            indices = " ".join(indices)

            cmd = "sudo ./TRRAnalyzer" + \
                    " --out " + out_filename + \
                    " --row_scout_file out_R-R.txt" +  \
                    " --num_row_groups " + num_row_groups + \
                    " --row_layout RAR" +  \
                    " --num_rounds 1" +  \
                    " --num_iterations 32" +  \
                    " --hammers_per_round " + hammers_per_round + \
                    " --cascaded_hammer " +  \
                    " --hammer_rgs_individually " +  \
                    " --refs_per_round " + refs_per_round + \
                    " --init_only_victims " +  \
                    " --row_group_indices " + indices + \
                    " --refs_after_init " + str(refs_after_init) + \
                    " --num_dummy_after_init 0" +  \
                    " --hammer_dummies_independently " +  \
                    " --hammer_dummies_first " +  \
                    " --dummy_hammers_per_round " + str(dummy_hammers_per_round) + \
                    " --num_dummy_aggrs " + str(num_dummy_aggrs) +  \
                    " --log_phys_scheme 1"
            
            """
                    # " --refs_after_init_no_dummy_hammer " +  \
                    # " --init_aggrs_first " +  \
                    # " --append " +  \
                    # " --location_out " \
                    # " --dummy_aggrs_bank " +  \
                    # " --dummy_aggr_ids " +  \
                    # " --dummy_ids_offset " +  \
                    # " --only_pick_rgs " +  \
                    # " > " + log_filename
            """

            os.system("sudo ../../ResetBoard/full_reset.sh")
            print(cmd)
            with open(log_filename, "w") as f:
                f.write(cmd + "\n")
            os.system(cmd + " >> " + log_filename)


