#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "breakpoint.h"
#include "dwarf/dwarf++.hh"
#include "elf/elf++.hh"
#include "mem.h"
#include "reg.h"
#include "signal.h"

#include <cstdint>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

enum class symType { notype, object, func, section, file };

static std::string symToString(symType st) {
  switch (st) {
    case symType::notype:
      return "notype";
    case symType::object:
      return "object";
    case symType::func:
      return "func";
    case symType::section:
      return "section";
    case symType::file:
      return "file";
    default:
      return "";
  }
}

struct Sym {
  symType type;
  std::string name;
  std::uintptr_t address;
};

static symType elfToSymType11(elf::stt sym) {
  switch (sym) {
    case elf::stt::notype:
      return symType::notype;
    case elf::stt::object:
      return symType::object;
    case elf::stt::func:
      return symType::func;
    case elf::stt::section:
      return symType::section;
    case elf::stt::file:
      return symType::file;
    default:
      return symType::notype;
  }
}

class Debugger {
private:
  std::string programName;                                   /**< program name */
  pid_t pid;                                                 /**< process id */
  uint64_t loadAddress = 0;                                  /**< load address */
  std::unordered_map<std::intptr_t, Breakpoint> breakpoints; /**< breakpoints */
  dwarf::dwarf pDwarf;                                       /**< dwarf instance*/
  elf::elf pElf;                                             /**< elf*/
  Memory memory;                                             /**< memory class*/

  /**
   * @brief To handle user input
   *
   * @param line
   */
  void handleCommand(const std::string &line);

  /**
   * @brief Auxilary function for `handleCommand` to split space
   *
   * @param s the user input
   * @param delimiter
   * @return std::vector<std::string> tokens
   */
  std::vector<std::string> split(const std::string &s, char delimiter);

  /**
   * @brief Auxiliary function for prefix. For example: user can type
   `continue` or `cont` or even just `c` for continue the process.
   *
   * @param s the input
   * @param of the complete string
   */
  bool isPrefix(const std::string &s, const std::string &of);

  /**
   * @brief Auxiliary function for suffix.
   *
   * @param s the input
   * @param of the complete string
   */
  bool isSuffix(const std::string &s, const std::string &of);

  /**
   * @brief Continue execution for the debugged program
   *
   * @details Use `ptrace` to continue the execution, however,
   * there may be the current address is a breakpoint, so we
   * need to first step in this instruction first, and do
   * the continuation.
   *
   */
  void continueExecution();

  /**
   * @brief Set breakpoint at the specified address
   *
   * @details Create a new breakpoint and enable it, add it
   * to the `breakpoints` data structure
   *
   * @param addr the virtual address
   */
  void setBreakPointAtAddress(std::intptr_t addr);

  /**
   * @brief Set breakpoint at function
   *
   * @param name the function name
   */
  void setBreakPointAtFunction(const std::string &name);

  // Set breakpoint at line

  /**
   * @brief Set breakpoint at file:line
   *
   */
  void setBreakPointAtSourceLine(const std::string &file, unsigned line);

  /**
   * @brief Wait for the signal when child process
   * hits the breakpoints and other situations.
   *
   */
  void waitForSignal();

  /**
   * @brief Step over the breakpoint
   *
   * @details Step the current breakpoint, if it is enabled,
   * we should first disable it, and call `ptrace` to step in,
   * And calls `waitForSignal` and re-enable the breakpoint.
   *
   */
  void stepOverBreakpoint();

  /**
   * @brief Step single instruction
   *
   */
  void singleStepInstruction();

  // Step single instruction with breakpoint check

  /**
   * @brief Step single instruction with breakpoint check
   *
   */
  void singleStepInstructionWithBreakpointCheck();

  /**
   * @brief Step out the current function.
   *
   * @details We'll find the return address of the current function,
   * so how to find the return address, remember the x86 call convention.
   * `push ebp; mov ebp esp`. So we can get the value of the register
   * `ebp`, and +8 bytes to get the return address. And we should set
   * the return address a breakpoint, because we need to get the control
   * for the user, and then disable the breakpoint.
   *
   * @note However, there would be a situation that the user has set the
   * correspond address a breakpoint,this is a corner case.
   *
   */
  void stepOut();

  /**
   * @brief Remove breakpoint at the specified virtual address
   *
   * @param address The virtual address
   */
  void removeBreakpoint(std::intptr_t address);

  /**
   * @brief Step the current line.
   *
   * @details Use DWARF to skip the instructions until the
   * current PC is in the current line.
   */
  void stepIn();

  /**
   * @brief Get PC offset
   *
   * @return uint64_t
   */
  uint64_t getOffsetPC();

  // Step over
  /**
   * @brief Step over a function.
   *
   * @note Conceptually, the solution is to set a breakpoint at
   * the next source line, but what is the next source line?
   * Real debuggers will often examine what instruction is being
   * executed and work out all of the possible branch
   * targets, then set breakpoints on all of them.
   *
   */
  void stepOver();

  // A helper function to offset address from DWARF info by the load address

  /**
   * @brief A helper function to offset address from DWARF info by the load address
   *
   */
  uint64_t offsetDwarfAddress(uint64_t address);

  /**
   * @brief Get the debug information entry from current pc.
   *
   * @details For every compilation units, if the pc is in the compilation
   * units, if the die contain contains the pc, return this die.
   *
   * @param pc the PC
   * @return dwarf::die Debug Information Entry
   */
  dwarf::die getFunctionFromPC(uint64_t pc);
  // Get line entry from PC

  /**
   * @brief Get line entry from PC
   *
   * @param pc
   * @return dwarf::line_table::iterator
   */
  dwarf::line_table::iterator getLineEntryFromPC(uint64_t pc);

  // To lookUpTheSymbol

  /**
   * @brief To find the sym to loop up the symbol table.
   *
   * @param name
   * @return std::vector<Sym>
   */
  std::vector<Sym> lookupSymbol(const std::string &name);

  /**
   * @brief To get the mapped load address
   *
   */
  void initializeLoadAddress();
  // To calculate the offset from the load address

  /**
   * @brief To calculate the offset from the load address
   *
   * @param address
   * @return uint64_t
   */
  uint64_t offsetLoadAddress(uint64_t address);

  /**
   * @brief Print the source code
   *
   * @param fileName the filename we could get it from DWARF
   * @param line the line
   * @param nLinesContext the window
   */
  void printSource(const std::string &fileName, unsigned line, unsigned nLinesContext = 2);

  /**
   * @brief Get the signal information
   *
   * @return siginfo_t
   */
  siginfo_t getSignalInfo();

  /**
   * @brief Handle the signal
   *
   */
  void handleSignalTrap(siginfo_t info);

public:
  /**
   * @brief Construct a new Debugger object
   *
   */
  Debugger(std::string name, pid_t p);

  /**
   * @brief The entry point of the debugger
   *
   */
  void run();
};

#endif  // DEBUGGER_H