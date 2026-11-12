#pragma once

/*
 * One VFileSystem009 on the host factory list. The caller supplies the only
 * root. A missing file or a path outside that root is a miss.
 */

void* csr_host_filesystem_factory(const char* name, int* code);
int csr_host_filesystem_read(const char* path, unsigned char* dest, int dest_cap);
