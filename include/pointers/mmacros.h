
#ifndef MMACROS_H
#define MMACROS_H

// memory write macros
#define MEMCOPY( target, source, len) \
memcpy( (target), (source), (len));         \
target = (PBYTE) (target) + (len)

#endif // MMACROS_H

