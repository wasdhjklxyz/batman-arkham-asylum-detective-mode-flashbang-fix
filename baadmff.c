#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define UPKG_SIGNATURE 0x9E2A83C1

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;

  const int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("open");
    return 1;
  }

  struct stat st = {0};
  if (fstat(fd, &st) != 0) {
    perror("fstat");
    close(fd);
    return 1;
  }

  void *pa = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (pa == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return 1;
  }

  const uint8_t *in = pa;
  const uint32_t signature = *(uint32_t *)in;
  if (signature != UPKG_SIGNATURE) {
    fprintf(stderr, "invalid signature\n");
    munmap(pa, st.st_size);
    close(fd);
    return 1;
  }
  in += sizeof(signature);

  const uint16_t version = *(uint16_t *)in;
  printf("version: %hu\n", version);
  in += sizeof(version);

  const uint16_t license = *(uint16_t *)in;
  printf("license: %d\n", license);
  in += sizeof(license);

  const int32_t size = *(int32_t *)in;
  printf("size: %d\n", size);
  in += sizeof(size);

  munmap(pa, st.st_size);
  close(fd);
  return 0;
}
