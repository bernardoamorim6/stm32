#include "reg.hpp"
using GpioaModer = hal::Reg<0x50000000U, hal::Mmio>;
using Pa5Mode    = hal::Field<GpioaModer, 10, 2>;
extern "C" void set_pa5_output(void) { Pa5Mode::write<1>(); }
