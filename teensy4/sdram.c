/* Startup-only replacement for teensy4/sdram.c in teensy-cores 14e5d77.
 * One W9825G6KH-6I, 32 MiB x16, CS0 at 0x90000000, BL8/CAS3.
 * Call before applications, DMA and other PFD2 consumers start. Destructive.
 * This is not the live-tuning benchmark driver and does not enable a watchdog.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Whole MHz requested, not a promise of an exactly representable clock. */
#ifndef TEENSY_SDRAM_CLOCK_MHZ
#define TEENSY_SDRAM_CLOCK_MHZ 166
#endif
#if (TEENSY_SDRAM_CLOCK_MHZ) < 1 || (TEENSY_SDRAM_CLOCK_MHZ) > 1000
#error "TEENSY_SDRAM_CLOCK_MHZ must be a whole number from 1 to 1000"
#endif
#ifdef TEENSY_SDRAM_216MHZ
#error "Replace TEENSY_SDRAM_216MHZ with TEENSY_SDRAM_CLOCK_MHZ"
#endif

enum { SDRAM_STARTUP_PLL3_PFD1 = 0, SDRAM_STARTUP_PLL2_PFD2 = 1 };
typedef struct {
    uint64_t pll3_numerator, pll2_numerator; /* actual parent Hz * 18 */
    uint8_t pll3_fraction, pll2_fraction;
    bool pll3_available, pll2_available, pll2_retune_allowed;
} sdram_startup_sources_t;
typedef struct {
    uint64_t numerator;
    uint32_t denominator, sdramcr1, sdramcr2, sdramcr3;
    uint32_t refresh_budget_cycles, refresh_interval_cycles;
    uint8_t source, fraction, divider;
} sdram_startup_plan_t;

static bool timing_field(uint32_t ns, uint32_t minimum, uint32_t maximum,
                         unsigned shift, sdram_startup_plan_t *p, uint32_t *reg) {
    const uint64_t divisor = UINT64_C(1000000000) * p->denominator;
    uint32_t cycles = (uint32_t)(((uint64_t)ns * p->numerator + divisor - 1u) / divisor);
    if (cycles < minimum) cycles = minimum;
    if (cycles - 1u > maximum) return false;
    *reg |= (cycles - 1u) << shift;
    return true;
}

static bool make_plan(uint64_t numerator, unsigned fraction, unsigned divider,
                      unsigned source, sdram_startup_plan_t *p) {
    memset(p, 0, sizeof(*p));
    p->numerator = numerator;
    p->denominator = fraction * divider;
    p->source = (uint8_t)source;
    p->fraction = (uint8_t)fraction;
    p->divider = (uint8_t)divider;
    if (!timing_field(42, 1, 15, 20, p, &p->sdramcr1) ||
        !timing_field(42, 1, 15, 16, p, &p->sdramcr1) ||
        !timing_field(12, 2, 7, 13, p, &p->sdramcr1) ||
        !timing_field(67, 1, 31, 8, p, &p->sdramcr1) ||
        !timing_field(18, 1, 15, 4, p, &p->sdramcr1) ||
        !timing_field(18, 1, 15, 0, p, &p->sdramcr1) ||
        !timing_field(72, 1, 255, 0, p, &p->sdramcr2) ||
        !timing_field(60, 2, 255, 8, p, &p->sdramcr2) ||
        !timing_field(60, 2, 255, 16, p, &p->sdramcr2)) return false;

    const uint32_t budget = (uint32_t)((numerator * 64u) /
                           ((uint64_t)p->denominator * 1000u * 8192u));
    if (budget < 34u) return false;
    /* Retain tested prescale10 where possible; shorten it for slow clocks.
     * Raw zero means256, so never encode zero as a short prescaler.
     */
    uint32_t prescale = (budget / 2u - 1u) / 16u;
    if (prescale > 10u) prescale = 10u;
    if (prescale < 1u) return false;
    const uint32_t period = prescale * 16u + 1u;
    uint32_t ticks = budget / period;
    if (ticks > 256u) ticks = 256u;
    if (ticks < 2u) return false;
    p->refresh_budget_cycles = budget;
    p->refresh_interval_cycles = period * ticks;
    p->sdramcr3 = 1u | (prescale << 8) | ((ticks - 1u) << 16) |
                   ((ticks == 256u ? 0u : ticks) << 24);
    return true;
}

static uint64_t frequency_error(const sdram_startup_plan_t *p, uint32_t target) {
    const uint64_t requested = (uint64_t)target * p->denominator;
    return p->numerator > requested ? p->numerator - requested : requested - p->numerator;
}

static bool better_plan(const sdram_startup_plan_t *p, const sdram_startup_plan_t *best,
                        uint32_t target, const sdram_startup_sources_t *s) {
    /* Exact rational comparisons: no Hz rounding or floating point bias. */
    const uint64_t a = frequency_error(p, target) * best->denominator;
    const uint64_t b = frequency_error(best, target) * p->denominator;
    if (a != b) return a < b;
    const uint64_t rate_a = p->numerator * best->denominator;
    const uint64_t rate_b = best->numerator * p->denominator;
    if (rate_a != rate_b) return rate_a > rate_b; /* Exact halfway: higher wins. */
    const bool retune_a = p->source == SDRAM_STARTUP_PLL2_PFD2 && p->fraction != s->pll2_fraction;
    const bool retune_b = best->source == SDRAM_STARTUP_PLL2_PFD2 && best->fraction != s->pll2_fraction;
    if (retune_a != retune_b) return !retune_a;
    if (p->fraction != best->fraction) return p->fraction > best->fraction;
    if (p->source != best->source) return p->source < best->source;
    return p->divider < best->divider;
}

static bool sdram_startup_choose(uint32_t target, const sdram_startup_sources_t *s,
                                 sdram_startup_plan_t *output) {
    if (!s || !output || !target || target > 1000000000u) return false;
    if (s->pll3_available && (s->pll3_numerator != UINT64_C(8640000000) ||
        s->pll3_fraction < 12u || s->pll3_fraction > 35u)) return false;
    if (s->pll2_available && ((s->pll2_numerator != UINT64_C(8640000000) &&
        s->pll2_numerator != UINT64_C(9504000000)) ||
        s->pll2_fraction < 12u || s->pll2_fraction > 35u)) return false;
    sdram_startup_plan_t best, candidate;
    bool found = false;
    for (unsigned source = 0; source < 2; ++source) {
        if (source == SDRAM_STARTUP_PLL3_PFD1 ? !s->pll3_available : !s->pll2_available)
            continue;
        const unsigned first = source == SDRAM_STARTUP_PLL3_PFD1 ? s->pll3_fraction : 12u;
        const unsigned last = source == SDRAM_STARTUP_PLL3_PFD1 ? s->pll3_fraction : 35u;
        const uint64_t numerator = source == SDRAM_STARTUP_PLL3_PFD1 ? s->pll3_numerator : s->pll2_numerator;
        for (unsigned fraction = first; fraction <= last; ++fraction) {
            if (source == SDRAM_STARTUP_PLL2_PFD2 && !s->pll2_retune_allowed &&
                fraction != s->pll2_fraction) continue;
            for (unsigned divider = 1; divider <= 8; ++divider) {
                if (!make_plan(numerator, fraction, divider, source, &candidate)) continue;
                if (!found || better_plan(&candidate, &best, target, s)) {
                    best = candidate;
                    found = true;
                }
            }
        }
    }
    if (found) *output = best;
    return found;
}

#ifndef SDRAM_STARTUP_PLAN_ONLY /* Offline tests compile the same pure planner. */
#include "imxrt.h"
#include "core_pins.h"

static sdram_startup_plan_t active_plan;
static bool initialized;

/* Keep the measured pad settings by default; customize groups independently.
 * DSE=1..7, SRE=0 slow/1 fast, SPEED=0..3. Fixed keeper/hysteresis bits retained.
 */
#define SDRAM_PAD(dse, sre, speed) \
    (0x11000u | ((uint32_t)(dse) << 3) | ((uint32_t)(speed) << 6) | (sre))
#ifndef TEENSY_SDRAM_PAD_CLK
#define TEENSY_SDRAM_PAD_CLK  SDRAM_PAD(7, 1, 3)
#endif
#ifndef TEENSY_SDRAM_PAD_ADDR
#define TEENSY_SDRAM_PAD_ADDR SDRAM_PAD(7, 1, 3)
#endif
#ifndef TEENSY_SDRAM_PAD_DQ
#define TEENSY_SDRAM_PAD_DQ   SDRAM_PAD(7, 1, 3)
#endif
#ifndef TEENSY_SDRAM_PAD_DQS
#define TEENSY_SDRAM_PAD_DQS  SDRAM_PAD(7, 1, 3)
#endif

#define SDRAM_BASE 0x90000000u
#define TIMEOUT_US 2000u

static void barriers(void) { __asm__ volatile("dsb\nisb" ::: "memory"); }
static uint32_t elapsed_ticks(uint32_t us) {
    return (F_CPU_ACTUAL / 1000000u) * us;
}
static void wait_us(uint32_t us) {
    const uint32_t start = ARM_DWT_CYCCNT, ticks = elapsed_ticks(us);
    while ((uint32_t)(ARM_DWT_CYCCNT - start) < ticks) {}
}
static bool wait_mask(volatile uint32_t *reg, uint32_t mask, uint32_t want) {
    const uint32_t start = ARM_DWT_CYCCNT;
    while ((*reg & mask) != want) {
        if ((uint32_t)(ARM_DWT_CYCCNT - start) >= elapsed_ticks(TIMEOUT_US))
            return false;
    }
    return true;
}

void configure_sdram_pins(void) {
    /* EMC00..07=DQ0..7; EMC08=DQM0; EMC09=A0 (previously skipped).
     * EMC30..37=DQ8..15; EMC38=DQM1; EMC26=CLK; EMC39=DQS.
     * EMC40/41 retain the original board's pad configuration, no extra CS mux.
     */
    const uintptr_t pads = (uintptr_t)&IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_00;
    const uintptr_t muxes = (uintptr_t)&IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00;
    for (unsigned i = 0; i < 42; ++i) {
        uint32_t setting = TEENSY_SDRAM_PAD_ADDR;
        if (i <= 8 || (i >= 30 && i <= 38)) setting = TEENSY_SDRAM_PAD_DQ;
        if (i == 26) setting = TEENSY_SDRAM_PAD_CLK;
        if (i == 39) setting = TEENSY_SDRAM_PAD_DQS;
        *(volatile uint32_t *)(pads + 4u * i) = setting;
    }
    for (unsigned i = 0; i < 40; ++i) {
        const bool input = i < 8 || (i >= 30 && i <= 37) || i == 39;
        *(volatile uint32_t *)(muxes + 4u * i) = input ? 0x10u : 0u;
    }
}

/* Keep the existing public command helpers, with bounded error-first polling. */
bool IPCommandComplete(void) {
    const uint32_t start = ARM_DWT_CYCCNT;
    for (;;) {
        const uint32_t flags = SEMC_INTR;
        if (flags & 2u) { SEMC_INTR = flags & 3u; return false; }
        if (flags & 1u) { SEMC_INTR = 1u; return true; }
        if ((uint32_t)(ARM_DWT_CYCCNT - start) >= elapsed_ticks(TIMEOUT_US))
            return false;
    }
}
bool SendIPCommand(uint32_t address, uint16_t command, uint32_t write, uint32_t *read) {
    const uint16_t mode = command & 15u;
    if (mode == 8u && read == NULL) return false;
    SEMC_INTR = 3u; /* DONE/ERR are W1C: never use |= on this register. */
    SEMC_IPCR0 = address;
    SEMC_IPCR1 = 2u;
    SEMC_IPCR2 = 0u;
    if (mode == 9u || mode == 10u) SEMC_IPTXDAT = write;
    SEMC_IPCMD = 0xA55A0000u | command;
    if (!IPCommandComplete()) return false;
    if (mode == 8u) *read = SEMC_IPRXDAT;
    return true;
}

static bool can_retune_pfd2(void) {
    const unsigned flash = (CCM_CSCMR1 >> 29) & 3u;
    if (flash == 0u || flash == 2u) return false;
    if (!(CCM_CBCDR & CCM_CBCDR_PERIPH_CLK_SEL) &&
        ((CCM_CBCMR >> 18) & 3u) == 1u) return false;
    return true;
}

static sdram_startup_sources_t read_sources(void) {
    sdram_startup_sources_t s = {0};
    const uint32_t pll3 = CCM_ANALOG_PLL_USB1, pfd3 = CCM_ANALOG_PFD_480;
    const uint32_t pll2 = CCM_ANALOG_PLL_SYS, pfd2 = CCM_ANALOG_PFD_528;
    s.pll3_numerator = UINT64_C(480000000) * 18u;
    s.pll2_numerator = ((pll2 & CCM_ANALOG_PLL_SYS_DIV_SELECT) ?
                       UINT64_C(528000000) : UINT64_C(480000000)) * 18u;
    s.pll3_fraction = (pfd3 >> 8) & 63u;
    s.pll2_fraction = (pfd2 >> 16) & 63u;
    s.pll2_retune_allowed = can_retune_pfd2();
    const uint32_t usb_need = CCM_ANALOG_PLL_USB1_LOCK |
                             CCM_ANALOG_PLL_USB1_ENABLE | CCM_ANALOG_PLL_USB1_POWER;
    s.pll3_available = (pll3 & usb_need) == usb_need &&
        !(pll3 & (CCM_ANALOG_PLL_USB1_BYPASS | CCM_ANALOG_PLL_USB1_DIV_SELECT)) &&
        !(pfd3 & (1u << 15)) && s.pll3_fraction >= 12u && s.pll3_fraction <= 35u;
    s.pll2_available = !(pll2 & (CCM_ANALOG_PLL_SYS_BYPASS | CCM_ANALOG_PLL_SYS_POWERDOWN)) &&
        (pll2 & CCM_ANALOG_PLL_SYS_LOCK) && (pll2 & CCM_ANALOG_PLL_SYS_ENABLE) &&
        CCM_ANALOG_PLL_SYS_NUM == 0u && !(CCM_ANALOG_PLL_SYS_SS & (1u << 15)) &&
        s.pll2_fraction >= 12u && s.pll2_fraction <= 35u &&
        (s.pll2_retune_allowed || !(pfd2 & (1u << 23)));
    return s;
}

static bool set_pfd2(unsigned fraction) {
    const uint32_t original = CCM_ANALOG_PFD_528;
    if (((original >> 16) & 63u) == fraction && !(original & (1u << 23)))
        return true; /* A shared active source is safe to leave unchanged. */
    if (!can_retune_pfd2()) return false;

    /* These peripherals must still be uninitialized. Temporarily gate their
     * clocks, then restore gate settings. This does not preserve active I/O.
     * Later drivers selecting PFD2 must account for its newly selected rate.
     */
    const uint32_t masks[8] = {
        CCM_CCGR0_TRACE(3),
        CCM_CCGR1_LPSPI1(3) | CCM_CCGR1_LPSPI2(3) |
            CCM_CCGR1_LPSPI3(3) | CCM_CCGR1_LPSPI4(3),
        CCM_CCGR2_CSI(3), CCM_CCGR3_LCDIF_PIX(3), 0u,
        CCM_CCGR5_SPDIF(3), CCM_CCGR6_USDHC1(3) | CCM_CCGR6_USDHC2(3),
        CCM_CCGR7_FLEXSPI2(3)
    };
    uint32_t saved[8];
    const uintptr_t gates = (uintptr_t)&CCM_CCGR0;
    for (unsigned i = 0; i < 8; ++i) {
        volatile uint32_t *gate = (volatile uint32_t *)(gates + 4u * i);
        saved[i] = *gate & masks[i];
        *gate &= ~masks[i];
    }
    barriers();

    bool ok = true;
    if (((original >> 16) & 63u) != fraction) {
        CCM_ANALOG_PFD_528_SET = 1u << 23; /* Gate only PFD2, not PLL2. */
        const uint32_t before = CCM_ANALOG_PFD_528;
        /* One FRAC update. STABLE is a toggle, not a level-ready flag. */
        CCM_ANALOG_PFD_528 = (before & ~(63u << 16)) | (fraction << 16);
        CCM_ANALOG_PFD_528_CLR = 1u << 23;
        ok = wait_mask(&CCM_ANALOG_PFD_528, 1u << 22,
                       (before ^ (1u << 22)) & (1u << 22));
    } else {
        CCM_ANALOG_PFD_528_CLR = 1u << 23;
    }
    wait_us(2);
    for (unsigned i = 0; i < 8; ++i) {
        volatile uint32_t *gate = (volatile uint32_t *)(gates + 4u * i);
        *gate = (*gate & ~masks[i]) | saved[i];
    }
    barriers();
    return ok && !(CCM_ANALOG_PFD_528 & (1u << 23)) &&
           ((CCM_ANALOG_PFD_528 >> 16) & 63u) == fraction;
}

static bool clock_matches(const sdram_startup_plan_t *p) {
    const uint32_t root = CCM_CBCDR;
    const unsigned source = (root & CCM_CBCDR_SEMC_ALT_CLK_SEL) ?
                            SDRAM_STARTUP_PLL3_PFD1 : SDRAM_STARTUP_PLL2_PFD2;
    if (!(root & CCM_CBCDR_SEMC_CLK_SEL) || source != p->source ||
        ((root >> 16) & 7u) + 1u != p->divider ||
        (CCM_CDHIPR & CCM_CDHIPR_SEMC_PODF_BUSY)) return false;
    const sdram_startup_sources_t s = read_sources();
    if (source == SDRAM_STARTUP_PLL3_PFD1)
        return s.pll3_available && s.pll3_fraction == p->fraction && s.pll3_numerator == p->numerator;
    return s.pll2_available && !(CCM_ANALOG_PFD_528 & (1u << 23)) &&
           s.pll2_fraction == p->fraction && s.pll2_numerator == p->numerator;
}

static bool select_sdram_clock(sdram_startup_plan_t *plan) {
    /* In this board's startup, reset_PFD() and set_arm_clock() ran already.
     * Flash must not use the SEMC-derived root even in the default mode.
     */
    if (((CCM_CSCMR1 >> 29) & 3u) == 0u) return false;
    const sdram_startup_sources_t sources = read_sources();
    const uint32_t target = (uint32_t)(TEENSY_SDRAM_CLOCK_MHZ) * 1000000u;
    if (!sdram_startup_choose(target, &sources, plan)) return false;
    CCM_CCGR3 &= ~CCM_CCGR3_SEMC(3);
    barriers();
    uint32_t root = CCM_CBCDR & ~(CCM_CBCDR_SEMC_PODF(7) |
                                  CCM_CBCDR_SEMC_ALT_CLK_SEL | CCM_CBCDR_SEMC_CLK_SEL);
    if (plan->source == SDRAM_STARTUP_PLL2_PFD2 && !set_pfd2(plan->fraction)) return false;
    root |= CCM_CBCDR_SEMC_CLK_SEL | CCM_CBCDR_SEMC_PODF(plan->divider - 1u);
    if (plan->source == SDRAM_STARTUP_PLL3_PFD1) root |= CCM_CBCDR_SEMC_ALT_CLK_SEL;
    CCM_CBCDR = root;
    barriers();
    CCM_CCGR3 |= CCM_CCGR3_SEMC(CCM_CCGR_ON);
    if (!wait_mask(&CCM_CDHIPR, CCM_CDHIPR_SEMC_PODF_BUSY, 0u)) return false;
    if (!clock_matches(plan)) return false;
    wait_us(2);
    return true;
}

uint32_t teensy_sdram_clock_hz(void) {
    if (!initialized || !clock_matches(&active_plan)) return 0u;
    return (uint32_t)((active_plan.numerator + active_plan.denominator / 2u) /
                      active_plan.denominator);
}

bool teensy_sdram_clock_ratio(uint64_t *numerator, uint32_t *denominator) {
    if (!numerator || !denominator || !initialized || !clock_matches(&active_plan)) return false;
    *numerator = active_plan.numerator;
    *denominator = active_plan.denominator;
    return true;
}

bool sdram_init(void) {
    initialized = false;
    ARM_DEMCR |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    if (F_CPU_ACTUAL < 1000000u) return false;
    sdram_startup_plan_t plan;
    if (!select_sdram_clock(&plan)) return false;

    SEMC_MCR = SEMC_MCR_SWRST;
    if (!wait_mask(&SEMC_MCR, SEMC_MCR_SWRST, 0u)) return false;
    for (unsigned i = 0; i < 9; ++i)
        *(volatile uint32_t *)((uintptr_t)&SEMC_BR0 + 4u * i) = 0u;
    SEMC_MCR = SEMC_MCR_MDIS | SEMC_MCR_CTO(0xFF) |
               SEMC_MCR_BTO(0x1F) | SEMC_MCR_DQSMD;
    configure_sdram_pins();
    SEMC_IOCR = 0u;
    SEMC_BMCR0 = SEMC_BMCR0_WQOS(5) | SEMC_BMCR0_WAGE(8) |
                 SEMC_BMCR0_WSH(0x40) | SEMC_BMCR0_WRWS(0x10);
    SEMC_BMCR1 = SEMC_BMCR1_WQOS(5) | SEMC_BMCR1_WAGE(8) |
                 SEMC_BMCR1_WPH(0x60) | SEMC_BMCR1_WRWS(0x24) | SEMC_BMCR1_WBR(0x40);
    SEMC_BR0 = SDRAM_BASE | SEMC_BR_MS(13) | SEMC_BR_VLD;
    SEMC_SDRAMCR0 = SEMC_SDRAMCR0_CL(3) | SEMC_SDRAMCR0_COL(3) |
                    SEMC_SDRAMCR0_BL(3) | SEMC_SDRAMCR0_PS;
    SEMC_SDRAMCR1 = plan.sdramcr1;
    SEMC_SDRAMCR2 = plan.sdramcr2;
    SEMC_SDRAMCR3 = plan.sdramcr3 & ~SEMC_SDRAMCR3_REN;
    SEMC_MCR &= ~SEMC_MCR_MDIS;
    wait_us(200);
    if (!SendIPCommand(SDRAM_BASE, 0x0Fu, 0u, NULL)) return false;
    for (unsigned i = 0; i < 8; ++i)
        if (!SendIPCommand(SDRAM_BASE, 0x0Cu, 0u, NULL)) return false;
    if (!SendIPCommand(SDRAM_BASE, 0x0Au, 0x33u, NULL)) return false;
    wait_us(1); /* Exceeds the two-clock mode-register recovery requirement. */
    SEMC_SDRAMCR3 = plan.sdramcr3;
    barriers();
    if (!wait_mask(&SEMC_STS0, SEMC_STS0_IDLE, SEMC_STS0_IDLE)) return false;
    active_plan = plan;
    initialized = true;
    return true;
}
#endif /* SDRAM_STARTUP_PLAN_ONLY */
