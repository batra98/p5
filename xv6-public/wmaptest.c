
#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "wmap.h"

int main() {
    uint addr = wmap(0x60000000, 4096, MAP_FIXED | MAP_ANONYMOUS, -1);
    printf(1, "Mapped address: 0x%x\n", addr);
    if (addr == (uint) -1) {
        printf(1, "Memory mapping failed\n");
        exit();
    }

    // Access the mapped memory to trigger the page fault and perform lazy allocation
    char *mapped_mem = (char *) addr;
    mapped_mem[0] = 'A';  // Writing to the mapped region to trigger a page fault

    // Verify that the memory access works and the lazy allocation was successful
    if (mapped_mem[0] == 'A') {
        printf(1, "Lazy allocation successful. Memory is accessible.\n");
    } else {
        printf(1, "Memory access failed after lazy allocation.\n");
    }

    exit();
}
