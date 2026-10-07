#include <stdint.h>

struct upkg_co {
  uint32_t count;
  uint32_t offset;
};

struct upkg_hdr {
  uint32_t signature; // Always 0x9E2A83C1
  uint16_t version;
  uint16_t license;
  uint32_t flags;
  struct upkg_co name;
  struct upkg_co export;
  struct upkg_co import;
};

int main(void) { return 0; }
