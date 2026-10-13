/* Forwarding stub for the literal MS-DOS include spelling
 *   #include "..\common\kbdcode.h"
 * gcc does not translate backslashes in header names, so the
 * original source only compiles if a file whose name really
 * contains those backslashes exists on the include path.
 *
 * The target lives in geninc/, the lower-cased mirror of the
 * original header tree that the Makefile generates (see
 * "make geninc"): the original sources spell the names in
 * lower case while the files on disk are upper case.
 */
#include "geninc/common/kbdcode.h"
