#include "instruction.h"
#include "prog.h"
#include "platform.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <list>
#include <time.h>
#include <math.h>

using namespace std;

#define TEST 1
#define TEST_ALL 0
#define CHANNEL_ID 8

#define TOTAL_NUM_BANKS 16
#define TOTAL_NUM_ROWS 16*1024 // per bank
#define TOTAL_NUM_COLS 32 // per row
#define MEM_ACCESS_SIZE 256 // per memory access to PC

const int NUM_BYTES_PER_READ = 32;

#define START_ROW 0
#define RANK 0
#define START_BANK 0
#define NUM_RANKS 2
#define TEST_NUM_ROWS 16*1024
#define TEST_NUM_COLS 32
#define NUM_ITERATIONS 5
#define NUM_PATTERNS 4

#define P_A_1  0x55555555
#define P_VICTIM  ~P_A_1
#define P_A_2 P_A_1
#define P_RETENTION 0xFFFFFFFF
#define TIMER_BASE    15151515*50 // 15151515 corresponds to 100 ms
#define T_AGG_ON 	5
#define HAMMER_COUNT 256000

// Stride register ids are fixed and should not be changed
// CASR should always be reg 0
// BASR should always be reg 1
// RASR should always be reg 2
#define CASR 0
#define BASR 1
#define RASR 2

#define BAR 3
#define RAR 4
#define CAR 5

#define NUM_ITERATIONS_REG 6
#define NUM_HAMMER_COUNT_REG 7
#define NUM_COLS_REG 8

#define LOOP_HAMMER 9
#define LOOP_COLS 10

#define PATTERN_REG_V 11
#define START_ROW_REG 12
#define PATTERN_REG_A_1 13
#define PATTERN_REG_A_2 14



/**
 * @return an instruction formed by NOPs
 */
Inst all_nops() {
	return  __pack_mininsts(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_NOP());
}

SoftMCPlatform platform;

uint map_row (uint row) {
	if (row & 0x8) {
	    uint mask = 0x6;
		return row ^ mask;
	} else
	    return row;
}

void SMC_initialize_row(uint channel, uint pattern, uint pc, uint bank, uint row)
  {
    Program p;
    p.add_inst(SMC_SEL_CH(channel, pc), SMC_NOP(), SMC_NOP(), SMC_NOP());
    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(row, RAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(1, CASR));
    p.add_inst(SMC_LI(1, BASR)); // Load 1 into BASR
    p.add_inst(SMC_LI(1, RASR)); // Load 1 into RASR
    p.add_inst(SMC_LI(pattern, PATTERN_REG_V)); // Load A+ row pattern into corresponding register

    // Load the cache line into the wide data register
    for(uint i = 0 ; i < 16 ; i++)
    {
      p.add_inst(SMC_LDWD(PATTERN_REG_V, i));
    }

    // Activate and write to the row
    p.add_inst(SMC_PRE(BAR, 0, 0, pc), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops()); 
    p.add_inst(all_nops()); 

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0, pc), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 32 ; i++)
    {
      p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, pc, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));
    
    p.add_inst(SMC_PRE(BAR, 0, 0, pc), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    p.add_inst(SMC_END());
    
    platform.execute(p);    
  }
  
void retention_test(unsigned int channel, unsigned int rankIdx, unsigned int retentionTime, unsigned int patternRetention, unsigned int startRow, unsigned int startBank) {

	// Stride registers are used to increment the row, bank, column registers without
    // using regular (non-DDR) instructions. This way the DRAM array can be traversed much faster
	Program p;
	
	p.add_inst(SMC_SEL_CH(channel, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP()); // Select target channel 8
    p.add_inst(SMC_LI(1, CASR)); // Load 1 into CASR, as this is how it should be with HBM to traverse all columns
    p.add_inst(SMC_LI(1, BASR)); // Load 1 into BASR
    p.add_inst(SMC_LI(1, RASR)); // Load 1 into RASR
	
    p.add_inst(SMC_LI(patternRetention, PATTERN_REG_V));
    
	p.add_inst(SMC_LI(TEST_NUM_COLS, NUM_COLS_REG));  // indicates the number of columns
	p.add_inst(SMC_LI(retentionTime, 7));
    
	p.add_inst(SMC_LI(startBank, BAR)); // Initialize BAR with START_BANK
	p.add_inst(SMC_LI(startRow, RAR)); // Initialize RAR with agr1
	p.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
	
	for(int i = 0 ; i < 16 ; i++)
		p.add_inst(SMC_LDWD(PATTERN_REG_V,i));

	
	// ---------------- Write PATTERN to row ----------------
		
		p.add_inst(SMC_LI(0,LOOP_COLS));  // Load loop variable  
		p.add_inst(SMC_LI(0, CAR)); 	    // Reset CAR with 0
		
		// PREcharge bank BAR and wait for tRP
		p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
		// ACT & wait for tRCD
		p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
			p.add_label("WR_COL");

			// Write to a whole row, increment CAR per WR
			p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, rankIdx, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
			
			p.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
			// If (LOOP_COLS < 32) than jump to "WR_AGGR_ROW_1_COL" label
			p.add_branch(p.BR_TYPE::BL, LOOP_COLS, NUM_COLS_REG, "WR_COL");
			
		// Wait for t(write-precharge)
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		p.add_inst(all_nops());


	// ------------------------------------------------------------------
	
	
	// PREcharge bank BAR
	p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
	p.add_inst(all_nops());
	p.add_inst(all_nops());
	
	
	// ---------------- Wait for retention time -------------------------
	
		p.add_inst(SMC_SLEEP(retentionTime));
	
	// ------------------------------------------------------------------
	
	
	// --------------------- Read Row ---------------------
		
		p.add_inst(SMC_LI(0,LOOP_COLS));       // Load loop variable  
		p.add_inst(SMC_LI(0, CAR)); 	         // Reset CAR with 0
		p.add_inst(SMC_LI(startRow, RAR)); // We want to read victim row
		
		// PREcharge bank BAR and wait for tRP
		p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
		// ACT & wait for tRCD
		p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
			p.add_label("RD_COL_BEGIN");
			
			p.add_inst(SMC_LI(1, RASR)); // used to add auto-generated instruction
			
			// Read from a whole row, increment CAR per WR
			p.add_inst(SMC_READ(BAR, 0, CAR, 1, rankIdx, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
			
			p.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
			// If (LOOP_COLS < 32) than jump to "WR_COL_BEGIN" label
			p.add_branch(p.BR_TYPE::BL, LOOP_COLS, NUM_COLS_REG, "RD_COL_BEGIN");
			
		// Wait for t(write-precharge)
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
	
	// ------------------------------------------------------------------
	
	p.add_inst(all_nops());
    p.add_inst(all_nops());
    p.add_inst(all_nops());
    p.add_inst(all_nops());
	
    // Tells SoftMC that the user p has ended
    p.add_inst(SMC_END());
    platform.execute(p);
	printf("Program Ready\n");
	//p.pretty_print();
	printf("Sent instructions\n");
	
}

void double_rowhammer_test(unsigned int channel, unsigned int rankIdx, unsigned int startBank, unsigned int hammerCount, unsigned int patternAggressor1, unsigned int patternAggressor2, unsigned int patternVictim, unsigned int agr1, unsigned int agr2, unsigned int victim, unsigned int RAS_scale) {

	// Stride registers are used to increment the row, bank, column registers without
    // using regular (non-DDR) instructions. This way the DRAM array can be traversed much faster
	Program p;
	
	p.add_inst(SMC_SEL_CH(channel, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP()); // Select target channel 8
    p.add_inst(SMC_LI(1, CASR)); // Load 1 into CASR, as this is how it should be with HBM to traverse all columns
    p.add_inst(SMC_LI(1, BASR)); // Load 1 into BASR
    p.add_inst(SMC_LI(1, RASR)); // Load 1 into RASR
	
    p.add_inst(SMC_LI(patternAggressor1, PATTERN_REG_A_1)); // Load A+ row pattern into corresponding register
    p.add_inst(SMC_LI(patternAggressor2, PATTERN_REG_A_2)); // Load A- row pattern into corresponding register
    p.add_inst(SMC_LI(patternVictim, PATTERN_REG_V)); // Load victim row pattern into corresponding register
    
	p.add_inst(SMC_LI(TEST_NUM_COLS, NUM_COLS_REG));  // indicates the number of columns
	p.add_inst(SMC_LI(hammerCount, NUM_HAMMER_COUNT_REG));
    
	p.add_inst(SMC_LI(startBank, BAR)); // Initialize BAR with START_BANK
	p.add_inst(SMC_LI(agr1, RAR)); // Initialize RAR with START_ROW
	p.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
	
	// Load wide data register with A+ row pattern
	for(int i = 0 ; i < 16 ; i++)
		p.add_inst(SMC_LDWD(PATTERN_REG_A_1,i));
	
	
	// ---------------- Write PATTERN to A+ row ----------------
		
		p.add_inst(SMC_LI(0,LOOP_COLS));  // Load loop variable  
		p.add_inst(SMC_LI(0, CAR)); 	    // Reset CAR with 0
		
		// PREcharge bank BAR and wait for tRP
		p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
		// ACT & wait for tRCD
		p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
			p.add_label("WR_AGGR_ROW_1_COL");

			// Write to a whole row, increment CAR per WR
			p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, rankIdx, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
			
			p.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
			// If (LOOP_COLS < 32) than jump to "WR_AGGR_ROW_1_COL" label
			p.add_branch(p.BR_TYPE::BL, LOOP_COLS, NUM_COLS_REG, "WR_AGGR_ROW_1_COL");
			
		// Wait for t(write-precharge)
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		p.add_inst(all_nops());


	// ------------------------------------------------------------------
	
	
	p.add_inst(SMC_LI(victim, RAR)); // We now want to write to the victim row
	// Load wide data register with victim row pattern
	for(int i = 0 ; i < 16 ; i++)
		p.add_inst(SMC_LDWD(PATTERN_REG_V,i));
	
	
	// ---------------- Write PATTERN to Victim row ----------------
		
		p.add_inst(SMC_LI(0,LOOP_COLS));  // Load loop variable  
		p.add_inst(SMC_LI(0, CAR)); 	    // Reset CAR with 0
		
		// PREcharge bank BAR and wait for tRP
		p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
		// ACT & wait for tRCD
		p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
			p.add_label("WR_VICTIM_COL");

			// Write to a whole row, increment CAR per WR
			p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, rankIdx, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
			
			p.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
			// If (LOOP_COLS < 32) than jump to "WR_VICTIM_COL" label
			p.add_branch(p.BR_TYPE::BL, LOOP_COLS, NUM_COLS_REG, "WR_VICTIM_COL");
			
		// Wait for t(write-precharge)
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		p.add_inst(all_nops());

	// ------------------------------------------------------------------
	
	p.add_inst(SMC_LI(agr2, RAR)); // We now want to write to the A- row
	// Load wide data register with A- row pattern
	for(int i = 0 ; i < 16 ; i++)
		p.add_inst(SMC_LDWD(PATTERN_REG_A_2,i));
	
	
	// ---------------- Write PATTERN to A- row ----------------
		
		p.add_inst(SMC_LI(0,LOOP_COLS));  // Load loop variable  
		p.add_inst(SMC_LI(0, CAR)); 	    // Reset CAR with 0
		
		// PREcharge bank BAR and wait for tRP
		p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
		// ACT & wait for tRCD
		p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
			p.add_label("WR_AGGR_ROW_2_COL");

			// Write to a whole row, increment CAR per WR
			p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, rankIdx, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
			
			p.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
			// If (LOOP_COLS < 32) than jump to "WR_COL_BEGIN" label
			p.add_branch(p.BR_TYPE::BL, LOOP_COLS, NUM_COLS_REG, "WR_AGGR_ROW_2_COL");
			
		// Wait for t(write-precharge)
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		p.add_inst(all_nops());

	// ------------------------------------------------------------------
	
	// PREcharge bank BAR
	p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
	p.add_inst(all_nops());
	p.add_inst(all_nops());
	
	// --------------------- Start Hammering ---------------------
		
		p.add_inst(SMC_LI(0,LOOP_HAMMER));  // Load loop variable
		
		p.add_label("HAMMER");
			
			// PRE and ACT A+ row
			
			// PREcharge bank BAR
			p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
			p.add_inst(SMC_LI(agr1, RAR));
			
			// ACT & wait for tRAS
			p.add_inst(SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP());
			p.add_inst(SMC_SLEEP(5));
			if (RAS_scale >= 1)
      			p.add_inst(SMC_SLEEP(5 * RAS_scale));
			
			// PRE and ACT A- row
			
			// PREcharge bank BAR
			p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
			p.add_inst(SMC_LI(agr2, RAR));
			p.add_inst(SMC_ADDI(LOOP_HAMMER, 1, LOOP_HAMMER));
			
			// ACT & wait for tRAS
			p.add_inst(SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP());
			if (RAS_scale >= 1)
				p.add_inst(SMC_SLEEP(5 * (RAS_scale)));
      			
			
		// If (LOOP_HAMMER < hammerCount) than jump to "HAMMER" label
		p.add_branch(p.BR_TYPE::BL, LOOP_HAMMER, NUM_HAMMER_COUNT_REG, "HAMMER");
	
	
	// --------------------- Read Victim Row ---------------------
		
		p.add_inst(SMC_LI(0,LOOP_COLS));       // Load loop variable  
		p.add_inst(SMC_LI(0, CAR)); 	         // Reset CAR with 0
		p.add_inst(SMC_LI(victim, RAR)); // We want to read victim row
		
		// PREcharge bank BAR and wait for tRP
		p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
		// ACT & wait for tRCD
		p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		
			p.add_label("RD_COL_BEGIN");
			
			p.add_inst(SMC_LI(1, RASR)); // used to add auto-generated instruction
			
			// Read from a whole row, increment CAR per WR
			p.add_inst(SMC_READ(BAR, 0, CAR, 1, rankIdx, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
			
			p.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
			// If (LOOP_COLS < 32) than jump to "WR_COL_BEGIN" label
			p.add_branch(p.BR_TYPE::BL, LOOP_COLS, NUM_COLS_REG, "RD_COL_BEGIN");
			
		// Wait for t(write-precharge)
		p.add_inst(all_nops());
		p.add_inst(all_nops());
		p.add_inst(all_nops());
	
	// ------------------------------------------------------------------
	
	p.add_inst(all_nops());
    p.add_inst(all_nops());
    p.add_inst(all_nops());
    p.add_inst(all_nops());
	
    // Tells SoftMC that the user program has ended
    p.add_inst(SMC_END());
	
	platform.execute(p);
	printf("Program Ready\n");
	// p.pretty_print();
	printf("Sent instructions\n");
	
}


int main(int argc, char**argv) {
    
   /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
    *																								  *
    *   In this test, we will run rowhammer attacks to determine the vulnerability of such attacks    *
    *	on different HBM rows across different banks										  	  	  *
	*																							      *
	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	
    int err;
    
    unsigned int testType = (argc > 1)?(atoi(argv[1])):1;
    unsigned int testAllRows = (argc > 2)?(atoi(argv[2])):0;
    unsigned int hammerCount = (argc > 3)?(atoi(argv[3])):256000;
    
	unsigned int numPatterns = (argc > 4)?(atoi(argv[4])):4;
	unsigned int numChannels = (argc > 5)?(atoi(argv[5])):8;
	unsigned int numPseudoChannels = (argc > 6)?(atoi(argv[6])):1;
	unsigned int numBanks = (argc > 7)?(atoi(argv[7])):1;
	unsigned int numRows = (argc > 8)?(atoi(argv[8])):9216;
    
	unsigned int startRow =  START_ROW;	
	unsigned int startBank = START_BANK;	
	unsigned int channel = CHANNEL_ID;
	unsigned int patternAggressor1 = P_A_1;
	unsigned int patternAggressor2 = P_A_2;
	unsigned int patternVictim = P_VICTIM;
	unsigned int patternRetention = P_RETENTION;
	unsigned int retentionTime = TIMER_BASE;		
	unsigned int row_size_in_bytes = TEST_NUM_COLS*64;
	unsigned int* temp_buffer;
	unsigned int numWords = (row_size_in_bytes/4)/2; // with 0s removed
	unsigned int word = 0;
	int offset; // used to remove 0s
	
	unsigned int agr1, agr2, victim;
	
	unsigned int row_errors[numRows];
	for (int i = 0; i < numRows; ++i)
		row_errors[i] = 0;
		
	unsigned int col_errors[TEST_NUM_COLS];
	for (int i = 0; i < TEST_NUM_COLS; ++i)
		col_errors[i] = 0;
		
	unsigned int patterns[5] = {0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xA7FD32E6};
	unsigned int ras_scale_list[10] = {0, 1, 2, 4, 8, 16, 32, 64, 118, 1063};
	
	// used to read victim row data
	// uint64_t* victim_row_buffer = (uint64_t*) malloc(sizeof(char)*row_size_in_bytes);	
	uint64_t victim_row_buffer[row_size_in_bytes/sizeof(uint64_t)];

	// converting to data words and remove 0s (a constraint imposed by how we read from HBM)
	unsigned int* data_words = (unsigned int*) malloc(sizeof(unsigned int)*numWords);
	// total_rows_data
	unsigned int* total_rows_data = (unsigned int*) malloc(sizeof(unsigned int)*numWords*numRows);
	memset(total_rows_data, 0, sizeof(unsigned int)*numWords*numRows);
	
	// What we need to do is count the number of failures in each bit of our memory segment
	// We need one counter for each bit
	unsigned int* error_counters = (unsigned int*) malloc(sizeof(unsigned int)*row_size_in_bytes*8);
	
	// Initialize the platform, opens file descriptors for the board PCI-E interface.
	if((err = platform.init()) != SOFTMC_SUCCESS){
		cerr << "Could not initialize SoftMC Platform: " << err << endl;
	}
	
	for (unsigned int rankIdx = 0; rankIdx < numPseudoChannels; rankIdx++) {
		for (unsigned int ch = 8; ch < (numChannels + 8); ch++) {
			for (unsigned int bank = 0; bank < numBanks; bank++) {
				for (unsigned int dp = 2; dp < 3; dp++) {
					for (unsigned int tAggOn = 0; tAggOn < 10; tAggOn++) {

						ofstream csv_file;
						csv_file.open("rowpress_ch-" + to_string(ch) + "_pch-" + to_string(rankIdx) + "_bank-" + to_string(bank) + "_dp-170" + "_aggon-" + to_string(tAggOn) +  ".csv");
						csv_file << "channel,pseudo_channel,bank,row,column,bit,aggon,data_pattern" << endl;
		
						if (TEST == 2)
							patternVictim = P_RETENTION;
						else
							patternVictim = patterns[dp];
						patternAggressor1 = ~patternVictim;
						patternAggressor2 = ~patternVictim;
						
						for (unsigned int row = 0; row < numRows; ++row) {
							if (testAllRows) {
								startRow = 0;
							} else {
								if (row < (numRows/3))
									startRow = 0;
								else if (row >= (numRows/3) && row < (2*(numRows/3)))
									startRow = (TOTAL_NUM_ROWS/2) - (numRows/3);
								else
									startRow = TOTAL_NUM_ROWS - numRows - 2;
							}
							for (unsigned int iterations = 0; iterations < NUM_ITERATIONS; ++iterations) {
								memset(victim_row_buffer, 0, sizeof(char)*row_size_in_bytes);
								memset(data_words, 0, sizeof(unsigned int)*numWords);
								memset(error_counters, 0, sizeof(unsigned int)*row_size_in_bytes*8);
								// reset the board to hopefully restore the board's state
								platform.reset_fpga();
								
								uint victim2 = map_row(startRow + row - 1);
								agr1 = map_row(startRow + row);
								victim = map_row(startRow + row + 1);
								agr2 = map_row(startRow + row + 2);
								
								for (uint a = 0; a < 18; ++a)
									SMC_initialize_row(ch, patternVictim, rankIdx, bank, map_row(startRow + row + a - 8));
								if (testType == 0) {
								} else if (testType == 1){
									double_rowhammer_test(ch, rankIdx, bank, hammerCount, patternAggressor1, patternAggressor2, patternVictim, agr1, agr2, victim, 0);
								} else if (testType == 2){
									double_rowhammer_test(ch, rankIdx, bank, hammerCount, patternAggressor1, patternAggressor2, patternVictim, agr1, agr2, victim, (ras_scale_list[tAggOn]));
								} else {
									retention_test(ch, rankIdx, retentionTime, patternRetention, agr1, bank);
								}
								
								platform.receiveData(victim_row_buffer, row_size_in_bytes); // read one row each iteration
								
								int buf_index = rankIdx == 0 ? 0 : NUM_BYTES_PER_READ/sizeof(uint64_t);
								int column = 0;

								for (;buf_index < row_size_in_bytes/sizeof(uint64_t);)
								{
									int dword = 0;
									for (int j = 0 ; j < NUM_BYTES_PER_READ/sizeof(uint64_t) ; j++, buf_index++)
									{
										uint64_t curr_word = victim_row_buffer[buf_index];
										// std::cout << std::hex << curr_word << std::dec << std::endl;
										for (int b = 0 ; b < 64 ; b++)
										{
											uint64_t mask = 1ULL << b;
											if ((curr_word & mask) != (0xAAAAAAAAAAAAAAAAULL & mask))
											{
												// cout << "Bitflip at: " << hex << buf_index << " " << b << dec << endl;
												csv_file << channel << "," << rankIdx << "," << bank << "," << row << "," << column << "," << (dword*64 + b) << "," << tAggOn << "," << 170 << endl;
											}
										}
										dword++;
										// print what is expected vs what we got
										// cout << "Expected: " << hex << dp.get_word() << " Got: " << buf[buf_index] << dec << endl;
									}
									column += 1;
									buf_index += NUM_BYTES_PER_READ/sizeof(uint64_t);
								}
							}
						}
						csv_file.close();
					}
				}
			}
		}
	}
    free(error_counters);
    free(total_rows_data);
    free(data_words);
    free(victim_row_buffer);
    
    return 0;

}
