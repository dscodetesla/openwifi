/* SPDX-License-Identifier: AGPL-3.0-or-later
 * PlutoSky-R2 fixed-40MHz post-FIR calibration. No command-line options.
 * Select measured RX data-delay and TX clock-delay windows, then confirm PN.
 * The fixed RX clock tap avoids the unstable clock-delay scan branch observed
 * on this board/bitstream. This does not claim a general AD9361 driver fix.
 * Exit 0: selected RX/TX passed, other state restored/read back.
 * Exit 1: precondition/calibration failure, original state restored if changed.
 * Exit 2: signal interrupted, original state restored. Exit 3: restore failure.
 * Does not write flash, DTS, defaults, sampling rate, or kernel tune policy.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

static volatile uint32_t *r;
static volatile sig_atomic_t stopped;
static const char *dbg = "/sys/kernel/debug/iio/iio:device0/";
static const char *sys = "/sys/bus/iio/devices/iio:device0/";
struct state {
 unsigned channel[4][4], rx, tx, dac_reset, dac_ctrl, dac_rate;
 char gain[2][80], mode[80], loopback[80], prbs[80];
};
static void signal_stop(int signo) { (void)signo; stopped = 1; }
static int put(const char *base, const char *name, const char *value)
{
 char path[512];
 FILE *f;
 int n, e;
 snprintf(path, sizeof path, "%s%s", base, name);
 f = fopen(path, "w");
 if (!f) return -1;
 n = fprintf(f, "%s\n", value);
 e = fclose(f);
 return n < 0 || e ? -1 : 0;
}
static int get(const char *base, const char *name, char *value, size_t size)
{
 char path[512];
 FILE *f;
 char *v;
 int e;
 snprintf(path, sizeof path, "%s%s", base, name);
 f = fopen(path, "r");
 if (!f) return -1;
 v = fgets(value, (int)size, f);
 e = fclose(f);
 if (!v || e) return -1;
 value[strcspn(value, "\r\n")] = 0;
 return 0;
}
static int spi_get(unsigned address, unsigned *value)
{
 char text[80], *end;
 unsigned long parsed;
 snprintf(text, sizeof text, "0x%x", address);
 if (put(dbg, "direct_reg_access", text) ||
     get(dbg, "direct_reg_access", text, sizeof text)) return -1;
 errno = 0;
 parsed = strtoul(text, &end, 0);
 if (errno || end == text || *end || parsed > 255) return -1;
 *value = (unsigned)parsed;
 return 0;
}
static int spi_set(unsigned address, unsigned value)
{
 char text[80];
 snprintf(text, sizeof text, "0x%x 0x%x", address, value);
 return put(dbg, "direct_reg_access", text);
}
static int delay_ms(unsigned milliseconds)
{
 struct timespec t = {(time_t)(milliseconds / 1000),
                      (long)(milliseconds % 1000) * 1000000};
 if (stopped) return -1;
 while (nanosleep(&t, &t)) {
  if (errno != EINTR || stopped) return -1;
 }
 return stopped ? -1 : 0;
}
static void barrier(void) { __sync_synchronize(); }
static int program_delay(unsigned address, unsigned value)
{
 unsigned actual;
 if (stopped || put(sys, "ensm_mode", "alert") ||
     spi_set(address, value) || put(sys, "ensm_mode", "fdd") ||
     spi_get(address, &actual)) return -1;
 return actual == value ? 0 : -1;
}
/* Return 0 only for four clean channels and active ADC status. */
static int measure(const char *phase, unsigned coordinate, unsigned window_ms)
{
 unsigned v[4], aggregate = 0, before, after;
 if (delay_ms(20)) return -1;
 /* Prove the intended PN phase before interpreting clean status registers. */
 for (unsigned c = 0; c < 4; c++) {
  unsigned want = !strncmp(phase, "TX_", 3) ? 9 : 0;
  if (r[(0x400 + 64 * c) / 4] != 0x251 ||
      ((r[(0x418 + 64 * c) / 4] >> 16) & 15) != want ||
      (want == 9 && (r[(0x4418 + 64 * c) / 4] != 9 ||
                     r[(0x4414 + 64 * c) / 4] != 0))) {
   fprintf(stderr, "PN phase readback failed\n");
   return -1;
  }
 }
 before = r[0x5c / 4];
 for (unsigned c = 0; c < 4; c++) r[(0x404 + 64 * c) / 4] = 6;
 barrier();
 if (delay_ms(window_ms)) return -1;
 for (unsigned c = 0; c < 4; c++) {
  v[c] = r[(0x404 + 64 * c) / 4];
  aggregate |= v[c];
 }
 after = r[0x5c / 4];
 printf("%s,%02x,%u,%u,%u,%u,%u,%08x,%08x\n",
        phase, coordinate, window_ms, v[0], v[1], v[2], v[3],
        before, after);
 return !stopped && !aggregate && (before & 1) && (after & 1) ? 0 : 1;
}
static unsigned longest_center(const unsigned pass[16], unsigned *length)
{
 unsigned best_start = 0, best_length = 0, start = 0, run = 0;
 for (unsigned i = 0; i < 16; i++) {
  if (pass[i]) {
   if (!run) start = i;
   run++;
   if (run > best_length) { best_start = start; best_length = run; }
  } else run = 0;
 }
 *length = best_length;
 return best_start + best_length / 2;
}
static int rx_setup(const struct state *s)
{
 if (put(dbg, "loopback", "0") || put(dbg, "bist_prbs", "2")) return -1;
 for (unsigned c = 0; c < 4; c++) {
  r[(0x400 + 64 * c) / 4] = 0x251;
  r[(0x418 + 64 * c) / 4] = s->channel[c][1] & ~0xf0000u;
 }
 barrier();
 return 0;
}
static int tx_setup(const struct state *s)
{
 if (put(dbg, "bist_prbs", "0") || put(dbg, "loopback", "1")) return -1;
 for (unsigned c = 0; c < 4; c++) {
  r[(0x418 + 64 * c) / 4] =
   (s->channel[c][1] & ~0xf0000u) | 0x90000;
  r[(0x4414 + 64 * c) / 4] = 0;
  r[(0x4418 + 64 * c) / 4] = 9;
 }
 r[0x4044 / 4] = 1;
 barrier();
 return 0;
}
static int verify_text(const char *base, const char *name, const char *expected)
{
 char actual[80];
 return get(base, name, actual, sizeof actual) || strcmp(actual, expected);
}
static int verify_gain(const char *name, const char *expected)
{
 char actual[80];
 return get(sys, name, actual, sizeof actual) ||
        strtod(actual, NULL) != strtod(expected, NULL);
}
/* Cleanup ignores stopped so all restoration steps are attempted after signals. */
static int restore(const struct state *s, unsigned rx, unsigned tx)
{
 int err = 0;
 unsigned actual_rx = 0, actual_tx = 0;
 err |= put(sys, "ensm_mode", "alert");
 err |= spi_set(6, rx);
 err |= spi_set(7, tx);
 for (unsigned c = 0; c < 4; c++) {
  r[(0x400 + 64 * c) / 4] = s->channel[c][0];
  r[(0x418 + 64 * c) / 4] = s->channel[c][1];
  r[(0x4414 + 64 * c) / 4] = s->channel[c][2];
  r[(0x4418 + 64 * c) / 4] = s->channel[c][3];
 }
 r[0x4044 / 4] = 1;
 barrier();
 err |= put(dbg, "loopback", s->loopback);
 err |= put(dbg, "bist_prbs", s->prbs);
 err |= put(sys, "ensm_mode", s->mode);
 err |= put(sys, "out_voltage0_hardwaregain", s->gain[0]);
 err |= put(sys, "out_voltage1_hardwaregain", s->gain[1]);
 err |= spi_get(6, &actual_rx);
 err |= spi_get(7, &actual_tx);
 if (actual_rx != rx || actual_tx != tx) err = 1;
 for (unsigned c = 0; c < 4; c++)
  if (r[(0x400 + 64 * c) / 4] != s->channel[c][0] ||
      r[(0x418 + 64 * c) / 4] != s->channel[c][1] ||
      r[(0x4414 + 64 * c) / 4] != s->channel[c][2] ||
      r[(0x4418 + 64 * c) / 4] != s->channel[c][3]) err = 1;
 if (r[0x4040 / 4] != s->dac_reset ||
     r[0x4048 / 4] != s->dac_ctrl || r[0x404c / 4] != s->dac_rate) err = 1;
 err |= verify_text(sys, "ensm_mode", s->mode);
 err |= verify_text(sys, "in_voltage_sampling_frequency", "40000000");
 err |= verify_text(sys, "out_voltage_sampling_frequency", "40000000");
 err |= verify_text(sys, "in_voltage_filter_fir_en", "1");
 err |= verify_text(sys, "out_voltage_filter_fir_en", "1");
 err |= verify_text(dbg, "loopback", s->loopback);
 err |= verify_text(dbg, "bist_prbs", s->prbs);
 err |= verify_gain("out_voltage0_hardwaregain", s->gain[0]);
 err |= verify_gain("out_voltage1_hardwaregain", s->gain[1]);
 return err ? -1 : 0;
}
int main(int argc, char **argv)
{
 struct state s = {0};
 char text[80];
 unsigned pass[16], length, rx_selected = 0, tx_selected = 0;
 int fd = -1, lockfd = -1, modified = 0, success = 0, rc = 1;
 (void)argv;
 if (argc != 1) { fprintf(stderr, "No arguments accepted\n"); return 1; }
 signal(SIGINT, signal_stop);
 signal(SIGTERM, signal_stop);
 signal(SIGHUP, signal_stop);
 setvbuf(stdout, NULL, _IOLBF, 0);
 if (get("/etc/", "openwifi-board", text, sizeof text) ||
     strcmp(text, "plutosky_r2")) {
  fprintf(stderr, "PlutoSky-R2 board profile required\n"); return 1;
 }
 errno = 0;
 if (!get("/sys/class/net/sdr0/", "flags", text, sizeof text)) {
  if (strtoul(text, NULL, 0) & 1) {
   fprintf(stderr, "sdr0 must be down before calibration\n"); return 1;
  }
 } else if (errno != ENOENT) {
  fprintf(stderr, "Cannot verify sdr0 state\n"); return 1;
 }
 lockfd = open("/run/r2-calibrate.lock", O_CREAT | O_RDWR, 0600);
 if (lockfd < 0 || flock(lockfd, LOCK_EX | LOCK_NB)) {
  fprintf(stderr, "Calibration lock unavailable\n"); goto end;
 }
 fd = open("/dev/mem", O_RDWR | O_SYNC);
 if (fd < 0) { perror("devmem"); goto end; }
 r = mmap(NULL, 0x5000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x79020000);
 close(fd); fd = -1;
 if (r == MAP_FAILED) { r = NULL; perror("mmap"); goto end; }
 if (r[0] != 0xa0300 || r[0x4000 / 4] != 0x90162 ||
     !(r[0x5c / 4] & 1) || (r[0x44 / 4] & 4) ||
     r[0x4040 / 4] != 3 ||
     (r[0x4048 / 4] & 32) || r[0x404c / 4] != 3) {
  fprintf(stderr, "Core identity/mode/activity guard failed\n"); goto end;
 }
 if (get(sys, "name", text, sizeof text) || strcmp(text, "ad9361-phy") ||
     get(sys, "in_voltage_sampling_frequency", text, sizeof text) ||
     strcmp(text, "40000000") ||
     verify_text(sys, "out_voltage_sampling_frequency", "40000000") ||
     verify_text(sys, "in_voltage_filter_fir_en", "1") ||
     verify_text(sys, "out_voltage_filter_fir_en", "1") ||
     get(sys, "out_voltage0_hardwaregain", s.gain[0], sizeof s.gain[0]) ||
     get(sys, "out_voltage1_hardwaregain", s.gain[1], sizeof s.gain[1]) ||
     get(sys, "ensm_mode", s.mode, sizeof s.mode) ||
     get(dbg, "loopback", s.loopback, sizeof s.loopback) ||
     get(dbg, "bist_prbs", s.prbs, sizeof s.prbs) ||
     spi_get(6, &s.rx) || spi_get(7, &s.tx)) goto end;
 if (strcmp(s.mode, "fdd") || strcmp(s.loopback, "0") ||
     strcmp(s.prbs, "0")) {
  fprintf(stderr, "FDD/no-loopback/no-PRBS baseline required\n"); goto end;
 }
 for (unsigned c = 0; c < 2; c++) s.gain[c][strcspn(s.gain[c], " ")] = 0;
 for (unsigned c = 0; c < 4; c++) {
  s.channel[c][0] = r[(0x400 + 64 * c) / 4];
  s.channel[c][1] = r[(0x418 + 64 * c) / 4];
  s.channel[c][2] = r[(0x4414 + 64 * c) / 4];
  s.channel[c][3] = r[(0x4418 + 64 * c) / 4];
 }
 s.dac_reset = r[0x4040 / 4];
 s.dac_ctrl = r[0x4048 / 4];
 s.dac_rate = r[0x404c / 4];
 printf("BASELINE rx=%02x tx=%02x fixed_rate=40000000\n", s.rx, s.tx);
 modified = 1;
 if (stopped || put(sys, "out_voltage0_hardwaregain", "-89.75") ||
     put(sys, "out_voltage1_hardwaregain", "-89.75") ||
     verify_gain("out_voltage0_hardwaregain", "-89.75") ||
     verify_gain("out_voltage1_hardwaregain", "-89.75") || rx_setup(&s))
  goto cleanup;
 printf("phase,coordinate,window_ms,pn0,pn1,pn2,pn3,adc_before,adc_after\n");
 /* A bad original RX delay is diagnostic, not an excuse to skip correction. */
 if (measure("RX_BASELINE", s.rx, 20) < 0) goto cleanup;
 for (unsigned data = 0; data < 16; data++) {
  int result;
  if (program_delay(6, data)) goto cleanup;
  result = measure("RX_DATA", data, 20);
  if (result < 0) goto cleanup;
  pass[data] = result == 0;
 }
 rx_selected = longest_center(pass, &length);
 if (!length || program_delay(6, rx_selected) ||
     measure("RX_CONFIRM", rx_selected, 500)) goto cleanup;
 if (tx_setup(&s)) goto cleanup;
 for (unsigned clock = 0; clock < 16; clock++) {
  int result;
  if (program_delay(7, clock << 4)) goto cleanup;
  result = measure("TX_CLOCK", clock << 4, 20);
  if (result < 0) goto cleanup;
  pass[clock] = result == 0;
 }
 tx_selected = longest_center(pass, &length) << 4;
 if (!length || program_delay(7, tx_selected) ||
     measure("TX_CONFIRM", tx_selected, 500)) goto cleanup;
 success = !stopped;
cleanup:
 if (modified) {
  unsigned final_rx = success && !stopped ? rx_selected : s.rx;
  unsigned final_tx = success && !stopped ? tx_selected : s.tx;
  if (restore(&s, final_rx, final_tx)) {
   fprintf(stderr, "RESTORE_FAILED; retrying original delay state\n");
   (void)restore(&s, s.rx, s.tx);
   rc = 3;
  } else if (success && !stopped) {
   printf("CALIBRATION_PASS rx=%02x tx=%02x other_state=RESTORED\n",
          final_rx, final_tx);
   rc = 0;
  } else {
   /* A signal during successful cleanup must not retain the new profile. */
   if ((final_rx != s.rx || final_tx != s.tx) && restore(&s, s.rx, s.tx)) rc = 3;
   else rc = stopped ? 2 : 1;
   fprintf(stderr, "CALIBRATION_NOT_APPLIED original_rx=%02x original_tx=%02x\n",
           s.rx, s.tx);
  }
 }
end:
 if (r) munmap((void *)r, 0x5000);
 if (fd >= 0) close(fd);
 if (lockfd >= 0) close(lockfd);
 return rc;
}
