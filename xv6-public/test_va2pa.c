#include "types.h"
#include "user.h"

int main() {
  uint va = 0x4000; 
  uint pa = va2pa(va);
  if (pa == -1) {
    printf(1, "Invalid virtual address\n");
  } else {
    printf(1, "Physical address: 0x%x\n", pa);
  }
  exit();
}
