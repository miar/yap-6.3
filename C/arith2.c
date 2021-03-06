/*************************************************************************
 *									 *
 *	 YAP Prolog 							 *
 *									 *
 *	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
 *									 *
 * Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-1997	 *
 *									 *
 **************************************************************************
 *									 *
 * File:		arith2.c						 *
 * Last rev:								 *
 * mods:									 *
 * comments:	arithmetical expression evaluation			 *
 *									 *
 *************************************************************************/
#ifdef SCCS
static char     SccsId[] = "%W% %G%";
#endif

/*
 * This file implements unary arithmetic operations in YAP
 *
 */

#include "Yap.h"
#include "Yatom.h"
#include "YapHeap.h"
#include "eval.h"

#include "arith2.h"

typedef struct init_un_eval {
  char          *OpName;
  arith2_op      f;
} InitBinEntry;


static Term
p_mod(Term t1, Term t2) {
  switch (ETypeOfTerm(t1)) {
  case (CELL)long_int_e:
    switch (ETypeOfTerm(t2)) {
    case (CELL)long_int_e:
      /* two integers */
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);
	Int mod;

	if (i2 == 0)
	  return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is " Int_FORMAT " mod 0", i1);
	if (i1 == Int_MIN && i2 == -1) {
#ifdef USE_GMP
	  return Yap_gmp_add_ints(Int_MAX, 1);	  
#else
	  return Yap_ArithError(EVALUATION_ERROR_INT_OVERFLOW, t1,
		    "// /2 with %d and %d", i1, i2);
#endif
	}
	mod = i1%i2;
	if (mod && (mod ^ i2) < 0)
	  mod += i2;
	RINT(mod);
      }
    case (CELL)double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
    case (CELL)big_int_e:
#ifdef USE_GMP
      return Yap_gmp_mod_int_big(IntegerOfTerm(t1), t2);
#endif
    default:
      RERROR();
      break;
    }
  case (CELL)double_e:
    return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
  case (CELL)big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* modulo between bignum and integer */
      {
	Int i2 = IntegerOfTerm(t2);

	if (i2 == 0)
	  return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is ... mod 0");
	return Yap_gmp_mod_big_int(t1, i2);
      }
    case (CELL)big_int_e:
      /* two bignums */
      return Yap_gmp_mod_big_big(t1, t2);
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
}

static Term
p_div2(Term t1, Term t2) {
  switch (ETypeOfTerm(t1)) {
  case (CELL)long_int_e:
    switch (ETypeOfTerm(t2)) {
    case (CELL)long_int_e:
      /* two integers */
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);
	Int res, mod;

	if (i2 == 0)
	  return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is " Int_FORMAT " div 0", i1);
	if (i1 == Int_MIN && i2 == -1) {
#ifdef USE_GMP
	  return Yap_gmp_add_ints(Int_MAX, 1);	  
#else
	  return Yap_ArithError(EVALUATION_ERROR_INT_OVERFLOW, t1,
		    "// /2 with %d and %d", i1, i2);
#endif
	}
	mod = i1%i2;
	if (mod && (mod ^ i2) < 0)
	  mod += i2;
	res = (i1 - mod) / i2;
	RINT(res);
      }
    case (CELL)double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "div/2");
    case (CELL)big_int_e:
#ifdef USE_GMP
      return Yap_gmp_div_int_big(IntegerOfTerm(t1), t2);
#endif
    default:
      RERROR();
      break;
    }
  case (CELL)double_e:
    return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "div/2");
  case (CELL)big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* modulo between bignum and integer */
      {
	Int i2 = IntegerOfTerm(t2);

	if (i2 == 0)
	  return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is ... div 0");
	return Yap_gmp_div2_big_int(t1, i2);
      }
    case (CELL)big_int_e:
      /* two bignums */
      return Yap_gmp_div2_big_big(t1, t2);
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
}

static Term
p_rem(Term t1, Term t2) {
  switch (ETypeOfTerm(t1)) {
  case (CELL)long_int_e:
    switch (ETypeOfTerm(t2)) {
    case (CELL)long_int_e:
      /* two integers */
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);

	if (i2 == 0)
	  return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is " Int_FORMAT " rem 0", i1);
	if (i1 == Int_MIN && i2 == -1) {
#ifdef USE_GMP
	  return Yap_gmp_add_ints(Int_MAX, 1);	  
#else
	  return Yap_ArithError(EVALUATION_ERROR_INT_OVERFLOW, t1,
		    "rem/2 with %d and %d", i1, i2);
#endif
	}
	RINT(i1%i2);
      }
    case (CELL)double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
    case (CELL)big_int_e:
#ifdef USE_GMP
      return Yap_gmp_rem_int_big(IntegerOfTerm(t1), t2);
#endif
    default:
      RERROR();
    }
    break;
  case (CELL)double_e:
    return Yap_ArithError(TYPE_ERROR_INTEGER, t1, "mod/2");
  case (CELL)big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      if (IntegerOfTerm(t2) == 0)
	return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is ... rem 0");
      return Yap_gmp_rem_big_int(t1, IntegerOfTerm(t2));
    case (CELL)big_int_e:
      /* two bignums */
      return Yap_gmp_rem_big_big(t1, t2);
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
}


static Term
p_rdiv(Term t1, Term t2) {
#ifdef USE_GMP
  switch (ETypeOfTerm(t1)) {
  case (CELL)double_e:
    return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "rdiv/2");
  case (CELL)long_int_e:
    switch (ETypeOfTerm(t2)) {
    case (CELL)long_int_e:
      /* two integers */
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);

	if (i2 == 0)
	  return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is " Int_FORMAT " rdiv 0", i1);
	return Yap_gmq_rdiv_int_int(i1, i2);
      }
    case (CELL)big_int_e:
      /* I know the term is much larger, so: */
      return Yap_gmq_rdiv_int_big(IntegerOfTerm(t1), t2);
    default:
      RERROR();
    }
    break;
  case (CELL)big_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      if (IntegerOfTerm(t2) == 0)
	return Yap_ArithError(EVALUATION_ERROR_ZERO_DIVISOR, t2, "X is ... rdiv  0");
      /* I know the term is much larger, so: */
      return Yap_gmq_rdiv_big_int(t1, IntegerOfTerm(t2));
    case (CELL)big_int_e:
      return Yap_gmq_rdiv_big_big(t1, t2);
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "mod/2");
    default:
      RERROR();
    }
  default:
    RERROR();
  }
#else
  RERROR();
#endif
}


/*
  Floating point division: /
*/
static Term
p_fdiv(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i2 = IntegerOfTerm(t2);

	/* two integers */
	RFLOAT((((Float)IntegerOfTerm(t1))/(Float)i2));
      }
    case double_e:
      {
	/* integer, double */
	Float fl1 = (Float)IntegerOfTerm(t1);
	Float fl2 = FloatOfTerm(t2);
	RFLOAT(fl1/fl2);
      }
    case (CELL)big_int_e:
#ifdef USE_GMP
      return Yap_gmp_fdiv_int_big(IntegerOfTerm(t1), t2);
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* float / integer */
      {
	Int i2 = IntegerOfTerm(t2);
	RFLOAT(FloatOfTerm(t1)/(Float)i2);
      }
    case double_e:
      {
	Float f2 = FloatOfTerm(t2);
	RFLOAT(FloatOfTerm(t1)/f2);
      }
    case big_int_e:
#ifdef USE_GMP
      return Yap_gmp_fdiv_float_big(FloatOfTerm(t1), t2);
#endif
    default:
      RERROR();
    }
    break;
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      return Yap_gmp_fdiv_big_int(t1, IntegerOfTerm(t2));
    case big_int_e:
      /* two bignums*/
      return Yap_gmp_fdiv_big_big(t1, t2);
    case double_e:
      return Yap_gmp_fdiv_big_float(t1, FloatOfTerm(t2));
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

/*
  xor #
*/
static Term
p_xor(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* two integers */
      RINT(IntegerOfTerm(t1) ^ IntegerOfTerm(t2));
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "#/2");
    case big_int_e:
#ifdef USE_GMP
      return Yap_gmp_xor_int_big(IntegerOfTerm(t1), t2);
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    return Yap_ArithError(TYPE_ERROR_INTEGER, t1, "#/2");
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      return Yap_gmp_xor_int_big(IntegerOfTerm(t2), t1);
    case big_int_e:
      return Yap_gmp_xor_big_big(t1, t2);
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "#/2");
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

/*
  atan2: arc tangent x/y
*/
static Term
p_atan2(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* two integers */
      RFLOAT(atan2(IntegerOfTerm(t1),IntegerOfTerm(t2)));
    case double_e:
      RFLOAT(atan2(IntegerOfTerm(t1),FloatOfTerm(t2)));
    case big_int_e:
#ifdef USE_GMP
      {
	Int i1 = IntegerOfTerm(t1);
	Float f2 = Yap_gmp_to_float(t2);
	RFLOAT(atan2(i1,f2));
      }
#endif
    default:
      RERROR();
      break;
    }
  case double_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* float / integer */
      {
	Int i2 = IntegerOfTerm(t2);
	RFLOAT(atan2(FloatOfTerm(t1),i2));
      }
    case double_e:
      {
	Float f2 = FloatOfTerm(t2);
	RFLOAT(atan2(FloatOfTerm(t1),f2));
      }
    case big_int_e:
#ifdef USE_GMP
      {
	RFLOAT(atan2(FloatOfTerm(t1),Yap_gmp_to_float(t2)));
      }
#endif
    default:
      RERROR();
    }
    break;
  case big_int_e:
#ifdef USE_GMP
    {
      Float dbl1 = Yap_gmp_to_float(t1);
      switch (ETypeOfTerm(t2)) {
      case long_int_e:
	{
	  Int i = IntegerOfTerm(t2);
	  RFLOAT(atan2(dbl1,i));
	}
      case big_int_e:
	/* two bignums */
	RFLOAT(atan2(dbl1,Yap_gmp_to_float(t2)));
      case double_e:
	{
	  Float dbl = FloatOfTerm(t2);
	  RFLOAT(atan2(dbl1,dbl));
	}
      default:
	RERROR();
      }
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}


/*
  power: x^y
*/
static Term
p_power(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i2 = IntegerOfTerm(t2);

	/* two integers */
	RFLOAT(pow(IntegerOfTerm(t1),i2));
      }
    case double_e:
      {
	/* integer, double */
	Float fl1 = (Float)IntegerOfTerm(t1);
	Float fl2 = FloatOfTerm(t2);
	RFLOAT(pow(fl1,fl2));
      }
    case big_int_e:
#ifdef USE_GMP
      {
	Int i1 = IntegerOfTerm(t1);
	Float f2 = Yap_gmp_to_float(t2);
	RFLOAT(pow(i1,f2));
      }
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* float / integer */
      {
	Int i2 = IntegerOfTerm(t2);
	RFLOAT(pow(FloatOfTerm(t1),i2));
      }
    case double_e:
      {
	Float f2 = FloatOfTerm(t2);
	RFLOAT(pow(FloatOfTerm(t1),f2));
      }
    case big_int_e:
#ifdef USE_GMP
      {
	RFLOAT(pow(FloatOfTerm(t1),Yap_gmp_to_float(t2)));
      }
#endif
    default:
      RERROR();
    }
    break;
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i = IntegerOfTerm(t2);
	RFLOAT(pow(Yap_gmp_to_float(t1),i));
      }
    case big_int_e:
      /* two bignums */
      RFLOAT(pow(Yap_gmp_to_float(t1),Yap_gmp_to_float(t2)));
    case double_e:
      {
	Float dbl = FloatOfTerm(t2);
	RFLOAT(pow(Yap_gmp_to_float(t1),dbl));
      }
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

/* next function is adapted from:
   Inline C++ integer exponentiation routines 
   Version 1.01
   Copyright (C) 1999-2004 John C. Bowman <bowman@math.ualberta.ca>
*/
static inline Int
ipow(Int x, Int p)
{
  Int r;

  if (p == 0) return ((CELL)1);
  if (x == 0 && p > 0) return 0L;
  if(p < 0) 
    return (-p % 2) ? x : ((CELL)1);
	
  r = ((CELL)1);
  for(;;) {
    if(p & 1) {
      if (mul_overflow((r*x), r, x)) {
	return 0;
      }
      r *= x;
    }
    if((p >>= 1) == 0)	return r;
    if (mul_overflow((x*x), x, x)) {
      return 0;
    }
    x *= x;
  }
}


/*
  power: x^y
*/
static Term
p_exp(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);
	Int pow = ipow(i1,i2);

	if (i2 < 0) {
	  return Yap_ArithError(DOMAIN_ERROR_NOT_LESS_THAN_ZERO, t2,
		    "%d ^ %d", i1, i2);
	}
#ifdef USE_GMP
	/* two integers */
	if ((i1 && !pow)) {
	  /* overflow */
	  return Yap_gmp_exp_int_int(i1, i2);
	}
#endif
	RINT(pow);
      }
    case double_e:
      {
	/* integer, double */
	Float fl1 = (Float)IntegerOfTerm(t1);
	Float fl2 = FloatOfTerm(t2);
	RFLOAT(pow(fl1,fl2));
      }
    case big_int_e:
#ifdef USE_GMP
      {
	Int i = IntegerOfTerm(t1);
	return Yap_gmp_exp_int_big(i,t2);
      }
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* float / integer */
      {
	Int i2 = IntegerOfTerm(t2);
	RFLOAT(pow(FloatOfTerm(t1),i2));
      }
    case double_e:
      {
	Float f2 = FloatOfTerm(t2);
	RFLOAT(pow(FloatOfTerm(t1),f2));
      }
    case big_int_e:
#ifdef USE_GMP
      {
	RFLOAT(pow(FloatOfTerm(t1),Yap_gmp_to_float(t2)));
      }
#endif
    default:
      RERROR();
    }
    break;
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i = IntegerOfTerm(t2);
	return Yap_gmp_exp_big_int(t1,i);
      }
    case big_int_e:
      /* two bignums, makes no sense */
      return Yap_gmp_exp_big_big(t1,t2);
    case double_e:
      {
	Float dbl = FloatOfTerm(t2);
	RFLOAT(pow(Yap_gmp_to_float(t1),dbl));
      }
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

static Int
gcd(Int m11,Int m21)
{
  /* Blankinship algorithm, provided by Miguel Filgueiras */
  Int m12=1, m22=0, k;

  while (m11>0 && m21>0)
    if (m11<m21) {
      k = m21/m11;  m21 -= k*m11;  m22 -= k*m12;
    } else {
      k=m11/m21;  m11 -= k*m21;  m12 -= k*m22;
    }
  if (m11<0 || m21<0) {		/* overflow? */
    /*    Oflow = 1; */
    Yap_ArithError(EVALUATION_ERROR_INT_OVERFLOW, MkIntegerTerm(m11),
	      "gcd/2 with %d and %d", m11, m21);
    return(1);
  }
  if (m11)  return(m11);
  return(m21);
}

#ifdef GCD_MULT
Int gcdmult(Int m11,Int m21,Int *pm11)	/* *pm11 gets multiplier of m11 */
{
  Int m12=1, m22=0, k;

  while (m11 && m21)
    if (m11<m21) {
      k = m21/m11;  m21 -= k*m11;  m22 -= k*m12;
    } else {
      k=m11/m21;  m11 -= k*m21;  m12 -= k*m22;
    }
  if (m11<0 || m21<0) {		/* overflow? */
    /*    Oflow = 1; */
    Yap_ArithError(EVALUATION_ERROR_INT_OVERFLOW, MkIntegerTerm(m11),
	      "gcdmult/2 with %d and %d", m11, m21);
    return(1);
  }
  if (m11) {
    *pm11 = m12;  return(m11);
  }
  *pm11 = m22;
  return(m21);
}
#endif

/*
  module gcd
*/
static Term
p_gcd(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* two integers */
      {
	Int i1 = IntegerOfTerm(t1), i2 = IntegerOfTerm(t2);
	i1 = (i1 >= 0 ? i1 : -i1);
	i2 = (i2 >= 0 ? i2 : -i2);

	RINT(gcd(i1,i2));
      }
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "gcd/2");
    case big_int_e:
#ifdef USE_GMP
      return Yap_gmp_gcd_int_big(IntegerOfTerm(t1), t2);
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    return Yap_ArithError(TYPE_ERROR_INTEGER, t1, "gcd/2");
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      return Yap_gmp_gcd_int_big(IntegerOfTerm(t2), t1);
    case big_int_e:
      return Yap_gmp_gcd_big_big(t1, t2);
    case double_e:
      return Yap_ArithError(TYPE_ERROR_INTEGER, t2, "gcd/2");
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

/*
  minimum: min(x,y)
*/
static Term
p_min(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);
	return((i1 < i2 ? t1 : t2));
      }
    case double_e:
      {
	/* integer, double */
	Int i = IntegerOfTerm(t1);
	Float fl = FloatOfTerm(t2);
	if (i <= fl) {
	  return t1;
	}
	return t2;
      }
    case big_int_e:
#ifdef USE_GMP
      if (Yap_gmp_cmp_int_big(IntegerOfTerm(t1), t2) < 0) {
	return t1;
      }
      return t2;
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* float / integer */
      {
	Int i = IntegerOfTerm(t2);
	Float fl = FloatOfTerm(t1);
	if (i <= fl) {
	  return t2;
	}
	return t1;
      }
    case double_e:
      {
	Float fl1 = FloatOfTerm(t1);
	Float fl2 = FloatOfTerm(t2);
	if (fl1 <= fl2) {
	  return t1;
	}
	return t2;
      }
    case big_int_e:
#ifdef USE_GMP
      if (Yap_gmp_cmp_float_big(FloatOfTerm(t1), t2) < 0) {
	return t1;
      }
      return t2;
#endif
    default:
      RERROR();
    }
    break;
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      if (Yap_gmp_cmp_big_int(t1, IntegerOfTerm(t2)) < 0) {
	return t1;
      }
      return t2;
    case big_int_e:
      if (Yap_gmp_cmp_big_big(t1, t2) < 0) {
	return t1;
      }
      return t2;
    case double_e:
      if (Yap_gmp_cmp_big_float(t1, FloatOfTerm(t2)) < 0) {
	return t1;
      }
      return t2;
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

/*
  maximum: max(x,y)
*/
static Term
p_max(Term t1, Term t2)
{
  switch (ETypeOfTerm(t1)) {
  case long_int_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      {
	Int i1 = IntegerOfTerm(t1);
	Int i2 = IntegerOfTerm(t2);
	return((i1 > i2 ? t1 : t2));
      }
    case double_e:
      {
	/* integer, double */
	Int i = IntegerOfTerm(t1);
	Float fl = FloatOfTerm(t2);
	if (i >= fl) {
	  return t1;
	}
	return t2;
      }
    case big_int_e:
#ifdef USE_GMP
      if (Yap_gmp_cmp_int_big(IntegerOfTerm(t1), t2) > 0) {
	return t1;
      }
      return t2;
#endif
    default:
      RERROR();
    }
    break;
  case double_e:
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      /* float / integer */
      {
	Int i = IntegerOfTerm(t2);
	Float fl = FloatOfTerm(t1);
	if (i >= fl) {
	  return t2;
	}
	return t1;
      }
    case double_e:
      {
	Float fl1 = FloatOfTerm(t1);
	Float fl2 = FloatOfTerm(t2);
	if (fl1 >= fl2) {
	  return t1;
	}
	return t2;
      }
    case big_int_e:
#ifdef USE_GMP
      if (Yap_gmp_cmp_float_big(FloatOfTerm(t1), t2) > 0) {
	return t1;
      }
      return t2;
#endif
    default:
      RERROR();
    }
    break;
  case big_int_e:
#ifdef USE_GMP
    switch (ETypeOfTerm(t2)) {
    case long_int_e:
      if (Yap_gmp_cmp_big_int(t1, IntegerOfTerm(t2)) > 0) {
	return t1;
      }
      return t2;
    case big_int_e:
      if (Yap_gmp_cmp_big_big(t1, t2) > 0) {
	return t1;
      }
      return t2;
    case double_e:
      if (Yap_gmp_cmp_big_float(t1, FloatOfTerm(t2)) > 0) {
	return t1;
      }
      return t2;
    default:
      RERROR();
    }
#endif
  default:
    RERROR();
  }
  RERROR();
}

static Term
eval2(Int fi, Term t1, Term t2) {
  arith2_op f = fi;
  switch (f) {
  case op_plus:
    return p_plus(t1, t2);
  case op_minus:
    return p_minus(t1, t2);
  case op_times:
    return p_times(t1, t2);
  case op_div:
    return p_div(t1, t2);
  case op_idiv:
    return p_div2(t1, t2);
  case op_and:
    return p_and(t1, t2);
  case op_or:
    return p_or(t1, t2);
  case op_sll:
    return p_sll(t1, t2);
  case op_slr:
    return p_slr(t1, t2);
  case op_mod:
    return p_mod(t1, t2);
  case op_rem:
    return p_rem(t1, t2);
  case op_fdiv:
    return p_fdiv(t1, t2);
  case op_xor:
    return p_xor(t1, t2);
  case op_atan2:
    return p_atan2(t1, t2);
  case op_power:
    return p_exp(t1, t2);
  case op_power2:
    return p_power(t1, t2);
  case op_gcd:
    return p_gcd(t1, t2);
  case op_min:
    return p_min(t1, t2);
  case op_max:
    return p_max(t1, t2);
  case op_rdiv:
    return p_rdiv(t1, t2);
  }
  RERROR();
}

Term Yap_eval_binary(Int f, Term t1, Term t2)
{
  return eval2(f,t1,t2);
}

static InitBinEntry InitBinTab[] = {
  {"+", op_plus},
  {"-", op_minus},
  {"*", op_times},
  {"/", op_fdiv},
  {"mod", op_mod},
  {"rem", op_rem},
  {"//", op_div},
  {"div", op_idiv},
  {"<<", op_sll},
  {">>", op_slr},
  {"/\\", op_and},
  {"\\/", op_or},
  {"#", op_xor},
  {"><", op_xor},
  {"xor", op_xor},
  {"atan", op_atan2},
  {"atan2", op_atan2},
  /* C-Prolog exponentiation */
  {"^", op_power},
  /* ISO-Prolog exponentiation */
  {"**", op_power2},
  /* Quintus exponentiation */
  {"exp", op_power2},
  {"gcd", op_gcd},
  {"min", op_min},
  {"max", op_max},
  {"rdiv", op_rdiv}
};

static Int 
p_binary_is( USES_REGS1 )
{				/* X is Y	 */
  Term t = Deref(ARG2);
  Term t1, t2;

  if (IsVarTerm(t)) {
    Yap_ArithError(INSTANTIATION_ERROR,t, "X is Y");
    return(FALSE);
  }
  t1 = Yap_Eval(Deref(ARG3));
  if (!Yap_FoundArithError(t1, ARG3)) {
    return FALSE;
  }
  t2 = Yap_Eval(Deref(ARG4));
  if (!Yap_FoundArithError(t2, ARG4)) {
    return FALSE;
  }
  if (IsIntTerm(t)) {
    Term tout = Yap_FoundArithError(eval2(IntOfTerm(t), t1, t2), 0L);
    if (!tout)
      return FALSE;
    return Yap_unify_constant(ARG1,tout);
  }
  if (IsAtomTerm(t)) {
    Atom name = AtomOfTerm(t);
    ExpEntry *p;
    Term out;

    if (EndOfPAEntr(p = RepExpProp(Yap_GetExpProp(name, 2)))) {
      Term ti[2];

      /* error */
      ti[0] = t;
      ti[1] = MkIntTerm(1);
      t = Yap_MkApplTerm(FunctorSlash, 2, ti);
      Yap_Error(TYPE_ERROR_EVALUABLE, t,
		"functor %s/%d for arithmetic expression",
		RepAtom(name)->StrOfAE,2);
      P = FAILCODE;
      return(FALSE);
    }
    if (!(out=Yap_FoundArithError(eval2(p->FOfEE, t1, t2), 0L)))
      return FALSE;
    return Yap_unify_constant(ARG1,out);
  }
  return FALSE;
}

static Int 
do_arith23(arith2_op op USES_REGS)
{				/* X is Y	 */
  Term t = Deref(ARG1);
  Int out;
  Term t1, t2;

  if (IsVarTerm(t)) {
    Yap_ArithError(INSTANTIATION_ERROR,t, "X is Y");
    return(FALSE);
  }
  t1 = Yap_Eval(t);
  if (t1 == 0L)
    return FALSE;
  t2 = Yap_Eval(Deref(ARG2));
  if (t2 == 0L)
    return FALSE;
  if (!(out=Yap_FoundArithError(eval2(op, t1, t2), 0L)))
    return FALSE;
  return Yap_unify_constant(ARG3,out);
}

static Int 
export_p_plus( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_plus PASS_REGS);
}

static Int 
export_p_minus( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_minus PASS_REGS);
}

static Int 
export_p_times( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_times PASS_REGS);
}

static Int 
export_p_div( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_div PASS_REGS);
}

static Int 
export_p_and( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_and PASS_REGS);
}

static Int 
export_p_or( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_or PASS_REGS);
}

static Int 
export_p_slr( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_slr PASS_REGS);
}

static Int 
export_p_sll( USES_REGS1 )
{				/* X is Y	 */
  return do_arith23(op_sll PASS_REGS);
}

static Int 
p_binary_op_as_integer( USES_REGS1 )
{				/* X is Y	 */
  Term t = Deref(ARG1);

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t, "X is Y");
    return(FALSE);
  }
  if (IsIntTerm(t)) {
    return Yap_unify_constant(ARG2,t);
  }
  if (IsAtomTerm(t)) {
    Atom name = AtomOfTerm(t);
    ExpEntry *p;

    if (EndOfPAEntr(p = RepExpProp(Yap_GetExpProp(name, 2)))) {
      return Yap_unify(ARG1,ARG2);
    }
    return Yap_unify_constant(ARG2,MkIntTerm(p->FOfEE));
  }
  return(FALSE);
}


void
Yap_InitBinaryExps(void)
{
  unsigned int    i;
  ExpEntry       *p;

  for (i = 0; i < sizeof(InitBinTab)/sizeof(InitBinEntry); ++i) {
    AtomEntry *ae = RepAtom(Yap_LookupAtom(InitBinTab[i].OpName));
    if (ae == NULL) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"at InitBinaryExps");
      return;
    }
    WRITE_LOCK(ae->ARWLock);
    if (Yap_GetExpPropHavingLock(ae, 2)) {
      WRITE_UNLOCK(ae->ARWLock);
      break;
    }
    p = (ExpEntry *) Yap_AllocAtomSpace(sizeof(ExpEntry));
    p->KindOfPE = ExpProperty;
    p->ArityOfEE = 2;
    p->ENoOfEE = 2;
    p->FOfEE = InitBinTab[i].f;
    AddPropToAtom(ae, (PropEntry *)p);
    WRITE_UNLOCK(ae->ARWLock);
  }
  Yap_InitCPred("is", 4, p_binary_is, TestPredFlag | SafePredFlag);
  Yap_InitCPred("$binary_op_as_integer", 2, p_binary_op_as_integer, TestPredFlag|SafePredFlag);
  Yap_InitAsmPred("$plus", 3, _plus, export_p_plus, SafePredFlag);
  Yap_InitAsmPred("$minus", 3, _minus, export_p_minus, SafePredFlag);
  Yap_InitAsmPred("$times", 3, _times, export_p_times, SafePredFlag);
  Yap_InitAsmPred("$div", 3, _div, export_p_div, SafePredFlag);
  Yap_InitAsmPred("$and", 3, _and, export_p_and, SafePredFlag);
  Yap_InitAsmPred("$or", 3, _or, export_p_or, SafePredFlag);
  Yap_InitAsmPred("$sll", 3, _sll, export_p_sll, SafePredFlag);
  Yap_InitAsmPred("$slr", 3, _slr, export_p_slr, SafePredFlag);
}

/* This routine is called from Restore to make sure we have the same arithmetic operators */
int
Yap_ReInitBinaryExps(void)
{
  return(TRUE);
}

