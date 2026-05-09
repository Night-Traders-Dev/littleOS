sed -i 's/#include <dirent.h>/#ifndef PICO_BUILD\n#include <dirent.h>\n#endif/' third_party/sagelang/src/c/stdlib.c
sed -i '/DIR\* d = opendir/i #ifndef PICO_BUILD' third_party/sagelang/src/c/stdlib.c
sed -i '/closedir(d);/a #else\n    return val_nil();\n#endif' third_party/sagelang/src/c/stdlib.c
