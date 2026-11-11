#pragma once

/*
 * One VFileSystem009 on the host factory list. The caller supplies the only
 * root. A missing file or a path outside that root is a miss.
 */

int csr_host_filesystem_set_root(const char* root);
void csr_host_filesystem_clear();
void* csr_host_filesystem_factory(const char* name, int* code);
