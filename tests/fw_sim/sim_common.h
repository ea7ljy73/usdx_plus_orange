// sim_common.h - utilidades compartidas por las simulaciones host (fw_sim).
#pragma once
#include <stdio.h>
#include <string.h>

static char evlog[4096];
void sim_ev(const char* s) { strncat(evlog, s, sizeof(evlog) - strlen(evlog) - 1); }
#define HAS(s) (strstr(evlog, s) != NULL)

static int fails = 0, ntest = 0;
#define CHECK(c, name) \
  do { \
    ntest++; \
    if(!(c)) { \
      fails++; \
      printf("FAIL %s\n", name); \
    } else \
      printf("ok %s\n", name); \
  } while(0)
#define SUMMARY() (printf("\n%d/%d ok\n", ntest - fails, ntest), fails ? 1 : 0)
