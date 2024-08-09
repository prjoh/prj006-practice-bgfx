/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 69 "MachineIndependent/glslang.y"


/* Based on:
ANSI C Yacc grammar

In 1985, Jeff Lee published his Yacc grammar (which is accompanied by a
matching Lex specification) for the April 30, 1985 draft version of the
ANSI C standard.  Tom Stockfisch reposted it to net.sources in 1987; that
original, as mentioned in the answer to question 17.25 of the comp.lang.c
FAQ, can be ftp'ed from ftp.uu.net, file usenet/net.sources/ansi.c.grammar.Z.

I intend to keep this version as close to the current C Standard grammar as
possible; please let me know if you discover discrepancies.

Jutta Degener, 1995
*/

#include "SymbolTable.h"
#include "ParseHelper.h"
#include "../Public/ShaderLang.h"
#include "attribute.h"

using namespace glslang;


#line 97 "MachineIndependent/glslang_tab.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "glslang_tab.cpp.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_CONST = 3,                      /* CONST  */
  YYSYMBOL_BOOL = 4,                       /* BOOL  */
  YYSYMBOL_INT = 5,                        /* INT  */
  YYSYMBOL_UINT = 6,                       /* UINT  */
  YYSYMBOL_FLOAT = 7,                      /* FLOAT  */
  YYSYMBOL_BVEC2 = 8,                      /* BVEC2  */
  YYSYMBOL_BVEC3 = 9,                      /* BVEC3  */
  YYSYMBOL_BVEC4 = 10,                     /* BVEC4  */
  YYSYMBOL_IVEC2 = 11,                     /* IVEC2  */
  YYSYMBOL_IVEC3 = 12,                     /* IVEC3  */
  YYSYMBOL_IVEC4 = 13,                     /* IVEC4  */
  YYSYMBOL_UVEC2 = 14,                     /* UVEC2  */
  YYSYMBOL_UVEC3 = 15,                     /* UVEC3  */
  YYSYMBOL_UVEC4 = 16,                     /* UVEC4  */
  YYSYMBOL_VEC2 = 17,                      /* VEC2  */
  YYSYMBOL_VEC3 = 18,                      /* VEC3  */
  YYSYMBOL_VEC4 = 19,                      /* VEC4  */
  YYSYMBOL_MAT2 = 20,                      /* MAT2  */
  YYSYMBOL_MAT3 = 21,                      /* MAT3  */
  YYSYMBOL_MAT4 = 22,                      /* MAT4  */
  YYSYMBOL_MAT2X2 = 23,                    /* MAT2X2  */
  YYSYMBOL_MAT2X3 = 24,                    /* MAT2X3  */
  YYSYMBOL_MAT2X4 = 25,                    /* MAT2X4  */
  YYSYMBOL_MAT3X2 = 26,                    /* MAT3X2  */
  YYSYMBOL_MAT3X3 = 27,                    /* MAT3X3  */
  YYSYMBOL_MAT3X4 = 28,                    /* MAT3X4  */
  YYSYMBOL_MAT4X2 = 29,                    /* MAT4X2  */
  YYSYMBOL_MAT4X3 = 30,                    /* MAT4X3  */
  YYSYMBOL_MAT4X4 = 31,                    /* MAT4X4  */
  YYSYMBOL_SAMPLER2D = 32,                 /* SAMPLER2D  */
  YYSYMBOL_SAMPLER3D = 33,                 /* SAMPLER3D  */
  YYSYMBOL_SAMPLERCUBE = 34,               /* SAMPLERCUBE  */
  YYSYMBOL_SAMPLER2DSHADOW = 35,           /* SAMPLER2DSHADOW  */
  YYSYMBOL_SAMPLERCUBESHADOW = 36,         /* SAMPLERCUBESHADOW  */
  YYSYMBOL_SAMPLER2DARRAY = 37,            /* SAMPLER2DARRAY  */
  YYSYMBOL_SAMPLER2DARRAYSHADOW = 38,      /* SAMPLER2DARRAYSHADOW  */
  YYSYMBOL_ISAMPLER2D = 39,                /* ISAMPLER2D  */
  YYSYMBOL_ISAMPLER3D = 40,                /* ISAMPLER3D  */
  YYSYMBOL_ISAMPLERCUBE = 41,              /* ISAMPLERCUBE  */
  YYSYMBOL_ISAMPLER2DARRAY = 42,           /* ISAMPLER2DARRAY  */
  YYSYMBOL_USAMPLER2D = 43,                /* USAMPLER2D  */
  YYSYMBOL_USAMPLER3D = 44,                /* USAMPLER3D  */
  YYSYMBOL_USAMPLERCUBE = 45,              /* USAMPLERCUBE  */
  YYSYMBOL_USAMPLER2DARRAY = 46,           /* USAMPLER2DARRAY  */
  YYSYMBOL_SAMPLER = 47,                   /* SAMPLER  */
  YYSYMBOL_SAMPLERSHADOW = 48,             /* SAMPLERSHADOW  */
  YYSYMBOL_TEXTURE2D = 49,                 /* TEXTURE2D  */
  YYSYMBOL_TEXTURE3D = 50,                 /* TEXTURE3D  */
  YYSYMBOL_TEXTURECUBE = 51,               /* TEXTURECUBE  */
  YYSYMBOL_TEXTURE2DARRAY = 52,            /* TEXTURE2DARRAY  */
  YYSYMBOL_ITEXTURE2D = 53,                /* ITEXTURE2D  */
  YYSYMBOL_ITEXTURE3D = 54,                /* ITEXTURE3D  */
  YYSYMBOL_ITEXTURECUBE = 55,              /* ITEXTURECUBE  */
  YYSYMBOL_ITEXTURE2DARRAY = 56,           /* ITEXTURE2DARRAY  */
  YYSYMBOL_UTEXTURE2D = 57,                /* UTEXTURE2D  */
  YYSYMBOL_UTEXTURE3D = 58,                /* UTEXTURE3D  */
  YYSYMBOL_UTEXTURECUBE = 59,              /* UTEXTURECUBE  */
  YYSYMBOL_UTEXTURE2DARRAY = 60,           /* UTEXTURE2DARRAY  */
  YYSYMBOL_ATTRIBUTE = 61,                 /* ATTRIBUTE  */
  YYSYMBOL_VARYING = 62,                   /* VARYING  */
  YYSYMBOL_FLOAT16_T = 63,                 /* FLOAT16_T  */
  YYSYMBOL_FLOAT32_T = 64,                 /* FLOAT32_T  */
  YYSYMBOL_DOUBLE = 65,                    /* DOUBLE  */
  YYSYMBOL_FLOAT64_T = 66,                 /* FLOAT64_T  */
  YYSYMBOL_INT64_T = 67,                   /* INT64_T  */
  YYSYMBOL_UINT64_T = 68,                  /* UINT64_T  */
  YYSYMBOL_INT32_T = 69,                   /* INT32_T  */
  YYSYMBOL_UINT32_T = 70,                  /* UINT32_T  */
  YYSYMBOL_INT16_T = 71,                   /* INT16_T  */
  YYSYMBOL_UINT16_T = 72,                  /* UINT16_T  */
  YYSYMBOL_INT8_T = 73,                    /* INT8_T  */
  YYSYMBOL_UINT8_T = 74,                   /* UINT8_T  */
  YYSYMBOL_I64VEC2 = 75,                   /* I64VEC2  */
  YYSYMBOL_I64VEC3 = 76,                   /* I64VEC3  */
  YYSYMBOL_I64VEC4 = 77,                   /* I64VEC4  */
  YYSYMBOL_U64VEC2 = 78,                   /* U64VEC2  */
  YYSYMBOL_U64VEC3 = 79,                   /* U64VEC3  */
  YYSYMBOL_U64VEC4 = 80,                   /* U64VEC4  */
  YYSYMBOL_I32VEC2 = 81,                   /* I32VEC2  */
  YYSYMBOL_I32VEC3 = 82,                   /* I32VEC3  */
  YYSYMBOL_I32VEC4 = 83,                   /* I32VEC4  */
  YYSYMBOL_U32VEC2 = 84,                   /* U32VEC2  */
  YYSYMBOL_U32VEC3 = 85,                   /* U32VEC3  */
  YYSYMBOL_U32VEC4 = 86,                   /* U32VEC4  */
  YYSYMBOL_I16VEC2 = 87,                   /* I16VEC2  */
  YYSYMBOL_I16VEC3 = 88,                   /* I16VEC3  */
  YYSYMBOL_I16VEC4 = 89,                   /* I16VEC4  */
  YYSYMBOL_U16VEC2 = 90,                   /* U16VEC2  */
  YYSYMBOL_U16VEC3 = 91,                   /* U16VEC3  */
  YYSYMBOL_U16VEC4 = 92,                   /* U16VEC4  */
  YYSYMBOL_I8VEC2 = 93,                    /* I8VEC2  */
  YYSYMBOL_I8VEC3 = 94,                    /* I8VEC3  */
  YYSYMBOL_I8VEC4 = 95,                    /* I8VEC4  */
  YYSYMBOL_U8VEC2 = 96,                    /* U8VEC2  */
  YYSYMBOL_U8VEC3 = 97,                    /* U8VEC3  */
  YYSYMBOL_U8VEC4 = 98,                    /* U8VEC4  */
  YYSYMBOL_DVEC2 = 99,                     /* DVEC2  */
  YYSYMBOL_DVEC3 = 100,                    /* DVEC3  */
  YYSYMBOL_DVEC4 = 101,                    /* DVEC4  */
  YYSYMBOL_DMAT2 = 102,                    /* DMAT2  */
  YYSYMBOL_DMAT3 = 103,                    /* DMAT3  */
  YYSYMBOL_DMAT4 = 104,                    /* DMAT4  */
  YYSYMBOL_F16VEC2 = 105,                  /* F16VEC2  */
  YYSYMBOL_F16VEC3 = 106,                  /* F16VEC3  */
  YYSYMBOL_F16VEC4 = 107,                  /* F16VEC4  */
  YYSYMBOL_F16MAT2 = 108,                  /* F16MAT2  */
  YYSYMBOL_F16MAT3 = 109,                  /* F16MAT3  */
  YYSYMBOL_F16MAT4 = 110,                  /* F16MAT4  */
  YYSYMBOL_F32VEC2 = 111,                  /* F32VEC2  */
  YYSYMBOL_F32VEC3 = 112,                  /* F32VEC3  */
  YYSYMBOL_F32VEC4 = 113,                  /* F32VEC4  */
  YYSYMBOL_F32MAT2 = 114,                  /* F32MAT2  */
  YYSYMBOL_F32MAT3 = 115,                  /* F32MAT3  */
  YYSYMBOL_F32MAT4 = 116,                  /* F32MAT4  */
  YYSYMBOL_F64VEC2 = 117,                  /* F64VEC2  */
  YYSYMBOL_F64VEC3 = 118,                  /* F64VEC3  */
  YYSYMBOL_F64VEC4 = 119,                  /* F64VEC4  */
  YYSYMBOL_F64MAT2 = 120,                  /* F64MAT2  */
  YYSYMBOL_F64MAT3 = 121,                  /* F64MAT3  */
  YYSYMBOL_F64MAT4 = 122,                  /* F64MAT4  */
  YYSYMBOL_DMAT2X2 = 123,                  /* DMAT2X2  */
  YYSYMBOL_DMAT2X3 = 124,                  /* DMAT2X3  */
  YYSYMBOL_DMAT2X4 = 125,                  /* DMAT2X4  */
  YYSYMBOL_DMAT3X2 = 126,                  /* DMAT3X2  */
  YYSYMBOL_DMAT3X3 = 127,                  /* DMAT3X3  */
  YYSYMBOL_DMAT3X4 = 128,                  /* DMAT3X4  */
  YYSYMBOL_DMAT4X2 = 129,                  /* DMAT4X2  */
  YYSYMBOL_DMAT4X3 = 130,                  /* DMAT4X3  */
  YYSYMBOL_DMAT4X4 = 131,                  /* DMAT4X4  */
  YYSYMBOL_F16MAT2X2 = 132,                /* F16MAT2X2  */
  YYSYMBOL_F16MAT2X3 = 133,                /* F16MAT2X3  */
  YYSYMBOL_F16MAT2X4 = 134,                /* F16MAT2X4  */
  YYSYMBOL_F16MAT3X2 = 135,                /* F16MAT3X2  */
  YYSYMBOL_F16MAT3X3 = 136,                /* F16MAT3X3  */
  YYSYMBOL_F16MAT3X4 = 137,                /* F16MAT3X4  */
  YYSYMBOL_F16MAT4X2 = 138,                /* F16MAT4X2  */
  YYSYMBOL_F16MAT4X3 = 139,                /* F16MAT4X3  */
  YYSYMBOL_F16MAT4X4 = 140,                /* F16MAT4X4  */
  YYSYMBOL_F32MAT2X2 = 141,                /* F32MAT2X2  */
  YYSYMBOL_F32MAT2X3 = 142,                /* F32MAT2X3  */
  YYSYMBOL_F32MAT2X4 = 143,                /* F32MAT2X4  */
  YYSYMBOL_F32MAT3X2 = 144,                /* F32MAT3X2  */
  YYSYMBOL_F32MAT3X3 = 145,                /* F32MAT3X3  */
  YYSYMBOL_F32MAT3X4 = 146,                /* F32MAT3X4  */
  YYSYMBOL_F32MAT4X2 = 147,                /* F32MAT4X2  */
  YYSYMBOL_F32MAT4X3 = 148,                /* F32MAT4X3  */
  YYSYMBOL_F32MAT4X4 = 149,                /* F32MAT4X4  */
  YYSYMBOL_F64MAT2X2 = 150,                /* F64MAT2X2  */
  YYSYMBOL_F64MAT2X3 = 151,                /* F64MAT2X3  */
  YYSYMBOL_F64MAT2X4 = 152,                /* F64MAT2X4  */
  YYSYMBOL_F64MAT3X2 = 153,                /* F64MAT3X2  */
  YYSYMBOL_F64MAT3X3 = 154,                /* F64MAT3X3  */
  YYSYMBOL_F64MAT3X4 = 155,                /* F64MAT3X4  */
  YYSYMBOL_F64MAT4X2 = 156,                /* F64MAT4X2  */
  YYSYMBOL_F64MAT4X3 = 157,                /* F64MAT4X3  */
  YYSYMBOL_F64MAT4X4 = 158,                /* F64MAT4X4  */
  YYSYMBOL_ATOMIC_UINT = 159,              /* ATOMIC_UINT  */
  YYSYMBOL_ACCSTRUCTNV = 160,              /* ACCSTRUCTNV  */
  YYSYMBOL_ACCSTRUCTEXT = 161,             /* ACCSTRUCTEXT  */
  YYSYMBOL_RAYQUERYEXT = 162,              /* RAYQUERYEXT  */
  YYSYMBOL_FCOOPMATNV = 163,               /* FCOOPMATNV  */
  YYSYMBOL_ICOOPMATNV = 164,               /* ICOOPMATNV  */
  YYSYMBOL_UCOOPMATNV = 165,               /* UCOOPMATNV  */
  YYSYMBOL_COOPMAT = 166,                  /* COOPMAT  */
  YYSYMBOL_HITOBJECTNV = 167,              /* HITOBJECTNV  */
  YYSYMBOL_HITOBJECTATTRNV = 168,          /* HITOBJECTATTRNV  */
  YYSYMBOL_SAMPLERCUBEARRAY = 169,         /* SAMPLERCUBEARRAY  */
  YYSYMBOL_SAMPLERCUBEARRAYSHADOW = 170,   /* SAMPLERCUBEARRAYSHADOW  */
  YYSYMBOL_ISAMPLERCUBEARRAY = 171,        /* ISAMPLERCUBEARRAY  */
  YYSYMBOL_USAMPLERCUBEARRAY = 172,        /* USAMPLERCUBEARRAY  */
  YYSYMBOL_SAMPLER1D = 173,                /* SAMPLER1D  */
  YYSYMBOL_SAMPLER1DARRAY = 174,           /* SAMPLER1DARRAY  */
  YYSYMBOL_SAMPLER1DARRAYSHADOW = 175,     /* SAMPLER1DARRAYSHADOW  */
  YYSYMBOL_ISAMPLER1D = 176,               /* ISAMPLER1D  */
  YYSYMBOL_SAMPLER1DSHADOW = 177,          /* SAMPLER1DSHADOW  */
  YYSYMBOL_SAMPLER2DRECT = 178,            /* SAMPLER2DRECT  */
  YYSYMBOL_SAMPLER2DRECTSHADOW = 179,      /* SAMPLER2DRECTSHADOW  */
  YYSYMBOL_ISAMPLER2DRECT = 180,           /* ISAMPLER2DRECT  */
  YYSYMBOL_USAMPLER2DRECT = 181,           /* USAMPLER2DRECT  */
  YYSYMBOL_SAMPLERBUFFER = 182,            /* SAMPLERBUFFER  */
  YYSYMBOL_ISAMPLERBUFFER = 183,           /* ISAMPLERBUFFER  */
  YYSYMBOL_USAMPLERBUFFER = 184,           /* USAMPLERBUFFER  */
  YYSYMBOL_SAMPLER2DMS = 185,              /* SAMPLER2DMS  */
  YYSYMBOL_ISAMPLER2DMS = 186,             /* ISAMPLER2DMS  */
  YYSYMBOL_USAMPLER2DMS = 187,             /* USAMPLER2DMS  */
  YYSYMBOL_SAMPLER2DMSARRAY = 188,         /* SAMPLER2DMSARRAY  */
  YYSYMBOL_ISAMPLER2DMSARRAY = 189,        /* ISAMPLER2DMSARRAY  */
  YYSYMBOL_USAMPLER2DMSARRAY = 190,        /* USAMPLER2DMSARRAY  */
  YYSYMBOL_SAMPLEREXTERNALOES = 191,       /* SAMPLEREXTERNALOES  */
  YYSYMBOL_SAMPLEREXTERNAL2DY2YEXT = 192,  /* SAMPLEREXTERNAL2DY2YEXT  */
  YYSYMBOL_ISAMPLER1DARRAY = 193,          /* ISAMPLER1DARRAY  */
  YYSYMBOL_USAMPLER1D = 194,               /* USAMPLER1D  */
  YYSYMBOL_USAMPLER1DARRAY = 195,          /* USAMPLER1DARRAY  */
  YYSYMBOL_F16SAMPLER1D = 196,             /* F16SAMPLER1D  */
  YYSYMBOL_F16SAMPLER2D = 197,             /* F16SAMPLER2D  */
  YYSYMBOL_F16SAMPLER3D = 198,             /* F16SAMPLER3D  */
  YYSYMBOL_F16SAMPLER2DRECT = 199,         /* F16SAMPLER2DRECT  */
  YYSYMBOL_F16SAMPLERCUBE = 200,           /* F16SAMPLERCUBE  */
  YYSYMBOL_F16SAMPLER1DARRAY = 201,        /* F16SAMPLER1DARRAY  */
  YYSYMBOL_F16SAMPLER2DARRAY = 202,        /* F16SAMPLER2DARRAY  */
  YYSYMBOL_F16SAMPLERCUBEARRAY = 203,      /* F16SAMPLERCUBEARRAY  */
  YYSYMBOL_F16SAMPLERBUFFER = 204,         /* F16SAMPLERBUFFER  */
  YYSYMBOL_F16SAMPLER2DMS = 205,           /* F16SAMPLER2DMS  */
  YYSYMBOL_F16SAMPLER2DMSARRAY = 206,      /* F16SAMPLER2DMSARRAY  */
  YYSYMBOL_F16SAMPLER1DSHADOW = 207,       /* F16SAMPLER1DSHADOW  */
  YYSYMBOL_F16SAMPLER2DSHADOW = 208,       /* F16SAMPLER2DSHADOW  */
  YYSYMBOL_F16SAMPLER1DARRAYSHADOW = 209,  /* F16SAMPLER1DARRAYSHADOW  */
  YYSYMBOL_F16SAMPLER2DARRAYSHADOW = 210,  /* F16SAMPLER2DARRAYSHADOW  */
  YYSYMBOL_F16SAMPLER2DRECTSHADOW = 211,   /* F16SAMPLER2DRECTSHADOW  */
  YYSYMBOL_F16SAMPLERCUBESHADOW = 212,     /* F16SAMPLERCUBESHADOW  */
  YYSYMBOL_F16SAMPLERCUBEARRAYSHADOW = 213, /* F16SAMPLERCUBEARRAYSHADOW  */
  YYSYMBOL_IMAGE1D = 214,                  /* IMAGE1D  */
  YYSYMBOL_IIMAGE1D = 215,                 /* IIMAGE1D  */
  YYSYMBOL_UIMAGE1D = 216,                 /* UIMAGE1D  */
  YYSYMBOL_IMAGE2D = 217,                  /* IMAGE2D  */
  YYSYMBOL_IIMAGE2D = 218,                 /* IIMAGE2D  */
  YYSYMBOL_UIMAGE2D = 219,                 /* UIMAGE2D  */
  YYSYMBOL_IMAGE3D = 220,                  /* IMAGE3D  */
  YYSYMBOL_IIMAGE3D = 221,                 /* IIMAGE3D  */
  YYSYMBOL_UIMAGE3D = 222,                 /* UIMAGE3D  */
  YYSYMBOL_IMAGE2DRECT = 223,              /* IMAGE2DRECT  */
  YYSYMBOL_IIMAGE2DRECT = 224,             /* IIMAGE2DRECT  */
  YYSYMBOL_UIMAGE2DRECT = 225,             /* UIMAGE2DRECT  */
  YYSYMBOL_IMAGECUBE = 226,                /* IMAGECUBE  */
  YYSYMBOL_IIMAGECUBE = 227,               /* IIMAGECUBE  */
  YYSYMBOL_UIMAGECUBE = 228,               /* UIMAGECUBE  */
  YYSYMBOL_IMAGEBUFFER = 229,              /* IMAGEBUFFER  */
  YYSYMBOL_IIMAGEBUFFER = 230,             /* IIMAGEBUFFER  */
  YYSYMBOL_UIMAGEBUFFER = 231,             /* UIMAGEBUFFER  */
  YYSYMBOL_IMAGE1DARRAY = 232,             /* IMAGE1DARRAY  */
  YYSYMBOL_IIMAGE1DARRAY = 233,            /* IIMAGE1DARRAY  */
  YYSYMBOL_UIMAGE1DARRAY = 234,            /* UIMAGE1DARRAY  */
  YYSYMBOL_IMAGE2DARRAY = 235,             /* IMAGE2DARRAY  */
  YYSYMBOL_IIMAGE2DARRAY = 236,            /* IIMAGE2DARRAY  */
  YYSYMBOL_UIMAGE2DARRAY = 237,            /* UIMAGE2DARRAY  */
  YYSYMBOL_IMAGECUBEARRAY = 238,           /* IMAGECUBEARRAY  */
  YYSYMBOL_IIMAGECUBEARRAY = 239,          /* IIMAGECUBEARRAY  */
  YYSYMBOL_UIMAGECUBEARRAY = 240,          /* UIMAGECUBEARRAY  */
  YYSYMBOL_IMAGE2DMS = 241,                /* IMAGE2DMS  */
  YYSYMBOL_IIMAGE2DMS = 242,               /* IIMAGE2DMS  */
  YYSYMBOL_UIMAGE2DMS = 243,               /* UIMAGE2DMS  */
  YYSYMBOL_IMAGE2DMSARRAY = 244,           /* IMAGE2DMSARRAY  */
  YYSYMBOL_IIMAGE2DMSARRAY = 245,          /* IIMAGE2DMSARRAY  */
  YYSYMBOL_UIMAGE2DMSARRAY = 246,          /* UIMAGE2DMSARRAY  */
  YYSYMBOL_F16IMAGE1D = 247,               /* F16IMAGE1D  */
  YYSYMBOL_F16IMAGE2D = 248,               /* F16IMAGE2D  */
  YYSYMBOL_F16IMAGE3D = 249,               /* F16IMAGE3D  */
  YYSYMBOL_F16IMAGE2DRECT = 250,           /* F16IMAGE2DRECT  */
  YYSYMBOL_F16IMAGECUBE = 251,             /* F16IMAGECUBE  */
  YYSYMBOL_F16IMAGE1DARRAY = 252,          /* F16IMAGE1DARRAY  */
  YYSYMBOL_F16IMAGE2DARRAY = 253,          /* F16IMAGE2DARRAY  */
  YYSYMBOL_F16IMAGECUBEARRAY = 254,        /* F16IMAGECUBEARRAY  */
  YYSYMBOL_F16IMAGEBUFFER = 255,           /* F16IMAGEBUFFER  */
  YYSYMBOL_F16IMAGE2DMS = 256,             /* F16IMAGE2DMS  */
  YYSYMBOL_F16IMAGE2DMSARRAY = 257,        /* F16IMAGE2DMSARRAY  */
  YYSYMBOL_I64IMAGE1D = 258,               /* I64IMAGE1D  */
  YYSYMBOL_U64IMAGE1D = 259,               /* U64IMAGE1D  */
  YYSYMBOL_I64IMAGE2D = 260,               /* I64IMAGE2D  */
  YYSYMBOL_U64IMAGE2D = 261,               /* U64IMAGE2D  */
  YYSYMBOL_I64IMAGE3D = 262,               /* I64IMAGE3D  */
  YYSYMBOL_U64IMAGE3D = 263,               /* U64IMAGE3D  */
  YYSYMBOL_I64IMAGE2DRECT = 264,           /* I64IMAGE2DRECT  */
  YYSYMBOL_U64IMAGE2DRECT = 265,           /* U64IMAGE2DRECT  */
  YYSYMBOL_I64IMAGECUBE = 266,             /* I64IMAGECUBE  */
  YYSYMBOL_U64IMAGECUBE = 267,             /* U64IMAGECUBE  */
  YYSYMBOL_I64IMAGEBUFFER = 268,           /* I64IMAGEBUFFER  */
  YYSYMBOL_U64IMAGEBUFFER = 269,           /* U64IMAGEBUFFER  */
  YYSYMBOL_I64IMAGE1DARRAY = 270,          /* I64IMAGE1DARRAY  */
  YYSYMBOL_U64IMAGE1DARRAY = 271,          /* U64IMAGE1DARRAY  */
  YYSYMBOL_I64IMAGE2DARRAY = 272,          /* I64IMAGE2DARRAY  */
  YYSYMBOL_U64IMAGE2DARRAY = 273,          /* U64IMAGE2DARRAY  */
  YYSYMBOL_I64IMAGECUBEARRAY = 274,        /* I64IMAGECUBEARRAY  */
  YYSYMBOL_U64IMAGECUBEARRAY = 275,        /* U64IMAGECUBEARRAY  */
  YYSYMBOL_I64IMAGE2DMS = 276,             /* I64IMAGE2DMS  */
  YYSYMBOL_U64IMAGE2DMS = 277,             /* U64IMAGE2DMS  */
  YYSYMBOL_I64IMAGE2DMSARRAY = 278,        /* I64IMAGE2DMSARRAY  */
  YYSYMBOL_U64IMAGE2DMSARRAY = 279,        /* U64IMAGE2DMSARRAY  */
  YYSYMBOL_TEXTURECUBEARRAY = 280,         /* TEXTURECUBEARRAY  */
  YYSYMBOL_ITEXTURECUBEARRAY = 281,        /* ITEXTURECUBEARRAY  */
  YYSYMBOL_UTEXTURECUBEARRAY = 282,        /* UTEXTURECUBEARRAY  */
  YYSYMBOL_TEXTURE1D = 283,                /* TEXTURE1D  */
  YYSYMBOL_ITEXTURE1D = 284,               /* ITEXTURE1D  */
  YYSYMBOL_UTEXTURE1D = 285,               /* UTEXTURE1D  */
  YYSYMBOL_TEXTURE1DARRAY = 286,           /* TEXTURE1DARRAY  */
  YYSYMBOL_ITEXTURE1DARRAY = 287,          /* ITEXTURE1DARRAY  */
  YYSYMBOL_UTEXTURE1DARRAY = 288,          /* UTEXTURE1DARRAY  */
  YYSYMBOL_TEXTURE2DRECT = 289,            /* TEXTURE2DRECT  */
  YYSYMBOL_ITEXTURE2DRECT = 290,           /* ITEXTURE2DRECT  */
  YYSYMBOL_UTEXTURE2DRECT = 291,           /* UTEXTURE2DRECT  */
  YYSYMBOL_TEXTUREBUFFER = 292,            /* TEXTUREBUFFER  */
  YYSYMBOL_ITEXTUREBUFFER = 293,           /* ITEXTUREBUFFER  */
  YYSYMBOL_UTEXTUREBUFFER = 294,           /* UTEXTUREBUFFER  */
  YYSYMBOL_TEXTURE2DMS = 295,              /* TEXTURE2DMS  */
  YYSYMBOL_ITEXTURE2DMS = 296,             /* ITEXTURE2DMS  */
  YYSYMBOL_UTEXTURE2DMS = 297,             /* UTEXTURE2DMS  */
  YYSYMBOL_TEXTURE2DMSARRAY = 298,         /* TEXTURE2DMSARRAY  */
  YYSYMBOL_ITEXTURE2DMSARRAY = 299,        /* ITEXTURE2DMSARRAY  */
  YYSYMBOL_UTEXTURE2DMSARRAY = 300,        /* UTEXTURE2DMSARRAY  */
  YYSYMBOL_F16TEXTURE1D = 301,             /* F16TEXTURE1D  */
  YYSYMBOL_F16TEXTURE2D = 302,             /* F16TEXTURE2D  */
  YYSYMBOL_F16TEXTURE3D = 303,             /* F16TEXTURE3D  */
  YYSYMBOL_F16TEXTURE2DRECT = 304,         /* F16TEXTURE2DRECT  */
  YYSYMBOL_F16TEXTURECUBE = 305,           /* F16TEXTURECUBE  */
  YYSYMBOL_F16TEXTURE1DARRAY = 306,        /* F16TEXTURE1DARRAY  */
  YYSYMBOL_F16TEXTURE2DARRAY = 307,        /* F16TEXTURE2DARRAY  */
  YYSYMBOL_F16TEXTURECUBEARRAY = 308,      /* F16TEXTURECUBEARRAY  */
  YYSYMBOL_F16TEXTUREBUFFER = 309,         /* F16TEXTUREBUFFER  */
  YYSYMBOL_F16TEXTURE2DMS = 310,           /* F16TEXTURE2DMS  */
  YYSYMBOL_F16TEXTURE2DMSARRAY = 311,      /* F16TEXTURE2DMSARRAY  */
  YYSYMBOL_SUBPASSINPUT = 312,             /* SUBPASSINPUT  */
  YYSYMBOL_SUBPASSINPUTMS = 313,           /* SUBPASSINPUTMS  */
  YYSYMBOL_ISUBPASSINPUT = 314,            /* ISUBPASSINPUT  */
  YYSYMBOL_ISUBPASSINPUTMS = 315,          /* ISUBPASSINPUTMS  */
  YYSYMBOL_USUBPASSINPUT = 316,            /* USUBPASSINPUT  */
  YYSYMBOL_USUBPASSINPUTMS = 317,          /* USUBPASSINPUTMS  */
  YYSYMBOL_F16SUBPASSINPUT = 318,          /* F16SUBPASSINPUT  */
  YYSYMBOL_F16SUBPASSINPUTMS = 319,        /* F16SUBPASSINPUTMS  */
  YYSYMBOL_SPIRV_INSTRUCTION = 320,        /* SPIRV_INSTRUCTION  */
  YYSYMBOL_SPIRV_EXECUTION_MODE = 321,     /* SPIRV_EXECUTION_MODE  */
  YYSYMBOL_SPIRV_EXECUTION_MODE_ID = 322,  /* SPIRV_EXECUTION_MODE_ID  */
  YYSYMBOL_SPIRV_DECORATE = 323,           /* SPIRV_DECORATE  */
  YYSYMBOL_SPIRV_DECORATE_ID = 324,        /* SPIRV_DECORATE_ID  */
  YYSYMBOL_SPIRV_DECORATE_STRING = 325,    /* SPIRV_DECORATE_STRING  */
  YYSYMBOL_SPIRV_TYPE = 326,               /* SPIRV_TYPE  */
  YYSYMBOL_SPIRV_STORAGE_CLASS = 327,      /* SPIRV_STORAGE_CLASS  */
  YYSYMBOL_SPIRV_BY_REFERENCE = 328,       /* SPIRV_BY_REFERENCE  */
  YYSYMBOL_SPIRV_LITERAL = 329,            /* SPIRV_LITERAL  */
  YYSYMBOL_ATTACHMENTEXT = 330,            /* ATTACHMENTEXT  */
  YYSYMBOL_IATTACHMENTEXT = 331,           /* IATTACHMENTEXT  */
  YYSYMBOL_UATTACHMENTEXT = 332,           /* UATTACHMENTEXT  */
  YYSYMBOL_LEFT_OP = 333,                  /* LEFT_OP  */
  YYSYMBOL_RIGHT_OP = 334,                 /* RIGHT_OP  */
  YYSYMBOL_INC_OP = 335,                   /* INC_OP  */
  YYSYMBOL_DEC_OP = 336,                   /* DEC_OP  */
  YYSYMBOL_LE_OP = 337,                    /* LE_OP  */
  YYSYMBOL_GE_OP = 338,                    /* GE_OP  */
  YYSYMBOL_EQ_OP = 339,                    /* EQ_OP  */
  YYSYMBOL_NE_OP = 340,                    /* NE_OP  */
  YYSYMBOL_AND_OP = 341,                   /* AND_OP  */
  YYSYMBOL_OR_OP = 342,                    /* OR_OP  */
  YYSYMBOL_XOR_OP = 343,                   /* XOR_OP  */
  YYSYMBOL_MUL_ASSIGN = 344,               /* MUL_ASSIGN  */
  YYSYMBOL_DIV_ASSIGN = 345,               /* DIV_ASSIGN  */
  YYSYMBOL_ADD_ASSIGN = 346,               /* ADD_ASSIGN  */
  YYSYMBOL_MOD_ASSIGN = 347,               /* MOD_ASSIGN  */
  YYSYMBOL_LEFT_ASSIGN = 348,              /* LEFT_ASSIGN  */
  YYSYMBOL_RIGHT_ASSIGN = 349,             /* RIGHT_ASSIGN  */
  YYSYMBOL_AND_ASSIGN = 350,               /* AND_ASSIGN  */
  YYSYMBOL_XOR_ASSIGN = 351,               /* XOR_ASSIGN  */
  YYSYMBOL_OR_ASSIGN = 352,                /* OR_ASSIGN  */
  YYSYMBOL_SUB_ASSIGN = 353,               /* SUB_ASSIGN  */
  YYSYMBOL_STRING_LITERAL = 354,           /* STRING_LITERAL  */
  YYSYMBOL_LEFT_PAREN = 355,               /* LEFT_PAREN  */
  YYSYMBOL_RIGHT_PAREN = 356,              /* RIGHT_PAREN  */
  YYSYMBOL_LEFT_BRACKET = 357,             /* LEFT_BRACKET  */
  YYSYMBOL_RIGHT_BRACKET = 358,            /* RIGHT_BRACKET  */
  YYSYMBOL_LEFT_BRACE = 359,               /* LEFT_BRACE  */
  YYSYMBOL_RIGHT_BRACE = 360,              /* RIGHT_BRACE  */
  YYSYMBOL_DOT = 361,                      /* DOT  */
  YYSYMBOL_COMMA = 362,                    /* COMMA  */
  YYSYMBOL_COLON = 363,                    /* COLON  */
  YYSYMBOL_EQUAL = 364,                    /* EQUAL  */
  YYSYMBOL_SEMICOLON = 365,                /* SEMICOLON  */
  YYSYMBOL_BANG = 366,                     /* BANG  */
  YYSYMBOL_DASH = 367,                     /* DASH  */
  YYSYMBOL_TILDE = 368,                    /* TILDE  */
  YYSYMBOL_PLUS = 369,                     /* PLUS  */
  YYSYMBOL_STAR = 370,                     /* STAR  */
  YYSYMBOL_SLASH = 371,                    /* SLASH  */
  YYSYMBOL_PERCENT = 372,                  /* PERCENT  */
  YYSYMBOL_LEFT_ANGLE = 373,               /* LEFT_ANGLE  */
  YYSYMBOL_RIGHT_ANGLE = 374,              /* RIGHT_ANGLE  */
  YYSYMBOL_VERTICAL_BAR = 375,             /* VERTICAL_BAR  */
  YYSYMBOL_CARET = 376,                    /* CARET  */
  YYSYMBOL_AMPERSAND = 377,                /* AMPERSAND  */
  YYSYMBOL_QUESTION = 378,                 /* QUESTION  */
  YYSYMBOL_INVARIANT = 379,                /* INVARIANT  */
  YYSYMBOL_HIGH_PRECISION = 380,           /* HIGH_PRECISION  */
  YYSYMBOL_MEDIUM_PRECISION = 381,         /* MEDIUM_PRECISION  */
  YYSYMBOL_LOW_PRECISION = 382,            /* LOW_PRECISION  */
  YYSYMBOL_PRECISION = 383,                /* PRECISION  */
  YYSYMBOL_PACKED = 384,                   /* PACKED  */
  YYSYMBOL_RESOURCE = 385,                 /* RESOURCE  */
  YYSYMBOL_SUPERP = 386,                   /* SUPERP  */
  YYSYMBOL_FLOATCONSTANT = 387,            /* FLOATCONSTANT  */
  YYSYMBOL_INTCONSTANT = 388,              /* INTCONSTANT  */
  YYSYMBOL_UINTCONSTANT = 389,             /* UINTCONSTANT  */
  YYSYMBOL_BOOLCONSTANT = 390,             /* BOOLCONSTANT  */
  YYSYMBOL_IDENTIFIER = 391,               /* IDENTIFIER  */
  YYSYMBOL_TYPE_NAME = 392,                /* TYPE_NAME  */
  YYSYMBOL_CENTROID = 393,                 /* CENTROID  */
  YYSYMBOL_IN = 394,                       /* IN  */
  YYSYMBOL_OUT = 395,                      /* OUT  */
  YYSYMBOL_INOUT = 396,                    /* INOUT  */
  YYSYMBOL_STRUCT = 397,                   /* STRUCT  */
  YYSYMBOL_VOID = 398,                     /* VOID  */
  YYSYMBOL_WHILE = 399,                    /* WHILE  */
  YYSYMBOL_BREAK = 400,                    /* BREAK  */
  YYSYMBOL_CONTINUE = 401,                 /* CONTINUE  */
  YYSYMBOL_DO = 402,                       /* DO  */
  YYSYMBOL_ELSE = 403,                     /* ELSE  */
  YYSYMBOL_FOR = 404,                      /* FOR  */
  YYSYMBOL_IF = 405,                       /* IF  */
  YYSYMBOL_DISCARD = 406,                  /* DISCARD  */
  YYSYMBOL_RETURN = 407,                   /* RETURN  */
  YYSYMBOL_SWITCH = 408,                   /* SWITCH  */
  YYSYMBOL_CASE = 409,                     /* CASE  */
  YYSYMBOL_DEFAULT = 410,                  /* DEFAULT  */
  YYSYMBOL_TERMINATE_INVOCATION = 411,     /* TERMINATE_INVOCATION  */
  YYSYMBOL_TERMINATE_RAY = 412,            /* TERMINATE_RAY  */
  YYSYMBOL_IGNORE_INTERSECTION = 413,      /* IGNORE_INTERSECTION  */
  YYSYMBOL_UNIFORM = 414,                  /* UNIFORM  */
  YYSYMBOL_SHARED = 415,                   /* SHARED  */
  YYSYMBOL_BUFFER = 416,                   /* BUFFER  */
  YYSYMBOL_TILEIMAGEEXT = 417,             /* TILEIMAGEEXT  */
  YYSYMBOL_FLAT = 418,                     /* FLAT  */
  YYSYMBOL_SMOOTH = 419,                   /* SMOOTH  */
  YYSYMBOL_LAYOUT = 420,                   /* LAYOUT  */
  YYSYMBOL_DOUBLECONSTANT = 421,           /* DOUBLECONSTANT  */
  YYSYMBOL_INT16CONSTANT = 422,            /* INT16CONSTANT  */
  YYSYMBOL_UINT16CONSTANT = 423,           /* UINT16CONSTANT  */
  YYSYMBOL_FLOAT16CONSTANT = 424,          /* FLOAT16CONSTANT  */
  YYSYMBOL_INT32CONSTANT = 425,            /* INT32CONSTANT  */
  YYSYMBOL_UINT32CONSTANT = 426,           /* UINT32CONSTANT  */
  YYSYMBOL_INT64CONSTANT = 427,            /* INT64CONSTANT  */
  YYSYMBOL_UINT64CONSTANT = 428,           /* UINT64CONSTANT  */
  YYSYMBOL_SUBROUTINE = 429,               /* SUBROUTINE  */
  YYSYMBOL_DEMOTE = 430,                   /* DEMOTE  */
  YYSYMBOL_PAYLOADNV = 431,                /* PAYLOADNV  */
  YYSYMBOL_PAYLOADINNV = 432,              /* PAYLOADINNV  */
  YYSYMBOL_HITATTRNV = 433,                /* HITATTRNV  */
  YYSYMBOL_CALLDATANV = 434,               /* CALLDATANV  */
  YYSYMBOL_CALLDATAINNV = 435,             /* CALLDATAINNV  */
  YYSYMBOL_PAYLOADEXT = 436,               /* PAYLOADEXT  */
  YYSYMBOL_PAYLOADINEXT = 437,             /* PAYLOADINEXT  */
  YYSYMBOL_HITATTREXT = 438,               /* HITATTREXT  */
  YYSYMBOL_CALLDATAEXT = 439,              /* CALLDATAEXT  */
  YYSYMBOL_CALLDATAINEXT = 440,            /* CALLDATAINEXT  */
  YYSYMBOL_PATCH = 441,                    /* PATCH  */
  YYSYMBOL_SAMPLE = 442,                   /* SAMPLE  */
  YYSYMBOL_NONUNIFORM = 443,               /* NONUNIFORM  */
  YYSYMBOL_COHERENT = 444,                 /* COHERENT  */
  YYSYMBOL_VOLATILE = 445,                 /* VOLATILE  */
  YYSYMBOL_RESTRICT = 446,                 /* RESTRICT  */
  YYSYMBOL_READONLY = 447,                 /* READONLY  */
  YYSYMBOL_WRITEONLY = 448,                /* WRITEONLY  */
  YYSYMBOL_DEVICECOHERENT = 449,           /* DEVICECOHERENT  */
  YYSYMBOL_QUEUEFAMILYCOHERENT = 450,      /* QUEUEFAMILYCOHERENT  */
  YYSYMBOL_WORKGROUPCOHERENT = 451,        /* WORKGROUPCOHERENT  */
  YYSYMBOL_SUBGROUPCOHERENT = 452,         /* SUBGROUPCOHERENT  */
  YYSYMBOL_NONPRIVATE = 453,               /* NONPRIVATE  */
  YYSYMBOL_SHADERCALLCOHERENT = 454,       /* SHADERCALLCOHERENT  */
  YYSYMBOL_NOPERSPECTIVE = 455,            /* NOPERSPECTIVE  */
  YYSYMBOL_EXPLICITINTERPAMD = 456,        /* EXPLICITINTERPAMD  */
  YYSYMBOL_PERVERTEXEXT = 457,             /* PERVERTEXEXT  */
  YYSYMBOL_PERVERTEXNV = 458,              /* PERVERTEXNV  */
  YYSYMBOL_PERPRIMITIVENV = 459,           /* PERPRIMITIVENV  */
  YYSYMBOL_PERVIEWNV = 460,                /* PERVIEWNV  */
  YYSYMBOL_PERTASKNV = 461,                /* PERTASKNV  */
  YYSYMBOL_PERPRIMITIVEEXT = 462,          /* PERPRIMITIVEEXT  */
  YYSYMBOL_TASKPAYLOADWORKGROUPEXT = 463,  /* TASKPAYLOADWORKGROUPEXT  */
  YYSYMBOL_PRECISE = 464,                  /* PRECISE  */
  YYSYMBOL_YYACCEPT = 465,                 /* $accept  */
  YYSYMBOL_variable_identifier = 466,      /* variable_identifier  */
  YYSYMBOL_primary_expression = 467,       /* primary_expression  */
  YYSYMBOL_postfix_expression = 468,       /* postfix_expression  */
  YYSYMBOL_integer_expression = 469,       /* integer_expression  */
  YYSYMBOL_function_call = 470,            /* function_call  */
  YYSYMBOL_function_call_or_method = 471,  /* function_call_or_method  */
  YYSYMBOL_function_call_generic = 472,    /* function_call_generic  */
  YYSYMBOL_function_call_header_no_parameters = 473, /* function_call_header_no_parameters  */
  YYSYMBOL_function_call_header_with_parameters = 474, /* function_call_header_with_parameters  */
  YYSYMBOL_function_call_header = 475,     /* function_call_header  */
  YYSYMBOL_function_identifier = 476,      /* function_identifier  */
  YYSYMBOL_unary_expression = 477,         /* unary_expression  */
  YYSYMBOL_unary_operator = 478,           /* unary_operator  */
  YYSYMBOL_multiplicative_expression = 479, /* multiplicative_expression  */
  YYSYMBOL_additive_expression = 480,      /* additive_expression  */
  YYSYMBOL_shift_expression = 481,         /* shift_expression  */
  YYSYMBOL_relational_expression = 482,    /* relational_expression  */
  YYSYMBOL_equality_expression = 483,      /* equality_expression  */
  YYSYMBOL_and_expression = 484,           /* and_expression  */
  YYSYMBOL_exclusive_or_expression = 485,  /* exclusive_or_expression  */
  YYSYMBOL_inclusive_or_expression = 486,  /* inclusive_or_expression  */
  YYSYMBOL_logical_and_expression = 487,   /* logical_and_expression  */
  YYSYMBOL_logical_xor_expression = 488,   /* logical_xor_expression  */
  YYSYMBOL_logical_or_expression = 489,    /* logical_or_expression  */
  YYSYMBOL_conditional_expression = 490,   /* conditional_expression  */
  YYSYMBOL_491_1 = 491,                    /* $@1  */
  YYSYMBOL_assignment_expression = 492,    /* assignment_expression  */
  YYSYMBOL_assignment_operator = 493,      /* assignment_operator  */
  YYSYMBOL_expression = 494,               /* expression  */
  YYSYMBOL_constant_expression = 495,      /* constant_expression  */
  YYSYMBOL_declaration = 496,              /* declaration  */
  YYSYMBOL_block_structure = 497,          /* block_structure  */
  YYSYMBOL_498_2 = 498,                    /* $@2  */
  YYSYMBOL_identifier_list = 499,          /* identifier_list  */
  YYSYMBOL_function_prototype = 500,       /* function_prototype  */
  YYSYMBOL_function_declarator = 501,      /* function_declarator  */
  YYSYMBOL_function_header_with_parameters = 502, /* function_header_with_parameters  */
  YYSYMBOL_function_header = 503,          /* function_header  */
  YYSYMBOL_parameter_declarator = 504,     /* parameter_declarator  */
  YYSYMBOL_parameter_declaration = 505,    /* parameter_declaration  */
  YYSYMBOL_parameter_type_specifier = 506, /* parameter_type_specifier  */
  YYSYMBOL_init_declarator_list = 507,     /* init_declarator_list  */
  YYSYMBOL_single_declaration = 508,       /* single_declaration  */
  YYSYMBOL_fully_specified_type = 509,     /* fully_specified_type  */
  YYSYMBOL_invariant_qualifier = 510,      /* invariant_qualifier  */
  YYSYMBOL_interpolation_qualifier = 511,  /* interpolation_qualifier  */
  YYSYMBOL_layout_qualifier = 512,         /* layout_qualifier  */
  YYSYMBOL_layout_qualifier_id_list = 513, /* layout_qualifier_id_list  */
  YYSYMBOL_layout_qualifier_id = 514,      /* layout_qualifier_id  */
  YYSYMBOL_precise_qualifier = 515,        /* precise_qualifier  */
  YYSYMBOL_type_qualifier = 516,           /* type_qualifier  */
  YYSYMBOL_single_type_qualifier = 517,    /* single_type_qualifier  */
  YYSYMBOL_storage_qualifier = 518,        /* storage_qualifier  */
  YYSYMBOL_non_uniform_qualifier = 519,    /* non_uniform_qualifier  */
  YYSYMBOL_type_name_list = 520,           /* type_name_list  */
  YYSYMBOL_type_specifier = 521,           /* type_specifier  */
  YYSYMBOL_array_specifier = 522,          /* array_specifier  */
  YYSYMBOL_type_parameter_specifier_opt = 523, /* type_parameter_specifier_opt  */
  YYSYMBOL_type_parameter_specifier = 524, /* type_parameter_specifier  */
  YYSYMBOL_type_parameter_specifier_list = 525, /* type_parameter_specifier_list  */
  YYSYMBOL_type_specifier_nonarray = 526,  /* type_specifier_nonarray  */
  YYSYMBOL_precision_qualifier = 527,      /* precision_qualifier  */
  YYSYMBOL_struct_specifier = 528,         /* struct_specifier  */
  YYSYMBOL_529_3 = 529,                    /* $@3  */
  YYSYMBOL_530_4 = 530,                    /* $@4  */
  YYSYMBOL_struct_declaration_list = 531,  /* struct_declaration_list  */
  YYSYMBOL_struct_declaration = 532,       /* struct_declaration  */
  YYSYMBOL_struct_declarator_list = 533,   /* struct_declarator_list  */
  YYSYMBOL_struct_declarator = 534,        /* struct_declarator  */
  YYSYMBOL_initializer = 535,              /* initializer  */
  YYSYMBOL_initializer_list = 536,         /* initializer_list  */
  YYSYMBOL_declaration_statement = 537,    /* declaration_statement  */
  YYSYMBOL_statement = 538,                /* statement  */
  YYSYMBOL_simple_statement = 539,         /* simple_statement  */
  YYSYMBOL_demote_statement = 540,         /* demote_statement  */
  YYSYMBOL_compound_statement = 541,       /* compound_statement  */
  YYSYMBOL_542_5 = 542,                    /* $@5  */
  YYSYMBOL_543_6 = 543,                    /* $@6  */
  YYSYMBOL_statement_no_new_scope = 544,   /* statement_no_new_scope  */
  YYSYMBOL_statement_scoped = 545,         /* statement_scoped  */
  YYSYMBOL_546_7 = 546,                    /* $@7  */
  YYSYMBOL_547_8 = 547,                    /* $@8  */
  YYSYMBOL_compound_statement_no_new_scope = 548, /* compound_statement_no_new_scope  */
  YYSYMBOL_statement_list = 549,           /* statement_list  */
  YYSYMBOL_expression_statement = 550,     /* expression_statement  */
  YYSYMBOL_selection_statement = 551,      /* selection_statement  */
  YYSYMBOL_selection_statement_nonattributed = 552, /* selection_statement_nonattributed  */
  YYSYMBOL_selection_rest_statement = 553, /* selection_rest_statement  */
  YYSYMBOL_condition = 554,                /* condition  */
  YYSYMBOL_switch_statement = 555,         /* switch_statement  */
  YYSYMBOL_switch_statement_nonattributed = 556, /* switch_statement_nonattributed  */
  YYSYMBOL_557_9 = 557,                    /* $@9  */
  YYSYMBOL_switch_statement_list = 558,    /* switch_statement_list  */
  YYSYMBOL_case_label = 559,               /* case_label  */
  YYSYMBOL_iteration_statement = 560,      /* iteration_statement  */
  YYSYMBOL_iteration_statement_nonattributed = 561, /* iteration_statement_nonattributed  */
  YYSYMBOL_562_10 = 562,                   /* $@10  */
  YYSYMBOL_563_11 = 563,                   /* $@11  */
  YYSYMBOL_564_12 = 564,                   /* $@12  */
  YYSYMBOL_for_init_statement = 565,       /* for_init_statement  */
  YYSYMBOL_conditionopt = 566,             /* conditionopt  */
  YYSYMBOL_for_rest_statement = 567,       /* for_rest_statement  */
  YYSYMBOL_jump_statement = 568,           /* jump_statement  */
  YYSYMBOL_translation_unit = 569,         /* translation_unit  */
  YYSYMBOL_external_declaration = 570,     /* external_declaration  */
  YYSYMBOL_function_definition = 571,      /* function_definition  */
  YYSYMBOL_572_13 = 572,                   /* $@13  */
  YYSYMBOL_attribute = 573,                /* attribute  */
  YYSYMBOL_attribute_list = 574,           /* attribute_list  */
  YYSYMBOL_single_attribute = 575,         /* single_attribute  */
  YYSYMBOL_spirv_requirements_list = 576,  /* spirv_requirements_list  */
  YYSYMBOL_spirv_requirements_parameter = 577, /* spirv_requirements_parameter  */
  YYSYMBOL_spirv_extension_list = 578,     /* spirv_extension_list  */
  YYSYMBOL_spirv_capability_list = 579,    /* spirv_capability_list  */
  YYSYMBOL_spirv_execution_mode_qualifier = 580, /* spirv_execution_mode_qualifier  */
  YYSYMBOL_spirv_execution_mode_parameter_list = 581, /* spirv_execution_mode_parameter_list  */
  YYSYMBOL_spirv_execution_mode_parameter = 582, /* spirv_execution_mode_parameter  */
  YYSYMBOL_spirv_execution_mode_id_parameter_list = 583, /* spirv_execution_mode_id_parameter_list  */
  YYSYMBOL_spirv_storage_class_qualifier = 584, /* spirv_storage_class_qualifier  */
  YYSYMBOL_spirv_decorate_qualifier = 585, /* spirv_decorate_qualifier  */
  YYSYMBOL_spirv_decorate_parameter_list = 586, /* spirv_decorate_parameter_list  */
  YYSYMBOL_spirv_decorate_parameter = 587, /* spirv_decorate_parameter  */
  YYSYMBOL_spirv_decorate_id_parameter_list = 588, /* spirv_decorate_id_parameter_list  */
  YYSYMBOL_spirv_decorate_id_parameter = 589, /* spirv_decorate_id_parameter  */
  YYSYMBOL_spirv_decorate_string_parameter_list = 590, /* spirv_decorate_string_parameter_list  */
  YYSYMBOL_spirv_type_specifier = 591,     /* spirv_type_specifier  */
  YYSYMBOL_spirv_type_parameter_list = 592, /* spirv_type_parameter_list  */
  YYSYMBOL_spirv_type_parameter = 593,     /* spirv_type_parameter  */
  YYSYMBOL_spirv_instruction_qualifier = 594, /* spirv_instruction_qualifier  */
  YYSYMBOL_spirv_instruction_qualifier_list = 595, /* spirv_instruction_qualifier_list  */
  YYSYMBOL_spirv_instruction_qualifier_id = 596 /* spirv_instruction_qualifier_id  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Second part of user prologue.  */
#line 136 "MachineIndependent/glslang.y"


/* windows only pragma */
#ifdef _MSC_VER
    #pragma warning(disable : 4065)
    #pragma warning(disable : 4127)
    #pragma warning(disable : 4244)
#endif

#define parseContext (*pParseContext)
#define yyerror(context, msg) context->parserError(msg)

extern int yylex(YYSTYPE*, TParseContext&);


#line 743 "MachineIndependent/glslang_tab.cpp"


#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  452
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   12701

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  465
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  132
/* YYNRULES -- Number of rules.  */
#define YYNRULES  700
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  946

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   719


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int16 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   395,   395,   401,   404,   409,   412,   415,   419,   423,
     426,   430,   434,   438,   442,   446,   450,   456,   464,   467,
     470,   473,   476,   481,   489,   496,   503,   509,   513,   520,
     523,   529,   536,   546,   554,   559,   587,   596,   602,   606,
     610,   630,   631,   632,   633,   639,   640,   645,   650,   659,
     660,   665,   673,   674,   680,   689,   690,   695,   700,   705,
     713,   714,   723,   735,   736,   745,   746,   755,   756,   765,
     766,   774,   775,   783,   784,   792,   793,   793,   811,   812,
     828,   832,   836,   840,   845,   849,   853,   857,   861,   865,
     869,   876,   879,   890,   897,   903,   910,   916,   921,   928,
     932,   936,   940,   945,   950,   959,   959,   970,   974,   981,
     985,   991,   997,  1007,  1010,  1017,  1025,  1045,  1068,  1083,
    1108,  1119,  1129,  1139,  1149,  1158,  1161,  1165,  1169,  1174,
    1182,  1189,  1194,  1199,  1204,  1213,  1223,  1250,  1259,  1266,
    1274,  1281,  1288,  1296,  1304,  1314,  1324,  1331,  1342,  1348,
    1351,  1358,  1362,  1366,  1375,  1385,  1388,  1399,  1402,  1405,
    1409,  1413,  1418,  1422,  1425,  1430,  1434,  1439,  1448,  1452,
    1457,  1463,  1469,  1476,  1481,  1486,  1494,  1500,  1512,  1526,
    1532,  1537,  1545,  1553,  1561,  1569,  1577,  1585,  1593,  1601,
    1609,  1616,  1623,  1627,  1632,  1637,  1642,  1647,  1652,  1657,
    1661,  1665,  1669,  1673,  1679,  1685,  1697,  1704,  1707,  1716,
    1723,  1734,  1739,  1747,  1751,  1761,  1764,  1770,  1776,  1781,
    1789,  1799,  1803,  1807,  1811,  1816,  1820,  1825,  1830,  1835,
    1840,  1845,  1850,  1855,  1860,  1865,  1871,  1877,  1883,  1888,
    1893,  1898,  1903,  1908,  1913,  1918,  1923,  1928,  1933,  1938,
    1944,  1951,  1956,  1961,  1966,  1971,  1976,  1981,  1986,  1991,
    1996,  2001,  2006,  2014,  2022,  2030,  2036,  2042,  2048,  2054,
    2060,  2066,  2072,  2078,  2084,  2090,  2096,  2102,  2108,  2114,
    2120,  2126,  2132,  2138,  2144,  2150,  2156,  2162,  2168,  2174,
    2180,  2186,  2192,  2198,  2204,  2210,  2216,  2222,  2228,  2236,
    2244,  2252,  2260,  2268,  2276,  2284,  2292,  2300,  2308,  2316,
    2324,  2330,  2336,  2342,  2348,  2354,  2360,  2366,  2372,  2378,
    2384,  2390,  2396,  2402,  2408,  2414,  2420,  2426,  2432,  2438,
    2444,  2450,  2456,  2462,  2468,  2474,  2480,  2486,  2492,  2498,
    2504,  2510,  2516,  2522,  2528,  2534,  2540,  2544,  2548,  2552,
    2557,  2563,  2568,  2573,  2578,  2583,  2588,  2593,  2599,  2604,
    2609,  2614,  2619,  2624,  2630,  2636,  2642,  2648,  2654,  2660,
    2666,  2672,  2678,  2684,  2690,  2696,  2702,  2708,  2713,  2718,
    2723,  2728,  2733,  2738,  2744,  2749,  2754,  2759,  2764,  2769,
    2774,  2779,  2785,  2790,  2795,  2800,  2805,  2810,  2815,  2820,
    2825,  2830,  2835,  2840,  2845,  2850,  2855,  2861,  2866,  2871,
    2877,  2883,  2888,  2893,  2898,  2904,  2909,  2914,  2919,  2925,
    2930,  2935,  2940,  2946,  2951,  2956,  2961,  2967,  2973,  2979,
    2985,  2990,  2996,  3002,  3008,  3013,  3018,  3023,  3028,  3033,
    3039,  3044,  3049,  3054,  3060,  3065,  3070,  3075,  3081,  3086,
    3091,  3096,  3102,  3107,  3112,  3117,  3123,  3128,  3133,  3138,
    3144,  3149,  3154,  3159,  3165,  3170,  3175,  3180,  3186,  3191,
    3196,  3201,  3207,  3212,  3217,  3222,  3228,  3233,  3238,  3243,
    3249,  3254,  3259,  3264,  3270,  3275,  3280,  3285,  3291,  3296,
    3301,  3306,  3312,  3317,  3322,  3327,  3333,  3338,  3343,  3348,
    3353,  3358,  3363,  3368,  3373,  3378,  3383,  3388,  3393,  3398,
    3403,  3408,  3413,  3418,  3423,  3428,  3433,  3438,  3443,  3448,
    3453,  3459,  3465,  3471,  3477,  3483,  3489,  3495,  3502,  3509,
    3515,  3521,  3527,  3533,  3540,  3547,  3554,  3561,  3565,  3570,
    3575,  3591,  3596,  3601,  3609,  3609,  3620,  3620,  3630,  3633,
    3646,  3668,  3695,  3699,  3705,  3710,  3721,  3725,  3731,  3737,
    3748,  3751,  3758,  3762,  3763,  3769,  3770,  3771,  3772,  3773,
    3774,  3775,  3777,  3783,  3792,  3793,  3797,  3793,  3809,  3810,
    3814,  3814,  3821,  3821,  3835,  3838,  3846,  3854,  3865,  3866,
    3870,  3874,  3882,  3889,  3893,  3901,  3905,  3918,  3922,  3930,
    3930,  3950,  3953,  3959,  3971,  3983,  3987,  3995,  3995,  4010,
    4010,  4028,  4028,  4049,  4052,  4058,  4061,  4067,  4071,  4078,
    4083,  4088,  4095,  4098,  4102,  4107,  4111,  4121,  4125,  4134,
    4137,  4141,  4150,  4150,  4192,  4197,  4200,  4205,  4208,  4215,
    4218,  4223,  4226,  4231,  4234,  4239,  4242,  4247,  4251,  4256,
    4260,  4265,  4269,  4276,  4279,  4284,  4287,  4290,  4293,  4296,
    4301,  4310,  4321,  4326,  4334,  4338,  4343,  4347,  4352,  4356,
    4361,  4365,  4372,  4375,  4380,  4383,  4386,  4389,  4394,  4397,
    4402,  4408,  4411,  4414,  4417,  4422,  4426,  4431,  4435,  4440,
    4444,  4451,  4454,  4459,  4462,  4467,  4470,  4476,  4479,  4484,
    4487
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "CONST", "BOOL", "INT",
  "UINT", "FLOAT", "BVEC2", "BVEC3", "BVEC4", "IVEC2", "IVEC3", "IVEC4",
  "UVEC2", "UVEC3", "UVEC4", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3",
  "MAT4", "MAT2X2", "MAT2X3", "MAT2X4", "MAT3X2", "MAT3X3", "MAT3X4",
  "MAT4X2", "MAT4X3", "MAT4X4", "SAMPLER2D", "SAMPLER3D", "SAMPLERCUBE",
  "SAMPLER2DSHADOW", "SAMPLERCUBESHADOW", "SAMPLER2DARRAY",
  "SAMPLER2DARRAYSHADOW", "ISAMPLER2D", "ISAMPLER3D", "ISAMPLERCUBE",
  "ISAMPLER2DARRAY", "USAMPLER2D", "USAMPLER3D", "USAMPLERCUBE",
  "USAMPLER2DARRAY", "SAMPLER", "SAMPLERSHADOW", "TEXTURE2D", "TEXTURE3D",
  "TEXTURECUBE", "TEXTURE2DARRAY", "ITEXTURE2D", "ITEXTURE3D",
  "ITEXTURECUBE", "ITEXTURE2DARRAY", "UTEXTURE2D", "UTEXTURE3D",
  "UTEXTURECUBE", "UTEXTURE2DARRAY", "ATTRIBUTE", "VARYING", "FLOAT16_T",
  "FLOAT32_T", "DOUBLE", "FLOAT64_T", "INT64_T", "UINT64_T", "INT32_T",
  "UINT32_T", "INT16_T", "UINT16_T", "INT8_T", "UINT8_T", "I64VEC2",
  "I64VEC3", "I64VEC4", "U64VEC2", "U64VEC3", "U64VEC4", "I32VEC2",
  "I32VEC3", "I32VEC4", "U32VEC2", "U32VEC3", "U32VEC4", "I16VEC2",
  "I16VEC3", "I16VEC4", "U16VEC2", "U16VEC3", "U16VEC4", "I8VEC2",
  "I8VEC3", "I8VEC4", "U8VEC2", "U8VEC3", "U8VEC4", "DVEC2", "DVEC3",
  "DVEC4", "DMAT2", "DMAT3", "DMAT4", "F16VEC2", "F16VEC3", "F16VEC4",
  "F16MAT2", "F16MAT3", "F16MAT4", "F32VEC2", "F32VEC3", "F32VEC4",
  "F32MAT2", "F32MAT3", "F32MAT4", "F64VEC2", "F64VEC3", "F64VEC4",
  "F64MAT2", "F64MAT3", "F64MAT4", "DMAT2X2", "DMAT2X3", "DMAT2X4",
  "DMAT3X2", "DMAT3X3", "DMAT3X4", "DMAT4X2", "DMAT4X3", "DMAT4X4",
  "F16MAT2X2", "F16MAT2X3", "F16MAT2X4", "F16MAT3X2", "F16MAT3X3",
  "F16MAT3X4", "F16MAT4X2", "F16MAT4X3", "F16MAT4X4", "F32MAT2X2",
  "F32MAT2X3", "F32MAT2X4", "F32MAT3X2", "F32MAT3X3", "F32MAT3X4",
  "F32MAT4X2", "F32MAT4X3", "F32MAT4X4", "F64MAT2X2", "F64MAT2X3",
  "F64MAT2X4", "F64MAT3X2", "F64MAT3X3", "F64MAT3X4", "F64MAT4X2",
  "F64MAT4X3", "F64MAT4X4", "ATOMIC_UINT", "ACCSTRUCTNV", "ACCSTRUCTEXT",
  "RAYQUERYEXT", "FCOOPMATNV", "ICOOPMATNV", "UCOOPMATNV", "COOPMAT",
  "HITOBJECTNV", "HITOBJECTATTRNV", "SAMPLERCUBEARRAY",
  "SAMPLERCUBEARRAYSHADOW", "ISAMPLERCUBEARRAY", "USAMPLERCUBEARRAY",
  "SAMPLER1D", "SAMPLER1DARRAY", "SAMPLER1DARRAYSHADOW", "ISAMPLER1D",
  "SAMPLER1DSHADOW", "SAMPLER2DRECT", "SAMPLER2DRECTSHADOW",
  "ISAMPLER2DRECT", "USAMPLER2DRECT", "SAMPLERBUFFER", "ISAMPLERBUFFER",
  "USAMPLERBUFFER", "SAMPLER2DMS", "ISAMPLER2DMS", "USAMPLER2DMS",
  "SAMPLER2DMSARRAY", "ISAMPLER2DMSARRAY", "USAMPLER2DMSARRAY",
  "SAMPLEREXTERNALOES", "SAMPLEREXTERNAL2DY2YEXT", "ISAMPLER1DARRAY",
  "USAMPLER1D", "USAMPLER1DARRAY", "F16SAMPLER1D", "F16SAMPLER2D",
  "F16SAMPLER3D", "F16SAMPLER2DRECT", "F16SAMPLERCUBE",
  "F16SAMPLER1DARRAY", "F16SAMPLER2DARRAY", "F16SAMPLERCUBEARRAY",
  "F16SAMPLERBUFFER", "F16SAMPLER2DMS", "F16SAMPLER2DMSARRAY",
  "F16SAMPLER1DSHADOW", "F16SAMPLER2DSHADOW", "F16SAMPLER1DARRAYSHADOW",
  "F16SAMPLER2DARRAYSHADOW", "F16SAMPLER2DRECTSHADOW",
  "F16SAMPLERCUBESHADOW", "F16SAMPLERCUBEARRAYSHADOW", "IMAGE1D",
  "IIMAGE1D", "UIMAGE1D", "IMAGE2D", "IIMAGE2D", "UIMAGE2D", "IMAGE3D",
  "IIMAGE3D", "UIMAGE3D", "IMAGE2DRECT", "IIMAGE2DRECT", "UIMAGE2DRECT",
  "IMAGECUBE", "IIMAGECUBE", "UIMAGECUBE", "IMAGEBUFFER", "IIMAGEBUFFER",
  "UIMAGEBUFFER", "IMAGE1DARRAY", "IIMAGE1DARRAY", "UIMAGE1DARRAY",
  "IMAGE2DARRAY", "IIMAGE2DARRAY", "UIMAGE2DARRAY", "IMAGECUBEARRAY",
  "IIMAGECUBEARRAY", "UIMAGECUBEARRAY", "IMAGE2DMS", "IIMAGE2DMS",
  "UIMAGE2DMS", "IMAGE2DMSARRAY", "IIMAGE2DMSARRAY", "UIMAGE2DMSARRAY",
  "F16IMAGE1D", "F16IMAGE2D", "F16IMAGE3D", "F16IMAGE2DRECT",
  "F16IMAGECUBE", "F16IMAGE1DARRAY", "F16IMAGE2DARRAY",
  "F16IMAGECUBEARRAY", "F16IMAGEBUFFER", "F16IMAGE2DMS",
  "F16IMAGE2DMSARRAY", "I64IMAGE1D", "U64IMAGE1D", "I64IMAGE2D",
  "U64IMAGE2D", "I64IMAGE3D", "U64IMAGE3D", "I64IMAGE2DRECT",
  "U64IMAGE2DRECT", "I64IMAGECUBE", "U64IMAGECUBE", "I64IMAGEBUFFER",
  "U64IMAGEBUFFER", "I64IMAGE1DARRAY", "U64IMAGE1DARRAY",
  "I64IMAGE2DARRAY", "U64IMAGE2DARRAY", "I64IMAGECUBEARRAY",
  "U64IMAGECUBEARRAY", "I64IMAGE2DMS", "U64IMAGE2DMS", "I64IMAGE2DMSARRAY",
  "U64IMAGE2DMSARRAY", "TEXTURECUBEARRAY", "ITEXTURECUBEARRAY",
  "UTEXTURECUBEARRAY", "TEXTURE1D", "ITEXTURE1D", "UTEXTURE1D",
  "TEXTURE1DARRAY", "ITEXTURE1DARRAY", "UTEXTURE1DARRAY", "TEXTURE2DRECT",
  "ITEXTURE2DRECT", "UTEXTURE2DRECT", "TEXTUREBUFFER", "ITEXTUREBUFFER",
  "UTEXTUREBUFFER", "TEXTURE2DMS", "ITEXTURE2DMS", "UTEXTURE2DMS",
  "TEXTURE2DMSARRAY", "ITEXTURE2DMSARRAY", "UTEXTURE2DMSARRAY",
  "F16TEXTURE1D", "F16TEXTURE2D", "F16TEXTURE3D", "F16TEXTURE2DRECT",
  "F16TEXTURECUBE", "F16TEXTURE1DARRAY", "F16TEXTURE2DARRAY",
  "F16TEXTURECUBEARRAY", "F16TEXTUREBUFFER", "F16TEXTURE2DMS",
  "F16TEXTURE2DMSARRAY", "SUBPASSINPUT", "SUBPASSINPUTMS", "ISUBPASSINPUT",
  "ISUBPASSINPUTMS", "USUBPASSINPUT", "USUBPASSINPUTMS", "F16SUBPASSINPUT",
  "F16SUBPASSINPUTMS", "SPIRV_INSTRUCTION", "SPIRV_EXECUTION_MODE",
  "SPIRV_EXECUTION_MODE_ID", "SPIRV_DECORATE", "SPIRV_DECORATE_ID",
  "SPIRV_DECORATE_STRING", "SPIRV_TYPE", "SPIRV_STORAGE_CLASS",
  "SPIRV_BY_REFERENCE", "SPIRV_LITERAL", "ATTACHMENTEXT", "IATTACHMENTEXT",
  "UATTACHMENTEXT", "LEFT_OP", "RIGHT_OP", "INC_OP", "DEC_OP", "LE_OP",
  "GE_OP", "EQ_OP", "NE_OP", "AND_OP", "OR_OP", "XOR_OP", "MUL_ASSIGN",
  "DIV_ASSIGN", "ADD_ASSIGN", "MOD_ASSIGN", "LEFT_ASSIGN", "RIGHT_ASSIGN",
  "AND_ASSIGN", "XOR_ASSIGN", "OR_ASSIGN", "SUB_ASSIGN", "STRING_LITERAL",
  "LEFT_PAREN", "RIGHT_PAREN", "LEFT_BRACKET", "RIGHT_BRACKET",
  "LEFT_BRACE", "RIGHT_BRACE", "DOT", "COMMA", "COLON", "EQUAL",
  "SEMICOLON", "BANG", "DASH", "TILDE", "PLUS", "STAR", "SLASH", "PERCENT",
  "LEFT_ANGLE", "RIGHT_ANGLE", "VERTICAL_BAR", "CARET", "AMPERSAND",
  "QUESTION", "INVARIANT", "HIGH_PRECISION", "MEDIUM_PRECISION",
  "LOW_PRECISION", "PRECISION", "PACKED", "RESOURCE", "SUPERP",
  "FLOATCONSTANT", "INTCONSTANT", "UINTCONSTANT", "BOOLCONSTANT",
  "IDENTIFIER", "TYPE_NAME", "CENTROID", "IN", "OUT", "INOUT", "STRUCT",
  "VOID", "WHILE", "BREAK", "CONTINUE", "DO", "ELSE", "FOR", "IF",
  "DISCARD", "RETURN", "SWITCH", "CASE", "DEFAULT", "TERMINATE_INVOCATION",
  "TERMINATE_RAY", "IGNORE_INTERSECTION", "UNIFORM", "SHARED", "BUFFER",
  "TILEIMAGEEXT", "FLAT", "SMOOTH", "LAYOUT", "DOUBLECONSTANT",
  "INT16CONSTANT", "UINT16CONSTANT", "FLOAT16CONSTANT", "INT32CONSTANT",
  "UINT32CONSTANT", "INT64CONSTANT", "UINT64CONSTANT", "SUBROUTINE",
  "DEMOTE", "PAYLOADNV", "PAYLOADINNV", "HITATTRNV", "CALLDATANV",
  "CALLDATAINNV", "PAYLOADEXT", "PAYLOADINEXT", "HITATTREXT",
  "CALLDATAEXT", "CALLDATAINEXT", "PATCH", "SAMPLE", "NONUNIFORM",
  "COHERENT", "VOLATILE", "RESTRICT", "READONLY", "WRITEONLY",
  "DEVICECOHERENT", "QUEUEFAMILYCOHERENT", "WORKGROUPCOHERENT",
  "SUBGROUPCOHERENT", "NONPRIVATE", "SHADERCALLCOHERENT", "NOPERSPECTIVE",
  "EXPLICITINTERPAMD", "PERVERTEXEXT", "PERVERTEXNV", "PERPRIMITIVENV",
  "PERVIEWNV", "PERTASKNV", "PERPRIMITIVEEXT", "TASKPAYLOADWORKGROUPEXT",
  "PRECISE", "$accept", "variable_identifier", "primary_expression",
  "postfix_expression", "integer_expression", "function_call",
  "function_call_or_method", "function_call_generic",
  "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_xor_expression",
  "logical_or_expression", "conditional_expression", "$@1",
  "assignment_expression", "assignment_operator", "expression",
  "constant_expression", "declaration", "block_structure", "$@2",
  "identifier_list", "function_prototype", "function_declarator",
  "function_header_with_parameters", "function_header",
  "parameter_declarator", "parameter_declaration",
  "parameter_type_specifier", "init_declarator_list", "single_declaration",
  "fully_specified_type", "invariant_qualifier", "interpolation_qualifier",
  "layout_qualifier", "layout_qualifier_id_list", "layout_qualifier_id",
  "precise_qualifier", "type_qualifier", "single_type_qualifier",
  "storage_qualifier", "non_uniform_qualifier", "type_name_list",
  "type_specifier", "array_specifier", "type_parameter_specifier_opt",
  "type_parameter_specifier", "type_parameter_specifier_list",
  "type_specifier_nonarray", "precision_qualifier", "struct_specifier",
  "$@3", "$@4", "struct_declaration_list", "struct_declaration",
  "struct_declarator_list", "struct_declarator", "initializer",
  "initializer_list", "declaration_statement", "statement",
  "simple_statement", "demote_statement", "compound_statement", "$@5",
  "$@6", "statement_no_new_scope", "statement_scoped", "$@7", "$@8",
  "compound_statement_no_new_scope", "statement_list",
  "expression_statement", "selection_statement",
  "selection_statement_nonattributed", "selection_rest_statement",
  "condition", "switch_statement", "switch_statement_nonattributed", "$@9",
  "switch_statement_list", "case_label", "iteration_statement",
  "iteration_statement_nonattributed", "$@10", "$@11", "$@12",
  "for_init_statement", "conditionopt", "for_rest_statement",
  "jump_statement", "translation_unit", "external_declaration",
  "function_definition", "$@13", "attribute", "attribute_list",
  "single_attribute", "spirv_requirements_list",
  "spirv_requirements_parameter", "spirv_extension_list",
  "spirv_capability_list", "spirv_execution_mode_qualifier",
  "spirv_execution_mode_parameter_list", "spirv_execution_mode_parameter",
  "spirv_execution_mode_id_parameter_list",
  "spirv_storage_class_qualifier", "spirv_decorate_qualifier",
  "spirv_decorate_parameter_list", "spirv_decorate_parameter",
  "spirv_decorate_id_parameter_list", "spirv_decorate_id_parameter",
  "spirv_decorate_string_parameter_list", "spirv_type_specifier",
  "spirv_type_parameter_list", "spirv_type_parameter",
  "spirv_instruction_qualifier", "spirv_instruction_qualifier_list",
  "spirv_instruction_qualifier_id", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-872)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-695)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    4648,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -305,  -301,
    -289,  -276,  -246,  -238,  -227,  -182,  -872,  -872,  -872,  -872,
    -872,  -168,  -872,  -872,  -872,  -872,  -872,   -55,  -872,  -872,
    -872,  -872,  -872,  -319,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -135,  -120,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -327,  -114,
     -81,  -124,  7882,  -313,  -872,  -101,  -872,  -872,  -872,  -872,
    5572,  -872,  -872,  -872,  -872,   -94,  -872,  -872,   952,  -872,
    -872,  7882,   -73,  -872,  -872,  -872,  6034,   -78,  -252,  -250,
    -216,  -197,  -136,   -78,  -127,   -49, 12303,  -872,   -13,  -346,
     -39,  -872,  -309,  -872,   -10,    -9,  7882,  -872,  -872,  -872,
    7882,   -38,   -37,  -872,  -267,  -872,  -236,  -872,  -872, 10983,
      -2,  -872,  -872,  -872,     3,   -35,  7882,  -872,    -8,    -6,
      -1,  -872,  -256,  -872,  -255,    -4,     4,     7,     8,  -237,
      10,    11,    13,    14,    15,    18,  -232,     9,    19,    27,
    -188,  -872,    -3,  7882,  -872,    20,  -872,  -229,  -872,  -872,
    -219,  9223,  -872,  -272,  1414,  -872,  -872,  -872,  -872,  -872,
      -2,  -277,  -872,  9663,  -265,  -872,   -23,  -872,  -112, 10983,
   10983,  -872, 10983,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -253,  -872,  -872,  -872,    29,  -204, 11423,    28,
    -872, 10983,  -872,    31,  -321,    17,    -9,    32,  -872,  -325,
     -78,  -872,     5,  -872,  -330,    33,  -125, 10983,  -123,  -872,
    -130,  -119,  -146,  -118,    34,  -103,   -78,  -872, 11863,  -872,
     -74, 10983,    36,   -49,  -872,  7882,    24,  6496,  -872,  7882,
   10983,  -872,  -346,  -872,    30,  -872,  -872,   -33,  -133,  -105,
    -303,   -11,   -14,    21,    23,    48,    52,  -316,    41,  -872,
   10103,  -872,    42,  -872,  -872,    46,    38,    40,  -872,    64,
      67,    60, 10543,    74, 10983,    68,    65,    69,    70,    73,
    -167,  -872,  -872,   -47,  -872,  -114,    77,    31,  -872,  -872,
    -872,  -872,  -872,  1876,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  5110,    17,  9663,  -261,  8343,  -872,  -872,
    9663,  7882,  -872,    50,  -872,  -872,  -872,  -203,  -872,  -872,
   10983,    51,  -872,  -872, 10983,    87,  -872,  -872,  -872, 10983,
    -872,  -872,  -872,  -312,  -872,  -872,  -200,    80,  -872,  -872,
    -872,  -872,  -872,  -872,  -199,  -872,  -196,  -872,  -872,  -195,
      71,  -872,  -872,  -872,  -872,  -169,  -872,  -164,  -872,  -872,
    -872,  -872,  -872,  -161,  -872,    83,  -872,  -160,    84,  -153,
      80,  -872,  -278,  -152,  -872,    91,    94,  -872,  -872,    24,
      -2,   -43,  -872,  -872,  -872,  6958,  -872,  -872,  -872, 10983,
   10983, 10983, 10983, 10983, 10983, 10983, 10983, 10983, 10983, 10983,
   10983, 10983, 10983, 10983, 10983, 10983, 10983, 10983,  -872,  -872,
    -872,    93,  -872,  2338,  -872,  -872,  -872,  2338,  -872, 10983,
    -872,  -872,   -42, 10983,   -32,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872, 10983, 10983,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    9663,  -872,  -872,   -76,  -872,  7420,  -872,  -872,    96,    95,
    -872,  -872,  -872,  -872,  -872,  -132,  -131,  -872,  -311,  -872,
    -330,  -872,  -330,  -872, 10983, 10983,  -872,  -130,  -872,  -130,
    -872,  -146,  -146,  -872,   101,    34,  -872, 11863,  -872, 10983,
    -872,  -872,   -41,    17,    24,  -872,  -872,  -872,  -872,  -872,
     -33,   -33,  -133,  -133,  -105,  -105,  -105,  -105,  -303,  -303,
     -11,   -14,    21,    23,    48,    52, 10983,  -872,  2338,  4186,
      59,  3724,  -151,  -872,  -150,  -872,  -872,  -872,  -872,  -872,
    8783,  -872,  -872,  -872,   105,  -872,    72,  -872,  -149,  -872,
    -148,  -872,  -141,  -872,  -140,  -872,  -139,  -138,  -872,  -872,
    -872,   -28,   102,    95,    75,   107,   106,  -872,  -872,  4186,
     108,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872, 10983,  -872,   100,  2800, 10983,  -872,   104,   109,
      76,   112,  3262,  -872,   113,  -872,  9663,  -872,  -872,  -872,
    -137, 10983,  2800,   108,  -872,  -872,  2338,  -872,   110,    95,
    -872,  -872,  2338,   114,  -872,  -872
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       0,   168,   225,   223,   224,   222,   229,   230,   231,   232,
     233,   234,   235,   236,   237,   226,   227,   228,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     351,   352,   353,   354,   355,   356,   357,   377,   378,   379,
     380,   381,   382,   383,   392,   405,   406,   393,   394,   396,
     395,   397,   398,   399,   400,   401,   402,   403,   404,   177,
     178,   251,   252,   250,   253,   260,   261,   258,   259,   256,
     257,   254,   255,   283,   284,   285,   295,   296,   297,   280,
     281,   282,   292,   293,   294,   277,   278,   279,   289,   290,
     291,   274,   275,   276,   286,   287,   288,   262,   263,   264,
     298,   299,   300,   265,   266,   267,   310,   311,   312,   268,
     269,   270,   322,   323,   324,   271,   272,   273,   334,   335,
     336,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     313,   314,   315,   316,   317,   318,   319,   320,   321,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   337,   338,
     339,   340,   341,   342,   343,   344,   345,   349,   346,   347,
     348,   533,   534,   535,   536,   538,   182,   361,   362,   385,
     388,   350,   359,   360,   376,   358,   407,   408,   411,   412,
     413,   415,   416,   417,   419,   420,   421,   423,   424,   520,
     521,   384,   386,   387,   363,   364,   365,   409,   366,   370,
     371,   374,   414,   418,   422,   367,   368,   372,   373,   410,
     369,   375,   454,   456,   457,   458,   460,   461,   462,   464,
     465,   466,   468,   469,   470,   472,   473,   474,   476,   477,
     478,   480,   481,   482,   484,   485,   486,   488,   489,   490,
     492,   493,   494,   496,   497,   455,   459,   463,   467,   471,
     479,   483,   487,   475,   491,   495,   498,   499,   500,   501,
     502,   503,   504,   505,   506,   507,   508,   509,   510,   511,
     512,   513,   514,   515,   516,   517,   518,   519,   389,   390,
     391,   425,   434,   436,   430,   435,   437,   438,   440,   441,
     442,   444,   445,   446,   448,   449,   450,   452,   453,   426,
     427,   428,   439,   429,   431,   432,   433,   443,   447,   451,
     525,   526,   529,   530,   531,   532,   527,   528,     0,     0,
       0,     0,     0,     0,     0,     0,   166,   167,   522,   523,
     524,     0,   631,   137,   541,   542,   543,     0,   540,   172,
     170,   171,   169,     0,   221,   173,   175,   176,   174,   139,
     138,     0,   203,   184,   186,   181,   188,   190,   185,   187,
     183,   189,   191,   179,   180,   206,   192,   199,   200,   201,
     202,   193,   194,   195,   196,   197,   198,   140,   141,   143,
     142,   144,   146,   147,   145,   205,   154,   630,     0,   632,
       0,   114,   113,     0,   125,   130,   161,   160,   158,   162,
       0,   155,   157,   163,   135,   216,   159,   539,     0,   627,
     629,     0,     0,   164,   165,   537,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   546,     0,     0,
       0,    99,     0,    94,     0,   109,     0,   121,   115,   123,
       0,   124,     0,    97,   131,   102,     0,   156,   136,     0,
     209,   215,     1,   628,     0,     0,     0,    96,     0,     0,
       0,   639,     0,   697,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   637,
       0,   635,     0,     0,   544,   151,   153,     0,   149,   207,
       0,     0,   100,     0,     0,   633,   110,   116,   120,   122,
     118,   126,   117,     0,   132,   105,     0,   103,     0,     0,
       0,     9,     0,    43,    42,    44,    41,     5,     6,     7,
       8,     2,    16,    14,    15,    17,    10,    11,    12,    13,
       3,    18,    37,    20,    25,    26,     0,     0,    30,     0,
     219,     0,    36,   218,     0,   210,   111,     0,    95,     0,
       0,   695,     0,   647,     0,     0,     0,     0,     0,   664,
       0,     0,     0,     0,     0,     0,     0,   689,     0,   662,
       0,     0,     0,     0,    98,     0,     0,     0,   548,     0,
       0,   148,     0,   204,     0,   211,    45,    49,    52,    55,
      60,    63,    65,    67,    69,    71,    73,    75,     0,    34,
       0,   101,   575,   584,   588,     0,     0,     0,   609,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      45,    78,    91,     0,   562,     0,   163,   135,   565,   586,
     564,   572,   563,     0,   566,   567,   590,   568,   597,   569,
     570,   605,   571,     0,   119,     0,   127,     0,   556,   134,
       0,     0,   107,     0,   104,    38,    39,     0,    22,    23,
       0,     0,    28,    27,     0,   221,    31,    33,    40,     0,
     217,   112,   699,     0,   700,   640,     0,     0,   698,   659,
     655,   656,   657,   658,     0,   653,     0,    93,   660,     0,
       0,   674,   675,   676,   677,     0,   672,     0,   681,   682,
     683,   684,   680,     0,   678,     0,   685,     0,     0,     0,
       2,   693,   216,     0,   691,     0,     0,   634,   636,     0,
     554,     0,   552,   547,   549,     0,   152,   150,   208,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    76,   212,
     213,     0,   574,     0,   607,   620,   619,     0,   611,     0,
     623,   621,     0,     0,     0,   604,   624,   625,   626,   573,
      81,    82,    84,    83,    86,    87,    88,    89,    90,    85,
      80,     0,     0,   589,   585,   587,   591,   598,   606,   129,
       0,   559,   560,     0,   133,     0,   108,     4,     0,    24,
      21,    32,   220,   643,   645,     0,     0,   696,     0,   649,
       0,   648,     0,   651,     0,     0,   666,     0,   665,     0,
     668,     0,     0,   670,     0,     0,   690,     0,   687,     0,
     663,   638,     0,   555,     0,   550,   545,    46,    47,    48,
      51,    50,    53,    54,    58,    59,    56,    57,    61,    62,
      64,    66,    68,    70,    72,    74,     0,   214,   576,     0,
       0,     0,     0,   622,     0,   603,    79,    92,   128,   557,
       0,   106,    19,   641,     0,   642,     0,   654,     0,   661,
       0,   673,     0,   679,     0,   686,     0,     0,   692,   551,
     553,     0,     0,   595,     0,     0,     0,   614,   613,   616,
     582,   599,   558,   561,   644,   646,   650,   652,   667,   669,
     671,   688,     0,   577,     0,     0,     0,   615,     0,     0,
     594,     0,     0,   592,     0,    77,     0,   579,   608,   578,
       0,   617,     0,   582,   581,   583,   601,   596,     0,   618,
     612,   593,   602,     0,   610,   600
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -872,  -544,  -872,  -872,  -872,  -872,  -872,  -872,  -872,  -872,
    -872,  -872,  -436,  -872,  -392,  -391,  -490,  -390,  -269,  -266,
    -268,  -264,  -262,  -260,  -872,  -482,  -872,  -499,  -872,  -492,
    -534,     6,  -872,  -872,  -872,     1,  -403,  -872,  -872,    45,
      44,    49,  -872,  -872,  -406,  -872,  -872,  -872,  -872,  -104,
    -872,  -389,  -375,  -872,    12,  -872,     0,  -433,  -872,  -872,
    -872,  -553,   145,  -872,  -872,  -872,  -560,  -556,  -233,  -344,
    -614,  -872,  -373,  -626,  -871,  -872,  -430,  -872,  -872,  -440,
    -437,  -872,  -872,    63,  -737,  -363,  -872,  -144,  -872,  -399,
    -872,  -142,  -872,  -872,  -872,  -872,  -134,  -872,  -872,  -872,
    -872,  -872,  -872,  -872,  -872,    97,  -872,  -872,     2,  -872,
     -71,  -308,  -416,  -872,  -872,  -872,  -304,  -307,  -302,  -872,
    -872,  -315,  -310,  -306,  -300,  -314,  -872,  -299,  -317,  -872,
    -395,  -538
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,   530,   531,   532,   798,   533,   534,   535,   536,   537,
     538,   539,   620,   541,   587,   588,   589,   590,   591,   592,
     593,   594,   595,   596,   597,   621,   856,   622,   781,   623,
     711,   624,   388,   651,   508,   625,   390,   391,   392,   437,
     438,   439,   393,   394,   395,   396,   397,   398,   487,   488,
     399,   400,   401,   402,   542,   490,   599,   493,   450,   451,
     544,   405,   406,   407,   579,   483,   577,   578,   721,   722,
     649,   793,   628,   629,   630,   631,   632,   753,   892,   928,
     920,   921,   922,   929,   633,   634,   635,   636,   923,   895,
     637,   638,   924,   943,   639,   640,   641,   859,   757,   861,
     899,   918,   919,   642,   408,   409,   410,   434,   643,   480,
     481,   460,   461,   805,   806,   412,   684,   685,   689,   413,
     414,   695,   696,   703,   704,   707,   415,   713,   714,   416,
     462,   463
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     404,   389,   411,   440,   648,   455,   387,   785,   454,   598,
     455,   504,   403,   540,   678,   712,   858,   545,   702,   725,
     657,   724,   456,   688,   679,   447,   747,   456,   476,   672,
     678,   789,   673,   792,   736,   737,   794,   716,   431,   666,
     427,   669,   803,   672,   927,   485,   726,   440,   491,   442,
     417,   935,   443,   670,   418,   586,   492,   680,   681,   682,
     683,   927,   748,   674,   432,   447,   419,   644,   646,   486,
     738,   739,   428,   655,   656,   687,   804,   674,  -694,   420,
     491,   447,   658,   659,  -694,   600,   687,   645,   502,   687,
     491,   795,   600,   601,   575,   449,   600,   503,   687,   650,
     551,   553,   -35,   790,   660,   668,   552,   554,   661,   421,
     466,   468,   470,   472,   474,   475,   478,   422,   751,   559,
     762,   586,   764,   505,   567,   560,   506,   581,   423,   507,
     568,   860,   586,   582,   675,   586,   464,   583,   467,   465,
     675,   465,   675,   584,   586,   675,   648,   675,   648,   675,
     675,   648,   663,   797,   675,   676,   807,   809,   664,   782,
     811,   813,   552,   810,   586,   801,   812,   814,   799,   724,
     572,   709,   469,   424,   573,   465,   868,   770,   771,   772,
     773,   774,   775,   776,   777,   778,   779,   816,   575,   425,
     575,   471,   818,   817,   465,   820,   823,   780,   819,   942,
     447,   821,   824,   826,   828,   900,   901,   906,   907,   827,
     829,   782,   782,   810,   814,   908,   909,   910,   911,   938,
     429,   817,   821,   824,   829,   782,   873,   875,   734,   735,
     874,   876,   785,   802,   732,   430,   733,   455,   436,   724,
     454,   698,   699,   700,   701,   521,   844,   845,   846,   847,
     653,   433,   473,   654,   456,   465,   903,   691,   692,   693,
     694,   477,   575,   686,   465,   690,   465,   862,   465,   697,
     705,   864,   465,   465,   712,   435,   712,   702,   702,   449,
     879,   688,   866,   867,   869,   708,   870,   833,   465,   678,
     444,   648,   457,   837,   838,   839,   586,   586,   586,   586,
     586,   586,   586,   586,   586,   586,   586,   586,   586,   586,
     586,   586,   937,   459,   715,   782,   785,   465,   783,   834,
     782,   834,   835,   863,   889,   334,   335,   336,   740,   741,
     782,   865,   687,   687,   782,   912,   575,   729,   730,   731,
     840,   841,   479,   842,   843,   687,   484,   687,   331,   494,
     848,   849,   489,   500,   501,   491,   547,   548,   549,   546,
     555,   550,   574,   742,   891,   569,   556,   893,   652,   557,
     558,   648,   561,   562,   600,   563,   564,   565,   586,   586,
     566,   570,   571,   667,   580,   662,   -34,   502,   706,   745,
     673,   586,   441,   586,   717,   746,   677,   743,   744,   749,
     448,   754,   752,   755,   403,   756,   575,   893,   404,   389,
     411,   404,   403,   925,   387,   720,   404,   458,   411,   758,
     403,   728,   759,   403,   930,   760,   482,   648,   403,   763,
     766,   765,   -36,   815,   767,   768,   441,   496,   769,   939,
     441,   796,   800,   -29,   808,   822,   825,   830,   403,   543,
     831,   857,   403,   894,   872,   885,   448,   782,   896,   904,
     905,   916,   913,   915,   926,   932,   914,  -580,   403,   931,
     456,   602,   936,   850,   945,   944,   852,   851,   727,   933,
     497,   853,   426,   576,   854,   498,   832,   855,   897,   499,
     890,   934,   940,   894,   627,   403,   941,   495,   898,   786,
     917,   787,   718,   877,   882,   453,   626,   881,   878,   788,
     456,   886,   888,   880,     0,     0,   884,     0,     0,     0,
       0,   883,     0,     0,     0,     0,     0,     0,   887,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   671,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   719,     0,   576,     0,   576,
       0,     0,     0,     0,     0,     0,     0,   403,     0,   403,
       0,   403,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   627,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   404,     0,   626,     0,     0,     0,     0,
       0,   576,     0,     0,     0,   403,     0,     0,     0,     0,
       0,     0,     0,   403,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   576,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   403,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   627,     0,     0,     0,   627,     0,     0,
       0,     0,     0,     0,     0,   626,     0,     0,     0,   626,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   576,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   403,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   627,   627,
       0,   627,     0,   411,     0,     0,     0,     0,     0,     0,
     626,   626,     0,   626,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   627,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   626,     0,     0,     0,   627,     0,     0,     0,     0,
       0,     0,   627,     0,     0,     0,     0,   626,     0,     0,
       0,     0,   627,     0,   626,     0,   627,     0,     0,     0,
       0,     0,   627,     0,   626,     0,     0,     0,   626,     0,
       0,     0,   452,     0,   626,     1,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   331,
       0,     0,     0,     0,     0,     0,     0,   332,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   333,   334,   335,   336,   337,     0,     0,     0,     0,
       0,     0,     0,     0,   338,   339,   340,   341,   342,   343,
     344,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   345,   346,   347,   348,
     349,   350,   351,     0,     0,     0,     0,     0,     0,     0,
       0,   352,     0,   353,   354,   355,   356,   357,   358,   359,
     360,   361,   362,   363,   364,   365,   366,   367,   368,   369,
     370,   371,   372,   373,   374,   375,   376,   377,   378,   379,
     380,   381,   382,   383,   384,   385,   386,     1,     2,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   260,   261,   262,   263,
     264,   265,   266,   267,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,   329,   330,     0,     0,   509,
     510,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   511,   512,
       0,   331,     0,   602,   603,     0,     0,     0,     0,   604,
     513,   514,   515,   516,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   333,   334,   335,   336,   337,     0,     0,
       0,   517,   518,   519,   520,   521,   338,   339,   340,   341,
     342,   343,   344,   605,   606,   607,   608,     0,   609,   610,
     611,   612,   613,   614,   615,   616,   617,   618,   345,   346,
     347,   348,   349,   350,   351,   522,   523,   524,   525,   526,
     527,   528,   529,   352,   619,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,   363,   364,   365,   366,   367,
     368,   369,   370,   371,   372,   373,   374,   375,   376,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,     1,
       2,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,     0,
       0,   509,   510,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     511,   512,     0,   331,     0,   602,   784,     0,     0,     0,
       0,   604,   513,   514,   515,   516,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   333,   334,   335,   336,   337,
       0,     0,     0,   517,   518,   519,   520,   521,   338,   339,
     340,   341,   342,   343,   344,   605,   606,   607,   608,     0,
     609,   610,   611,   612,   613,   614,   615,   616,   617,   618,
     345,   346,   347,   348,   349,   350,   351,   522,   523,   524,
     525,   526,   527,   528,   529,   352,   619,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   367,   368,   369,   370,   371,   372,   373,   374,   375,
     376,   377,   378,   379,   380,   381,   382,   383,   384,   385,
     386,     1,     2,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
     270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,     0,     0,   509,   510,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   511,   512,     0,   331,     0,   602,     0,     0,
       0,     0,     0,   604,   513,   514,   515,   516,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   333,   334,   335,
     336,   337,     0,     0,     0,   517,   518,   519,   520,   521,
     338,   339,   340,   341,   342,   343,   344,   605,   606,   607,
     608,     0,   609,   610,   611,   612,   613,   614,   615,   616,
     617,   618,   345,   346,   347,   348,   349,   350,   351,   522,
     523,   524,   525,   526,   527,   528,   529,   352,   619,   353,
     354,   355,   356,   357,   358,   359,   360,   361,   362,   363,
     364,   365,   366,   367,   368,   369,   370,   371,   372,   373,
     374,   375,   376,   377,   378,   379,   380,   381,   382,   383,
     384,   385,   386,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,     0,     0,   509,   510,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   511,   512,     0,   331,     0,   494,
       0,     0,     0,     0,     0,   604,   513,   514,   515,   516,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   333,
     334,   335,   336,   337,     0,     0,     0,   517,   518,   519,
     520,   521,   338,   339,   340,   341,   342,   343,   344,   605,
     606,   607,   608,     0,   609,   610,   611,   612,   613,   614,
     615,   616,   617,   618,   345,   346,   347,   348,   349,   350,
     351,   522,   523,   524,   525,   526,   527,   528,   529,   352,
     619,   353,   354,   355,   356,   357,   358,   359,   360,   361,
     362,   363,   364,   365,   366,   367,   368,   369,   370,   371,
     372,   373,   374,   375,   376,   377,   378,   379,   380,   381,
     382,   383,   384,   385,   386,     1,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,     0,     0,   509,   510,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   511,   512,     0,   331,
       0,     0,     0,     0,     0,     0,     0,   604,   513,   514,
     515,   516,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   333,   334,   335,   336,   337,     0,     0,     0,   517,
     518,   519,   520,   521,   338,   339,   340,   341,   342,   343,
     344,   605,   606,   607,   608,     0,   609,   610,   611,   612,
     613,   614,   615,   616,   617,   618,   345,   346,   347,   348,
     349,   350,   351,   522,   523,   524,   525,   526,   527,   528,
     529,   352,   619,   353,   354,   355,   356,   357,   358,   359,
     360,   361,   362,   363,   364,   365,   366,   367,   368,   369,
     370,   371,   372,   373,   374,   375,   376,   377,   378,   379,
     380,   381,   382,   383,   384,   385,   386,     1,     2,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   260,   261,   262,   263,
     264,   265,   266,   267,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,   329,   330,     0,     0,   509,
     510,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   511,   512,
       0,   331,     0,     0,     0,     0,     0,     0,     0,   604,
     513,   514,   515,   516,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   333,   334,   335,   336,   337,     0,     0,
       0,   517,   518,   519,   520,   521,   338,   339,   340,   341,
     342,   343,   344,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   345,   346,
     347,   348,   349,   350,   351,   522,   523,   524,   525,   526,
     527,   528,   529,   352,     0,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,   363,   364,   365,   366,   367,
     368,   369,   370,   371,   372,   373,   374,   375,   376,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,     1,
       2,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,     0,     0,     0,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,     0,
       0,   509,   510,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     511,   512,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   513,   514,   515,   516,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   333,   334,   335,   336,     0,
       0,     0,     0,   517,   518,   519,   520,   521,   338,   339,
     340,   341,   342,   343,   344,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     345,   346,   347,   348,   349,   350,   351,   522,   523,   524,
     525,   526,   527,   528,   529,   352,     0,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   367,   368,   369,   370,   371,   372,   373,   374,   375,
     376,   377,   378,   379,   380,   381,   382,   383,   384,   385,
     386,     1,     2,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
     270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   331,     0,     0,     0,     0,
       0,     0,     0,   332,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   333,   334,   335,
     336,   337,     0,     0,     0,     0,     0,     0,     0,     0,
     338,   339,   340,   341,   342,   343,   344,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   345,   346,   347,   348,   349,   350,   351,     0,
       0,     0,     0,     0,     0,     0,     0,   352,     0,   353,
     354,   355,   356,   357,   358,   359,   360,   361,   362,   363,
     364,   365,   366,   367,   368,   369,   370,   371,   372,   373,
     374,   375,   376,   377,   378,   379,   380,   381,   382,   383,
     384,   385,   386,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
       0,     0,     0,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   333,
     334,   335,   336,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   338,   339,   340,   341,   342,   343,   344,   605,
       0,     0,   608,     0,   609,   610,     0,     0,   613,     0,
       0,     0,     0,     0,   345,   346,   347,   348,   349,   350,
     351,     0,     0,     0,     0,     0,     0,     0,     0,   352,
       0,   353,   354,   355,   356,   357,   358,   359,   360,   361,
     362,   363,   364,   365,   366,   367,   368,   369,   370,   371,
     372,   373,   374,   375,   376,   377,   378,   379,   380,   381,
     382,   383,   384,   385,   386,     1,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,     0,     0,     0,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   445,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   333,   334,   335,   336,     0,     0,     0,     0,     0,
       0,     0,     0,   446,   338,   339,   340,   341,   342,   343,
     344,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   345,   346,   347,   348,
     349,   350,   351,     0,     0,     0,     0,     0,     0,     0,
       0,   352,     0,   353,   354,   355,   356,   357,   358,   359,
     360,   361,   362,   363,   364,   365,   366,   367,   368,   369,
     370,   371,   372,   373,   374,   375,   376,   377,   378,   379,
     380,   381,   382,   383,   384,   385,   386,     1,     2,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   260,   261,   262,   263,
     264,   265,   266,   267,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   315,   316,   317,     0,     0,     0,   321,   322,   323,
     324,   325,   326,   327,   328,   329,   330,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   331,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   333,   334,   335,   336,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   338,   339,   340,   341,
     342,   343,   344,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   345,   346,
     347,   348,   349,   350,   351,     0,     0,     0,     0,     0,
       0,     0,     0,   352,     0,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,   363,   364,   365,   366,   367,
     368,   369,   370,   371,   372,   373,   374,   375,   376,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,     1,
       2,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,     0,     0,     0,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   723,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   333,   334,   335,   336,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   338,   339,
     340,   341,   342,   343,   344,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     345,   346,   347,   348,   349,   350,   351,     0,     0,     0,
       0,     0,     0,     0,     0,   352,     0,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   367,   368,   369,   370,   371,   372,   373,   374,   375,
     376,   377,   378,   379,   380,   381,   382,   383,   384,   385,
     386,     1,     2,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
     270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     310,   311,   312,   313,   314,   315,   316,   317,     0,     0,
       0,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   836,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   333,   334,   335,
     336,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     338,   339,   340,   341,   342,   343,   344,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   345,   346,   347,   348,   349,   350,   351,     0,
       0,     0,     0,     0,     0,     0,     0,   352,     0,   353,
     354,   355,   356,   357,   358,   359,   360,   361,   362,   363,
     364,   365,   366,   367,   368,   369,   370,   371,   372,   373,
     374,   375,   376,   377,   378,   379,   380,   381,   382,   383,
     384,   385,   386,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
       0,     0,     0,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     871,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   333,
     334,   335,   336,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   338,   339,   340,   341,   342,   343,   344,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   345,   346,   347,   348,   349,   350,
     351,     0,     0,     0,     0,     0,     0,     0,     0,   352,
       0,   353,   354,   355,   356,   357,   358,   359,   360,   361,
     362,   363,   364,   365,   366,   367,   368,   369,   370,   371,
     372,   373,   374,   375,   376,   377,   378,   379,   380,   381,
     382,   383,   384,   385,   386,     1,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,     0,     0,     0,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   333,   334,   335,   336,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   338,   339,   340,   341,   342,   343,
     344,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   345,   346,   347,   348,
     349,   350,   351,     0,     0,     0,     0,     0,     0,     0,
       0,   352,     0,   353,   354,   355,   356,   357,   358,   359,
     360,   361,   362,   363,   364,   365,   366,   367,   368,   369,
     370,   371,   372,   373,   374,   375,   376,   377,   378,   379,
     380,   381,   382,   383,   384,   385,   386,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,   647,   791,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,   647,   902,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,   585,     0,     0,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,   647,     0,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,   750,     0,     0,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   761,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   521,   338,     0,     0,     0,     0,
     343,   665,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,   509,   510,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   511,   512,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   513,
     514,   515,   516,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     517,   518,   519,   520,   710,   338,     0,     0,     0,     0,
     343,   344,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   522,   523,   524,   525,   526,   527,
     528,   529,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   365,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,     0,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,     0,     0,     0,     0,     0,     0,   324,
       0,     0,     0,   328,   329,   330,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   338,     0,     0,     0,     0,
     343,   344
};

static const yytype_int16 yycheck[] =
{
       0,     0,     0,   392,   503,   411,     0,   633,   411,   491,
     416,   444,     0,   449,   552,   568,   753,   450,   562,   579,
     512,   577,   411,   557,   354,   400,   342,   416,   423,   354,
     568,   645,   357,   647,   337,   338,   650,   571,   365,   538,
     359,   362,   354,   354,   915,   391,   580,   436,   357,   362,
     355,   922,   365,   374,   355,   491,   365,   387,   388,   389,
     390,   932,   378,   388,   391,   440,   355,   500,   501,   415,
     373,   374,   391,   509,   510,   557,   388,   388,   356,   355,
     357,   456,   335,   336,   362,   357,   568,   364,   355,   571,
     357,   651,   357,   365,   483,   373,   357,   364,   580,   364,
     356,   356,   355,   364,   357,   541,   362,   362,   361,   355,
     418,   419,   420,   421,   422,   423,   424,   355,   600,   356,
     612,   557,   614,   359,   356,   362,   362,   356,   355,   365,
     362,   757,   568,   362,   550,   571,   388,   356,   388,   391,
     556,   391,   558,   362,   580,   561,   645,   563,   647,   565,
     566,   650,   356,   356,   570,   550,   356,   356,   362,   362,
     356,   356,   362,   362,   600,   664,   362,   362,   660,   725,
     358,   566,   388,   355,   362,   391,   790,   344,   345,   346,
     347,   348,   349,   350,   351,   352,   353,   356,   577,   357,
     579,   388,   356,   362,   391,   356,   356,   364,   362,   936,
     575,   362,   362,   356,   356,   356,   356,   356,   356,   362,
     362,   362,   362,   362,   362,   356,   356,   356,   356,   356,
     355,   362,   362,   362,   362,   362,   358,   358,   333,   334,
     362,   362,   858,   669,   367,   355,   369,   643,   362,   795,
     643,   387,   388,   389,   390,   391,   736,   737,   738,   739,
     362,   365,   388,   365,   643,   391,   870,   387,   388,   389,
     390,   388,   651,   388,   391,   388,   391,   759,   391,   388,
     388,   763,   391,   391,   827,   356,   829,   821,   822,   373,
     814,   815,   781,   782,   360,   388,   362,   720,   391,   827,
     391,   790,   365,   729,   730,   731,   732,   733,   734,   735,
     736,   737,   738,   739,   740,   741,   742,   743,   744,   745,
     746,   747,   926,   391,   388,   362,   942,   391,   365,   362,
     362,   362,   365,   365,   365,   380,   381,   382,   339,   340,
     362,   363,   814,   815,   362,   363,   725,   370,   371,   372,
     732,   733,   391,   734,   735,   827,   359,   829,   357,   359,
     740,   741,   391,   391,   391,   357,   391,   365,   364,   356,
     364,   362,   365,   377,   856,   356,   362,   859,   391,   362,
     362,   870,   362,   362,   357,   362,   362,   362,   814,   815,
     362,   362,   355,   355,   364,   356,   355,   355,   354,   341,
     357,   827,   392,   829,   358,   343,   391,   376,   375,   358,
     400,   355,   360,   365,   392,   365,   795,   899,   408,   408,
     408,   411,   400,   912,   408,   391,   416,   416,   416,   355,
     408,   391,   355,   411,   916,   365,   426,   926,   416,   355,
     365,   363,   355,   362,   365,   365,   436,   435,   365,   931,
     440,   391,   391,   356,   364,   362,   362,   356,   436,   449,
     356,   358,   440,   859,   358,   354,   456,   362,   399,   354,
     388,   355,   360,   356,   364,   356,   391,   359,   456,   365,
     859,   359,   359,   742,   360,   365,   744,   743,   582,   403,
     436,   745,   337,   483,   746,   440,   719,   747,   861,   440,
     834,   921,   932,   899,   494,   483,   933,   434,   861,   643,
     899,   643,   573,   810,   819,   408,   494,   817,   812,   643,
     899,   825,   829,   815,    -1,    -1,   822,    -1,    -1,    -1,
      -1,   821,    -1,    -1,    -1,    -1,    -1,    -1,   827,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   546,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   575,    -1,   577,    -1,   579,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   575,    -1,   577,
      -1,   579,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   633,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   643,    -1,   633,    -1,    -1,    -1,    -1,
      -1,   651,    -1,    -1,    -1,   643,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   651,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   725,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   725,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   753,    -1,    -1,    -1,   757,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   753,    -1,    -1,    -1,   757,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   795,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   795,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   858,   859,
      -1,   861,    -1,   861,    -1,    -1,    -1,    -1,    -1,    -1,
     858,   859,    -1,   861,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   899,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   899,    -1,    -1,    -1,   915,    -1,    -1,    -1,    -1,
      -1,    -1,   922,    -1,    -1,    -1,    -1,   915,    -1,    -1,
      -1,    -1,   932,    -1,   922,    -1,   936,    -1,    -1,    -1,
      -1,    -1,   942,    -1,   932,    -1,    -1,    -1,   936,    -1,
      -1,    -1,     0,    -1,   942,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   357,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   365,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   379,   380,   381,   382,   383,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   392,   393,   394,   395,   396,   397,
     398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   414,   415,   416,   417,
     418,   419,   420,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   429,    -1,   431,   432,   433,   434,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,   447,
     448,   449,   450,   451,   452,   453,   454,   455,   456,   457,
     458,   459,   460,   461,   462,   463,   464,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,    -1,    -1,   335,
     336,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,
      -1,   357,    -1,   359,   360,    -1,    -1,    -1,    -1,   365,
     366,   367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   379,   380,   381,   382,   383,    -1,    -1,
      -1,   387,   388,   389,   390,   391,   392,   393,   394,   395,
     396,   397,   398,   399,   400,   401,   402,    -1,   404,   405,
     406,   407,   408,   409,   410,   411,   412,   413,   414,   415,
     416,   417,   418,   419,   420,   421,   422,   423,   424,   425,
     426,   427,   428,   429,   430,   431,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     456,   457,   458,   459,   460,   461,   462,   463,   464,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   260,   261,   262,   263,
     264,   265,   266,   267,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   315,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,   329,   330,   331,   332,    -1,
      -1,   335,   336,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     354,   355,    -1,   357,    -1,   359,   360,    -1,    -1,    -1,
      -1,   365,   366,   367,   368,   369,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   379,   380,   381,   382,   383,
      -1,    -1,    -1,   387,   388,   389,   390,   391,   392,   393,
     394,   395,   396,   397,   398,   399,   400,   401,   402,    -1,
     404,   405,   406,   407,   408,   409,   410,   411,   412,   413,
     414,   415,   416,   417,   418,   419,   420,   421,   422,   423,
     424,   425,   426,   427,   428,   429,   430,   431,   432,   433,
     434,   435,   436,   437,   438,   439,   440,   441,   442,   443,
     444,   445,   446,   447,   448,   449,   450,   451,   452,   453,
     454,   455,   456,   457,   458,   459,   460,   461,   462,   463,
     464,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,   331,
     332,    -1,    -1,   335,   336,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   354,   355,    -1,   357,    -1,   359,    -1,    -1,
      -1,    -1,    -1,   365,   366,   367,   368,   369,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,   380,   381,
     382,   383,    -1,    -1,    -1,   387,   388,   389,   390,   391,
     392,   393,   394,   395,   396,   397,   398,   399,   400,   401,
     402,    -1,   404,   405,   406,   407,   408,   409,   410,   411,
     412,   413,   414,   415,   416,   417,   418,   419,   420,   421,
     422,   423,   424,   425,   426,   427,   428,   429,   430,   431,
     432,   433,   434,   435,   436,   437,   438,   439,   440,   441,
     442,   443,   444,   445,   446,   447,   448,   449,   450,   451,
     452,   453,   454,   455,   456,   457,   458,   459,   460,   461,
     462,   463,   464,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
     270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,   331,   332,    -1,    -1,   335,   336,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   354,   355,    -1,   357,    -1,   359,
      -1,    -1,    -1,    -1,    -1,   365,   366,   367,   368,   369,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,
     380,   381,   382,   383,    -1,    -1,    -1,   387,   388,   389,
     390,   391,   392,   393,   394,   395,   396,   397,   398,   399,
     400,   401,   402,    -1,   404,   405,   406,   407,   408,   409,
     410,   411,   412,   413,   414,   415,   416,   417,   418,   419,
     420,   421,   422,   423,   424,   425,   426,   427,   428,   429,
     430,   431,   432,   433,   434,   435,   436,   437,   438,   439,
     440,   441,   442,   443,   444,   445,   446,   447,   448,   449,
     450,   451,   452,   453,   454,   455,   456,   457,   458,   459,
     460,   461,   462,   463,   464,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,    -1,    -1,   335,   336,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   354,   355,    -1,   357,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   365,   366,   367,
     368,   369,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   379,   380,   381,   382,   383,    -1,    -1,    -1,   387,
     388,   389,   390,   391,   392,   393,   394,   395,   396,   397,
     398,   399,   400,   401,   402,    -1,   404,   405,   406,   407,
     408,   409,   410,   411,   412,   413,   414,   415,   416,   417,
     418,   419,   420,   421,   422,   423,   424,   425,   426,   427,
     428,   429,   430,   431,   432,   433,   434,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,   447,
     448,   449,   450,   451,   452,   453,   454,   455,   456,   457,
     458,   459,   460,   461,   462,   463,   464,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,    -1,    -1,   335,
     336,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,
      -1,   357,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   365,
     366,   367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   379,   380,   381,   382,   383,    -1,    -1,
      -1,   387,   388,   389,   390,   391,   392,   393,   394,   395,
     396,   397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   414,   415,
     416,   417,   418,   419,   420,   421,   422,   423,   424,   425,
     426,   427,   428,   429,    -1,   431,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     456,   457,   458,   459,   460,   461,   462,   463,   464,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   260,   261,   262,   263,
     264,   265,   266,   267,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   315,   316,   317,   318,   319,    -1,    -1,    -1,   323,
     324,   325,   326,   327,   328,   329,   330,   331,   332,    -1,
      -1,   335,   336,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     354,   355,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   366,   367,   368,   369,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   379,   380,   381,   382,    -1,
      -1,    -1,    -1,   387,   388,   389,   390,   391,   392,   393,
     394,   395,   396,   397,   398,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     414,   415,   416,   417,   418,   419,   420,   421,   422,   423,
     424,   425,   426,   427,   428,   429,    -1,   431,   432,   433,
     434,   435,   436,   437,   438,   439,   440,   441,   442,   443,
     444,   445,   446,   447,   448,   449,   450,   451,   452,   453,
     454,   455,   456,   457,   458,   459,   460,   461,   462,   463,
     464,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,   331,
     332,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   357,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   365,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,   380,   381,
     382,   383,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     392,   393,   394,   395,   396,   397,   398,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   414,   415,   416,   417,   418,   419,   420,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   429,    -1,   431,
     432,   433,   434,   435,   436,   437,   438,   439,   440,   441,
     442,   443,   444,   445,   446,   447,   448,   449,   450,   451,
     452,   453,   454,   455,   456,   457,   458,   459,   460,   461,
     462,   463,   464,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
     270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
      -1,    -1,    -1,   323,   324,   325,   326,   327,   328,   329,
     330,   331,   332,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,
     380,   381,   382,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   392,   393,   394,   395,   396,   397,   398,   399,
      -1,    -1,   402,    -1,   404,   405,    -1,    -1,   408,    -1,
      -1,    -1,    -1,    -1,   414,   415,   416,   417,   418,   419,
     420,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   429,
      -1,   431,   432,   433,   434,   435,   436,   437,   438,   439,
     440,   441,   442,   443,   444,   445,   446,   447,   448,   449,
     450,   451,   452,   453,   454,   455,   456,   457,   458,   459,
     460,   461,   462,   463,   464,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,    -1,    -1,    -1,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   365,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   379,   380,   381,   382,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   391,   392,   393,   394,   395,   396,   397,
     398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   414,   415,   416,   417,
     418,   419,   420,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   429,    -1,   431,   432,   433,   434,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,   447,
     448,   449,   450,   451,   452,   453,   454,   455,   456,   457,
     458,   459,   460,   461,   462,   463,   464,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,    -1,    -1,    -1,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   357,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   379,   380,   381,   382,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   392,   393,   394,   395,
     396,   397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   414,   415,
     416,   417,   418,   419,   420,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   429,    -1,   431,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     456,   457,   458,   459,   460,   461,   462,   463,   464,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   260,   261,   262,   263,
     264,   265,   266,   267,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   315,   316,   317,   318,   319,    -1,    -1,    -1,   323,
     324,   325,   326,   327,   328,   329,   330,   331,   332,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   360,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   379,   380,   381,   382,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   392,   393,
     394,   395,   396,   397,   398,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     414,   415,   416,   417,   418,   419,   420,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   429,    -1,   431,   432,   433,
     434,   435,   436,   437,   438,   439,   440,   441,   442,   443,
     444,   445,   446,   447,   448,   449,   450,   451,   452,   453,
     454,   455,   456,   457,   458,   459,   460,   461,   462,   463,
     464,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,   318,   319,    -1,    -1,
      -1,   323,   324,   325,   326,   327,   328,   329,   330,   331,
     332,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   360,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,   380,   381,
     382,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     392,   393,   394,   395,   396,   397,   398,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   414,   415,   416,   417,   418,   419,   420,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   429,    -1,   431,
     432,   433,   434,   435,   436,   437,   438,   439,   440,   441,
     442,   443,   444,   445,   446,   447,   448,   449,   450,   451,
     452,   453,   454,   455,   456,   457,   458,   459,   460,   461,
     462,   463,   464,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
     270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
     310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
      -1,    -1,    -1,   323,   324,   325,   326,   327,   328,   329,
     330,   331,   332,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     360,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   379,
     380,   381,   382,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   392,   393,   394,   395,   396,   397,   398,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   414,   415,   416,   417,   418,   419,
     420,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   429,
      -1,   431,   432,   433,   434,   435,   436,   437,   438,   439,
     440,   441,   442,   443,   444,   445,   446,   447,   448,   449,
     450,   451,   452,   453,   454,   455,   456,   457,   458,   459,
     460,   461,   462,   463,   464,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,    -1,    -1,    -1,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   379,   380,   381,   382,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   392,   393,   394,   395,   396,   397,
     398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   414,   415,   416,   417,
     418,   419,   420,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   429,    -1,   431,   432,   433,   434,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,   447,
     448,   449,   450,   451,   452,   453,   454,   455,   456,   457,
     458,   459,   460,   461,   462,   463,   464,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,    -1,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   233,   234,   235,   236,
     237,   238,   239,   240,   241,   242,   243,   244,   245,   246,
     247,   248,   249,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   271,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   312,   313,   314,   315,   316,
     317,   318,   319,    -1,    -1,    -1,    -1,    -1,    -1,   326,
      -1,    -1,    -1,   330,   331,   332,    -1,    -1,   335,   336,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,    -1,
      -1,    -1,   359,   360,    -1,    -1,    -1,    -1,    -1,   366,
     367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     387,   388,   389,   390,   391,   392,    -1,    -1,    -1,    -1,
     397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   421,   422,   423,   424,   425,   426,
     427,   428,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   443,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,    -1,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   233,   234,   235,   236,
     237,   238,   239,   240,   241,   242,   243,   244,   245,   246,
     247,   248,   249,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   271,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   312,   313,   314,   315,   316,
     317,   318,   319,    -1,    -1,    -1,    -1,    -1,    -1,   326,
      -1,    -1,    -1,   330,   331,   332,    -1,    -1,   335,   336,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,    -1,
      -1,    -1,   359,   360,    -1,    -1,    -1,    -1,    -1,   366,
     367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     387,   388,   389,   390,   391,   392,    -1,    -1,    -1,    -1,
     397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   421,   422,   423,   424,   425,   426,
     427,   428,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   443,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,    -1,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   233,   234,   235,   236,
     237,   238,   239,   240,   241,   242,   243,   244,   245,   246,
     247,   248,   249,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   271,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   312,   313,   314,   315,   316,
     317,   318,   319,    -1,    -1,    -1,    -1,    -1,    -1,   326,
      -1,    -1,    -1,   330,   331,   332,    -1,    -1,   335,   336,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,    -1,
      -1,   358,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   366,
     367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     387,   388,   389,   390,   391,   392,    -1,    -1,    -1,    -1,
     397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   421,   422,   423,   424,   425,   426,
     427,   428,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   443,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,    -1,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   233,   234,   235,   236,
     237,   238,   239,   240,   241,   242,   243,   244,   245,   246,
     247,   248,   249,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   271,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   312,   313,   314,   315,   316,
     317,   318,   319,    -1,    -1,    -1,    -1,    -1,    -1,   326,
      -1,    -1,    -1,   330,   331,   332,    -1,    -1,   335,   336,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,    -1,
      -1,    -1,   359,    -1,    -1,    -1,    -1,    -1,    -1,   366,
     367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     387,   388,   389,   390,   391,   392,    -1,    -1,    -1,    -1,
     397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   421,   422,   423,   424,   425,   426,
     427,   428,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   443,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,    -1,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   233,   234,   235,   236,
     237,   238,   239,   240,   241,   242,   243,   244,   245,   246,
     247,   248,   249,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   271,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   312,   313,   314,   315,   316,
     317,   318,   319,    -1,    -1,    -1,    -1,    -1,    -1,   326,
      -1,    -1,    -1,   330,   331,   332,    -1,    -1,   335,   336,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   354,   355,    -1,
      -1,   358,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   366,
     367,   368,   369,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     387,   388,   389,   390,   391,   392,    -1,    -1,    -1,    -1,
     397,   398,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   421,   422,   423,   424,   425,   426,
     427,   428,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   443,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    