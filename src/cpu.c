#include "ibm.h"
#include "cpu.h"
#include "model.h"
#include "io.h"
#include "x86_ops.h"
#include "mem.h"
#include "pci.h"
#include "codegen.h"

int isa_cycles;
static uint8_t ccr0, ccr1, ccr2, ccr3, ccr4, ccr5, ccr6;

OpFn *x86_dynarec_opcodes;
OpFn *x86_dynarec_opcodes_0f;
OpFn *x86_dynarec_opcodes_d8_a16;
OpFn *x86_dynarec_opcodes_d8_a32;
OpFn *x86_dynarec_opcodes_d9_a16;
OpFn *x86_dynarec_opcodes_d9_a32;
OpFn *x86_dynarec_opcodes_da_a16;
OpFn *x86_dynarec_opcodes_da_a32;
OpFn *x86_dynarec_opcodes_db_a16;
OpFn *x86_dynarec_opcodes_db_a32;
OpFn *x86_dynarec_opcodes_dc_a16;
OpFn *x86_dynarec_opcodes_dc_a32;
OpFn *x86_dynarec_opcodes_dd_a16;
OpFn *x86_dynarec_opcodes_dd_a32;
OpFn *x86_dynarec_opcodes_de_a16;
OpFn *x86_dynarec_opcodes_de_a32;
OpFn *x86_dynarec_opcodes_df_a16;
OpFn *x86_dynarec_opcodes_df_a32;

OpFn *x86_opcodes;
OpFn *x86_opcodes_0f;
OpFn *x86_opcodes_d8_a16;
OpFn *x86_opcodes_d8_a32;
OpFn *x86_opcodes_d9_a16;
OpFn *x86_opcodes_d9_a32;
OpFn *x86_opcodes_da_a16;
OpFn *x86_opcodes_da_a32;
OpFn *x86_opcodes_db_a16;
OpFn *x86_opcodes_db_a32;
OpFn *x86_opcodes_dc_a16;
OpFn *x86_opcodes_dc_a32;
OpFn *x86_opcodes_dd_a16;
OpFn *x86_opcodes_dd_a32;
OpFn *x86_opcodes_de_a16;
OpFn *x86_opcodes_de_a32;
OpFn *x86_opcodes_df_a16;
OpFn *x86_opcodes_df_a32;

enum
{
        CPUID_FPU = (1 << 0),
        CPUID_TSC = (1 << 4),
        CPUID_MSR = (1 << 5),
        CPUID_CMPXCHG8B = (1 << 8),
        CPUID_CMOV = (1 << 15),
        CPUID_MMX = (1 << 23)
};

int cpu = 3, cpu_manufacturer = 0;
CPU *cpu_s;
int cpu_multi;
int cpu_iscyrix;
int cpu_16bitbus;
int cpu_busspeed;
int cpu_hasrdtsc;
int cpu_hasMMX, cpu_hasMSR;
int cpu_hasCR4;
int cpu_use_dynarec;
int cpu_cyrix_alignment;

uint64_t cpu_CR4_mask;

int cpu_cycles_read, cpu_cycles_read_l, cpu_cycles_write, cpu_cycles_write_l;
int cpu_prefetch_cycles, cpu_prefetch_width;
int cpu_waitstates;
int cpu_cache_int_enabled, cpu_cache_ext_enabled;

int is386;

uint64_t tsc = 0;

int timing_rr;
int timing_mr, timing_mrl;
int timing_rm, timing_rml;
int timing_mm, timing_mml;
int timing_bt, timing_bnt;
int timing_int, timing_int_rm, timing_int_v86, timing_int_pm, timing_int_pm_outer;
int timing_iret_rm, timing_iret_v86, timing_iret_pm, timing_iret_pm_outer;
int timing_call_rm, timing_call_pm, timing_call_pm_gate, timing_call_pm_gate_inner;
int timing_retf_rm, timing_retf_pm, timing_retf_pm_outer;
int timing_jmp_rm, timing_jmp_pm, timing_jmp_pm_gate;
int timing_misaligned;

static struct
{
        uint32_t tr1, tr12;
        uint32_t cesr;
        uint32_t fcr;
        uint64_t fcr2, fcr3;
} msr;

/*Available cpuspeeds :
        0 = 16 MHz
        1 = 20 MHz
        2 = 25 MHz
        3 = 33 MHz
        4 = 40 MHz
        5 = 50 MHz
        6 = 66 MHz
        7 = 75 MHz
        8 = 80 MHz
        9 = 90 MHz
        10 = 100 MHz
        11 = 120 MHz
        12 = 133 MHz
        13 = 150 MHz
        14 = 160 MHz
        15 = 166 MHz
        16 = 180 MHz
        17 = 200 MHz
*/

CPU cpus_8088[] =
{
        /*8088 standard*/
        {"8088/4.77",    CPU_8088,  0,  4772728,   1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8088/7.16",    CPU_8088,  1, 14318184/2, 1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8088/8",       CPU_8088,  1,  8000000,   1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8088/10",      CPU_8088,  2, 10000000,   1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8088/12",      CPU_8088,  3, 12000000,   1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8088/16",      CPU_8088,  4, 16000000,   1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_pcjr[] =
{
        /*8088 PCjr*/
        {"8088/4.77",    CPU_8088,  0,  4772728, 1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_europc[] =
{
         /*8088 EuroPC*/
         {"8088/4.77",    CPU_8088,  0,  4772728,   1, 0, 0, 0, 0, 0, 0,0,0,0},
         {"8088/7.16",    CPU_8088,  1, 14318184/2, 1, 0, 0, 0, 0, 0, 0,0,0,0},
         {"8088/9.54",    CPU_8088,  1,  4772728*2, 1, 0, 0, 0, 0, 0, 0,0,0,0},
         {"",             -1,        0, 0, 0, 0}
};

CPU cpus_8086[] =
{
        /*8086 standard*/
        {"8086/7.16",    CPU_8086,  1, 14318184/2,     1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8086/8",       CPU_8086,  1,  8000000,       1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8086/9.54",    CPU_8086,  1,  4772728*2,     1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8086/10",      CPU_8086,  2, 10000000,       1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8086/12",      CPU_8086,  3, 12000000,       1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"8086/16",      CPU_8086,  4, 16000000,       1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_pc1512[] =
{
        /*8086 Amstrad*/
        {"8086/8",       CPU_8086,  1,  8000000,       1, 0, 0, 0, 0, 0, 0,0,0,0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_286[] =
{
        /*286*/
        {"286/6",        CPU_286,   0,  6000000, 1, 0, 0, 0, 0, 0, 2,2,2,2},
        {"286/8",        CPU_286,   1,  8000000, 1, 0, 0, 0, 0, 0, 2,2,2,2},
        {"286/10",       CPU_286,   2, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2},
        {"286/12",       CPU_286,   3, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3},
        {"286/16",       CPU_286,   4, 16000000, 1, 0, 0, 0, 0, 0, 3,3,3,3},
        {"286/20",       CPU_286,   5, 20000000, 1, 0, 0, 0, 0, 0, 4,4,4,4},
        {"286/25",       CPU_286,   6, 25000000, 1, 0, 0, 0, 0, 0, 4,4,4,4},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ibmat[] =
{
        /*286*/
        {"286/6",        CPU_286,   0,  6000000, 1, 0, 0, 0, 0, 0, 3,3,3,3},
        {"286/8",        CPU_286,   0,  8000000, 1, 0, 0, 0, 0, 0, 3,3,3,3},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ps1_m2011[] =
{
        /*286*/
        {"286/10",       CPU_286,   2, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ps2_m30_286[] =
{
        /*286*/
        {"286/10",       CPU_286,   2, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2},
        {"286/12",       CPU_286,   3, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3},
        {"286/16",       CPU_286,   4, 16000000, 1, 0, 0, 0, 0, 0, 3,3,3,3},
        {"286/20",       CPU_286,   5, 20000000, 1, 0, 0, 0, 0, 0, 4,4,4,4},
        {"286/25",       CPU_286,   6, 25000000, 1, 0, 0, 0, 0, 0, 4,4,4,4},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_i386SX[] =
{
        /*i386SX*/
        {"i386SX/16",    CPU_386SX, 0, 16000000, 1, 0, 0x2308, 0, 0, 0, 3,3,3,3},
        {"i386SX/20",    CPU_386SX, 1, 20000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3},
        {"i386SX/25",    CPU_386SX, 2, 25000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3},
        {"i386SX/33",    CPU_386SX, 3, 33333333, 1, 0, 0x2308, 0, 0, 0, 6,6,3,3},
        {"",             -1,        0, 0, 0}
};

CPU cpus_i386DX[] =
{
        /*i386DX*/
        {"i386DX/16",    CPU_386DX, 0, 16000000, 1, 0, 0x0308, 0, 0, 0, 3,3,3,3},
        {"i386DX/20",    CPU_386DX, 1, 20000000, 1, 0, 0x0308, 0, 0, 0, 4,4,3,3},
        {"i386DX/25",    CPU_386DX, 2, 25000000, 1, 0, 0x0308, 0, 0, 0, 4,4,3,3},
        {"i386DX/33",    CPU_386DX, 3, 33333333, 1, 0, 0x0308, 0, 0, 0, 6,6,3,3},
        {"",             -1,        0, 0, 0}
};

CPU cpus_acer[] =
{
        /*i386SX*/
        {"i386SX/25",    CPU_386SX, 2, 25000000, 1, 0, 0x2308, 0, 0, 0, 4,4,4,4},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am386SX[] =
{
        /*Am386*/
        {"Am386SX/16",   CPU_386SX, 0, 16000000, 1, 0, 0x2308, 0, 0, 0, 3,3,3,3},
        {"Am386SX/20",   CPU_386SX, 1, 20000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3},
        {"Am386SX/25",   CPU_386SX, 2, 25000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3},
        {"Am386SX/33",   CPU_386SX, 3, 33333333, 1, 0, 0x2308, 0, 0, 0, 6,6,3,3},
        {"Am386SX/40",   CPU_386SX, 4, 40000000, 1, 0, 0x2308, 0, 0, 0, 7,7,3,3},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am386DX[] =
{
        /*Am386*/
        {"Am386DX/25",   CPU_386DX, 2, 25000000, 1, 0, 0x0308, 0, 0, 0, 4,4,3,3},
        {"Am386DX/33",   CPU_386DX, 3, 33333333, 1, 0, 0x0308, 0, 0, 0, 6,6,3,3},
        {"Am386DX/40",   CPU_386DX, 4, 40000000, 1, 0, 0x0308, 0, 0, 0, 7,7,3,3},
        {"",             -1,        0, 0, 0}
};

CPU cpus_486SLC[] =
{
        /*Cx486SLC*/
        {"Cx486SLC/20",  CPU_486SLC, 1, 20000000, 1, 0, 0x400, 0, 0x0000, 0, 4,4,3,3},
        {"Cx486SLC/25",  CPU_486SLC, 2, 25000000, 1, 0, 0x400, 0, 0x0000, 0, 4,4,3,3},
        {"Cx486SLC/33",  CPU_486SLC, 3, 33333333, 1, 0, 0x400, 0, 0x0000, 0, 6,6,3,3},
        {"Cx486SRx2/32", CPU_486SLC, 3, 32000000, 2, 0, 0x406, 0, 0x0006, 0, 6,6,6,6},
        {"Cx486SRx2/40", CPU_486SLC, 4, 40000000, 2, 0, 0x406, 0, 0x0006, 0, 8,8,6,6},
        {"Cx486SRx2/50", CPU_486SLC, 5, 50000000, 2, 0, 0x406, 0, 0x0006, 0, 8,8,6,6},
        {"",             -1,        0, 0, 0}
};

CPU cpus_486DLC[] =
{
        /*Cx486DLC*/
        {"Cx486DLC/25",  CPU_486DLC, 2, 25000000, 1, 0, 0x401, 0, 0x0001, 0, 4,4,3,3},
        {"Cx486DLC/33",  CPU_486DLC, 3, 33333333, 1, 0, 0x401, 0, 0x0001, 0, 6,6,3,3},
        {"Cx486DLC/40",  CPU_486DLC, 4, 40000000, 1, 0, 0x401, 0, 0x0001, 0, 7,7,3,3},
        {"Cx486DRx2/32", CPU_486DLC, 3, 32000000, 2, 0, 0x407, 0, 0x0007, 0, 6,6,6,6},
        {"Cx486DRx2/40", CPU_486DLC, 4, 40000000, 2, 0, 0x407, 0, 0x0007, 0, 8,8,6,6},
        {"Cx486DRx2/50", CPU_486DLC, 5, 50000000, 2, 0, 0x407, 0, 0x0007, 0, 8,8,6,6},
        {"Cx486DRx2/66", CPU_486DLC, 6, 66666666, 2, 0, 0x407, 0, 0x0007, 0, 12,12,6,6},
        {"",             -1,        0, 0, 0}
};

CPU cpus_i486[] =
{
        /*i486*/
        {"i486SX/16",    CPU_i486SX, 0,  16000000, 1, 16000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 3,3,3,3},
        {"i486SX/20",    CPU_i486SX, 1,  20000000, 1, 20000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 4,4,3,3},
        {"i486SX/25",    CPU_i486SX, 2,  25000000, 1, 25000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 4,4,3,3},
        {"i486SX/33",    CPU_i486SX, 3,  33333333, 1, 33333333, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3},
        {"i486SX2/50",   CPU_i486SX, 5,  50000000, 2, 25000000, 0x45b, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6},
        {"i486DX/25",    CPU_i486DX, 2,  25000000, 1, 25000000, 0x404, 0, 0, CPU_SUPPORTS_DYNAREC, 4,4,3,3},
        {"i486DX/33",    CPU_i486DX, 3,  33333333, 1, 33333333, 0x404, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3},
        {"i486DX/50",    CPU_i486DX, 5,  50000000, 1, 25000000, 0x404, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,4,4},
        {"i486DX2/40",   CPU_i486DX, 4,  40000000, 2, 20000000, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6},
        {"i486DX2/50",   CPU_i486DX, 5,  50000000, 2, 25000000, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6},
        {"i486DX2/66",   CPU_i486DX, 6,  66666666, 2, 33333333, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6},
        {"iDX4/75",      CPU_i486DX, 7,  75000000, 3, 25000000, 0x481, 0x481, 0, CPU_SUPPORTS_DYNAREC, 12,12,9,9}, /*CPUID available on DX4, >= 75 MHz*/
        {"iDX4/100",     CPU_i486DX,10, 100000000, 3, 33333333, 0x481, 0x481, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9}, /*Is on some real Intel DX2s, limit here is pretty arbitary*/
        {"Pentium OverDrive/63",       CPU_PENTIUM,     6,  62500000, 3, 25000000, 0x1531, 0x1531, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,7,7},
        {"Pentium OverDrive/83",       CPU_PENTIUM,     8,  83333333, 3, 33333333, 0x1532, 0x1532, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,8,8},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am486[] =
{
        /*Am486/5x86*/
        {"Am486SX/33",   CPU_Am486SX,  3,  33333333, 1, 33333333, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3},
        {"Am486SX/40",   CPU_Am486SX,  4,  40000000, 1, 20000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 7,7,3,3},
        {"Am486SX2/50",  CPU_Am486SX,  5,  50000000, 2, 25000000, 0x45b, 0x45b, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6}, /*CPUID available on SX2, DX2, DX4, 5x86, >= 50 MHz*/
        {"Am486SX2/66",  CPU_Am486SX,  6,  66666666, 2, 33333333, 0x45b, 0x45b, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6}, /*Isn't on all real AMD SX2s and DX2s, availability here is pretty arbitary (and distinguishes them from the Intel chips)*/
        {"Am486DX/33",   CPU_Am486DX,  3,  33333333, 1, 33333333, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3},
        {"Am486DX/40",   CPU_Am486DX,  4,  40000000, 1, 20000000, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 7,7,3,3},
        {"Am486DX2/50",  CPU_Am486DX,  5,  50000000, 2, 25000000, 0x470, 0x470, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6},
        {"Am486DX2/66",  CPU_Am486DX,  6,  66666666, 2, 33333333, 0x470, 0x470, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6},
        {"Am486DX2/80",  CPU_Am486DX,  8,  80000000, 2, 20000000, 0x470, 0x470, 0, CPU_SUPPORTS_DYNAREC, 14,14,6,6},
        {"Am486DX4/75",  CPU_Am486DX,  7,  75000000, 3, 25000000, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 12,12,9,9},
        {"Am486DX4/90",  CPU_Am486DX,  9,  90000000, 3, 30000000, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 15,15,9,9},
        {"Am486DX4/100", CPU_Am486DX, 10, 100000000, 3, 33333333, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 15,15,9,9},
        {"Am486DX4/120", CPU_Am486DX, 11, 120000000, 3, 20000000, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 21,21,9,9},
        {"Am5x86/P75",   CPU_Am486DX, 12, 133333333, 4, 33333333, 0x4e0, 0x4e0, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12},
        {"Am5x86/P75+",  CPU_Am486DX, 13, 160000000, 4, 20000000, 0x4e0, 0x4e0, 0, CPU_SUPPORTS_DYNAREC, 28,28,12,12},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Cx486[] =
{
        /*Cx486/5x86*/
        {"Cx486S/25",    CPU_Cx486S,   2,  25000000, 1, 25000000, 0x420, 0, 0x0010, CPU_SUPPORTS_DYNAREC, 4,4,3,3},
        {"Cx486S/33",    CPU_Cx486S,   3,  33333333, 1, 33333333, 0x420, 0, 0x0010, CPU_SUPPORTS_DYNAREC, 6,6,3,3},
        {"Cx486S/40",    CPU_Cx486S,   4,  40000000, 1, 20000000, 0x420, 0, 0x0010, CPU_SUPPORTS_DYNAREC, 7,7,3,3},
        {"Cx486DX/33",   CPU_Cx486DX,  3,  33333333, 1, 33333333, 0x430, 0, 0x051a, CPU_SUPPORTS_DYNAREC, 6,6,3,3},
        {"Cx486DX/40",   CPU_Cx486DX,  4,  40000000, 1, 20000000, 0x430, 0, 0x051a, CPU_SUPPORTS_DYNAREC, 7,7,3,3},
        {"Cx486DX2/50",  CPU_Cx486DX,  5,  50000000, 2, 25000000, 0x430, 0, 0x081b, CPU_SUPPORTS_DYNAREC, 8,8,6,6},
        {"Cx486DX2/66",  CPU_Cx486DX,  6,  66666666, 2, 33333333, 0x430, 0, 0x0b1b, CPU_SUPPORTS_DYNAREC, 12,12,6,6},
        {"Cx486DX2/80",  CPU_Cx486DX,  8,  80000000, 2, 20000000, 0x430, 0, 0x311b, CPU_SUPPORTS_DYNAREC, 14,14,16,16},
        {"Cx486DX4/75",  CPU_Cx486DX,  7,  75000000, 3, 25000000, 0x480, 0, 0x361f, CPU_SUPPORTS_DYNAREC, 12,12,9,9},
        {"Cx486DX4/100", CPU_Cx486DX, 10, 100000000, 3, 33333333, 0x480, 0, 0x361f, CPU_SUPPORTS_DYNAREC, 15,15,9,9},
        {"Cx5x86/100",   CPU_Cx5x86,  10, 100000000, 3, 33333333, 0x480, 0, 0x002f, CPU_SUPPORTS_DYNAREC, 15,15,9,9},
        {"Cx5x86/120",   CPU_Cx5x86,  11, 120000000, 3, 20000000, 0x480, 0, 0x002f, CPU_SUPPORTS_DYNAREC, 21,21,9,9},
        {"Cx5x86/133",   CPU_Cx5x86,  12, 133333333, 4, 33333333, 0x480, 0, 0x002f, CPU_SUPPORTS_DYNAREC, 24,24,12,12},
        {"",             -1,        0, 0, 0}
};

 CPU cpus_6x86[] =
 {
          /*Cyrix 6x86*/
          {"6x86-P90",  CPU_Cx6x86, 17,     80000000, 3, 40000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 8,8,6,6},
          {"6x86-PR120+",  CPU_Cx6x86, 17,   100000000, 3, 25000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6},
          {"6x86-PR133+",  CPU_Cx6x86, 17,   110000000, 3, 27500000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6},
          {"6x86-PR150+",  CPU_Cx6x86, 17,   120000000, 3, 30000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
          {"6x86-PR166+",  CPU_Cx6x86, 17,   133333333, 3, 33333333, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
          {"6x86-PR200+",  CPU_Cx6x86, 17,   150000000, 3, 37500000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},

          /*Cyrix 6x86L*/
          {"6x86L-PR133+",  CPU_Cx6x86L, 19,   110000000, 3, 27500000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6},
          {"6x86L-PR150+",  CPU_Cx6x86L, 19,   120000000, 3, 30000000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
          {"6x86L-PR166+",  CPU_Cx6x86L, 19,   133333333, 3, 33333333, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
          {"6x86L-PR200+",  CPU_Cx6x86L, 19,   150000000, 3, 37500000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
          
          /*Cyrix 6x86MX*/
          {"6x86MX-PR166",  CPU_Cx6x86MX, 18, 133333333, 3, 33333333, 0x600, 0x600, 0x0451, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
          {"6x86MX-PR200",  CPU_Cx6x86MX, 18, 166666666, 3, 33333333, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
          {"6x86MX-PR233",  CPU_Cx6x86MX, 18, 188888888, 3, 37500000, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
          {"6x86MX-PR266",  CPU_Cx6x86MX, 18, 207500000, 3, 41666667, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7},
          {"6x86MX-PR300",  CPU_Cx6x86MX, 18, 233333333, 3, 33333333, 0x600, 0x600, 0x0454, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,7,7},
          {"6x86MX-PR333",  CPU_Cx6x86MX, 18, 250000000, 3, 41666667, 0x600, 0x600, 0x0453, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 20,20,9,9},
          {"6x86MX-PR366",  CPU_Cx6x86MX, 18, 250000000, 3, 33333333, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12},
          {"6x86MX-PR400",  CPU_Cx6x86MX, 18, 285000000, 3, 31666667, 0x600, 0x600, 0x0453, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
          {"",             -1,        0, 0, 0}
 };
 
 

CPU cpus_WinChip[] =
{
        /*IDT WinChip*/
        {"WinChip 75",   CPU_WINCHIP,  7,  75000000, 2, 25000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 8,8,4,4},
        {"WinChip 90",   CPU_WINCHIP,  9,  90000000, 2, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 9,9,4,4},
        {"WinChip 100",  CPU_WINCHIP, 10, 100000000, 2, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 9,9,4,4},
        {"WinChip 120",  CPU_WINCHIP, 11, 120000000, 2, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6},
        {"WinChip 133",  CPU_WINCHIP, 12, 133333333, 2, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6},
        {"WinChip 150",  CPU_WINCHIP, 13, 150000000, 3, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 15,15,7,7},
        {"WinChip 166",  CPU_WINCHIP, 15, 166666666, 3, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 15,15,7,7},
        {"WinChip 180",  CPU_WINCHIP, 16, 180000000, 3, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9},
        {"WinChip 200",  CPU_WINCHIP, 17, 200000000, 3, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9},
        {"WinChip 225",  CPU_WINCHIP, 17, 225000000, 3, 37500000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9},
        {"WinChip 240",  CPU_WINCHIP, 17, 240000000, 6, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Pentium5V[] =
{
        /*Intel Pentium (5V, socket 4)*/
        {"Pentium 60",       CPU_PENTIUM,     6,  60000000, 1, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3},
        {"Pentium 66",       CPU_PENTIUM,     6,  66666666, 1, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3},
        {"Pentium OverDrive 120",CPU_PENTIUM,    14, 120000000, 2, 30000000, 0x51A, 0x51A, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"Pentium OverDrive 133",CPU_PENTIUM,    16, 133333333, 2, 33333333, 0x51A, 0x51A, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"",             -1,        0, 0, 0}
};

CPU cpus_PentiumS5[] =
{
        /*Intel Pentium (Socket 5)*/
        {"Pentium 75",       CPU_PENTIUM,     9,  75000000, 2, 25000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4},
        {"Pentium 90",       CPU_PENTIUM,    12,  90000000, 2, 30000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4},
        {"Pentium 100/50",   CPU_PENTIUM,    13, 100000000, 2, 25000000, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6},
        {"Pentium 100/66",   CPU_PENTIUM,    13, 100000000, 2, 33333333, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4},
        {"Pentium 120",      CPU_PENTIUM,    14, 120000000, 2, 30000000, 0x526, 0x526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"Pentium OverDrive 125",CPU_PENTIUM,15, 125000000, 3, 25000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7},
        {"Pentium OverDrive 150",CPU_PENTIUM,17, 150000000, 3, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive 166",CPU_PENTIUM,17, 166666666, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive MMX 125",       CPU_PENTIUMMMX,15,125000000, 3, 25000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7},
        {"Pentium OverDrive MMX 150/60",    CPU_PENTIUMMMX,17,150000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive MMX 166",       CPU_PENTIUMMMX,19,166000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive MMX 180",       CPU_PENTIUMMMX,20,180000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"Pentium OverDrive MMX 200",       CPU_PENTIUMMMX,21,200000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Pentium[] =
{
        /*Intel Pentium*/
        {"Pentium 75",       CPU_PENTIUM,     9,  75000000, 2, 25000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4},
        {"Pentium 90",       CPU_PENTIUM,    12,  90000000, 2, 30000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4},
        {"Pentium 100/50",   CPU_PENTIUM,    13, 100000000, 2, 25000000, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6},
        {"Pentium 100/66",   CPU_PENTIUM,    13, 100000000, 2, 33333333, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4},
        {"Pentium 120",      CPU_PENTIUM,    14, 120000000, 2, 30000000, 0x526, 0x526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"Pentium 133",      CPU_PENTIUM,    16, 133333333, 2, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"Pentium 150",      CPU_PENTIUM,    17, 150000000, 3, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium 166",      CPU_PENTIUM,    19, 166666666, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium 200",      CPU_PENTIUM,    21, 200000000, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"Pentium MMX 166",  CPU_PENTIUMMMX, 19, 166666666, 3, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium MMX 200",  CPU_PENTIUMMMX, 21, 200000000, 3, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"Pentium MMX 233",  CPU_PENTIUMMMX, 24, 233333333, 4, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10},
        {"Mobile Pentium MMX 120",  CPU_PENTIUMMMX, 14, 120000000, 2, 30000000, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"Mobile Pentium MMX 133",  CPU_PENTIUMMMX, 16, 133333333, 2, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6},
        {"Mobile Pentium MMX 150",  CPU_PENTIUMMMX, 17, 150000000, 3, 30000000, 0x544, 0x544, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Mobile Pentium MMX 166",  CPU_PENTIUMMMX, 19, 166666666, 3, 33333333, 0x544, 0x544, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Mobile Pentium MMX 200",  CPU_PENTIUMMMX, 21, 200000000, 3, 33333333, 0x581, 0x581, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"Mobile Pentium MMX 233",  CPU_PENTIUMMMX, 24, 233333333, 4, 33333333, 0x581, 0x581, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10},
        {"Mobile Pentium MMX 266",  CPU_PENTIUMMMX, 26, 266666666, 4, 33333333, 0x582, 0x582, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12},
        {"Mobile Pentium MMX 300",  CPU_PENTIUMMMX, 28, 300000000, 5, 33333333, 0x582, 0x582, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13},
        {"Pentium OverDrive 125",CPU_PENTIUM,15, 125000000, 3, 25000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7},
        {"Pentium OverDrive 150",CPU_PENTIUM,17, 150000000, 3, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive 166",CPU_PENTIUM,17, 166666666, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive MMX 125",       CPU_PENTIUMMMX,15,125000000, 3, 25000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7},
        {"Pentium OverDrive MMX 150/60",    CPU_PENTIUMMMX,17,150000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive MMX 166",       CPU_PENTIUMMMX,19,166000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7},
        {"Pentium OverDrive MMX 180",       CPU_PENTIUMMMX,20,180000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"Pentium OverDrive MMX 200",       CPU_PENTIUMMMX,21,200000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9},
        {"",             -1,        0, 0, 0}
};

void cpu_set_edx()
{
        EDX = models[model].cpu[cpu_manufacturer].cpus[cpu].edx_reset;
}

void cpu_set()
{
        CPU *cpu_s;
        
        if (!models[model].cpu[cpu_manufacturer].cpus)
        {
                /*CPU is invalid, set to default*/
                cpu_manufacturer = 0;
                cpu = 0;
        }
        
        cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];

        CPUID    = cpu_s->cpuid_model;
        cpuspeed = cpu_s->speed;
        is8086   = (cpu_s->cpu_type > CPU_8088);
        is386    = (cpu_s->cpu_type >= CPU_386SX);
        is486    = (cpu_s->cpu_type >= CPU_i486SX) || (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC);
        hasfpu   = (cpu_s->cpu_type >= CPU_i486DX);
         cpu_iscyrix = (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC || cpu_s->cpu_type == CPU_Cx486S || cpu_s->cpu_type == CPU_Cx486DX || cpu_s->cpu_type == CPU_Cx5x86 || cpu_s->cpu_type == CPU_Cx6x86 || cpu_s->cpu_type == CPU_Cx6x86MX || cpu_s->cpu_type == CPU_Cx6x86L || cpu_s->cpu_type == CPU_CxGX1);
        cpu_16bitbus = (cpu_s->cpu_type == CPU_286 || cpu_s->cpu_type == CPU_386SX || cpu_s->cpu_type == CPU_486SLC);
        if (cpu_s->multi) 
           cpu_busspeed = cpu_s->rspeed / cpu_s->multi;
        cpu_multi = cpu_s->multi;
        cpu_hasrdtsc = 0;
        cpu_hasMMX = 0;
        cpu_hasMSR = 0;
        cpu_hasCR4 = 0;
        ccr0 = ccr1 = ccr2 = ccr3 = ccr4 = ccr5 = ccr6 = 0;

        cpu_update_waitstates();
        
        isa_cycles = (int)(((int64_t)cpu_s->rspeed << ISA_CYCLES_SHIFT) / 8000000ll);

        if (cpu_s->pci_speed)
        {
                pci_nonburst_time = 4*cpu_s->rspeed / cpu_s->pci_speed;
                pci_burst_time = cpu_s->rspeed / cpu_s->pci_speed;
        }
        else
        {
                pci_nonburst_time = 4;
                pci_burst_time = 1;
        }
        pclog("PCI burst=%i nonburst=%i\n", pci_burst_time, pci_nonburst_time);

        if (cpu_iscyrix)
           io_sethandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
        else
           io_removehandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
        
        pclog("hasfpu - %i\n",hasfpu);
        pclog("is486 - %i  %i\n",is486,cpu_s->cpu_type);

        x86_setopcodes(ops_386, ops_386_0f, dynarec_ops_386, dynarec_ops_386_0f);

        if (hasfpu)
        {
                x86_dynarec_opcodes_d8_a16 = dynarec_ops_fpu_d8_a16;
                x86_dynarec_opcodes_d8_a32 = dynarec_ops_fpu_d8_a32;
                x86_dynarec_opcodes_d9_a16 = dynarec_ops_fpu_d9_a16;
                x86_dynarec_opcodes_d9_a32 = dynarec_ops_fpu_d9_a32;
                x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_da_a16;
                x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_da_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_db_a16;
                x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_db_a32;
                x86_dynarec_opcodes_dc_a16 = dynarec_ops_fpu_dc_a16;
                x86_dynarec_opcodes_dc_a32 = dynarec_ops_fpu_dc_a32;
                x86_dynarec_opcodes_dd_a16 = dynarec_ops_fpu_dd_a16;
                x86_dynarec_opcodes_dd_a32 = dynarec_ops_fpu_dd_a32;
                x86_dynarec_opcodes_de_a16 = dynarec_ops_fpu_de_a16;
                x86_dynarec_opcodes_de_a32 = dynarec_ops_fpu_de_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_df_a16;
                x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_df_a32;
        }
        else
        {
                x86_dynarec_opcodes_d8_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_d8_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_d9_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_d9_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_da_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_da_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_db_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_dc_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_dc_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_dd_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_dd_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_de_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_de_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_df_a32 = dynarec_ops_nofpu_a32;
        }
        codegen_timing_set(&codegen_timing_486);

        if (hasfpu)
        {
                x86_opcodes_d8_a16 = ops_fpu_d8_a16;
                x86_opcodes_d8_a32 = ops_fpu_d8_a32;
                x86_opcodes_d9_a16 = ops_fpu_d9_a16;
                x86_opcodes_d9_a32 = ops_fpu_d9_a32;
                x86_opcodes_da_a16 = ops_fpu_da_a16;
                x86_opcodes_da_a32 = ops_fpu_da_a32;
                x86_opcodes_db_a16 = ops_fpu_db_a16;
                x86_opcodes_db_a32 = ops_fpu_db_a32;
                x86_opcodes_dc_a16 = ops_fpu_dc_a16;
                x86_opcodes_dc_a32 = ops_fpu_dc_a32;
                x86_opcodes_dd_a16 = ops_fpu_dd_a16;
                x86_opcodes_dd_a32 = ops_fpu_dd_a32;
                x86_opcodes_de_a16 = ops_fpu_de_a16;
                x86_opcodes_de_a32 = ops_fpu_de_a32;
                x86_opcodes_df_a16 = ops_fpu_df_a16;
                x86_opcodes_df_a32 = ops_fpu_df_a32;
        }
        else
        {
                x86_opcodes_d8_a16 = ops_nofpu_a16;
                x86_opcodes_d8_a32 = ops_nofpu_a32;
                x86_opcodes_d9_a16 = ops_nofpu_a16;
                x86_opcodes_d9_a32 = ops_nofpu_a32;
                x86_opcodes_da_a16 = ops_nofpu_a16;
                x86_opcodes_da_a32 = ops_nofpu_a32;
                x86_opcodes_db_a16 = ops_nofpu_a16;
                x86_opcodes_db_a32 = ops_nofpu_a32;
                x86_opcodes_dc_a16 = ops_nofpu_a16;
                x86_opcodes_dc_a32 = ops_nofpu_a32;
                x86_opcodes_dd_a16 = ops_nofpu_a16;
                x86_opcodes_dd_a32 = ops_nofpu_a32;
                x86_opcodes_de_a16 = ops_nofpu_a16;
                x86_opcodes_de_a32 = ops_nofpu_a32;
                x86_opcodes_df_a16 = ops_nofpu_a16;
                x86_opcodes_df_a32 = ops_nofpu_a32;
        }

        memset(&msr, 0, sizeof(msr));

        timing_misaligned = 0;
        cpu_cyrix_alignment = 0;
        
        switch (cpu_s->cpu_type)
        {
                case CPU_8088:
                case CPU_8086:
                break;
                
                case CPU_286:
                x86_setopcodes(ops_286, ops_286_0f, dynarec_ops_286, dynarec_ops_286_0f);
                timing_rr  = 2;   /*register dest - register src*/
                timing_rm  = 7;   /*register dest - memory src*/
                timing_mr  = 7;   /*memory dest   - register src*/
                timing_mm  = 7;   /*memory dest   - memory src*/
                timing_rml = 9;   /*register dest - memory src long*/
                timing_mrl = 11;  /*memory dest   - register src long*/
                timing_mml = 11;  /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3;   /*branch not taken*/
                timing_int = 0;
                timing_int_rm       = 23;
                timing_int_v86      = 0;
                timing_int_pm       = 40;
                timing_int_pm_outer = 78;
                timing_iret_rm       = 17;
                timing_iret_v86      = 0;
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 55;
                timing_call_rm            = 13;
                timing_call_pm            = 26;
                timing_call_pm_gate       = 52;
                timing_call_pm_gate_inner = 82;
                timing_retf_rm       = 15;
                timing_retf_pm       = 25;
                timing_retf_pm_outer = 55;
                timing_jmp_rm      = 11;
                timing_jmp_pm      = 23;
                timing_jmp_pm_gate = 38;
                break;

                case CPU_386SX:
                timing_rr  = 2;   /*register dest - register src*/
                timing_rm  = 6;   /*register dest - memory src*/
                timing_mr  = 7;   /*memory dest   - register src*/
                timing_mm  = 6;   /*memory dest   - memory src*/
                timing_rml = 8;   /*register dest - memory src long*/
                timing_mrl = 11;  /*memory dest   - register src long*/
                timing_mml = 10;  /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3;   /*branch not taken*/
                timing_int = 0;
                timing_int_rm       = 37;
                timing_int_v86      = 59;
                timing_int_pm       = 99;
                timing_int_pm_outer = 119;
                timing_iret_rm       = 22;
                timing_iret_v86      = 60;
                timing_iret_pm       = 38;
                timing_iret_pm_outer = 82;
                timing_call_rm            = 17;
                timing_call_pm            = 34;
                timing_call_pm_gate       = 52;
                timing_call_pm_gate_inner = 86;
                timing_retf_rm       = 18;
                timing_retf_pm       = 32;
                timing_retf_pm_outer = 68;
                timing_jmp_rm      = 12;
                timing_jmp_pm      = 27;
                timing_jmp_pm_gate = 45;
                break;

                case CPU_386DX:
                timing_rr  = 2; /*register dest - register src*/
                timing_rm  = 6; /*register dest - memory src*/
                timing_mr  = 7; /*memory dest   - register src*/
                timing_mm  = 6; /*memory dest   - memory src*/
                timing_rml = 6; /*register dest - memory src long*/
                timing_mrl = 7; /*memory dest   - register src long*/
                timing_mml = 6; /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3; /*branch not taken*/
                timing_int = 0;
                timing_int_rm       = 37;
                timing_int_v86      = 59;
                timing_int_pm       = 99;
                timing_int_pm_outer = 119;
                timing_iret_rm       = 22;
                timing_iret_v86      = 60;
                timing_iret_pm       = 38;
                timing_iret_pm_outer = 82;
                timing_call_rm            = 17;
                timing_call_pm            = 34;
                timing_call_pm_gate       = 52;
                timing_call_pm_gate_inner = 86;
                timing_retf_rm       = 18;
                timing_retf_pm       = 32;
                timing_retf_pm_outer = 68;
                timing_jmp_rm      = 12;
                timing_jmp_pm      = 27;
                timing_jmp_pm_gate = 45;
                break;
                
                case CPU_486SLC:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 3; /*register dest - memory src*/
                timing_mr  = 5; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 5; /*register dest - memory src long*/
                timing_mrl = 7; /*memory dest   - register src long*/
                timing_mml = 7;
                timing_bt  = 6-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                /*unknown*/
                timing_int = 4;
                timing_int_rm       = 14;
                timing_int_v86      = 82;
                timing_int_pm       = 49;
                timing_int_pm_outer = 77;
                timing_iret_rm       = 14;
                timing_iret_v86      = 66;
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 66;
                timing_call_rm = 12;
                timing_call_pm = 30;
                timing_call_pm_gate = 41;
                timing_call_pm_gate_inner = 83;
                timing_retf_rm       = 13;
                timing_retf_pm       = 26;
                timing_retf_pm_outer = 61;
                timing_jmp_rm      = 9;
                timing_jmp_pm      = 26;
                timing_jmp_pm_gate = 37;
                timing_misaligned = 3;
                break;
                
                case CPU_486DLC:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 3; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 3; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 6-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                /*unknown*/
                timing_int = 4;
                timing_int_rm       = 14;
                timing_int_v86      = 82;
                timing_int_pm       = 49;
                timing_int_pm_outer = 77;
                timing_iret_rm       = 14;
                timing_iret_v86      = 66;
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 66;
                timing_call_rm = 12;
                timing_call_pm = 30;
                timing_call_pm_gate = 41;
                timing_call_pm_gate_inner = 83;
                timing_retf_rm       = 13;
                timing_retf_pm       = 26;
                timing_retf_pm_outer = 61;
                timing_jmp_rm      = 9;
                timing_jmp_pm      = 26;
                timing_jmp_pm_gate = 37;
                timing_misaligned = 3;
                break;
                
                case CPU_i486SX:
                case CPU_i486DX:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                timing_int = 4;
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 15;
                timing_iret_v86      = 36; /*unknown*/
                timing_iret_pm       = 20;
                timing_iret_pm_outer = 36;
                timing_call_rm = 18;
                timing_call_pm = 20;
                timing_call_pm_gate = 35;
                timing_call_pm_gate_inner = 69;
                timing_retf_rm       = 13;
                timing_retf_pm       = 17;
                timing_retf_pm_outer = 35;
                timing_jmp_rm      = 17;
                timing_jmp_pm      = 19;
                timing_jmp_pm_gate = 32;
                timing_misaligned = 3;
                break;

                case CPU_Am486SX:
                case CPU_Am486DX:
                /*AMD timing identical to Intel*/
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                timing_int = 4;
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 15;
                timing_iret_v86      = 36; /*unknown*/
                timing_iret_pm       = 20;
                timing_iret_pm_outer = 36;
                timing_call_rm = 18;
                timing_call_pm = 20;
                timing_call_pm_gate = 35;
                timing_call_pm_gate_inner = 69;
                timing_retf_rm       = 13;
                timing_retf_pm       = 17;
                timing_retf_pm_outer = 35;
                timing_jmp_rm      = 17;
                timing_jmp_pm      = 19;
                timing_jmp_pm_gate = 32;
                timing_misaligned = 3;
                break;
                
                case CPU_Cx486S:
                case CPU_Cx486DX:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 3; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 3; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 4-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                timing_int = 4;
                timing_int_rm       = 14;
                timing_int_v86      = 82;
                timing_int_pm       = 49;
                timing_int_pm_outer = 77;
                timing_iret_rm       = 14;
                timing_iret_v86      = 66; /*unknown*/
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 66;
                timing_call_rm = 12;
                timing_call_pm = 30;
                timing_call_pm_gate = 41;
                timing_call_pm_gate_inner = 83;
                timing_retf_rm       = 13;
                timing_retf_pm       = 26;
                timing_retf_pm_outer = 61;
                timing_jmp_rm      = 9;
                timing_jmp_pm      = 26;
                timing_jmp_pm_gate = 37;
                timing_misaligned = 3;
                break;
                
                case CPU_Cx5x86:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 5-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                timing_int = 0;
                timing_int_rm       = 9;
                timing_int_v86      = 82; /*unknown*/
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 4;
                timing_call_pm = 15;
                timing_call_pm_gate = 26;
                timing_call_pm_gate_inner = 35;
                timing_retf_rm       = 4;
                timing_retf_pm       = 7;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 5;
                timing_jmp_pm      = 7;
                timing_jmp_pm_gate = 17;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                break;

                case CPU_WINCHIP:
                x86_setopcodes(ops_386, ops_winchip_0f, dynarec_ops_386, dynarec_ops_winchip_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                /*unknown*/
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 4;
                timing_call_pm = 15;
                timing_call_pm_gate = 26;
                timing_call_pm_gate_inner = 35;
                timing_retf_rm       = 4;
                timing_retf_pm       = 7;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 5;
                timing_jmp_pm      = 7;
                timing_jmp_pm_gate = 17;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                codegen_timing_set(&codegen_timing_winchip);
                break;

                case CPU_PENTIUM:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int = 6;
                timing_int_rm       = 11;
                timing_int_v86      = 54;
                timing_int_pm       = 25;
                timing_int_pm_outer = 42;
                timing_iret_rm       = 7;
                timing_iret_v86      = 27; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 27;
                timing_call_rm = 4;
                timing_call_pm = 4;
                timing_call_pm_gate = 22;
                timing_call_pm_gate_inner = 44;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 3;
                timing_jmp_pm      = 3;
                timing_jmp_pm_gate = 18;
                timing_misaligned = 3;
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 0;
                cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                codegen_timing_set(&codegen_timing_pentium);
                break;

                case CPU_PENTIUMMMX:
                x86_setopcodes(ops_386, ops_pentiummmx_0f, dynarec_ops_386, dynarec_ops_pentiummmx_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                timing_int = 6;
                timing_int_rm       = 11;
                timing_int_v86      = 54;
                timing_int_pm       = 25;
                timing_int_pm_outer = 42;
                timing_iret_rm       = 7;
                timing_iret_v86      = 27; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 27;
                timing_call_rm = 4;
                timing_call_pm = 4;
                timing_call_pm_gate = 22;
                timing_call_pm_gate_inner = 44;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 3;
                timing_jmp_pm      = 3;
                timing_jmp_pm_gate = 18;
                timing_misaligned = 3;
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 1;
                cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                codegen_timing_set(&codegen_timing_pentium);
                break;

  		case CPU_Cx6x86:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int_rm       = 9;
                timing_int_v86      = 46;
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 3;
                timing_call_pm = 4;
                timing_call_pm_gate = 15;
                timing_call_pm_gate_inner = 26;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 1;
                timing_jmp_pm      = 4;
                timing_jmp_pm_gate = 14;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 0;
                cpu_hasMSR = 0;
                cpu_hasCR4 = 0;
  		codegen_timing_set(&codegen_timing_686);
  		CPUID = 0; /*Disabled on powerup by default*/
                break;

                case CPU_Cx6x86L:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int_rm       = 9;
                timing_int_v86      = 46;
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 3;
                timing_call_pm = 4;
                timing_call_pm_gate = 15;
                timing_call_pm_gate_inner = 26;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 1;
                timing_jmp_pm      = 4;
                timing_jmp_pm_gate = 14;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 0;
                cpu_hasMSR = 0;
                cpu_hasCR4 = 0;
         	codegen_timing_set(&codegen_timing_686);
         	ccr4 = 0x80;
                break;


                case CPU_CxGX1:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 5-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 0;
                cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_PCE;
         	codegen_timing_set(&codegen_timing_686);
                break;

  
                case CPU_Cx6x86MX:
                x86_setopcodes(ops_386, ops_c6x86mx_0f, dynarec_ops_386, dynarec_ops_c6x86mx_0f);
                x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
                x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
                x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
                x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
                x86_opcodes_da_a16 = ops_fpu_686_da_a16;
                x86_opcodes_da_a32 = ops_fpu_686_da_a32;
                x86_opcodes_db_a16 = ops_fpu_686_db_a16;
                x86_opcodes_db_a32 = ops_fpu_686_db_a32;
                x86_opcodes_df_a16 = ops_fpu_686_df_a16;
                x86_opcodes_df_a32 = ops_fpu_686_df_a32;
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int_rm       = 9;
                timing_int_v86      = 46;
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 3;
                timing_call_pm = 4;
                timing_call_pm_gate = 15;
                timing_call_pm_gate_inner = 26;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 1;
                timing_jmp_pm      = 4;
                timing_jmp_pm_gate = 14;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 1;
                cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_PCE;
         	codegen_timing_set(&codegen_timing_686);
         	ccr4 = 0x80;
                break;

                default:
                fatal("cpu_set : unknown CPU type %i\n", cpu_s->cpu_type);
        }
}

void cpu_CPUID()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_i486DX:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x756e6547;
                        EDX = 0x49656e69;
                        ECX = 0x6c65746e;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU; /*FPU*/
                }
                else
                   EAX = 0;
                break;

                case CPU_Am486SX:
                if (!EAX)
                {
                        EAX = 1;
                        EBX = 0x68747541;
                        ECX = 0x444D4163;
                        EDX = 0x69746E65;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = EDX = 0; /*No FPU*/
                }
                else
                   EAX = 0;
                break;

                case CPU_Am486DX:
                if (!EAX)
                {
                        EAX = 1;
                        EBX = 0x68747541;
                        ECX = 0x444D4163;
                        EDX = 0x69746E65;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU; /*FPU*/
                }
                else
                   EAX = 0;
                break;
                
                case CPU_WINCHIP:
                if (!EAX)
                {
                        EAX = 1;
                        if (msr.fcr2 & (1 << 14))
                        {
                                EBX = msr.fcr3 >> 32;
                                ECX = msr.fcr3 & 0xffffffff;
                                EDX = msr.fcr2 >> 32;
                        }
                        else
                        {
                                EBX = 0x746e6543; /*CentaurHauls*/
                                ECX = 0x736c7561;                        
                                EDX = 0x48727561;
                        }
                }
                else if (EAX == 1)
                {
                        EAX = 0x540;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
                        if (msr.fcr & (1 << 9))
                                EDX |= CPUID_MMX;
                }
                else
                   EAX = 0;
                break;

                case CPU_PENTIUM:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x756e6547;
                        EDX = 0x49656e69;
                        ECX = 0x6c65746e;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
                }
                else
                        EAX = 0;
                break;

                case CPU_PENTIUMMMX:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x756e6547;
                        EDX = 0x49656e69;
                        ECX = 0x6c65746e;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                }
                else
                        EAX = 0;
                break;


                case CPU_Cx6x86:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU;
                }
                else
                        EAX = 0;
                break;


                case CPU_Cx6x86L:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_CMPXCHG8B;
                }
                else
                        EAX = 0;
                break;


                case CPU_CxGX1:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
                }
                else
                        EAX = 0;
                break;



                case CPU_Cx6x86MX:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_CMOV | CPUID_MMX;
                }
                else
                        EAX = 0;
                break;

        }
}

void cpu_RDMSR()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_WINCHIP:
                EAX = EDX = 0;
                switch (ECX)
                {
                        case 0x02:
                        EAX = msr.tr1;
                        break;
                        case 0x0e:
                        EAX = msr.tr12;
                        break;
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                        case 0x11:
                        EAX = msr.cesr;
                        break;
                        case 0x107:
                        EAX = msr.fcr;
                        break;
                        case 0x108:
                        EAX = msr.fcr2 & 0xffffffff;
                        EDX = msr.fcr2 >> 32;
                        break;
                        case 0x10a:
                        EAX = cpu_multi & 3;
                        break;
                }
                break;

                case CPU_PENTIUM:
                case CPU_PENTIUMMMX:
                EAX = EDX = 0;
                switch (ECX)
                {
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                }
                break;
                case CPU_Cx6x86:
                case CPU_Cx6x86L:
                case CPU_CxGX1:
                case CPU_Cx6x86MX:
                switch (ECX)
                {
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                }
 		break;
        }
}

void cpu_WRMSR()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_WINCHIP:
                switch (ECX)
                {
                        case 0x02:
                        msr.tr1 = EAX & 2;
                        break;
                        case 0x0e:
                        msr.tr12 = EAX & 0x228;
                        break;
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                        case 0x11:
                        msr.cesr = EAX & 0xff00ff;
                        break;
                        case 0x107:
                        msr.fcr = EAX;
                        cpu_hasMMX = EAX & (1 << 9);
                        if (EAX & (1 << 29))
                                CPUID = 0;
                        else
                                CPUID = models[model].cpu[cpu_manufacturer].cpus[cpu].cpuid_model;
                        break;
                        case 0x108:
                        msr.fcr2 = EAX | ((uint64_t)EDX << 32);
                        break;
                        case 0x109:
                        msr.fcr3 = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;

                case CPU_PENTIUM:
                case CPU_PENTIUMMMX:
                switch (ECX)
                {
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
                case CPU_Cx6x86:
                case CPU_Cx6x86L:
                case CPU_CxGX1:
                case CPU_Cx6x86MX:
                switch (ECX)
                {
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
        }
}

static int cyrix_addr;

void cyrix_write(uint16_t addr, uint8_t val, void *priv)
{
        if (!(addr & 1))
                cyrix_addr = val;
        else switch (cyrix_addr)
        {
                case 0xc0: /*CCR0*/
                ccr0 = val;
                break;
                case 0xc1: /*CCR1*/
                ccr1 = val;
                break;
                case 0xc2: /*CCR2*/
                ccr2 = val;
                break;
                case 0xc3: /*CCR3*/
                ccr3 = val;
                break;
                case 0xe8: /*CCR4*/
                if ((ccr3 & 0xf0) == 0x10)
                {
                        ccr4 = val;
                        if (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type >= CPU_Cx6x86)
                        {
                                if (val & 0x80)
                                        CPUID = models[model].cpu[cpu_manufacturer].cpus[cpu].cpuid_model;
                                else
                                        CPUID = 0;
                        }
                }
                break;
                case 0xe9: /*CCR5*/
                if ((ccr3 & 0xf0) == 0x10)
                        ccr5 = val;
                break;
                case 0xea: /*CCR6*/
                if ((ccr3 & 0xf0) == 0x10)
                        ccr6 = val;
                break;
        }
}

uint8_t cyrix_read(uint16_t addr, void *priv)
{
        if (addr & 1)
        {
                switch (cyrix_addr)
                {
                        case 0xc0: return ccr0;
                        case 0xc1: return ccr1;
                        case 0xc2: return ccr2;
                        case 0xc3: return ccr3;
                        case 0xe8: return ((ccr3 & 0xf0) == 0x10) ? ccr4 : 0xff;
                        case 0xe9: return ((ccr3 & 0xf0) == 0x10) ? ccr5 : 0xff;
                        case 0xea: return ((ccr3 & 0xf0) == 0x10) ? ccr6 : 0xff;
                        case 0xfe: return models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id & 0xff;
                        case 0xff: return models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id >> 8;
                }
                if ((cyrix_addr & ~0xf0) == 0xc0) return 0xff;
                if (cyrix_addr == 0x20 && models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type == CPU_Cx5x86) return 0xff;
        }
        return 0xff;
}

void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f, OpFn *dynarec_opcodes, OpFn *dynarec_opcodes_0f)
{
        x86_opcodes = opcodes;
        x86_opcodes_0f = opcodes_0f;
        x86_dynarec_opcodes = dynarec_opcodes;
        x86_dynarec_opcodes_0f = dynarec_opcodes_0f;
}

void cpu_update_waitstates()
{
        cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];
        
        cpu_prefetch_width = cpu_16bitbus ? 2 : 4;
        
        if (cpu_cache_int_enabled)
        {
                /* Disable prefetch emulation */
                cpu_prefetch_cycles = 0;
        }
        else if (cpu_waitstates && (cpu_s->cpu_type >= CPU_286 && cpu_s->cpu_type <= CPU_386DX))
        {
                /* Waitstates override */
                cpu_prefetch_cycles = cpu_waitstates+1;
                cpu_cycles_read = cpu_waitstates+1;
                cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
                cpu_cycles_write = cpu_waitstates+1;
                cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
        }
        else if (cpu_cache_ext_enabled)
        {
                /* Use cache timings */
                cpu_prefetch_cycles = cpu_s->cache_read_cycles;
                cpu_cycles_read = cpu_s->cache_read_cycles;
                cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu_s->cache_read_cycles;
                cpu_cycles_write = cpu_s->cache_write_cycles;
                cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu_s->cache_write_cycles;
        }
        else
        {
                /* Use memory timings */
                cpu_prefetch_cycles = cpu_s->mem_read_cycles;
                cpu_cycles_read = cpu_s->mem_read_cycles;
                cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu_s->mem_read_cycles;
                cpu_cycles_write = cpu_s->mem_write_cycles;
                cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu_s->mem_write_cycles;
        }
}
