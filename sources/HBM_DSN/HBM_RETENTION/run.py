import argparse
import os

parser = argparse.ArgumentParser(description='HBM_REVENG')

parser.add_argument('-c', '--channels', type=int, nargs='+', help='list of channels to test')
parser.add_argument('-p', '--pseudochannels', type=int, nargs='+', help='list of pseudochannels to test')
parser.add_argument('-b', '--banks', type=int, nargs='+', help='list of banks to test')

args = parser.parse_args()

BASE_CMD = "./Rev -c {channel} -b {banks} -p {pseudo} -s 0 -n 16384 -d {dp} -r 5300"
BASE_CMD = "./Rev -c {channel} -b {banks} -p {pseudo} -s 0 -n 16384 -d {dp} -r 32"
BASE_CMD = "./Rev -c {channel} -b {banks} -p {pseudo} -s 0 -n 16384 -d {dp} -r 128"
BASE_CMD = "./Rev -c {channel} -b {banks} -p {pseudo} -s 0 -n 16384 -d {dp} -r 588"

for channel in args.channels:
    for pc in args.pseudochannels:
        for bank in args.banks:
            for dp in [0x00, 0xff, 0x55, 0xaa]:
                print("Testing channel: {}".format(channel), "Testing pseudochannel: {}".format(pc), "Testing bank: {}".format(bank), "Testing data pattern: {}".format(dp))
                cmd = BASE_CMD.format(channel=channel, banks=bank, pseudo=pc, dp=dp)
                print(cmd)
                os.system(cmd)

# example command line to run this script
# python run.py -c 8 9 10 11 -n 1000
