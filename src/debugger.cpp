#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include "sys/wait.h"
#include "sys/ptrace.h"
#include "sys/user.h"
#include "linenoise.h"
#include "spdlog/spdlog.h"
#include "breakpoint.h"
#include "reg.h"
#include "debugger.h"

Debugger::Debugger(std::string name, pid_t p)
    : programName{name}, pid{p}, memory{p} {}

std::vector<std::string> Debugger::split(const std::string &s, char delimiter) {
  std::vector<std::string> out{};
  std::stringstream ss {s};
  std::string item;

  while(std::getline(ss,item,delimiter)) {
    out.push_back(item);
  }

  return out;
}

bool Debugger::isPrefix(const std::string& s, const std::string& of) {
  if (s.size() > of.size()) return false;
  return std::equal(s.cbegin(), s.cend(), of.cbegin());
}

void Debugger::continueExecution() {
  stepOverBreakpoint();
  ptrace(PTRACE_CONT, pid, nullptr, nullptr);
  int waitStatus;
  int options = 0;
  waitpid(pid, &waitStatus, options);
}

void Debugger::setBreakPointAtAddress(std::intptr_t addr) {
  spdlog::info("Set breakpoint at address 0x{0:x}", addr);
  Breakpoint breakpoint{pid, addr};
  breakpoint.enable();
  breakpoints[addr] = breakpoint;
}

void Debugger::stepOverBreakpoint() {
  // Remember to substract 1, the the CPU
  // execute INT3, the PC will add 1 and traps.
  uint64_t maybeBreakPointLocation = memory.getPC() - 1;

  // The address must be stored in the breakpoints
  if(breakpoints.count(maybeBreakPointLocation)) {
    Breakpoint& bp = breakpoints[maybeBreakPointLocation];
    if(bp.isEnabled()) {
      // Set PC back
      memory.setPC(maybeBreakPointLocation);
      bp.disable();

      ptrace(PTRACE_SINGLESTEP, pid, nullptr, nullptr);
      waitForSignal();
      bp.enable();
    }
  }
}

void Debugger::handleCommand(const std::string& line) {
  auto args = split(line, ' ');
  // Get the first command
  auto command = args[0];

  if(isPrefix(command, "cont")) {
    continueExecution();
  } else if(isPrefix(command, "break")) {
    /*
      * For simplicity, this code assumes that user
      * input 0xADRRESS
    */
    std::string address {args[1],2};
    setBreakPointAtAddress(std::stol(address,0, 16));
  } else if(isPrefix(command, "register")) {
    if(isPrefix(args[1], "dump")) {
      memory.dumpRegisters();
    } else if(isPrefix(args[1], "read")) {
      spdlog::info("0x{:016x}",
        memory.getRegisterValue(memory.getRegisterFromName(args[2])));
    } else if(isPrefix(args[1], "write")) {
      std::string value {args[3], 2}; // assume 0x(value)
      memory.setRegisterValue(memory.getRegisterFromName(args[2]),
        std::stol(value,0,16));
    }
  } else if(isPrefix(command, "memory")) {
    std::string address {args[2], 2};
    if(isPrefix(args[1], "read")) {
      spdlog::info("{:016x}", memory.readMemory(std::stol(address, 0, 16)));
    }
    if(isPrefix(args[1], "write")) {
      std::string value {args[3], 2};
      memory.writeMemory(std::stol(address, 0, 16), std::stol(value, 0, 16));
    }
  }else {
    spdlog::error("Unknown command");
  }
}

void Debugger:: waitForSignal() {
  int wait_status;
  auto options = 0;
  waitpid(pid, &wait_status, options);
}

void Debugger::run() {
  /*
    * When the traced process is launched, it will be
    * sent a `SIGTRAP` signal, which is a trace or
    * breakpoint trap. We can wait until this signal
    * is sent using the `waitpid` function
  */
  waitForSignal();

  char* line = nullptr;
  // User linenoise library to handle user input for convenience
  while((line = linenoise("miniDebugger> ")) != nullptr) {
    // To handle the use input
    handleCommand(line);
    // To add the line to the history to support history and navigation
    linenoiseHistoryAdd(line);
    linenoiseFree(line);
  }
}
