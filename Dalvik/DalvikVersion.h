#ifndef DALVIK_VERSION_H_
#define DALVIK_VERSION_H_

#define DALVIK_MAJOR_VERSION    1
#define DALVIK_MINOR_VERSION    6
#define DALVIK_BUG_VERSION      0

/*
 * VM build number.  This must change whenever something that affects the
 * way classes load changes, e.g. field ordering or vtable layout.  Changing
 * this guarantees that the optimized form of the DEX file is regenerated.
 */
#define DALVIK_VM_BUILD         27

#endif
