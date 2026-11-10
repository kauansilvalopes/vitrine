/* miniz (MIT, third_party/miniz) built into the app: GameTDB's title databases come as zip files.
 * Under src/third_party/ the build compiles it with warnings off (vendored code, as published). */
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#include "../../third_party/miniz/miniz.c"
#include "../../third_party/miniz/miniz_tdef.c"
#include "../../third_party/miniz/miniz_tinfl.c"
#include "../../third_party/miniz/miniz_zip.c"
