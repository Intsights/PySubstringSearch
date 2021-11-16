#pragma once
#include <cstdint>

#include "rust/cxx.h"

void calculate_suffix_array(
    rust::Slice<const uint8_t> input,
    rust::Slice<int32_t> output
);
