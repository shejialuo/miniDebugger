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

bool Debugger::isSuffix(const std::string& s, const std::string& of) {
    if (s.size() > of.size()) return false;
    auto diff = of.size() - s.size();
    return std::equal(s.begin(), s.end(), of.begin() + diff);
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

void Debugger::setBreakPointAtFunction(const std::string& name) {
  for(const auto& compilationUnit: pDwarf.compilation_units()) {
    for(const auto& die: compilationUnit.root()) {
      if(die.has(dwarf::DW_AT::name) && at_name(die) == name) {
        auto lowPC = at_low_pc(die);
        auto entry = getLineEntryFromPC(lowPC);
        ++entry; //skip prologue
        setBreakPointAtAddress(offsetDwarfAddress(entry->address));
      }
    }
  }
}

void Debugger::setBreakPointAtSouceLine(const std::string& file, unsigned line) {
  for (const auto& compilationUnit: pDwarf.compilation_units()) {
    if(isSuffix(file, at_name(compilationUnit.root()))) {
      const auto& lt = compilationUnit.get_line_table();

      for(const auto& entry: lt) {
        if(entry.is_stmt && entry.line == line) {
          setBreakPointAtAddress(offsetDwarfAddress(entry.address));
          return;
        }
      }
    }
  }
}

std::vector<Sym> Debugger::lookupSymbol(const std::string& name) {
  std::vector<Sym> syms;
  for(auto& section: pElf.sections()) {
    if(section.get_hdr().type != elf::sht::symtab && section.get_hdr().type != elf::sht::dynsym) {
      continue;
    }

    for(auto sym: section.as_symtab()) {
      if(sym.get_name() == name) {
        auto &data = sym.get_data();
        syms.push_back(Sym{elfToSymType11(data.type()), sym.get_name(), data.value});
      }
    }
  }
  return syms;
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

void Debugger::singleStepInstruction() {
  ptrace(PTRACE_SINGLESTEP, pid, nullptr, nullptr);
  waitForSignal();
}

void Debugger::singleStepInstructionWithBreakpointCheck() {
  // First, check to see if we need to disable and enale breakpoint
  if(breakpoints.count(memory.getPC())) {
    stepOverBreakpoint();
  } else {
    singleStepInstruction();
  }
}

void Debugger::stepOut() {
  auto framePointer = memory.getRegisterValue(Reg::rbp);
  auto returnAddress = memory.readMemory(framePointer + 8);

  bool shouldRemoveBreakpoint = false;
  if(!breakpoints.count(returnAddress)) {
    setBreakPointAtAddress(returnAddress);
    shouldRemoveBreakpoint = true;
  }

  continueExecution();

  if(shouldRemoveBreakpoint) {
    removeBreakpoint(returnAddress);
  }
}

void Debugger::removeBreakpoint(std::intptr_t address) {
  if(breakpoints.at(address).isEnabled()) {
    breakpoints.at(address).disable();
  }
  breakpoints.erase(address);
}

void Debugger::stepIn() {
  /*
    * A simple algorithm is to just keep on stepping
    * over instructions until we get to a new line.
  */
  auto line = getLineEntryFromPC(getOffsetPC())->line;
  while(getLineEntryFromPC(getOffsetPC())->line == line) {
    singleStepInstructionWithBreakpointCheck();
  }
  auto lineEntry = getLineEntryFromPC(getOffsetPC());
  printSource(lineEntry->file->path, lineEntry->line);
}

uint64_t Debugger::getOffsetPC() {
  return offsetLoadAddress(memory.getPC());
}

void Debugger::stepOver() {
  auto func = getFunctionFromPC(getOffsetPC());
  auto funcEntry = at_low_pc(func);
  auto funcEnd = at_high_pc(func);

  auto line = getLineEntryFromPC(funcEntry);
  auto startLine = getLineEntryFromPC(getOffsetPC());

  std::vector<std::intptr_t> toDelete {};

  while(line->address < funcEnd) {
    auto loadAddress = offsetDwarfAddress(line->address);
    if(line->address != startLine->address && !breakpoints.count(loadAddress)) {
      setBreakPointAtAddress(loadAddress);
      toDelete.push_back(loadAddress);
    }
    ++line;
  }

  auto framePointer = memory.getRegisterValue(Reg::rbp);
  auto returnAddress = memory.readMemory(framePointer + 8);

  if(!breakpoints.count(returnAddress)) {
    setBreakPointAtAddress(returnAddress);
    toDelete.push_back(returnAddress);
  }

  continueExecution();

  for(auto address: toDelete) {
    removeBreakpoint(address);
  }
}

uint64_t Debugger::offsetDwarfAddress(uint64_t address) {
  return address + loadAddress;
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
    if(args[1][0] == '0' && args[1][1] == 'x') {
      std::string address {args[1],2};
      setBreakPointAtAddress(std::stol(address,0, 16));
    } else if (args[1].find(':') != std::string::npos) {
      auto fileAndLine = split(args[1], ':');
      setBreakPointAtSouceLine(fileAndLine[0], std::stoi(fileAndLine[1]));
    } else {
      setBreakPointAtFunction(args[1]);
    }
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
  } else if(isPrefix(command, "step")) {
    stepIn();
  } else if(isPrefix(command, "next")) {
    stepOver();
  } else if(isPrefix(command, "finish")) {
    stepOut();
  } else if(isPrefix(command, "symbol")) {
    auto syms = lookupSymbol(args[1]);
    for(auto sym: syms) {
      spdlog::info("{0} {1} 0x{2:x}", sym.name, symToString(sym.type), sym.address);
    }
  }
  else {
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
