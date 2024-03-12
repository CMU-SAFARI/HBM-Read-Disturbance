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


unsigned int map_row (unsigned int row) {
	if (row & 0x8) {
    uint mask = 0x6;
		return row ^ mask;
	} else
    return row;
}

#define all_nops() SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_NOP()

void initialize_rows (const int channel, const int pseudo_channel, const int bank, const int start_row, const int end_row, const DataPattern dp)
{
  const int CASR = 0;
  const int BASR = 1;
  const int RASR = 2;

  const int CAR = 3;
  const int BAR = 4;
  const int RAR = 5;

  const int PATTERN_REG = 6;

  const int END_ROW_REG = 8;

  Program program;

  // set stride register values
  program.add_inst(SMC_LI(1, CASR));
  program.add_inst(SMC_LI(0, BASR)); 
  program.add_inst(SMC_LI(1, RASR)); 

  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
  program.add_inst(SMC_LI(bank, BAR)); // Initialize BAR (bank address register) with 0
  program.add_inst(SMC_LI(start_row, RAR)); // Initialize RAR with 0
  
  program.add_inst(SMC_LI(end_row, END_ROW_REG));

  // load wdata reg with the data pattern
  program.add_inst(SMC_LI(dp.get_word(), PATTERN_REG));
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(PATTERN_REG,i));

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

  program.add_inst(SMC_END());

  platform.execute(program);

}

void test_hcn (const int channel, const int bank, const int pseudo_channel, const int aggr1_row, const int victim_row, const int aggr2_row, const DataPattern aggr_dp, const DataPattern vict_dp, const int hc)
{
  const int CASR = 0;
  const int BASR = 1;
  const int RASR = 2;

  const int CAR = 3;
  const int BAR = 4;
  const int RAR_V = 5;
  const int RAR_A1 = 6;
  const int RAR_A2 = 7;

  const int AGGR_PATTERN_REG = 9;
  const int VICTIM_PATTERN_REG = 10;

  const int HAMMER_COUNT_REG = 11;
  const int HAMMER_COUNT_LIMIT_REG = 12;

  Program program;

  // set stride register values
  program.add_inst(SMC_LI(1, CASR));
  program.add_inst(SMC_LI(0, BASR)); 
  program.add_inst(SMC_LI(1, RASR)); 

  program.add_inst(SMC_LI(hc, HAMMER_COUNT_LIMIT_REG)); 
  program.add_inst(SMC_LI(0, HAMMER_COUNT_REG)); 


  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
  program.add_inst(SMC_LI(bank, BAR)); // Initialize BAR (bank address register) with bank
  program.add_inst(SMC_LI(victim_row, RAR_V)); 
  program.add_inst(SMC_LI(aggr1_row, RAR_A1)); 
  program.add_inst(SMC_LI(aggr2_row, RAR_A2)); 

  program.add_inst(SMC_SEL_CH(channel, pseudo_channel), SMC_NOP(), SMC_NOP(), SMC_NOP());
  program.add_inst(SMC_SLEEP(10));
  // initialize the aggressor row

  // load wdata reg with the data pattern
  program.add_inst(SMC_LI(aggr_dp.get_word(), AGGR_PATTERN_REG));
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(AGGR_PATTERN_REG,i));

  // initialize aggr1 and aggr2

  // ============================================================================
  // ===========================      AGGR1      ================================
  // ============================================================================
  
  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
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
    SMC_ACT(BAR, 0, RAR_A1, 0, pseudo_channel),
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

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  // ============================================================================
  // ===========================      AGGR2      ================================
  // ============================================================================
  
  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
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
    SMC_ACT(BAR, 0, RAR_A2, 0, pseudo_channel),
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

  // ============================================================================
  // ===========================      VICTIM     ================================
  // ============================================================================
  
  // load wdata reg with the data pattern
  program.add_inst(SMC_LI(vict_dp.get_word(), VICTIM_PATTERN_REG));
  for(int i = 0 ; i < 16 ; i++)
    program.add_inst(SMC_LDWD(VICTIM_PATTERN_REG,i));

  program.add_inst(SMC_LI(0, CAR)); // Initialize CAR with 0
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
    SMC_ACT(BAR, 0, RAR_V, 0, pseudo_channel),
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

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  // ============================================================================
  // ===========================      HAMMER     ================================
  // ============================================================================

  program.add_label("HAMMER");

  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  program.add_inst(
    SMC_ACT(BAR, 0, RAR_A1, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops()); // 1.666 * 4
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops()); // 33.32 ns (by precharge)
  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );
  program.add_inst(all_nops()); // 1.666 * 4
  program.add_inst(all_nops()); 
  program.add_inst(SMC_ADDI(HAMMER_COUNT_REG, 1, HAMMER_COUNT_REG));
  program.add_inst(
    SMC_ACT(BAR, 0, RAR_A2, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  program.add_branch(program.BR_TYPE::BL, HAMMER_COUNT_REG, HAMMER_COUNT_LIMIT_REG, "HAMMER");

  // ============================================================================
  // ===========================   READ VICTIM   ================================
  // ============================================================================

  program.add_inst(
    SMC_PRE(BAR, 0, 0, pseudo_channel),
    SMC_NOP(), SMC_NOP(), SMC_NOP()
  );

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());
  
  program.add_inst(
    SMC_ACT(BAR, 0, RAR_V, 0, pseudo_channel),
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

  program.add_inst(all_nops());
  program.add_inst(all_nops());
  program.add_inst(all_nops());

  program.add_inst(SMC_END());

  platform.execute(program);

}

// Initialize 8 rows surrounding victim, hammer victim and read victim to @param buf
void initialize_hammer_read (int channel, int pseudo_channel, int bank, int victim_row, DataPattern dp_aggr, DataPattern dp_victim, int hc, uint64_t* buf)
{
  // initialize the rows first
  int init_start = max(victim_row - 8, 0);
  int init_end = min(victim_row + 8, 16*1024);

  for (int init_r = init_start ; init_r < init_end ; init_r++)
  {
    int mapped_row = map_row(init_r);
    initialize_rows (channel, pseudo_channel, bank, init_r, init_r, dp_aggr);
  }

  // hammer
  test_hcn(channel, bank, pseudo_channel, map_row(victim_row-1), map_row(victim_row), map_row(victim_row+1), dp_aggr, dp_victim, hc);

  platform.receiveData(buf, ROW_SIZE_IN_BYTES); // read one row each iteration
}

// @return true if there is a bitflip, false otherwise
bool check_bitflip(int pseudo_channel, uint64_t* buf, DataPattern dp_victim)
{
  int buf_index = pseudo_channel == 0 ? 0 : NUM_BYTES_PER_READ/sizeof(uint64_t);
  int column = 0;

  for (;buf_index < ROW_SIZE_IN_BYTES/sizeof(uint64_t);)
  {
    int dword = 0;
    bool bitflipped = false;
    for (int j = 0 ; j < NUM_BYTES_PER_READ/sizeof(uint64_t) ; j++, buf_index++)
    {
      uint64_t curr_word = buf[buf_index];
      // print what is expected vs what we got
      // cout << "Expected: " << hex << dp_victim.get_dword() << " Got: " << buf[buf_index] << dec << endl;
      // There is at least one bitflip
      if (curr_word ^ dp_victim.get_dword())
        return true;
    }
    column += 1;
    buf_index += NUM_BYTES_PER_READ/sizeof(uint64_t);
  }
  return false;
}

// @return array of bit indices indicating the positions of the bitflips
vector<int> find_bitflip_positions(int pseudo_channel, uint64_t* buf, DataPattern dp_victim)
{
  vector<int> indices;
  int buf_index = pseudo_channel == 0 ? 0 : NUM_BYTES_PER_READ/sizeof(uint64_t);
  int column = 0;

  for (;buf_index < ROW_SIZE_IN_BYTES/sizeof(uint64_t);)
  {
    int dword = 0;
    bool bitflipped = false;
    for (int j = 0 ; j < NUM_BYTES_PER_READ/sizeof(uint64_t) ; j++, buf_index++)
    {
      uint64_t curr_word = buf[buf_index];
      // print what is expected vs what we got
      // cout << "Expected: " << hex << dp_victim.get_dword() << " Got: " << buf[buf_index] << dec << endl;
      // There is at least one bitflip
      if (curr_word ^ dp_victim.get_dword())
      {
        // cout << "GOT " << curr_word << " EXPECTED " << dp_victim.get_dword() << endl;
        for (int b = 0 ; b < 64 ; b++)
        {
          uint64_t mask = 1ULL << b;
          if ((curr_word & mask) != (dp_victim.get_dword() & mask))
          { 
            indices.push_back(column * 256 + dword * 64 + b);
          }
        }
      }
      dword++;
    }
    column += 1;
    buf_index += NUM_BYTES_PER_READ/sizeof(uint64_t);
  }

  return indices;
}

vector<int> find_bitflip_positions(int pseudo_channel, uint64_t* buf, DataPattern dp_victim, int selected_column)
{
  vector<int> indices;
  int buf_index = pseudo_channel == 0 ? 0 : NUM_BYTES_PER_READ/sizeof(uint64_t);
  int column = 0;

  for (;buf_index < ROW_SIZE_IN_BYTES/sizeof(uint64_t);)
  {
    int dword = 0;
    bool bitflipped = false;
    for (int j = 0 ; j < NUM_BYTES_PER_READ/sizeof(uint64_t) ; j++, buf_index++)
    {
      if (column == selected_column)
      {
        uint64_t curr_word = buf[buf_index];
        // print what is expected vs what we got
        // cout << "Expected: " << hex << dp_victim.get_dword() << " Got: " << buf[buf_index] << dec << endl;
        // There is at least one bitflip
        if (curr_word ^ dp_victim.get_dword())
        {
          // cout << "GOT " << curr_word << " EXPECTED " << dp_victim.get_dword() << endl;
          for (int b = 0 ; b < 64 ; b++)
          {
            uint64_t mask = 1ULL << b;
            if ((curr_word & mask) != (dp_victim.get_dword() & mask))
            { 
              indices.push_back(column * 256 + dword * 64 + b);
            }
          }
        }
      }
      dword++;
    }
    column += 1;
    buf_index += NUM_BYTES_PER_READ/sizeof(uint64_t);
  }

  return indices;
}

int main (int argc, char *argv[])
{
  int channel, pseudo_channel, bank, start_row, num_rows, up_to_this_hcn;
  bool word_level_analysis;
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
  ("datapattern,d", po::value<std::string>(&data_pattern_hex)->required(), "Aggressor data pattern (byte) in hex format: e.g., 0x55")
  ("uptohcfirst,t", po::value<int>(&up_to_this_hcn)->required(), "Test up to HCn where n is up_to_this_hcn")
  ("wordlevel,w", po::bool_switch(&word_level_analysis)->default_value(false), "Test one random word from each row for hcnth analysis");
  
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
  DataPattern dp_aggr(data_pattern_byte);
  DataPattern dp_victim(~data_pattern_byte);

  if(word_level_analysis)
    csv_file.open("hcnth_wordlevel_ch-" + to_string(channel) + "_pch-" + to_string(pseudo_channel) + "_bank-" + to_string(bank) + "_dp-" + data_pattern_hex +  ".csv", ios_base::app);
  else
    csv_file.open("hcnth_ch-" + to_string(channel) + "_pch-" + to_string(pseudo_channel) + "_bank-" + to_string(bank) + "_dp-" + data_pattern_hex +  ".csv", ios_base::app);
  csv_file << "channel,pseudo_channel,bank,row,bit_index,data_pattern,hcnth,hc" << endl;

  // Initialize the platform, opens file descriptors for the board PCI-E interface.
  if(platform.init() != SOFTMC_SUCCESS){
      cerr << "Could not initialize SoftMC Platform" << endl;
  }
  platform.reset_fpga();

  int selected_column = -1;

  int hc_curr = 0;
  for (int i = start_row ; i < start_row + num_rows ; i+=1)
  {
    cout << "Row: " << i << endl;
    // Find a good hcfirst estimate first
    int hc_curr = 150000; // start with an HCfirst assumption of 150K
    int hc_step = hc_curr/2;
    for (int step = 0 ; step < 17 ; step++)
    {    
      bool bitflip_any = false;

      for (int t = 0 ; t < 100 ; t++)
      {
        uint64_t buf[ROW_SIZE_IN_BYTES/sizeof(uint64_t)];

        initialize_hammer_read(channel, pseudo_channel, bank, i, dp_aggr, dp_victim, hc_curr, buf);
        bool bitflip = check_bitflip(pseudo_channel, buf, dp_victim);

        if (bitflip)
        {
          if (word_level_analysis)
          {
            vector<int> bfs = find_bitflip_positions(pseudo_channel, buf, dp_victim);
            selected_column = bfs[0] / 256;
          }
          bitflip_any = true;
          break;
        }
      }

      if (bitflip_any)
      {
        cout << "Got bf @HC: " << hc_curr << endl;
        hc_curr -= hc_step;
        hc_step /= 2;
        // need to give a little boost
        if (step == 16)
        {
          hc_step = 16;
          step -= 4;
        }
      } else
      {
        cout << "Got NO bf @HC: " << hc_curr << endl;        
        hc_curr += hc_step;
        hc_step /= 2;
      }
    }  

    // At this point, we know that hc_curr does NOT result in a bitflip at the very least for 100 repeated tests
    // each element is the bit index of a bitflip in a row
    // bitflip_position_at_hcn[0] is the first bitflip (HCfirst)
    // bitflip_position_at_hcn[1] is the bitflip we get by hammering for the min. hmmer count needed to induce the second bitflip (HCsecond)
    // ...
    vector<int> bitflip_position_at_hcn; 
    uint64_t buf[ROW_SIZE_IN_BYTES/sizeof(uint64_t)];

    for (int hcn = 1 ; hcn < up_to_this_hcn ; hcn++)
    {
      hc_step = hc_curr / 2;
      if (word_level_analysis && hcn > 1)
        hc_step = hc_curr*3/2;
      // declare set (using C++ version of set) of integers to store bitflip positions
      set<int> all_bitflip_positions;

      int last_hc_that_worked = hc_curr;
      set<int> last_bitflip_positions_that_worked;

      for (int step = 0 ; step < 17 ; step++)
      {    
        bool too_many_bitflips = false;
        bool enough_bitflips_at_least_once = false;

        // clear all_bitflip_positions
        all_bitflip_positions.clear();

        for (int t = 0 ; t < 100 ; t++)
        {
          initialize_hammer_read(channel, pseudo_channel, bank, i, dp_aggr, dp_victim, hc_curr, buf);
          vector<int> bfs;
          if (word_level_analysis)
            bfs = find_bitflip_positions(pseudo_channel, buf, dp_victim, selected_column);
          else
            bfs = find_bitflip_positions(pseudo_channel, buf, dp_victim);
          // add all bfs to set
          for (int b : bfs)
            all_bitflip_positions.insert(b);

          if (bfs.size() > hcn)
          {
            too_many_bitflips = true;
            break;
          }
          else if (bfs.size() == hcn)
          {
            enough_bitflips_at_least_once = true;
          }
        }

        if (too_many_bitflips)
        {
          cout << "Got too many bfs @HC: " << hc_curr << endl;
          hc_curr -= hc_step;
          hc_step /= 2;
          // need to give a little boost
          if (step == 16)
          {
            hc_step = 16;
            step -= 4;
          }
        } else if (enough_bitflips_at_least_once) // means we can still go lower
        {
          cout << "Got enough bf @HC: " << hc_curr << endl;
          last_hc_that_worked = hc_curr;
          hc_curr -= hc_step;
          hc_step /= 2;
          // set last worked values
          last_bitflip_positions_that_worked = all_bitflip_positions;
        } else
        {
          cout << "Got NOT enough bf @HC: " << hc_curr << endl;        
          hc_curr += hc_step;
          hc_step /= 2;
        }
      }

      // find the new bitflip in all_bitflip_positions that does not exist in bitflip_position_at_hcn
      for (int b : last_bitflip_positions_that_worked)
      {
        if (find(bitflip_position_at_hcn.begin(), bitflip_position_at_hcn.end(), b) == bitflip_position_at_hcn.end())
        {
          bitflip_position_at_hcn.push_back(b);
          break;
        }
      }

      cout << "HC" << hcn << ": " << last_hc_that_worked << " bit " <<  bitflip_position_at_hcn[hcn-1] << endl;

      // write to CSV file
      csv_file << channel << "," << pseudo_channel << "," << bank << "," << i << "," << bitflip_position_at_hcn[hcn-1] << "," << data_pattern_hex << "," << last_hc_that_worked << "," << hcn << endl;
    }
  }
  csv_file.close();
}
