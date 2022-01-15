#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <fcntl.h>
#include "signal.h"
#include "sys/wait.h"
#include "sys/ptrace.h"
#include "sys/user.h"
#include "linenoise.h"
#include "spdlog/spdlog.h"
#include "elf/elf++.hh"
#include "dwarf/dwarf++.hh"
#include "breakpoint.h"
#include "reg.h"
#include "debugger.h"

Debugger::Debugger(std::string name, pid_t p): memory(p) {
  programName = name;
  pid = p;
  auto fd = open(programName.c_str(), O_RDONLY);
  pElf = elf::elf{elf::create_mmap_loader(fd)};
  pDwarf = dwarf::dwarf{dwarf::elf::create_loader(pElf)};
  close(fd);
}

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
  waitForSignal();
}

void Debugger::setBreakPointAtAddress(std::intptr_t addr) {
  spdlog::info("Set breakpoint at address 0x{0:x}", addr);
  Breakpoint breakpoint{pid, addr};
  breakpoint.enable();
  breakpoints[addr] = breakpoint;
}

void Debugger::stepOverBreakpoint() {

  // The address must be stored in the breakpoints
  if(breakpoints.count(memory.getPC())) {
    Breakpoint& bp = breakpoints[memory.getPC()];
    if(bp.isEnabled()) {
      bp.disable();
      ptrace(PTRACE_SINGLESTEP, pid, nullptr, nullptr);
      waitForSignal();
      bp.enable();
    }
  }
}

dwarf::die Debugger::getFunctionFromPC(uint64_t pc) {
  for(auto &compilationUnit: pDwarf.compilation_units()) {
    if(dwarf::die_pc_range(compilationUnit.root()).contains(pc)) {
      for(const auto& die: compilationUnit.root()) {
        if(die.tag == dwarf::DW_TAG::subprogram) {
          return die;
        }
      }
    }
  }
  spdlog::error("Cannot find function");
  throw std::out_of_range{"Cannot find function"};
}

dwarf::line_table::iterator Debugger::getLineEntryFromPC(uint64_t pc) {
  for(auto &compilationUnit: pDwarf.compilation_units()) {
    if(dwarf::die_pc_range(compilationUnit.root()).contains(pc)) {
      auto &lt = compilationUnit.get_line_table();
      auto it = lt.find_address(pc);
      if(it == lt.end()) {
        spdlog::error("Cannot find line entry");
        throw std::out_of_range{"Cannot find line entry"};
      } else {
        return it;
      }
    }
  }
  spdlog::error("Cannot find line entry");
  throw std::out_of_range{"Cannot find line entry"};
}

void Debugger::initializeLoadAddress() {
  if(pElf.get_hdr().type == elf::et::dyn) {
    std::ifstream map("/proc/" + std::to_string(pid) + "/maps");
    std::string address;
    std::getline(map, address, '-');
    map.close();
    spdlog::info("The load address is {}", address);
    loadAddress = std::stol(address, 0, 16);
  }
}

uint64_t Debugger::offsetLoadAddress(uint64_t address) {
  return address - loadAddress;
}

void Debugger::printSource(const std::string& fileName,unsigned line, unsigned nLinesContext) {
  std::ifstream file {fileName};

  // Work out a window around the desired value
  auto startLine = line <= nLinesContext ? 1 : line - nLinesContext;
  auto endLine = line + nLinesContext + (line < nLinesContext ?
                 nLinesContext - line : 0) + 1;

  char c{};
  unsigned int currentLine = 1;
  while(currentLine != startLine && file.get(c)) {
    if(c == '\n') {
      ++currentLine;
    }
  }

  std::cout << (currentLine==line ? "> " : "  ");

  while(currentLine <= endLine && file.get(c)) {
    std::cout << c;
    if(c == '\n') {
      ++currentLine;
      std::cout << (currentLine==line ? "> " : "  ");
    }
  }

  std::cout << std::endl;
  file.close();
}

siginfo_t Debugger::getSignalInfo() {
  siginfo_t info;
  ptrace(PTRACE_GETSIGINFO, pid, nullptr, &info);
  return info;
}

void Debugger::handleSignaltrap(siginfo_t info) {
  /*
    * Handle `SIGTRAP`. It suffices to know that
    * `SI_KERNEL` or `TRAP_BRKPT` will be sent when
    * a breakpoint is hit, and `TRAP_TRACE` will be
    * sent on single step completion.
  */
  switch(info.si_code) {
  case SI_KERNEL:
  case TRAP_BRKPT: {
    // Put the PC back where it should be
    memory.setPC(memory.getPC() - 1);
    spdlog::info("Hit breakpoint at address 0x{:x}", memory.getPC());
    uint64_t offsetPC = offsetLoadAddress(memory.getPC());
    dwarf::line_table::iterator lineEntry = getLineEntryFromPC(offsetPC);
    printSource(lineEntry->file->path, lineEntry->line);
    return;
  }
  // This will be set if the signal was sent by single stepping
  case TRAP_TRACE:
    return;
  default:
    spdlog::info("Unknown SIGTRAP code {}", info.si_code);
    return;
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
  int waitStatus;
  int options = 0;
  waitpid(pid, &waitStatus, options);

  siginfo_t siginfo = getSignalInfo();

  switch(siginfo.si_signo) {
  case SIGTRAP:
    handleSignaltrap(siginfo);
    break;
  case SIGSEGV:
    spdlog::error("Yay, segfault. Reason: {}", siginfo.si_code);
    break;
  default:
    spdlog::info("Got signal {}", strsignal(siginfo.si_signo));
  }
}

void Debugger::run() {
  /*
    * When the traced process is launched, it will be
    * sent a `SIGTRAP` signal, which is a trace or
    * breakpoint trap. We can wait until this signal
    * is sent using the `waitpid` function
  */
  waitForSignal();
  initializeLoadAddress();

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
