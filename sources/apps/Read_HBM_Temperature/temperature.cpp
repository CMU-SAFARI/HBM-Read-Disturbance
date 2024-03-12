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

int main()
{
  SoftMCPlatform platform(false);
  int err;
  uint32_t wr_pattern;

  // Initialize the platform, opens file descriptors for the board PCI-E interface.
  if((err = platform.init()) != SOFTMC_SUCCESS){
      cerr << "Could not initialize SoftMC Platform: " << err << endl;
  }
  // reset the board to hopefully restore the board's state
  platform.reset_fpga();

  // open a txt file
  ofstream myfile;
  myfile.open ("record_temperature_16_jul.txt");
  while (true)
  {
    platform.return_HBM_temperature();
    myfile << platform.return_HBM_temperature() << endl;
    sleep(5);
  }
}
