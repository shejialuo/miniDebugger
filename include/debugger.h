#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <unistd.h>
#include "signal.h"
#include "elf/elf++.hh"
#include "dwarf/dwarf++.hh"
#include "mem.h"
#include "breakpoint.h"
#include "reg.h"

class Debugger {
private:
  // program name
  std::string programName;
  // process id
  pid_t pid;
  // load address
  uint64_t loadAddress = 0;
  // breakpoint
  std::unordered_map<std::intptr_t, Breakpoint> breakpoints;
  // dwarf
  dwarf::dwarf pDwarf;
  // elf
  elf::elf pElf;
  // memory class
  Memory memory;
  // To handle user input
  void handleCommand(const std::string& line);
  // Aux function for hanadleCommand to split space
  std::vector<std::string> split(const std::string &s, char delimiter);
  /*
     Aux function for prefix
     * For example: user can type `continue` or `cont` or
     * even just `c` for continue the process.
  */
  bool isPrefix(const std::string& s, const std::string& of);
  // Continue execution for the debugged program
  void continueExecution();
  // Set breakpoint at the specified address
  void setBreakPointAtAddress(std::intptr_t addr);
  // Wait for signal
  void waitForSignal();
  // Step over the breakpoint, note that stepping over.
  void stepOverBreakpoint();
  // Get function from PC
  dwarf::die getFunctionFromPC(uint64_t pc);
  // Get line entry from PC
  dwarf::line_table::iterator getLineEntryFromPC(uint64_t pc);
  // To get the mapped load address
  void initializeLoadAddress();
  // To calculate the offset from the load address
  uint64_t offsetLoadAddress(uint64_t address);
  // Print the source code
  void printSource(const std::string& fileName,unsigned line, unsigned nLinesContext = 2);
  // Get the signal information
  siginfo_t getSignalInfo();
  // Handle the signal
  void handleSignaltrap(siginfo_t info);
public:
  Debugger(std::string name, pid_t p);
  // Run the loop
  void run();
};

#endif // DEBUGGER_H