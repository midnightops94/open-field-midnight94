.section .rodata
.balign 4
.global _ppg_bank_index
_ppg_bank_index:
.incbin "examples/wave/core/native-banks/packed-ppg-index.u32le"
.global _ppg_bank_pcm
_ppg_bank_pcm:
.incbin "examples/wave/core/native-banks/packed-ppg-pcm.s16le"
