/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OPENAGC_TEST_DLFCN_H
#define OPENAGC_TEST_DLFCN_H

#define RTLD_NOW 2
#define RTLD_LOCAL 0

void *dlopen(const char *name, int flags);
void *dlsym(void *module, const char *name);
int dlclose(void *module);

#endif
