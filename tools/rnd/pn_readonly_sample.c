/* SPDX-License-Identifier: MIT
 * Read-only observer for this R2 AXI AD9361 mapping, Linux 40201abd70d8.
 * Does NOT start calibration, clear W1C status, or write any device register.
 * Parent must separately authorize/coordinate any digital_tune operation.
 */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define AXI_BASE UINT64_C(0x79020000)
#define AXI_SPAN 0x5000u
#define CH 4
struct sample {
    uint64_t us;
    uint32_t adc_status, dac_reset, dac_ctrl, dac_rate;
    uint32_t adc_ctrl[CH], pnctrl[CH], status[CH], dsel[CH], iqctrl[CH];
    uint32_t post_pnctrl[CH], post_dsel[CH];
};
static uint32_t rd(volatile const unsigned char *p, unsigned off)
{
    uint32_t v = *(volatile const uint32_t *)(p + off);
    __sync_synchronize();
    return v;
}
static uint64_t now_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) { perror("clock_gettime"); exit(1); }
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
static unsigned arg(const char *s, unsigned low, unsigned high)
{
    char *e; errno = 0;
    unsigned long v = strtoul(s, &e, 10);
    if (errno || !*s || *e || v < low || v > high) {
        fprintf(stderr, "argument outside [%u,%u]: %s\n", low, high, s); exit(2);
    }
    return (unsigned)v;
}
int main(int argc, char **argv)
{
    if (argc > 3) {
        fprintf(stderr, "usage: %s [seconds=10,1..60] [interval_us=500,100..100000]\n", argv[0]);
        return 2;
    }
    const unsigned seconds = argc > 1 ? arg(argv[1], 1, 60) : 10;
    const unsigned interval = argc > 2 ? arg(argv[2], 100, 100000) : 500;
    const size_t count = (size_t)seconds * 1000000u / interval;
    struct sample *out = calloc(count, sizeof(*out));
    if (!out) { perror("calloc"); return 1; }
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0 || (AXI_BASE % (uint64_t)page)) {
        fprintf(stderr, "unexpected page size/alignment\n"); free(out); return 1;
    }
    size_t len = ((AXI_SPAN + (size_t)page - 1) / (size_t)page) * (size_t)page;
    int fd = open("/dev/mem", O_RDONLY | O_SYNC | O_CLOEXEC);
    if (fd < 0) { perror("open /dev/mem read-only"); free(out); return 1; }
    void *map = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, (off_t)AXI_BASE);
    if (map == MAP_FAILED) { perror("mmap PROT_READ"); close(fd); free(out); return 1; }
    volatile const unsigned char *p = map;
    fprintf(stderr, "READY observer_only base=0x%08" PRIx64 " samples=%zu interval_us=%u; no MMIO writes\n",
            AXI_BASE, count, interval);
    fflush(stderr);
    uint64_t start = now_ns();
    for (size_t n = 0; n < count; n++) {
        struct sample *s = &out[n];
        s->us = (now_ns() - start) / 1000;
        for (unsigned c = 0; c < CH; c++) {
            s->dsel[c] = rd(p, 0x4418u + c * 0x40u);
            s->pnctrl[c] = rd(p, 0x0418u + c * 0x40u);
        }
        for (unsigned c = 0; c < CH; c++) s->status[c] = rd(p, 0x0404u + c * 0x40u);
        s->adc_status = rd(p, 0x005cu);
        s->dac_reset = rd(p, 0x4040u);
        s->dac_ctrl = rd(p, 0x4048u);
        s->dac_rate = rd(p, 0x404cu);
        for (unsigned c = 0; c < CH; c++) {
            s->adc_ctrl[c] = rd(p, 0x0400u + c * 0x40u);
            s->iqctrl[c] = rd(p, 0x4414u + c * 0x40u);
            s->post_pnctrl[c] = rd(p, 0x0418u + c * 0x40u);
            s->post_dsel[c] = rd(p, 0x4418u + c * 0x40u);
        }
        if (n + 1 < count) {
            uint64_t target = start + (uint64_t)(n + 1) * interval * 1000;
            struct timespec t = {(time_t)(target / 1000000000), (long)(target % 1000000000)};
            int rc;
            do { rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); } while (rc == EINTR);
            if (rc) { errno = rc; perror("clock_nanosleep"); munmap(map,len); close(fd); free(out); return 1; }
        }
    }
    munmap(map, len); close(fd);
    puts("# All register columns are hexadecimal. status bit2=PN_ERR bit1=PN_OOS bit0=OVER_RANGE.");
    puts("# tx_candidate=stable all4 DAC source9 and ADC PN_CUSTOM9; not proof of SPI loopback/mute state.");
    printf("us,tx_candidate,transition,adc_status,dac_reset,dac_ctrl,dac_rate");
    for (unsigned c = 0; c < CH; c++) printf(",status%u,adcctrl%u,pnctrl%u,dsel%u,iqctrl%u,postpn%u,postdsel%u", c,c,c,c,c,c,c);
    putchar('\n');
    for (size_t n = 0; n < count; n++) {
        const struct sample *s = &out[n];
        unsigned transition = 0, tx = 1;
        for (unsigned c = 0; c < CH; c++) {
            if (s->pnctrl[c] != s->post_pnctrl[c] || s->dsel[c] != s->post_dsel[c]) transition = 1;
            if (((s->pnctrl[c] >> 16) & 15u) != 9u || (s->dsel[c] & 15u) != 9u) tx = 0;
        }
        if (transition) tx = 0;
        printf("%" PRIu64 ",%u,%u,%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32,
               s->us, tx, transition, s->adc_status, s->dac_reset, s->dac_ctrl, s->dac_rate);
        for (unsigned c = 0; c < CH; c++) printf(",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32,
              s->status[c],s->adc_ctrl[c],s->pnctrl[c],s->dsel[c],s->iqctrl[c],s->post_pnctrl[c],s->post_dsel[c]);
        putchar('\n');
    }
    free(out);
    return ferror(stdout) ? 1 : 0;
}
