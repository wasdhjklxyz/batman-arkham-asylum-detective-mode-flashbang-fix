#include <stdint.h>
#include <stdio.h>

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

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;

  FILE *fp = fopen(argv[1], "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }

  struct upkg_hdr header = {0};
  if (fread(&header, sizeof(header), 1, fp) != 1) {
    // FIXME: Use feof() or ferror() to test if actually an error
    perror("fread");
    fclose(fp);
    return 1;
  }

  printf("signature: %x\n", header.signature);
  printf("version: %x\n", header.version);

  fclose(fp);
  return 0;
}
