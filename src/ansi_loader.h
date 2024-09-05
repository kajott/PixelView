// SPDX-FileCopyrightText: 2024 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace ANSI {

//! list of extension codes (as used in string_util.h) for all recognized file types
extern const uint32_t fileExts[];

//! render an ANSI file into a 32-bit image
void* render(const char* filename, int &width, int &height);

}  // namespace ANSI
