#ifndef MEM_H
#define MEM_H

#include "reg.h"

#include <cstdint>

class Memory {
private:
  pid_t pid;

public:
  Memory(pid_t p);

  /**
   * @brief Get the Register Value object
   *
   * @param r the register
   * @return uint64_t the value
   */
  uint64_t getRegisterValue(Reg r);

  /**
   * @brief Set the Register Value object
   *
   */
  void setRegisterValue(Reg r, uint64_t value);

  /**
   * @brief Get the Register Value From Dwarf Register object
   *
   */
  uint64_t getRegisterValueFromDwarfRegister(unsigned regNum);

  /**
   * @brief Get the Register Name object
   *
   */
  std::string getRegisterName(Reg r);

  /**
   * @brief Get the register from the input register nam
   *
   */
  Reg getRegisterFromName(const std::string &name);

  /**
   * @brief Dump the contents of all registers
   *
   */
  void dumpRegisters();

  /**
   * @brief Read address's content
   *
   */
  uint64_t readMemory(uint64_t address);

  /**
   * @brief  Write content to the address
   *
   */
  void writeMemory(uint64_t address, uint64_t value);

  /**
   * @brief Get the current PC from the current rip
   *
   */
  uint64_t getPC();

  /**
   * @brief Set the current PC
   *
   */
  void setPC(uint64_t pc);
};

#endif  // MEM_H
