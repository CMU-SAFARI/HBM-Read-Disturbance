#!/bin/bash
PROJECT_ROOT_DIR=""

# exit if project root directory is not set
if [ -z "$PROJECT_ROOT_DIR" ]
then
    echo "Please set PROJECT_ROOT_DIR variable in this script"
    exit 1
fi

# Reset infra, just in case
/bin/bash $PROJECT_ROOT_DIR/sources/apps/ResetBoard/full_reset.sh

# RowHammer experiments
cd $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RH
make
./RH 1 0 256000 4 8 2 3 3072
cd ../HBM_HCF
make
./HCF 0 4 8 2 3 3072

# Intra-subarray spatial variation experiments
cd ../HBM_RH
make
./RH 1 1 256000 4 3 1 1 16384

# RowPress experiments
cd ../HBM_ROWPRESS
make
./ROWPRESS 0 1 3 1 1 384

cd ../HBM_RP_Bitflips
make
./ROWPRESS 2 0 150000 4 3 1 1 384

# Retention experiments
cd ../HBM_RETENTION
make
channels="8 9 10 11 12 13 14 15"

for channel in $channels
do
    ./Rev -c $channel -b 0 -p 0 -s 0 -n 16384 -d 0xAA -r 32 -t 8
done

for channel in $channels
do
    ./Rev -c $channel -b 0 -p 0 -s 0 -n 16384 -d 0xAA -r 1170 -t 32
done

for channel in $channels
do
    ./Rev -c $channel -b 0 -p 0 -s 0 -n 16384 -d 0xAA -r 10530 -t 128
done

# HCnth experiments
cd ../HBM_HCN
make
./HCN -c 9 -b 0 -p 0 -s 32 -n 32 -d 0xFF -t 11
./HCN -c 14 -b 0 -p 0 -s 32 -n 32 -d 0xFF -t 11
./HCN -c 9 -b 0 -p 0 -s 32 -n 32 -d 0xAA -t 11
./HCN -c 14 -b 0 -p 0 -s 32 -n 32 -d 0xAA -t 11
./HCN -c 9 -b 0 -p 0 -s 32 -n 32 -d 0x55 -t 11
./HCN -c 14 -b 0 -p 0 -s 32 -n 32 -d 0x55 -t 11
./HCN -c 9 -b 0 -p 0 -s 32 -n 32 -d 0x00 -t 11
./HCN -c 14 -b 0 -p 0 -s 32 -n 32 -d 0x00 -t 11

./HCN -c 9 -b 0 -p 0 -s 8224 -n 32 -d 0xFF -t 11
./HCN -c 14 -b 0 -p 0 -s 8224 -n 32 -d 0xFF -t 11
./HCN -c 9 -b 0 -p 0 -s 8224 -n 32 -d 0xAA -t 11
./HCN -c 14 -b 0 -p 0 -s 8224 -n 32 -d 0xAA -t 11
./HCN -c 9 -b 0 -p 0 -s 8224 -n 32 -d 0x55 -t 11
./HCN -c 14 -b 0 -p 0 -s 8224 -n 32 -d 0x55 -t 11
./HCN -c 9 -b 0 -p 0 -s 8224 -n 32 -d 0x00 -t 11
./HCN -c 14 -b 0 -p 0 -s 8224 -n 32 -d 0x00 -t 11

./HCN -c 9 -b 0 -p 0 -s 16320 -n 32 -d 0xFF -t 11
./HCN -c 14 -b 0 -p 0 -s 16320 -n 32 -d 0xFF -t 11
./HCN -c 9 -b 0 -p 0 -s 16320 -n 32 -d 0xAA -t 11
./HCN -c 14 -b 0 -p 0 -s 16320 -n 32 -d 0xAA -t 11
./HCN -c 9 -b 0 -p 0 -s 16320 -n 32 -d 0x55 -t 11
./HCN -c 14 -b 0 -p 0 -s 16320 -n 32 -d 0x55 -t 11
./HCN -c 9 -b 0 -p 0 -s 16320 -n 32 -d 0x00 -t 11
./HCN -c 14 -b 0 -p 0 -s 16320 -n 32 -d 0x00 -t 11

# UTRR experiments
cd ../HBM_UTRR/U-TRR/RowHammerAttacker
python3 run_tests.py

# copy data back to fast_forward_data directory
mkdir -p fast_forward_data/safari-fpga56-new
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RH/*.csv fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RH/*.txt fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_HCF/*.csv fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_HCF/*.txt fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_ROWPRESS/*.csv fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_ROWPRESS/*.txt fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RP_Bitflips/*.csv fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RP_Bitflips/*.txt fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RETENTION/*.csv fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_RETENTION/*.txt fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_HCN/*.csv fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_HCN/*.txt fast_forward_data/safari-fpga56-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_UTRR/U-TRR/RowHammerAttacker/result/*.log fast_forward_data/utrr_results-new/
cp $PROJECT_ROOT_DIR/sources/HBM_DSN/HBM_UTRR/U-TRR/RowHammerAttacker/result/*.txt fast_forward_data/utrr_results-new/
