#ifndef PPG_NATIVE_READER_HPP
#define PPG_NATIVE_READER_HPP

namespace ppg_reader {
using i16 = __INT16_TYPE__;
using i32 = __INT32_TYPE__;
using u32 = __UINT32_TYPE__;
constexpr u32 banks = 8, mips = 6, frames = 16, cells = 128, one_q16 = 65536;
}

// Phase is one cycle in Q0.32. Mip, frame, and controlled grain use Q16.
extern "C" ppg_reader::i32 ppg_v2_read_q30(ppg_reader::u32 bank, ppg_reader::u32 mip,
                                          ppg_reader::u32 position, ppg_reader::u32 phase,
                                          ppg_reader::u32 grain);
extern "C" ppg_reader::i16 ppg_v2_cell(ppg_reader::u32 bank, ppg_reader::u32 mip,
                                        ppg_reader::u32 frame, ppg_reader::u32 sample);

#endif
