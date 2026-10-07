#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define UPKG_SIGNATURE 0x9E2A83C1
#define UPKG_GUID_BYTES 16

const char *parse_str(const uint8_t *in, size_t len, size_t *size) {
  if (!in || !size || len < sizeof(int32_t))
    return NULL;

  const int32_t str_len = *(int32_t *)in;
  if (str_len < 1 || (size_t)str_len > len - sizeof(int32_t))
    return NULL;
  in += sizeof(str_len);

  const char *str = (char *)in;
  if (str[str_len - 1] != '\0')
    return NULL;

  *size = str_len + sizeof(str_len);
  return str;
}

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

  size_t folder_name_size;
  const char *folder_name = parse_str(
      in, (size_t)st.st_size - (in - (uint8_t *)pa), &folder_name_size);
  if (!folder_name) {
    fprintf(stderr, "failed to parse string\n");
    munmap(pa, st.st_size);
    close(fd);
    return 1;
  }
  printf("folder name: %s\n", folder_name);
  in += folder_name_size;

  /* PackageFlags, NameCount, NameOffset, ExportCount, ExportOffset,
   * ImportCount, ImportOffset, DependsOffset, GUID
   */
  in += (sizeof(uint32_t) * 8) + UPKG_GUID_BYTES;

  munmap(pa, st.st_size);
  close(fd);
  return 0;
}
