#ifndef SOFTMC_UTILS_H
#define SOFTMC_UTILS_H

#include <cstdint>
#include <vector>
#include <exception>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "instruction.h"
#include "prog.h"
#include "tools/perfect_hash.h"

#define CACHE_LINE_BITS 256

typedef uint32_t SMC_REG;

struct OutOfSoftMCRegsException : public std::exception {
   const char * what () const throw () {
      return "No more SoftMC registers to allocate.";
   }
};

static uint32_t label_counter = 0;
std::string createSMCLabel(const std::string& name) {
    return name + std::to_string(label_counter++);
}

void waitMS(const uint ret_time_ms) {
    
    static constexpr std::chrono::duration<double, std::milli> min_sleep_duration(1);
    auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count() < ret_time_ms) {
        std::this_thread::sleep_for(min_sleep_duration);
    }
}

Inst all_nops()
{
  return  __pack_mininsts(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_NOP());
}

int add_op_with_delay (Program& prog, Mininst ins, int before_cycles, int after_cycles) {
    
    int remaining = before_cycles < 0 ? 0 : before_cycles;

    while(remaining >= 4) {
        prog.add_inst(all_nops());
        remaining -= 4;
    }

    switch(remaining) {
        case 0:
            prog.add_inst(__pack_mininsts(ins, SMC_NOP(), SMC_NOP(), SMC_NOP()));
            remaining = after_cycles - 3;
            break;
        case 1:
            prog.add_inst(__pack_mininsts(SMC_NOP(), ins, SMC_NOP(), SMC_NOP()));
            remaining = after_cycles - 2;
            break;
        case 2:
            prog.add_inst(__pack_mininsts(SMC_NOP(), SMC_NOP(), ins, SMC_NOP()));
            remaining = after_cycles - 1;
            break;
        case 3:
            prog.add_inst(__pack_mininsts(SMC_NOP(), SMC_NOP(), SMC_NOP(), ins));
            remaining = after_cycles;
            break;

        default:
            assert(false && "This line should not be reached. Possible bug in the program.");
    }

    while (remaining >= 4) {
        prog.add_inst(all_nops());
        remaining -= 4;
    }

    return remaining;
}

int add_op_with_delay (Program& prog, Inst ins, int before_cycles, int after_cycles) {
    
    int remaining = before_cycles;

    while(remaining > 0) {
        prog.add_inst(all_nops());
        remaining -= 4;
    }

    prog.add_inst(ins);
    remaining = after_cycles;

    while (remaining >= 4) {
        prog.add_inst(all_nops());
        remaining -= 4;
    }

    return remaining;
}

std::string bin_to_hex(const char* bin, const int length) {
	
	std::string hex_str;

	for (auto it = bin; it < bin+length; it += 4) {
		hex_str.push_back(Perfect_Hash::hexchar(it));
	}

	return hex_str;
}

class SoftMCRegAllocator {

public:
    SoftMCRegAllocator(uint32_t num_regs, const std::vector<uint32_t>& reserved_regs) {
        free_regs.reserve(num_regs);

        for(uint32_t i = 0; i < num_regs; i++) {
            if(std::find(reserved_regs.begin(), reserved_regs.end(), i) == reserved_regs.end())
                free_regs.emplace_back(i);
        }
    }

    SMC_REG allocate_SMC_REG() {
        if(free_regs.size() == 0)
            throw OutOfSoftMCRegsException();

        SMC_REG ret_reg = *(free_regs.begin());
        free_regs.erase(free_regs.begin());
        return ret_reg;
    }

    void free_SMC_REG(const SMC_REG r) {
        // make sure r is not in the free list
        auto it = std::find(free_regs.begin(), free_regs.end(), r);
        assert(it == free_regs.end());

        free_regs.push_back(r);
    }

    uint num_free_regs() const {
        return free_regs.size();
    }

private:
    std::vector<uint32_t> free_regs;
};


typedef uint PhysicalRowID;
typedef uint LogicalRowID;

typedef enum LogPhysRowIDScheme {
    SEQUENTIAL,
    SAMSUNG,
    MAX
} LogPhysRowIDScheme;

LogPhysRowIDScheme logical_physical_conversion_scheme = LogPhysRowIDScheme::SEQUENTIAL;

PhysicalRowID to_physical_row_id(uint logical_row_id) {
    
    switch(logical_physical_conversion_scheme) {
        case LogPhysRowIDScheme::SEQUENTIAL: {
            return logical_row_id;
            break;
        }
        case LogPhysRowIDScheme::SAMSUNG: {
            if(logical_row_id & 0x8) {
                PhysicalRowID phys_row_id = logical_row_id & 0xFFFFFFF9;
                phys_row_id |= (~logical_row_id & 0x00000006); // set bit pos 3 and 2

                return phys_row_id;
            } else {
                return logical_row_id;
            }
            break;
        }
        default: {
            std::cerr << "ERROR: unimplemented logical to physical row id conversion scheme!" << std::endl;
            return -1;
        }
    }
}

LogicalRowID to_logical_row_id(uint physical_row_id) {
    switch(logical_physical_conversion_scheme) {
        case LogPhysRowIDScheme::SEQUENTIAL: {
            return physical_row_id;
            break;
        }
        case LogPhysRowIDScheme::SAMSUNG: {
            if(physical_row_id & 0x8) {
                PhysicalRowID log_row_id = physical_row_id & 0xFFFFFFF9;
                log_row_id |= (~physical_row_id & 0x00000006); // set bit pos 3 and 2

                return log_row_id;
            } else {
                return physical_row_id;
            }
            break;
        }
        default: {
            std::cerr << "ERROR: unimplemented physical to logical row id conversion scheme!" << std::endl;
        }
    }

    return 0;
}


#endif // SOFTMC_UTILS_H