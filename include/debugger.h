#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <unistd.h>
#include "breakpoint.h"

class Debugger {
private:
  // program name
  std::string programName;
  // process id
  pid_t pid;
  // breakpoint
  std::unordered_map<std::intptr_t, Breakpoint> breakpoints;
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
public:
  Debugger(std::string name, pid_t p);
  // Run the loop
  void run();
};

#endif // DEBUGGER_H