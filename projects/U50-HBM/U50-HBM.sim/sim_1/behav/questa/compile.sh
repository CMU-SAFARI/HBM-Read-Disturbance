#!/bin/bash -f
# ****************************************************************************
# Vivado (TM) v2020.2.1 (64-bit)
#
# Filename    : compile.sh
# Simulator   : Mentor Graphics Questa Advanced Simulator
# Description : Script for compiling the simulation design source files
#
# Generated by Vivado on Wed Aug 31 17:31:20 CEST 2022
# SW Build 3080587 on Fri Dec 11 14:53:26 MST 2020
#
# Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
#
# usage: compile.sh
#
# ****************************************************************************
bin_path="/usr/pack/questa-2020.1-kgf/questasim/bin"
set -Eeuo pipefail
source sim_tb_top_compile.do 2>&1 | tee compile.log

