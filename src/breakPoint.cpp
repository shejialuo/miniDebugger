#include <cstdint>
#include "sys/ptrace.h"
#include "breakpoint.h"

Breakpoint::Breakpoint(pid_t p, std::intptr_t addr)
  :pid{p}, address{addr}, enabled{false}, savedData{} {}

void Breakpoint::enable() {
  auto data = ptrace(PTRACE_PEEKDATA, pid, address, nullptr);
  // Store the lower 8 bit
  savedData = static_cast<uint8_t>(data & 0xff);
  uint64_t int3 = 0xcc;
  uint64_t dataWithInt3 = (data & ~0xff) | int3;
  /*
    * Substitue the low 8bit to the origina data to
    * achieve interrupt operation
  */
  ptrace(PTRACE_POKEDATA, pid, address, dataWithInt3);
  enabled = true;
}

void Breakpoint::disable() {
  auto data = ptrace(PTRACE_PEEKDATA, pid, address, nullptr);
  // recover the lower 8 bit
  auto restoredData = (data & ~0xff) | savedData;
  ptrace(PTRACE_POKEDATA, pid, address,restoredData);
  enabled = false;
}
