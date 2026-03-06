/* This code is placed in the Public Domain by its author, Stuart Caie */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "explode.c"

char *defimp(unsigned char *in, unsigned int in_len,
             unsigned char **out_ptr, unsigned int *out_len_ptr)
{
  unsigned int id, end_off, out_len, ok;
  unsigned char *out;

  *out_ptr = NULL;

  if (in_len < 0x30) return "too short to be a FImp file";

  id       = (in[0x00] << 24) | (in[0x01] << 16) | (in[0x02] << 8) | in[0x03];
  out_len  = (in[0x04] << 24) | (in[0x05] << 16) | (in[0x06] << 8) | in[0x07];
  end_off  = (in[0x08] << 24) | (in[0x09] << 16) | (in[0x0A] << 8) | in[0x0B];
 
  /* check for magic ID 'IMP!', or one of the other IDs used by Imploder 
     clones; ATN!, BDPI, CHFI,P1+r436F=323536\P1+r6B75=1B4F41\P1+r6B64=1B4F42\P1+r6B72=1B4F43\P1+r6B6C=1B4F44\P1+r2332=1B5B313B3248\P1+r2334=1B5B313B3244\P1+r2569=1B5B313B3243\P1+r2A37=1B5B313B3246\P1+r6B31=1B4F50\ Dupa, EDAM, FLT!, M.H., PARA and RDC9 */ 
  if (id != 0x494d5021 && id != 0x41544e21 && id != 0x42445049 && 
      id != 0x43484649 && id != 0x44757061 && id != 0x4544414d && 
      id != 0x464c5421 && id != 0x4d2e482e && id != 0x50415241 && 
      id != 0x52444339) 
    return "unknown file format";

  /* sanity checks */
  if (end_off & 1) return "compressed size not even";
  if (end_off < 14) return "compressed size too short";
  if ((end_off + 0x2E) > in_len) return "not enough input data";
  if ((end_off + 0x26) > out_len) return "output length less than input";

  /* allocate memory */
  if (!(out = malloc(out_len))) return "can't allocate memory";
  *out_ptr = out;
  *out_len_ptr = out_len;

  /* copy input data into output buffer */
  memcpy(&out[0x00],    &in[end_off + 0x08], 4);
  memcpy(&out[0x04],    &in[end_off + 0x04], 4);
  memcpy(&out[0x08],    &in[end_off + 0x00], 4);
  memcpy(&out[0x0C],    &in[0x0C], end_off - 0x0C);
  memcpy(&out[end_off], &in[end_off + 0x0C], 4);

  if (in[end_off + 0x10] & 0x80) {
    out[end_off + 4] = in[end_off + 0x11];
    ok = explode(out, &in[end_off + 0x12], end_off + 5, out_len);
  }
  else {
    out[end_off - 1] = in[end_off + 0x11];
    ok = explode(out, &in[end_off + 0x12], end_off + 4, out_len);
  }

  return ok ? NULL : "can't deplode";
}

size_t filelen;
void *loadfile(char *name) {
  void *mem = NULL; FILE *fd;
  if ((fd = fopen(name, "rb"))) {
    if ((fseek(fd, 0, SEEK_END) == 0) && (filelen = ftell(fd))
    &&  (fseek(fd, 0, SEEK_SET) == 0) && (mem = malloc(filelen))) {
      if (fread(mem, 1, filelen, fd) < filelen) { free(mem); mem = NULL; }
    }
    fclose(fd);
  }
  return mem;
}

int savefile(char *name, void *mem, size_t length) {
  FILE *fd = fopen(name, "wb");
  int ok = fd && (fwrite(mem, 1, length, fd) == length);
  if (fd) fclose(fd);
  return ok;
}
