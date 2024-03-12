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
#define CHANNEL_ID 8

#define TOTAL_NUM_BANKS 1
#define START_PATTERN 0
#define NUM_PATTERNS 4
#define TOTAL_NUM_ROWS 16*1024 // per bank
#define TOTAL_NUM_COLS 32 // per row
#define MEM_ACCESS_SIZE 256 // per memory access to PC

#define START_ROW 0
#define RANK 0
#define START_BANK 0
#define NUM_RANKS 1
#define TEST_NUM_ROWS 3*3*1024
#define TEST_NUM_COLS 32
#define HAMMER_INCREMENT 512
#define HC_FIRST_RESOLUTION 512

#define P_A_1  0x00000000
#define P_VICTIM  ~P_A_1
#define P_A_2 P_A_1

#define HAMMER_COUNT 256000
#define MAX_HAMMER_COUNT 256000
#define NUM_ITERATIONS 5
#define T_AGG_ON 5

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

void single_rowhammer_test(unsigned int channel, unsigned int rankIdx, unsigned int startBank, unsigned int hammerCount, unsigned int patternAggressor1, unsigned int patternAggressor2, unsigned int patternVictim, unsigned int agr1, unsigned int victim) {

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
	p.add_inst(SMC_LI(agr1, RAR)); // Initialize RAR with agr1
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
	
	// PREcharge bank BAR
	p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
	p.add_inst(all_nops());
	p.add_inst(all_nops());
	
	// --------------------- Start Hammering ---------------------
		
		p.add_inst(SMC_LI(0,LOOP_HAMMER));  // Load loop variable
		
		p.add_label("HAMMER");
			
			// PRE and ACT A row
			p.add_inst(SMC_LI(agr1, RAR));
			
			// PREcharge bank BAR
			p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
			p.add_inst(all_nops());
			p.add_inst(all_nops());
			
			// ACT & wait for tRAS
			p.add_inst(SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
			p.add_inst(SMC_SLEEP(T_AGG_ON));
			
			p.add_inst(SMC_ADDI(LOOP_HAMMER, 1, LOOP_HAMMER));
			
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
	p.pretty_print();
	printf("Sent instructions\n");
	
}
  
  void double_rowhammer_test(unsigned int channel, unsigned int rankIdx, unsigned int startBank, unsigned int hammerCount, unsigned int patternAggressor1, unsigned int patternAggressor2, unsigned int patternVictim, unsigned int agr1, unsigned int agr2, unsigned int victim) {

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
			
			// PRE and ACT A- row
			
			// PREcharge bank BAR
			p.add_inst(SMC_PRE(BAR, 0, 0, rankIdx), SMC_NOP(), SMC_NOP(), SMC_NOP());
			p.add_inst(SMC_LI(agr2, RAR));
			
			// ACT & wait for tRAS
			p.add_inst(SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0, rankIdx), SMC_NOP());
			p.add_inst(SMC_SLEEP(4));
			p.add_inst(SMC_ADDI(LOOP_HAMMER, 1, LOOP_HAMMER));
			
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
	p.pretty_print();
	printf("Sent instructions\n");
	
}

unsigned int map_row (unsigned int row) {
	if (row & 0x8) {
	    uint mask = 0x6;
		return row ^ mask;
	} else
	    return row;
}


int main(int argc, char**argv) {
    
   /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
    *																								  *
    *   In this test, we will run rowhammer attacks to determine the vulnerability of such attacks    *
    *	on different HBM rows across different banks										  	  	  *
	*																							      *
	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	
    int err;
    
    //unsigned int testType = (argc > 1)?(atoi(argv[1])):1;
    unsigned int testAllRows = (argc > 1)?(atoi(argv[1])):0;
    //unsigned int hammerCount = (argc > 3)?(atoi(argv[3])):256000;
    
	unsigned int numPatterns = (argc > 2)?(atoi(argv[2])):4;
	unsigned int numChannels = (argc > 3)?(atoi(argv[3])):8;
	unsigned int numPseudoChannels = (argc > 4)?(atoi(argv[4])):1;
	unsigned int numBanks = (argc > 5)?(atoi(argv[5])):1;
	unsigned int numRows = (argc > 6)?(atoi(argv[6])):9216;
	
	unsigned int startRow =  START_ROW;	
	unsigned int startBank = START_BANK;
	unsigned int channel = CHANNEL_ID;	
	unsigned int patternAggressor1 = P_A_1;
	unsigned int patternAggressor2 = P_A_2;
	unsigned int patternVictim = P_VICTIM;
	unsigned int hammerCount = HAMMER_COUNT;
	unsigned int upper_hammerCount;		
	unsigned int hcFirst = MAX_HAMMER_COUNT;
	unsigned int row_size_in_bytes = TEST_NUM_COLS*64;
	unsigned int* temp_buffer;
	unsigned int numWords = (row_size_in_bytes/4)/2; // with 0s removed
	unsigned int word = 0;
	int offset; // used to remove 0s
	
	unsigned int patterns[5] = {0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0xA7FD32E6};
	std::string pattern_names[5] = {"00", "FF", "55", "AA", "random"};
	
	unsigned int agr1, agr2, victim;
	
	unsigned int row_errors[numRows];
	for (int i = 0; i < numRows; ++i)
		row_errors[i] = 0;
		
	unsigned int row_hcFirst[numRows];
	for (int i = 0; i < numRows; ++i)
		row_hcFirst[i] = 0;
	
	// used to read victim row data
	char* victim_row_buffer = (char*) malloc(sizeof(char)*row_size_in_bytes);	
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
				for (unsigned int dp = 0; dp < numPatterns; dp++) {
					patternVictim = patterns[dp];
					patternAggressor1 = ~patternVictim;
					patternAggressor2 = ~patternVictim;
					for (unsigned int row = 0; row < numRows; ++row) {
						hcFirst = MAX_HAMMER_COUNT;
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
							hammerCount = MAX_HAMMER_COUNT;
							upper_hammerCount = MAX_HAMMER_COUNT;
							while (true) {
								row_errors[row] = 0;
								memset(victim_row_buffer, 0, sizeof(char)*row_size_in_bytes);
								memset(data_words, 0, sizeof(unsigned int)*numWords);
								memset(error_counters, 0, sizeof(unsigned int)*row_size_in_bytes*8);
								// reset the board to hopefully restore the board's state
								platform.reset_fpga();
								
								agr1 = map_row(startRow + row);
								victim = map_row(startRow + row + 1);
								agr2 = map_row(startRow + row + 2);
								
								if (TEST == 0) 
									single_rowhammer_test(ch, rankIdx, bank, hammerCount, patternAggressor1, patternAggressor2, patternVictim, agr1, victim);
								else
									double_rowhammer_test(ch, rankIdx, bank, hammerCount, patternAggressor1, patternAggressor2, patternVictim, agr1, agr2, victim);
								
								platform.receiveData(victim_row_buffer, row_size_in_bytes); // receive data in bytes
								temp_buffer = (unsigned int*) victim_row_buffer; // convert to data words
								
								// Get rid of the 0s read because of how we designed HBM Bender
								// based on whether we access PC0 or PC1
								offset = (rankIdx == 0) ? -8 : 0;
								for (unsigned int i = 0; i < numWords; ++i) {
									if (i%8 == 0)
										offset += 8;
									data_words[i] = temp_buffer[i + offset];
									total_rows_data[row*numWords + i] = data_words[i];
									word = total_rows_data[row*numWords + i];
									
									// Count the number of errors occured for each HBM cell within a word
									for (unsigned int j = 0; j < 32; ++j) {
										if (((word >> j) & 0x00000001) != ((patternVictim >> j) & 0x00000001))
											error_counters[32*i + j]++;
									}
								}
								
								for (unsigned int i = 0; i < row_size_in_bytes*8; ++i) {
									if (error_counters[i] == 1)
										row_errors[row]++;
								}
								
								if (row_errors[row] >= 1) {
									upper_hammerCount = hammerCount;
									hammerCount = hammerCount / 2;
								} else {
									if (hammerCount == MAX_HAMMER_COUNT)
										break;
									hammerCount = (hammerCount + upper_hammerCount) / 2;
								} 
								
								if (std::abs((int)upper_hammerCount - (int)hammerCount) <= HC_FIRST_RESOLUTION) {
									break;
								}	
							}
							if (hammerCount < hcFirst)
								hcFirst = hammerCount;
						}
						row_hcFirst[row] = hcFirst;
					}
					
					char buf1[80];
					
					snprintf(buf1, 80, "hcfirst_CH_%d_PC_%d_bank_%d_pattern_%s.txt", ch, rankIdx, bank, pattern_names[dp].c_str());
					
					FILE *file1;
						file1 = fopen(buf1,"w");
						for (unsigned int i = 0; i < numRows; ++i)
							fprintf(file1, "%d\n", row_hcFirst[i]);
					fclose(file1);
				
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
