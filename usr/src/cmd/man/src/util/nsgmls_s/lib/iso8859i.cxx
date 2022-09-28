#pragma ident	"@(#)ISO8859InputCodingSystem.cxx	1.2	97/04/24 SMI"
// Copyright (c) 1995 James Clark
// See the file COPYING for copying permission.

#include "splib.h"
#ifdef SP_MULTI_BYTE

#include "ISO8859InputCodingSystem.h"
#include "macros.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

inline const Char *ISO8859InputCodingSystem::partMap(int part)
{
  ASSERT(2 <= part && part <= 9);
  return maps[part - 2];
}
  
ISO8859InputCodingSystem::ISO8859InputCodingSystem(int part)
: TranslateInputCodingSystem(partMap(part))
{
}

#define INVALID 0xFFFD

// Tables mapping ISO 8859-2 through ISO 8859-9 to ISO 10646.
// Generated from Unicode Consortium data.

const Char ISO8859InputCodingSystem::maps[8][256] = {
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, 260, 728, 321, 164, 317, 346, 167,
   168, 352, 350, 356, 377, 173, 381, 379,
   176, 261, 731, 322, 180, 318, 347, 711,
   184, 353, 351, 357, 378, 733, 382, 380,
   340, 193, 194, 258, 196, 313, 262, 199,
   268, 201, 280, 203, 282, 205, 206, 270,
   272, 323, 327, 211, 212, 336, 214, 215,
   344, 366, 218, 368, 220, 221, 354, 223,
   341, 225, 226, 259, 228, 314, 263, 231,
   269, 233, 281, 235, 283, 237, 238, 271,
   273, 324, 328, 243, 244, 337, 246, 247,
   345, 367, 250, 369, 252, 253, 355, 729,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, 294, 728, 163, 164, INVALID, 292, 167,
   168, 304, 350, 286, 308, 173, INVALID, 379,
   176, 295, 178, 179, 180, 181, 293, 183,
   184, 305, 351, 287, 309, 189, INVALID, 380,
   192, 193, 194, INVALID, 196, 266, 264, 199,
   200, 201, 202, 203, 204, 205, 206, 207,
   INVALID, 209, 210, 211, 212, 288, 214, 215,
   284, 217, 218, 219, 220, 364, 348, 223,
   224, 225, 226, INVALID, 228, 267, 265, 231,
   232, 233, 234, 235, 236, 237, 238, 239,
   INVALID, 241, 242, 243, 244, 289, 246, 247,
   285, 249, 250, 251, 252, 365, 349, 729,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, 260, 312, 342, 164, 296, 315, 167,
   168, 352, 274, 290, 358, 173, 381, 175,
   176, 261, 731, 343, 180, 297, 316, 711,
   184, 353, 275, 291, 359, 330, 382, 331,
   256, 193, 194, 195, 196, 197, 198, 302,
   268, 201, 280, 203, 278, 205, 206, 298,
   272, 325, 332, 310, 212, 213, 214, 215,
   216, 370, 218, 219, 220, 360, 362, 223,
   257, 225, 226, 227, 228, 229, 230, 303,
   269, 233, 281, 235, 279, 237, 238, 299,
   273, 326, 333, 311, 244, 245, 246, 247,
   248, 371, 250, 251, 252, 361, 363, 729,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, 1025, 1026, 1027, 1028, 1029, 1030, 1031,
   1032, 1033, 1034, 1035, 1036, 173, 1038, 1039,
   1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047,
   1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055,
   1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063,
   1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071,
   1072, 1073, 1074, 1075, 1076, 1077, 1078, 1079,
   1080, 1081, 1082, 1083, 1084, 1085, 1086, 1087,
   1088, 1089, 1090, 1091, 1092, 1093, 1094, 1095,
   1096, 1097, 1098, 1099, 1100, 1101, 1102, 1103,
   8470, 1105, 1106, 1107, 1108, 1109, 1110, 1111,
   1112, 1113, 1114, 1115, 1116, 167, 1118, 1119,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   1632, 1633, 1634, 1635, 1636, 1637, 1638, 1639,
   1640, 1641, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, INVALID, INVALID, INVALID, 164, INVALID, INVALID, INVALID,
   INVALID, INVALID, INVALID, INVALID, 1548, 173, INVALID, INVALID,
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
   INVALID, INVALID, INVALID, 1563, INVALID, INVALID, INVALID, 1567,
   INVALID, 1569, 1570, 1571, 1572, 1573, 1574, 1575,
   1576, 1577, 1578, 1579, 1580, 1581, 1582, 1583,
   1584, 1585, 1586, 1587, 1588, 1589, 1590, 1591,
   1592, 1593, 1594, INVALID, INVALID, INVALID, INVALID, INVALID,
   1600, 1601, 1602, 1603, 1604, 1605, 1606, 1607,
   1608, 1609, 1610, 1611, 1612, 1613, 1614, 1615,
   1616, 1617, 1618, INVALID, INVALID, INVALID, INVALID, INVALID,
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, 701, 700, 163, INVALID, INVALID, 166, 167,
   168, 169, INVALID, 171, 172, 173, INVALID, 8213,
   176, 177, 178, 179, 900, 901, 902, 183,
   904, 905, 906, 187, 908, 189, 910, 911,
   912, 913, 914, 915, 916, 917, 918, 919,
   920, 921, 922, 923, 924, 925, 926, 927,
   928, 929, INVALID, 931, 932, 933, 934, 935,
   936, 937, 938, 939, 940, 941, 942, 943,
   944, 945, 946, 947, 948, 949, 950, 951,
   952, 953, 954, 955, 956, 957, 958, 959,
   960, 961, 962, 963, 964, 965, 966, 967,
   968, 969, 970, 971, 972, 973, 974, INVALID,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, INVALID, 162, 163, 164, 165, 166, 167,
   168, 169, 215, 171, 172, 173, 174, 8254,
   176, 177, 178, 179, 180, 181, 182, 183,
   184, 185, 247, 187, 188, 189, 190, INVALID,
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, 8215,
   1488, 1489, 1490, 1491, 1492, 1493, 1494, 1495,
   1496, 1497, 1498, 1499, 1500, 1501, 1502, 1503,
   1504, 1505, 1506, 1507, 1508, 1509, 1510, 1511,
   1512, 1513, 1514, INVALID, INVALID, INVALID, INVALID, INVALID,
 },
 {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71,
   72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99, 100, 101, 102, 103,
   104, 105, 106, 107, 108, 109, 110, 111,
   112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127,
   128, 129, 130, 131, 132, 133, 134, 135,
   136, 137, 138, 139, 140, 141, 142, 143,
   144, 145, 146, 147, 148, 149, 150, 151,
   152, 153, 154, 155, 156, 157, 158, 159,
   160, 161, 162, 163, 164, 165, 166, 167,
   168, 169, 170, 171, 172, 173, 174, 175,
   176, 177, 178, 179, 180, 181, 182, 183,
   184, 185, 186, 187, 188, 189, 190, 191,
   192, 193, 194, 195, 196, 197, 198, 199,
   200, 201, 202, 203, 204, 205, 206, 207,
   286, 209, 210, 211, 212, 213, 214, 215,
   216, 217, 218, 219, 220, 304, 350, 223,
   224, 225, 226, 227, 228, 229, 230, 231,
   232, 233, 234, 235, 236, 237, 238, 239,
   287, 241, 242, 243, 244, 245, 246, 247,
   248, 249, 250, 251, 252, 305, 351, 255,
 }
};

#ifdef SP_NAMESPACE
}
#endif

#else /* not SP_MULTI_BYTE */

#ifndef __GNUG__
static char non_empty_translation_unit;	// sigh
#endif

#endif /* not SP_MULTI_BYTE */