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

using namespace std;

#define USE_SMC_FILE 0

#define NUM_BANKS 16
#define NUM_ROWS 1024*32

#define NUM_ROWS_REG  8
#define NUM_BANKS_REG 11
// Stride register ids are fixed and should not be changed
// CASR should always be reg 0
// BASR should always be reg 1
// RASR should always be reg 2
#define CASR 0
#define BASR 1
#define RASR 2

#define PATTERN_REG 12

#define BAR 7
#define RAR 6
#define CAR 4

#define NUM_COLS_REG 14
#define LOOP_COLS 13

#define TEMP_PATTERN_REG 15

/**
 * @return an instruction formed by NOPs
 */
Inst all_nops()
{
  return  __pack_mininsts(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_NOP());
}

uint32_t wr_pattern;

Program generateProgram()
{

  Program program;
  /* SoftMC programs are formed sequentially by adding
    * instructions to the program one by one. Instructions
    * can be added to the program using add_inst().
    */

  program.add_inst(SMC_LI(NUM_ROWS, NUM_ROWS_REG)); // load NUM_ROWS into NUM_ROWS_REG
  program.add_inst(SMC_LI(NUM_BANKS, NUM_BANKS_REG)); // load NUM_BANKS into NUM_BANKS_REG
  
  // Stride registers are used to increment the row,bank,column registers without
  // using regular (non-DDR) instructions. This way the DRAM array can be traversed much faster

  program.add_inst(SMC_LI(8, CASR)); // Load 8 into CASR since each READ reads 8 columns
  program.add_inst(SMC_LI(1, BASR)); // Load 1 into BASR
  program.add_inst(SMC_LI(1, RASR)); // Load 1 into RASR

  // Initialize PRNG, this test will randomly generate
  // the first data to write into DRAM, it will modify
  // the data by cyclic shifting and multiplying it with itself
  // during runtime (this will be further explained down below).
  srand(time(NULL)+getpid());
  uint16_t rand1 = (uint16_t) rand();
  wr_pattern = rand1 << 16;
  wr_pattern |= ((uint16_t)rand());
  printf("wr_pattern: %x\n", wr_pattern);

  // Load WR data into pattern register whose content
  // will be replicated among all words in the wide register
  // Wide register is a 512-bit register, its content will
  // be written to DRAM with SMC_WR commands.
  program.add_inst(SMC_LI(wr_pattern, PATTERN_REG)); 
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(PATTERN_REG,i));

  program.add_inst(SMC_LI(0, BAR)); // Initialize BAR (bank address register) with 0

  // add_label is used to add a label that can be used as a branch target to the program
  program.add_label("BANK_BEGIN");

  program.add_inst(SMC_LI(0, RAR)); // Initialize RAR with 0
  
  program.add_label("ROW_BEGIN");

  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0

  // Four DRAM commands are issued at the same time
  // to DRAM by SoftMC.
  
  // PREcharge bank BAR and wait for tRP (11 NOPs, 11 SoftMC cycles) 
  program.add_inst(
    SMC_PRE(BAR, 0, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  
  // ACT & wait for tRCD
  program.add_inst(
    SMC_ACT(BAR, 0, RAR, 0, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  // We are now going to loop over all 512-bits in the row
  // Since each row is 2^13 bytes we need to WR 128 times
  program.add_inst(SMC_LI(128,NUM_COLS_REG));  // Load COL_SIZE register
  program.add_inst(SMC_LI(0,LOOP_COLS));   // Load loop variable

  /*
    * Try to write a different pattern to each cache line
    * in DRAM.
    */
  program.add_label("WR_BEGIN");

  // Write to a whole row, increment CAR per WR
  program.add_inst(
    SMC_WRITE(BAR, 0, CAR, 1, 0, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  
  // Reload wide reg with the new data pattern
  program.add_inst(SMC_SRC(PATTERN_REG,PATTERN_REG));
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(PATTERN_REG,i));
  
  program.add_inst(SMC_ADDI(LOOP_COLS,1,LOOP_COLS));
  // add a branch lower instruction which will jump to WR_BEGIN
  // as long as LOOP_COLS<NUM_COLS, to iterate over the whole row
  program.add_branch(program.BR_TYPE::BL,LOOP_COLS,NUM_COLS_REG, "WR_BEGIN");
  // END "WR_BEGIN" LOOP

  // Add more randomness to the PATTERN_REG by multiplying it by 3
  // This happens when a row is written to
  program.add_inst(SMC_MV(PATTERN_REG,TEMP_PATTERN_REG));
  program.add_inst(SMC_ADD(PATTERN_REG,TEMP_PATTERN_REG,PATTERN_REG));
  program.add_inst(SMC_ADD(PATTERN_REG,TEMP_PATTERN_REG,PATTERN_REG));
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(PATTERN_REG,i));
  
  // Wait for t(write-precharge)
  // & precharge the open bank
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(
    SMC_PRE(BAR, 0, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  program.add_inst(SMC_LI(0, CAR)); // reload CAR we are now going to read the whole row
  
  // PRE & wait for tRP
  program.add_inst(
    SMC_PRE(BAR, 0, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  // ACT & wait for tRCD
  program.add_inst(
    SMC_ACT(BAR, 0, RAR, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  // Read from a whole row
  // Essentially this can be imagined as an unrolled read whole row loop
  for(int i = 0 ; i < 128 ; i++){
    program.add_inst(
      SMC_READ(BAR, 0, CAR, 1, 0, 0),
      SMC_NOP(), SMC_NOP(), SMC_NOP()
    );
    program.add_inst(all_nops());
  }
  
  // Wait for t(read-precharge)
  // & precharge the open bank
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(
    SMC_PRE(BAR, 0, 0),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  // Increment row address by one since we are finished with this row
  program.add_inst(SMC_ADDI(RAR,1,RAR));
  program.add_branch(program.BR_TYPE::BL,RAR,NUM_ROWS_REG, "ROW_BEGIN");
  program.add_inst(SMC_ADDI(BAR,1,BAR));
  program.add_branch(program.BR_TYPE::BL,BAR,NUM_BANKS_REG,"BANK_BEGIN");
  
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  program.dump_registers();

  // Tells SoftMC that the user program has ended
  program.add_inst(SMC_END());

  return program;
}

int main()
{
  SoftMCPlatform platform_dimm0((int) 1);

  int err;
  // buffer allocated for reading data from the board
  char* buf_dimm0 = (char*) malloc(sizeof(char)*32*1024);

  // Initialize the platform, opens file descriptors for the board PCI-E interface.
  if((err = platform_dimm0.init()) != SOFTMC_SUCCESS){
      cerr << "Could not initialize SoftMC Platform 0: " << err << endl;
  }


  platform_dimm0.reset_fpga();

  Program p;

  p.add_inst(all_nops());
  p.add_inst(all_nops());
  p.add_inst(all_nops());
  p.add_inst(SMC_END());

  platform_dimm0.execute(p);

  // Read count
  int rc = 0;

  uint32_t pattern = wr_pattern;  
  uint64_t err_count = 0;  
  // Below loop reads continuously from the FPGA board
  // since we know in what order we are issuing our reads
  // we know what physical address they are reading
  platform_dimm0.receiveData(buf_dimm0, 8*1024); // read one row each iteration
}
