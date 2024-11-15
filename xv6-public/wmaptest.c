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
    

    struct wmapinfo info;
    if (getwmapinfo(&info) == 0) {
        printf(1, "Total mmaps: %d\n", info.total_mmaps);
        for (int i = 0; i < info.total_mmaps; i++) {
            printf(1, "Region %d: addr = 0x%x, length = %d, loaded pages = %d\n",
                   i, info.addr[i], info.length[i], info.n_loaded_pages[i]);
        }
    } else {
        printf(1, "Failed to get wmapinfo.\n");
    }
    
    // Verify that the memory access works and the lazy allocation was successful
    if (mapped_mem[0] == 'A') {
        printf(1, "Lazy allocation successful. Memory is accessible.\n");
    } else {
        printf(1, "Memory access failed after lazy allocation.\n");
    }
    
    if (wunmap(addr) == (uint) -1) {
        printf(1, "Memory unmapping failed\n");
        exit();
    }
    printf(1, "Memory unmapped successfully\n");
    
    if (getwmapinfo(&info) == 0) {
        printf(1, "Total mmaps: %d\n", info.total_mmaps);
        for (int i = 0; i < info.total_mmaps; i++) {
            printf(1, "Region %d: addr = 0x%x, length = %d, loaded pages = %d\n",
                   i, info.addr[i], info.length[i], info.n_loaded_pages[i]);
        }
    } else {
        printf(1, "Failed to get wmapinfo.\n");
    }


    exit();
}
