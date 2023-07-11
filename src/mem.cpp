#include "mem.h"

#include "spdlog/spdlog.h"
#include "sys/ptrace.h"

#include <algorithm>

Memory::Memory(pid_t p) : pid(p) {}

uint64_t Memory::getRegisterValue(Reg r) {
  user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
  auto it = std::find_if(std::begin(Registers), std::end(Registers), [r](auto &&rd) { return rd.reg == r; });
  return *(reinterpret_cast<uint64_t *>(&regs) + (it - std::begin(Registers)));
}

void Memory::setRegisterValue(Reg r, uint64_t value) {
  user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
  auto it = std::find_if(std::begin(Registers), std::end(Registers), [r](auto &&rd) { return rd.reg == r; });
  *(reinterpret_cast<uint64_t *>(&regs) + (it - std::begin(Registers))) = value;
  ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
}

uint64_t Memory::getRegisterValueFromDwarfRegister(unsigned regNum) {
  auto it =
      std::find_if(std::begin(Registers), std::end(Registers), [regNum](auto &&rd) { return rd.dwarfReg == regNum; });
  if (it == std::end(Registers)) {
    spdlog::error("Unknown dwarf register");
  }

  return getRegisterValue(it->reg);
}

std::string Memory::getRegisterName(Reg r) {
  auto it = std::find_if(std::begin(Registers), std::end(Registers), [r](auto &&rd) { return rd.reg == r; });
  return it->name;
}

Reg Memory::getRegisterFromName(const std::string &name) {
  auto it = std::find_if(std::begin(Registers), std::end(Registers), [name](auto &&rd) { return rd.name == name; });
  return it->reg;
}

void Memory::dumpRegisters() {
  for (const auto &registerDescriptor : Registers) {
    spdlog::info("{:8} 0x{:016x}", registerDescriptor.name, getRegisterValue(registerDescriptor.reg));
  }
}

uint64_t Memory::readMemory(uint64_t address) { return ptrace(PTRACE_PEEKDATA, pid, address, nullptr); }

void Memory::writeMemory(uint64_t address, uint64_t value) { ptrace(PTRACE_POKEDATA, pid, address, value); }

uint64_t Memory::getPC() { return getRegisterValue(Reg::rip); }

void Memory::setPC(uint64_t pc) { setRegisterValue(Reg::rip, pc); }
