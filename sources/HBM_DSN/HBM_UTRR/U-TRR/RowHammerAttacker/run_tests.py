import os

os.system("make")


num_dummy_rows_list = [3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
hammers_per_ref_loop_list = [
    "10 10",
    "12 12",
    "14 14",
    "16 16",
    "18 18",
    "20 20",
    "22 22",
    "24 24",
    "26 26",
    "28 28",
    "30 30",
    "32 32",
    "34 34",
    "36 36",
    "38 38",
]

before = True
after = False
trrref_sync = False

for num_dummy_rows in num_dummy_rows_list:
    for hammers_per_ref_loop in hammers_per_ref_loop_list:
        for shift_refs in [7]:
            # out_filename = "f" + str(shift) + ".txt"
            out_filename = "result/" + \
                    "dummy_" + str(num_dummy_rows) + \
                    "-h_" + hammers_per_ref_loop.replace(" ", "_") + \
                    "-before_" + str(before) + \
                    "-after_" + str(after) + \
                    "-trrrefsync_" + str(trrref_sync) + \
                    ".txt"
            # out_filename = "f"
            log_filename = out_filename[:-4] + ".log"

            cmd = "./RowHammerAttacker" + \
                    " --out " + out_filename + \
                    " --bank 1" + \
                    " --range 0 200" + \
                    " --row_layout VAVAV" + \
                    " --hammers_per_ref_loop " + hammers_per_ref_loop + \
                    " --num_ref_loops 10000" + \
                    " --refs_per_loop 1" + \
                    " --num_dummy_rows " + str(num_dummy_rows) + \
                    " --hammer_dummies_independently " + \
                    " --shift_dummies_after " + \
                    " --dummy_banks 0 1 2 3" + \
                    " --shift_refs 10000" + \
                    " --cascaded_hammer_dummy " + \
                    " --log_phys_scheme 1"
            
            if before:
                cmd += " --hammer_dummies_before "
            if after:
                cmd += " --hammer_dummies_after "
            if trrref_sync:
                cmd += " --trrref_sync --shift_refs " + str(shift_refs)
                    
            """
                    " --append" + \
                    " --trrref_sync " + + \
                    " --shift_refs " + + \
                    " --fake_ref " + + \
                    " --fake_hammer " + + \
                    " --fake_dummy_hammer " + + \
                    " --cascaded_hammer_aggr " + + \
                    " --cascaded_hammer_dummy " + + \
                    " --input_victims " + + \
                    " --input_aggressors " + + \
                    
            """

            os.system("../../../../apps/ResetBoard/full_reset.sh")
            os.system("mkdir -p result")
            print(cmd)
            with open(log_filename, "w") as f:
                    f.write(cmd + "\n")
            os.system(cmd + " >> " + log_filename)
