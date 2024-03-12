import os



os.system("make")


for row_group_pattern in ["RRR-RRR"]:
    
    os.system("sudo ../../ResetBoard/full_reset.sh")
    if row_group_pattern == "R-R":
        num_row_groups = "15"
    elif row_group_pattern == "RRR-RRR":
        num_row_groups = "4"

    out_filename = "./out_" + row_group_pattern + ".txt"
    log_filename = "./out_" + row_group_pattern + ".log"

    cmd = "sudo ./RowScout " + \
        "-b 1 -r 64 --log_phys_scheme 1 " + \
        "-i 1 --append  --range 0 16384 " + \
        "-o " + out_filename + " " + \
        "--row_group_pattern " + row_group_pattern + " " + \
        "--num_row_groups " + num_row_groups

    with open(out_filename, "a") as f:
        f.write("CMD: " + cmd + "\n")

    print("Running: " + cmd)
    os.system(cmd + " > " + log_filename)



