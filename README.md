# DSN 2024 Artifact for Read Disturbance in High Bandwidth Memory: A Detailed Experimental Study on HBM2 DRAM Chips

### Paper Abstract

We experimentally demonstrate the effects of read disturbance (RowHammer and RowPress) and uncover the inner workings of undocumented read disturbance defense mechanisms in High Bandwidth Memory (HBM). Detailed characterization of six real HBM2 DRAM chips shows that (1) the read disturbance vulnerability significantly varies between different HBM2 chips and between different components (e.g., 3D-stacked channels) inside a chip. (2) The DRAM rows at the end and in the middle of a bank are more resilient to read disturbance. (3) Fewer additional activations are sufficient to induce more read disturbance bitflips in a DRAM row if the row exhibits the first bitflip at a relatively high activation count. (4) A modern HBM2 chip implements undocumented read disturbance defenses that track potential aggressor rows based on how many times they are activated. We describe how our findings could be leveraged to develop more powerful read disturbance attacks and more efficient defense mechanisms.

### Artifact Abstract

Our artifact contains the data, source code, and scripts needed to reproduce our results, including all figures in the paper. We provide original characterization data from our real-chip characterization and source code of the DRAM Bender programs used to perform the characterization. We provide Python scripts and Jupyter Notebooks to analyze and plot the results.

### Artifact Prerequisites

**Hardware requirements** 

To reproduce our real-DRAM characterization results (figures) using the provided raw data from our experiments, the reader needs a similar hardware setup to the one described below (and the one shown in Figure 2 in the paper).

* An x86 machine with PCIe 3.0 x16 slot
* HBM-based FPGA development board supported by DRAM Bender (e.g., Xilinx Alveo U50)
* Approximately 100 GiB disk space

**Software requirements**

* Ubuntu 20.04 (or similar) Linux
* GNU Make
* C++17 compiler (tested with GCC 9)
* Python 3.8.10+
* Python packages: pandas, scipy, matplotlib, and seaborn
* DRAM Bender
* Boost 1.71+
* Xilinx Vivado 2020.2

## Repository File Structure

```
.
+-- fast_forward_data/                  # Experimental characterization results
+-- prebuilt/                           # Prebuilt DRAM Bender bitstream for the U50 board
|   +-- .../           
+-- projects/                           # Vivado project for the U50 board
+-- sources/                       
        +-- HBM_DSN/                    # DRAM Bender programs used for the characterization study
        +-- api/                        # DRAM Bender software API code
        +-- apps/                       # Example DRAM Bender applications
        +-- hdl/                        # DRAM Bender's Verilog sources
        +-- xdma_driver/                # Xilinx PCIe driver sources
+-- create_figures.ipynb                # Jupyter notebook to create all figures in the paper
+-- create_figures_with_new_data.ipynb  # Create figures with new data from Chip 2
+-- run_all_experiments.sh              # Script that runs all characterization experiments
+-- simple_test.sh                      # Simple test to verify correct installation
+-- README.md                           # This file
```

## Artifact Usage

We describe two methods of using our artifact: 1) remote access, 2) personal computer. First, the artifact evaluator remotely accesses (anonymously) our infrastructure (an x86 machine with one HBM2-based DRAM Bender setup) during the artifact evaluation process to reproduce our results. Second, the artifact evaluator reproduces the results using their own HBM2-based DRAM Bender setup. We strongly recommend that the evaluator uses the first method. Please contact us through HotCRP and/or the artifact evaluation (AE) committee for details. 

### Remote access

SSH into safari-fpga56 using the instructions provided via AE committee or HotCRP.

**Reproducing figures from existing characterization data.** All experimental data presented in the paper is in `fast_forward_data` directory. We provide `create_figures.ipynb` Jupyter notebook that the evaluator can use to replot all figures from existing data.

**Reproducing all characterization data for Chip 2.** This machine is connected to a DRAM Bender setup that contains Chip 2. 

One artifact evaluator at a time can run all characterization experiments described in the paper for Chip 2. 

1) Run `./simple_test.sh` to verify that the infrastructure works correctly. A successful run outputs `0 out of 2147483648 bytes have errors, last pattern: <random_value>` on the last line.
2) Run `./run_all_experiments.sh` to perform all characterization experiments. We expect this to take approximately *3 weeks*. When the script successfully finishes, all reproduced characterization data gets copied to `fast_forward_data/safari-fpga56-new`.
3) Use the `create_figures_with_new_data.ipynb` Jupyter notebook to reproduce Figures 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, and 15 in the paper, substituting existing characterization results for Chip 2 with the newly-generated characterization results. 

### Personal computer

Please follow the steps below in your system that closely resembles the one described earlier in this file (in Hardware Requirements) to install DRAM Bender.

#### Installation Guide:

**Installing the Xilinx XDMA driver.** Host computer communicates with the board using a PCIe bus. You need to have [Xilinx XDMA](https://www.xilinx.com/support/answers/65444.html) driver installed on your system to enable the communication with the FPGA via the PCIe bus, we provide a modified version of the Xilinx XDMA driver in our repo under sources/xdma-driver. Do not forget to attach your FPGA board to a PCIe slot.

If you encounter build errors during compilation of the XDMA driver it is likely due to a kernel version mismatch. You can try downloading the newest version of the XDMA drivers from [Xilinx's Github repo](https://github.com/Xilinx/dma_ip_drivers/tree/master/XDMA/linux-kernel).

You may refer to the readme file under sources/xdma-driver for detailed information on installing the driver.

Please load the driver with the option enable_credit_mp set to "1": `insmod ../driver/xdma.ko enable_credit_mp=1` (or just run the `load_driver.sh` script). Otherwise DRAM Bender won't be able to send and receive data correctly to the FPGA.

**Creating and Downloading the DRAM Bender bitfile to the FPGA.** You will find all that you need inside "prebuilt" folder.

1. You need to download the bitfile (`XCU50_NA_HBM_NA_NA.bit`) into your FPGA using an appropriate tool (we recommend Xilinx Vivado 2020.2 for the Alveo U50 board).
    * You can also use the provided shell script `program_FPGA.sh` to program the board with one of the prebuilt bitfiles. You need to define VIVADO_EXEC variable before running the script `export VIVADO_EXEC=/path/to/vivado/executable`.
2. Reboot the computer once you program the FPGA board.
3. Load the Xilinx XDMA driver using the script provided in `sources/xdma-driver` directory `sudo ./load_driver.sh`.

**Installing Boost.**

This branch of the repo requires some headers in boost lib to be present. You may download and build boost-lib from [here](https://www.boost.org/users/history/version_1_65_0.html) or if you are using ubuntu you can install it using `apt install libboost-all-dev`. 

DRAM Bender programs under source/ assume headers in boost lib to be present under project directory. Assuming you have DRAM Bender setup in `/home/yourname/DRAMBender`, headers from boost library should be under `/home/yourname/DRAMBender/boost-lib`. You can also modify Makefiles under source/apps/ if you want to install boost library somewhere else.

If you have installed ubuntu using `apt install libboost-all-dev`, you only need to make sure that you are linking your executables with libboost `-lboost`.

#### Running experiments

**Reproducing figures from existing characterization data.** All experimental data presented in the paper is in `fast_forward_data` directory. We provide `create_figures.ipynb` Jupyter notebook that the evaluator can use to replot all figures from existing data.

**Running experiments on your HBM2 chip.** 

You could run all characterization experiments described in the paper on your HBM2 chip. 

1) Run `./simple_test.sh` to verify that the infrastructure works correctly. A successful run outputs `0 out of 2147483648 bytes have errors, last pattern: <random_value>` on the last line.
2) Run `./run_all_experiments.sh` to perform all characterization experiments. We expect this to take approximately *3 weeks*. When the script successfully finishes, all reproduced characterization data gets copied to `fast_forward_data/safari-fpga56-new`.
3) Use the `create_figures_with_new_data.ipynb` Jupyter notebook to reproduce Figures 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, and 15 in the paper, substituting existing characterization results for Chip 2 with the newly-generated characterization results. 

