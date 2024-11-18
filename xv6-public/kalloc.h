#ifndef KALLOC_H
#define KALLOC_H

// A free page. (Page is a just of chunk of storage in bits ex: 4KB chunk of 1's and 0's)
struct run {
  struct run *next;
  unsigned char refCount;
};

#endif
