#ifndef MEM_H
#define MEM_H

#include "reg.h"

class Memory{
private:
  pid_t pid;
public:
  Memory(pid_t p);
  // Get the register value
  uint64_t getRegisterValue(Reg r);
  // Set the register value
  void setRegisterValue(Reg r, uint64_t value);
  // Get the register value from dwarf register
  uint64_t getRegisterValueFromDwarfRegister(unsigned regNum);
  // Get the register name
  std::string getRegisterName(Reg r);
  // Get the register from the input register name
  Reg getRegisterFromName(const std::string& name);
  // Dump the contents of all registers
  void dumpRegisters();
  // Read address's content
  uint64_t readMemory(uint64_t address);
  // Write content to the address
  void writeMemory(uint64_t address, uint64_t value);
  // Get the current PC from the current rip
  uint64_t getPC();
  // Set the current PC
  void setPC(uint64_t pc);
};

#endif // MEM_H
