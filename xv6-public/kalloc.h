#ifndef KALLOC_H
#define KALLOC_H

void increase_ref_count(void* va);
void decrease_ref_count(void *va);
void get_reference_count(void* va);
void increase_ref_count_physical_page(uint pa);

#endif