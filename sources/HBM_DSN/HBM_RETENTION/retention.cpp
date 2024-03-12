#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>

#include "instruction.h"
#include "platform.h"
#include "prog.h"

using namespace std;
namespace po = boost::program_options;

ofstream csv_file;

SoftMCPlatform platform;

class DataPattern
{
private:
  uint8_t m_byte;

public:
  DataPattern(const uint8_t byte) : m_byte(byte) {}
  DataPattern(const DataPattern& dp) : m_byte(dp.m_byte) {}

  uint8_t get_byte() const { 
    return m_byte; 
  }

  uint16_t get_sword() const {
    return (m_byte << 8) | m_byte;
  }

  uint32_t get_word() const {
    return (m_byte << 24) | (m_byte << 16) | (m_byte << 8) | m_byte;
  }

  uint64_t get_dword() const {
    uint64_t word_value = get_word();
    return ((word_value << 32) | word_value);
  }

  void invert() {
    m_byte = ~m_byte;
  }

};

const int NUM_PC = 2;
const int NUM_COLS = 32;
const int NUM_BYTES_PER_READ = 32;
const int ROW_SIZE_IN_BYTES = NUM_COLS * NUM_BYTES_PER_READ * NUM_PC;
const int NUM_SEARCH_STEPS = 20;
const uint64_t HBM_PERIOD_PS = 1667;

uint64_t ms_to_fpga_cycle(const uint64_t ms)
{
  return (ms * 1000 * 1000 * 1000ULL) / HBM_PERIOD_PS / 4;
}

int bulk_retention_test_many_rows (const int channel, const int bank, 
    const int pseudo_channel, const int start_row, const int num_rows, 
    const int retention_time, const DataPattern dp)
{
  const int CASR = 0;
  const int BASR = 1;
  const int RASR = 2;

  const int CAR = 3;
  const int BAR = 4;
  const int RAR = 5;

  const int PATTERN_REG = 6;

  const int END_ROW_REG = 7;

  #define all_nops() SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_NOP()

  Program program;

  // set stride register values
  program.add_inst(SMC_LI(1, CASR));
  program.add_inst(SMC_LI(0, BASR)); 
  program.add_inst(SMC_LI(1, RASR)); 

  // load wdata reg with the data pattern
  program.add_inst(SMC_LI(dp.get_word(), PATTERN_REG));
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(PATTERN_REG,i));

  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
  program.add_inst(SMC_LI(bank, BAR)); // Initialize BAR (bank address register) with 0
  program.add_inst(SMC_LI(start_row, RAR)); // Initialize RAR with 0
  program.add_inst(SMC_LI(start_row + num_rows, END_ROW_REG)); 

  program.add_inst(SMC_SEL_CH(channel, pseudo_channel), SMC_NOP(), SMC_NOP(), SMC_NOP());
  program.add_inst(SMC_SLEEP(10));

  program.add_label("WRITE_BEGIN");

  // PREcharge bank BAR and wait for tRP (11 NOPs, 11 SoftMC cycles) 
  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  
  // ACT & wait for tRCD
  program.add_inst(
    SMC_ACT(BAR, 0, RAR, 1, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  for (int i = 0 ; i < NUM_COLS ; i++)
  {
    program.add_inst(
      SMC_WRITE(BAR, 0, CAR, 1, pseudo_channel, 0),
      SMC_NOP(), SMC_NOP(), SMC_NOP()
    );
    program.add_inst(all_nops());
  }

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  program.add_branch(program.BR_TYPE::BL, RAR, END_ROW_REG, "WRITE_BEGIN");

  program.add_inst(SMC_SLEEP(ms_to_fpga_cycle(retention_time)));

  program.add_inst(SMC_LI(start_row, RAR)); // Initialize RAR with 0

  program.add_label("READ_BEGIN");
 
  // PREcharge bank BAR and wait for tRP (11 NOPs, 11 SoftMC cycles) 
  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  
  // ACT & wait for tRCD
  program.add_inst(
    SMC_ACT(BAR, 0, RAR, 1, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  for (int i = 0 ; i < NUM_COLS ; i++)
  {
    program.add_inst(
      SMC_READ(BAR, 0, CAR, 1, pseudo_channel, 0),
      SMC_NOP(), SMC_NOP(), SMC_NOP()
    );
    program.add_inst(all_nops());
  }

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
 
  program.add_branch(program.BR_TYPE::BL, RAR, END_ROW_REG, "READ_BEGIN");

  program.add_inst(SMC_END());

  platform.execute(program);
  cout << "Executed\n" << endl;

  uint64_t buf[ROW_SIZE_IN_BYTES/sizeof(uint64_t)];
  int bitflips = 0;

  

  for (int i = start_row ; i < start_row + num_rows ; i++)
  {
    platform.receiveData(buf, ROW_SIZE_IN_BYTES); // read one row each iteration
    cout << "READ: " << i << endl;
    
    int buf_index = pseudo_channel == 0 ? 0 : NUM_BYTES_PER_READ/sizeof(uint64_t);
    int column = 0;

    for (;buf_index < ROW_SIZE_IN_BYTES/sizeof(uint64_t);)
    {
      int dword = 0;
      for (int j = 0 ; j < NUM_BYTES_PER_READ/sizeof(uint64_t) ; j++, buf_index++)
      {
        uint64_t curr_word = buf[buf_index];
        for (int b = 0 ; b < 64 ; b++)
        {
          uint64_t mask = 1ULL << b;
          if ((curr_word & mask) != (dp.get_dword() & mask))
          {
            // cout << "Bitflip at: " << hex << buf_index << " " << b << dec << endl;
            bitflips++;
            csv_file << channel << "," << pseudo_channel << "," << bank << "," << i << "," << column << "," << (dword*64 + b) << "," << retention_time << "," << ((int) dp.get_byte()) << endl;
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

  return bitflips;
}


int main (int argc, char *argv[])
{
  int channel, pseudo_channel, bank, start_row, num_rows, retention_time, bulk_rows;
  string data_pattern_hex;

  // create a CSV file where we will store bitflip locations
  // columns: channel, pseudo_channel, bank, row, column, bit, retention_time_in_ms, data_pattern
  // Declare the supported options
  po::options_description desc("Allowed options");
  desc.add_options()
  ("help,h", "produce help message")
  ("ch,c", po::value<int>(&channel)->required(), "HBM channel to test")
  ("bank,b", po::value<int>(&bank)->required(), "HBM bank to test")
  ("pc,p", po::value<int>(&pseudo_channel)->required(), "HBM pseudo channel to test")
  ("startrow,s", po::value<int>(&start_row)->required(), "Start from this row")
  ("numrows,n", po::value<int>(&num_rows)->required(), "Test this many rows")
  ("datapattern,d", po::value<std::string>(&data_pattern_hex)->required(), "Data pattern (byte) in hex format: e.g., 0x55")
  ("bulkrows,t", po::value<int>(&bulk_rows)->required(), "Test this many rows *at a time*")
  ("retentiontime,r", po::value<int>(&retention_time), "Retention time to test (in ms) for test type 1");
  
  // Parse the command line arguments
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    // Handle required options explicitly
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 1;
    }

    po::notify(vm);
  } catch (po::required_option& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << desc << std::endl;
    return 1;
  }

  // convert data_pattern_hex into a byte
  uint8_t data_pattern_byte = static_cast<uint8_t>(std::stoi(data_pattern_hex, nullptr, 16));
  
  // Create a data pattern object
  DataPattern dp(data_pattern_byte);

  csv_file.open("retention_ch-" + to_string(channel) + "_pch-" + to_string(pseudo_channel) + "_bank-" + to_string(bank) + "_dp-" + data_pattern_hex + "_ms-" + to_string(retention_time) +  ".csv");
  csv_file << "channel,pseudo_channel,bank,row,column,bit,retention_time_in_ms,data_pattern" << endl;

  // Initialize the platform, opens file descriptors for the board PCI-E interface.
  if(platform.init() != SOFTMC_SUCCESS){
      cerr << "Could not initialize SoftMC Platform" << endl;
  }
  platform.reset_fpga();

  for (int i = start_row ; i < start_row + num_rows ; i+=bulk_rows) // test 8 rows at a time
  {
    bulk_retention_test_many_rows (channel, bank, pseudo_channel, i, bulk_rows, retention_time, dp);
    cout << "Tested rows: " << i << " to " << i+bulk_rows << endl;
  }
}
