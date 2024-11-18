Goals:

xv6 Memory management:

- You should be able to explain how xv6 manages the free physical pages?
    - (why should you use V2P and P2V macros and when?).

## Step 1

1) Try to implement a basic `wmap`. Do not worry about file-backed mapping at the moment, just try `MAP_ANONYMOUS` | `MAP_FIXED` | `MAP_SHARED`. It should just check if that particular region asked by the user is available or not. If it is, you should map the pages in that range. The goal of this step is to make you familiar with xv6 memory helpers (e.g. `mappages`, `kalloc`). You should get comfortable in working with PTEs (`pte_t`).
2) Implement `wunmap`. For now, just remove the mappings.
3) Implement `wmapinfo` and `va2pa`. As mentioned earlier, most of the tests depend on these two syscalls to work.
4) Support file-backed mapping. You'll need to change the `wmap` so that it's able to use the provided fd to find the file. You'll also need to revisit `wunmap` to write the changes made to the mapped memory back to disk when you're removing the mapping. You can assume that the offset is always 0. 


Note: 6 & 7 later.


----------


## COW


How to identify different types of page fault in trap handler in xv6 for T_PGFLT?