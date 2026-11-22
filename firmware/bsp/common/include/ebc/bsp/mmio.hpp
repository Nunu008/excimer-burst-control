// SPDX-License-Identifier: MIT
//
// Type-safe memory-mapped I/O. A register is a type: its address, width and access
// rights are template parameters, so writing a read-only status register or using
// the wrong width is a compile error, and the generated code is a single load or
// store. C++20 deprecated compound assignment on volatile; every access here is an
// explicit read or write, which is also what a reviewer wants to see.
#pragma once

#include <concepts>
#include <cstdint>

namespace ebc::bsp {

enum class Access : std::uint8_t { read_only, write_only, read_write };

template <std::uintptr_t Address, std::unsigned_integral Word = std::uint32_t,
          Access Rights = Access::read_write>
struct Register {
  static constexpr std::uintptr_t address = Address;

  [[nodiscard]] static Word read() noexcept
    requires(Rights != Access::write_only)
  {
    return *reinterpret_cast<const volatile Word*>(Address);
  }

  static void write(Word value) noexcept
    requires(Rights != Access::read_only)
  {
    *reinterpret_cast<volatile Word*>(Address) = value;
  }

  static void set_bits(Word mask) noexcept
    requires(Rights == Access::read_write)
  {
    write(static_cast<Word>(read() | mask));
  }

  static void clear_bits(Word mask) noexcept
    requires(Rights == Access::read_write)
  {
    write(static_cast<Word>(read() & static_cast<Word>(~mask)));
  }

  [[nodiscard]] static bool any(Word mask) noexcept
    requires(Rights != Access::write_only)
  {
    return (read() & mask) != 0;
  }
};

template <std::uintptr_t Base, std::uintptr_t Offset, Access Rights = Access::read_write>
using Reg32 = Register<Base + Offset, std::uint32_t, Rights>;

}  // namespace ebc::bsp
