#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include <cstdint>
#include <unistd.h>

class Breakpoint {
private:
  pid_t pid;
  std::intptr_t address;
  bool enabled;
  uint8_t savedData; /**< data which used to be at the break point address */

public:
  Breakpoint() = default;
  /**
   * @brief Construct a new Breakpoint object
   *
   * @param p the current process
   * @param addr the virtual address
   */
  Breakpoint(pid_t p, std::intptr_t addr);

  /**
   * @brief Inject INT3 for enabling breakpoint
   *
   * @details Use `PTRACE_PEEKDATA` to get the current
   * `addr`'s content, rewrite the lower 8 bit to `0xcc`
   *
   */
  void enable();

  /**
   * @brief Restore the acommand for disabling breakpoint
   *
   */
  void disable();

  bool isEnabled() const { return enabled; }

  std::intptr_t getAddress() const { return address; }
};

#endif  // BREAKPOINT_H
