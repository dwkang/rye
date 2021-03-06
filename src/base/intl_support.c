/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

/*
 * intl_support.c : platform independent internationalization functions.
 */

#ident "$Id$"

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <ctype.h>
#include <wctype.h>

#include "error_manager.h"
#include "intl_support.h"
#include "language_support.h"
#include "chartype.h"
#include "system_parameter.h"
#if defined (ENABLE_UNUSED_FUNCTION)
#include "charset_converters.h"
#endif

#define IS_8BIT(c)              ((c) >> 7)
/* Special values for EUC encodings */
#ifndef SS3
#define SS3                     143
#endif

#define LOCALE_C        "C"
#define LOCALE_KOREAN   "korean"

#define CHAR_BYTE_TO_LOWER(c) ((c) + ('a' - 'A'))
#define CHAR_BYTE_TO_UPPER(c) ((c) - ('a' - 'A'))

/* conversion from turkish ISO 8859-9 to UTF-8 */
#define ISO_8859_9_FIRST_CP 0x11e
#define ISO_8859_9_LAST_CP 0x15f

static CONV_CP_TO_BYTES iso8859_9_to_utf8_conv[256];
static CONV_CP_TO_BYTES utf8_cp_to_iso_8859_9_conv[ISO_8859_9_LAST_CP -
						   ISO_8859_9_FIRST_CP + 1];

/* conversion from Latin 1 ISO 8859-1 to UTF-8: */
static CONV_CP_TO_BYTES iso8859_1_to_utf8_conv[256];


/* General EUC string manipulations */
#if defined (ENABLE_UNUSED_FUNCTION)
static int intl_tolower_euc (unsigned char *s, int length_in_chars);
static int intl_toupper_euc (unsigned char *s, int length_in_chars);
static int intl_count_euc_chars (unsigned char *s, int length_in_bytes);
static int intl_count_euc_bytes (unsigned char *s, int length_in_chars);
#endif

/* UTF-8 string manipulations */
static int intl_tolower_utf8 (const ALPHABET_DATA * a,
			      unsigned char *s, unsigned char *d,
			      int length_in_chars, int *d_size);
static int intl_toupper_utf8 (const ALPHABET_DATA * a,
			      unsigned char *s, unsigned char *d,
			      int length_in_chars, int *d_size);
static int intl_count_utf8_bytes (unsigned char *s, int length_in_chars);
static int intl_char_tolower_utf8 (const ALPHABET_DATA * a,
				   unsigned char *s, const int size,
				   unsigned char *d, unsigned char **next);
static int intl_char_toupper_utf8 (const ALPHABET_DATA * a,
				   unsigned char *s, const int size,
				   unsigned char *d, unsigned char **next);
static int intl_strcasecmp_utf8_one_cp (const ALPHABET_DATA * alphabet,
					unsigned char *str1,
					unsigned char *str2,
					const int size_str1,
					const int size_str2,
					unsigned int cp1, unsigned int cp2,
					int *skip_size1, int *skip_size2);
static void intl_init_conv_iso8859_9_to_utf8 (void);
static void intl_init_conv_iso8859_1_to_utf8 (void);


TEXT_CONVERSION con_iso_8859_9_conv = {
  TEXT_CONV_ISO_88599_BUILTIN,	/* type */
  (char *) "28599",		/* Windows Code page */
  (char *) "iso88599",		/* Linux charset identifiers */
  {0},				/* byte flags : not used for ISO */
  0, 0, NULL,			/* UTF-8 to console : filled by init function */
  0, 0, NULL,			/* console to UTF-8 : filled by init function */
  intl_text_utf8_to_single_byte,	/* UTF-8 to console conversion function */
  intl_text_single_byte_to_utf8,	/* console to UTF-8 conversion function */
  intl_init_conv_iso8859_9_to_utf8,	/* init function */
};

TEXT_CONVERSION con_iso_8859_1_conv = {
  TEXT_CONV_ISO_88591_BUILTIN,	/* type */
  (char *) "28591",		/* Windows Code page */
  (char *) "iso88591",		/* Linux charset identifiers */
  {0},				/* byte flags : not used for ISO */
  0, 0, NULL,			/* UTF-8 to console : filled by init function */
  0, 0, NULL,			/* console to UTF-8 : filled by init function */
  intl_text_utf8_to_single_byte,	/* UTF-8 to console conversion function */
  intl_text_single_byte_to_utf8,	/* console to UTF-8 conversion function */
  intl_init_conv_iso8859_1_to_utf8,	/* init function */
};


/*
 * intl_mbs_chr() - find first occurrence of the given character
 *   return: a pointer to the first occurrence of the given character in
 *           the given multibyte string, or NULL if no occurrence is found
 *   mbs(in)
 *   wc(in)
 */
char *
intl_mbs_chr (const char *mbs, wchar_t wc)
{
  assert (mbs != NULL);

  return (char *) (strchr (mbs, (int) wc));
}

/*
 * intl_mbs_len() - computes the number of multibyte character sequences in the multibyte
 *             character string, not including the terminating zero byte
 *   return: number of characters if  success.
 *           On error, 0 is returned and errno is set.
 *              EINVAL  : mbs contains an invalid byte sequence.
 *   mbs(in)
 */
int
intl_mbs_len (const char *mbs)
{
  assert (mbs != NULL);

  return strlen (mbs);
}

/*
 * intl_mbs_nth() - finds the nth multibyte character in the multibyte string
 *   return: a pointer to the nth character in n.
 *           NULL if either an error occurs or there are not n characters
 *                in the string
 *   mbs(in)
 *   n(in)
 */

const char *
intl_mbs_nth (const char *mbs, size_t n)
{
  assert (mbs != NULL);
  if (mbs == NULL)
    {
      return NULL;
    }

  if (strlen (mbs) < (int) n)
    {
      errno = EINVAL;
      return NULL;
    }

  return &mbs[n];
}

/*
 * intl_mbs_spn() - return the size of the prefix of the given multibyte string
 *             consisting of the given wide characters.
 *   return: size in bytes.
 *           If mbs contains an invalid byte sequence,
 *           errno is set and 0 is returned.
 *   mbs(in)
 *   chars(in)
 */
int
intl_mbs_spn (const char *mbs, const wchar_t * chars)
{
  assert (mbs != NULL);
  assert (chars != NULL);

  return (int) strspn (mbs, (const char *) chars);
}

/*
 * intl_mbs_casecmp() - compares successive multi-byte character elements
 *                 from two multi-byte strings
 *   return: 0 if all the multi-byte character elements are the same,
 *           positive number if mbs1 is greater than mbs2,
 *           negative number otherwise.
 *   mbs1(in)
 *   mbs2(in)
 *
 * Note: This function does not use the collating sequences specified
 *       in the LC_COLLATE category of the current locale.
 *       This function set errno if mbs1 or mbs2 contain one or more
 *       invalid multi-byte characters.
 */
int
intl_mbs_casecmp (const char *mbs1, const char *mbs2)
{
  assert (mbs1 != NULL);
  assert (mbs2 != NULL);

  return strcasecmp (mbs1, mbs2);
}

/*
 * intl_mbs_ncasecmp() - compares the first n successive multi-byte character elements
 *                  from two multi-byte strings
 *   return: 0 if the first n multi-byte character elements are the same,
 *           positive number if mbs1 is greater than mbs2,
 *           negative number otherwise.
 *   mbs1(in)
 *   mbs2(in)
 *   n (in)
 *
 * Note: This function does not use the collating sequences specified
 *       in the LC_COLLATE category of the current locale.
 *       This function set errno if mbs1 or mbs2 contain one or more
 *       invalid multi-byte characters.
 */
int
intl_mbs_ncasecmp (const char *mbs1, const char *mbs2, size_t n)
{
  assert (mbs1 != NULL);
  assert (mbs2 != NULL);

  return strncasecmp (mbs1, mbs2, n);
}

/*
 * intl_mbs_ncpy() - Copy characters from mbs2 to mbs1 at most (n-1) bytes
 *   return: mbs1, null-terminated string.
 *   mbs1(out)
 *   mbs2(in)
 *   n(in): size of destination buffer, including null-terminator
 *
 * Note: If mbs2 contains an invalid multi-byte character, errno is set and the
 *   function returns NULL.  In this case, the contents of mbs1 are undefined.
 */

char *
intl_mbs_ncpy (char *mbs1, const char *mbs2, size_t n)
{
  assert (mbs1 != NULL);
  assert (mbs2 != NULL);

  size_t src_len = strlen (mbs2);

  strncpy (mbs1, mbs2, n - 1);
  if (src_len < n)
    {
      mbs1[src_len] = '\0';
    }
  else
    {
      mbs1[n - 1] = '\0';
    }

  return mbs1;
}

/*
 * ISO 8859-1 encoding functions
 */

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_tolower_iso8859() - replaces all upper case ISO88591 characters
 *                          with their lower case codes.
 *   return: character counts
 *   s(in/out): string to lowercase
 *   length(in): length of the string
 */
int
intl_tolower_iso8859 (unsigned char *s, int length)
{
  int char_count = length;
  unsigned char *end;

  assert (s != NULL);

  for (end = s + length; s < end; s++)
    {
      if (char_isupper_iso8859 (*s))
	{
	  *s = CHAR_BYTE_TO_LOWER (*s);
	}
    }

  return char_count;
}

/*
 * intl_toupper_iso8859() - replaces all lower case ISO88591 characters
 *                          with their upper case codes.
 *   return: character counts
 *   s(in/out): string to uppercase
 *   length(in): length of the string
 */
int
intl_toupper_iso8859 (unsigned char *s, int length)
{
  int char_count = length;
  unsigned char *end;

  assert (s != NULL);

  for (end = s + length; s < end; s++)
    {
      if (char_islower_iso8859 (*s))
	{
	  *s = CHAR_BYTE_TO_UPPER (*s);
	}
    }

  return char_count;
}

/*
 * general routines for EUC encoding
 */

/*
 * intl_nextchar_euc() - returns a pointer to the next character in the EUC encoded
 *              string.
 *   return: pointer to the next EUC character in the string.
 *   s(in): string
 *   curr_char_length(out): length of the character at s
 */
unsigned char *
intl_nextchar_euc (unsigned char *s, int *curr_char_length)
{
  assert (s != NULL);

  if (!IS_8BIT (*s))		/* Detected ASCII character */
    {
      *curr_char_length = 1;
    }
  else if (*s == SS3)		/* Detected Code Set 3 character */
    {
      *curr_char_length = 3;
    }
  else				/* Detected 2 byte character (CS1 or CS2) */
    {
      *curr_char_length = 2;
    }

  return (s + (*curr_char_length));
}

/*
 * intl_prevchar_euc() - returns a pointer to the previous character in the EUC
 *                   encoded string.
 *   return: pointer to the previous EUC character in the string s.
 *   s(in): string
 *   s_start(in) : start of buffer string
 *   prev_char_length(out): length of the previous character
 */
unsigned char *
intl_prevchar_euc (unsigned char *s, const unsigned char *s_start,
		   int *prev_char_length)
{
  assert (s != NULL);
  assert (s > s_start);

  if (s - 3 >= s_start && *(s - 3) == SS3)
    {
      *prev_char_length = 3;
      return s - 3;
    }
  else if (s - 2 >= s_start && IS_8BIT (*(s - 2)))
    {
      *prev_char_length = 2;
      return s - 2;
    }

  *prev_char_length = 1;
  return --s;
}

/*
 * intl_tolower_euc() - Replaces all upper case ASCII characters inside an EUC
 *                      encoded string with their lower case codes.
 *   return: character counts
 *   s(in/out): EUC string to lowercase
 *   length_in_chars(in): length of the string measured in characters
 */
static int
intl_tolower_euc (unsigned char *s, int length_in_chars)
{
  int char_count;
  int dummy;

  assert (s != NULL);

  for (char_count = 0; char_count < length_in_chars; char_count++)
    {
      *s = char_tolower (*s);
      s = intl_nextchar_euc (s, &dummy);
    }

  return char_count;
}
#endif

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_toupper_euc() - Replaces all upper case ASCII characters inside an EUC
 *                      encoded string with their upper case codes.
 *   return: character counts
 *   s(in/out): EUC string to uppercase
 *   length_in_chars(in): length of the string measured in characters
 */
static int
intl_toupper_euc (unsigned char *s, int length_in_chars)
{
  int char_count;
  int dummy;

  assert (s != NULL);

  for (char_count = 0; char_count < length_in_chars; char_count++)
    {
      *s = char_toupper (*s);
      s = intl_nextchar_euc (s, &dummy);
    }

  return char_count;
}
#endif

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_count_euc_chars() - Counts the number of EUC encoded characters in the
 *                     string.  Embedded NULL characters are counted.
 *   return: none
 *   s(in): string
 *   length_in_bytes(in): length of the string
 *   char_count(out): number of EUC encoded characters found
 *
 * Note: Only whole characters are counted.
 *       if s[length_in_bytes-1] is not the last byte of a multi-byte
 *       character or a single byte character, then that character is not
 *       counted.
 */
static int
intl_count_euc_chars (unsigned char *s, int length_in_bytes)
{
  unsigned char *end;
  int dummy;
  int char_count;

  assert (s != NULL);

  for (end = s + length_in_bytes, char_count = 0; s < end;)
    {
      s = intl_nextchar_euc (s, &dummy);
      if (s <= end)
	{
	  char_count++;
	}
    }

  return char_count;
}
#endif

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_count_euc_bytes() - Counts the number of bytes it takes to encode the
 *                     next <length_in_chars> EUC characters in the string
 *   return:  byte counts
 *   s(in): EUC encoded string
 *   lenth_in_chars(in): length of the string in characters
 *   byte_count(out): number of bytes used for encode
 */
static int
intl_count_euc_bytes (unsigned char *s, int length_in_chars)
{
  int char_count;
  int char_width;
  int byte_count;

  assert (s != NULL);

  for (char_count = 0, byte_count = 0; char_count < length_in_chars;
       char_count++)
    {
      s = intl_nextchar_euc (s, &char_width);
      byte_count += char_width;
    }

  return byte_count;
}
#endif

/*
 * string handling functions
 */

/*
 * intl_char_count() - Counts the number of characters in the string
 *   return: number of characters found
 *   src(in): string of characters to count
 *   length_in_bytes(in): length of the string
 *   char_count(out): number of characters found
 *
 * Note: Embedded NULL characters are counted.
 */
int
intl_char_count (unsigned char *src, int length_in_bytes, int *char_count)
{
  *char_count = intl_count_utf8_chars (src, length_in_bytes);
  return *char_count;
}

/*
 * intl_char_size() - returns the number of bytes in a string given the
 *                   start and character length of the string
 *   return: none
 *   src(in): number of byets
 *   length_in_chars(in): legnth of the string in characters
 *   bytes_count(out): number of byets used for encode teh number of
 *                     characters specified
 *
 * Note: Embedded NULL's are counted as characters.
 */
int
intl_char_size (unsigned char *src, int length_in_chars, int *byte_count)
{
  *byte_count = intl_count_utf8_bytes (src, length_in_chars);
  return *byte_count;
}


/*
 * intl_prev_char() - returns pointer to the previous char in string
 *
 *   return : pointer to previous character
 *   s(in) : string
 *   s_start(in) : start of buffer string
 *   prev_char_size(out) : size of previous character
 */
unsigned char *
intl_prev_char (unsigned char *s, const unsigned char *s_start,
		int *prev_char_size)
{
  assert (s > s_start);
  return intl_prevchar_utf8 (s, s_start, prev_char_size);
}

/*
 * intl_next_char () - returns pointer to the next char in string
 *
 *   return: Pointer to the next character in the string.
 *   s(in) : string
 *   current_char_size(out) : length of the character at s
 *
 * Note: Returns a pointer to the next character in the string.
 *	 curr_char_length is set to the byte length of the current character.
 */
unsigned char *
intl_next_char (unsigned char *s, int *current_char_size)
{
  return intl_nextchar_utf8 (s, current_char_size);
}

/*
 * intl_cmp_char() - compares the first character of two strings
 *   return: zero if character are equal, non-zero otherwise
 *   s1(in):
 *   s2(in):
 *   char_size(in): size of char in bytes of the first character in s1
 *
 *  Note: it is assumed that both strings contain at least one character of
 *	  the given codeset.
 *
 */
int
intl_cmp_char (const unsigned char *s1, const unsigned char *s2,
	       int *char_size)
{
  *char_size = intl_Len_utf8_char[*s1];
  return memcmp (s1, s2, *char_size);
}

/*
 * intl_pad_char() - returns the pad character of requested codeset
 *   return: none
 *   codeset(in): International codeset.
 *   pad_char(in/out): Pointer to array which will be filled with
 *		       the pad character.
 *   pad_size(out): Size of pad character.
 *
 * Note:
 *     There is a pad character associated with every character code
 *     set.  This function will retrieve the pad character for a given
 *     code set.  The pad character is written into an array that must
 *     allocated by the caller.
 *
 */
void
intl_pad_char (unsigned char *pad_char, int *pad_size)
{
  pad_char[0] = ' ';
  *pad_size = 1;
}

/*
 * intl_pad_size() - Returns the byte size of the pad character for the given
 *		     codeset.
 *   return: size of pading char
 *   codeset(in): International codeset.
 *
 * Note:
 *     There is a pad character associated with every character code
 *     set.  This function will retrieve the pad character for a given
 *     code set.  The pad character is written into an array that must
 *     allocated by the caller.
 *
 */
int
intl_pad_size (void)
{
  return 1;
}

/*
 * intl_upper_string_size() - determine the size required for holding
 *			     upper case of the input string
 *   return: required size
 *   alphabet(in): alphabet data
 *   src(in): string to uppercase
 *   src_size(in): buffer size
 *   src_length(in): length of the string measured in characters
 */
int
intl_upper_string_size (const void *alphabet, unsigned char *src,
			int src_size, int src_length)
{
  int char_count;
  int req_size = src_size;
  unsigned char upper[INTL_UTF8_MAX_CHAR_SIZE];
  unsigned char *next = NULL;

  assert (alphabet != NULL);
  assert (((ALPHABET_DATA *) alphabet)->codeset == INTL_CODESET_UTF8);

  req_size = 0;
  for (char_count = 0; char_count < src_length && src_size > 0; char_count++)
    {
      req_size += intl_char_toupper_utf8 (alphabet, src, src_size,
					  upper, &next);
      src_size -= (next - src);
      src = next;
    }

  return req_size;
}

/*
 * intl_upper_string() - replace all lower case characters with their
 *                       upper case characters
 *   return: character counts
 *   alphabet(in): alphabet data
 *   src(in/out): string source to uppercase
 *   dst(in/out): output string
 *   length_in_chars(in): length of the string measured in characters
 */
int
intl_upper_string (const void *alphabet, unsigned char *src,
		   unsigned char *dst, int length_in_chars)
{
  int char_count = 0;
  int dummy_size;

  assert (alphabet != NULL);
  assert (((ALPHABET_DATA *) alphabet)->codeset == INTL_CODESET_UTF8);

  char_count = intl_toupper_utf8 (alphabet, src, dst, length_in_chars,
				  &dummy_size);
  return char_count;
}

/*
 * intl_lower_string_size() - determine the size required for holding
 *			     lower case of the input string
 *   return: required size
 *   alphabet(in): alphabet data
 *   src(in): string to lowercase
 *   src_size(in): buffer size
 *   src_length(in): length of the string measured in characters
 */
int
intl_lower_string_size (const void *alphabet, unsigned char *src,
			int src_size, int src_length)
{
  int char_count;
  int req_size = src_size;
  unsigned char lower[INTL_UTF8_MAX_CHAR_SIZE];
  unsigned char *next;

  assert (alphabet != NULL);
  assert (((ALPHABET_DATA *) alphabet)->codeset == INTL_CODESET_UTF8);

  req_size = 0;
  for (char_count = 0; char_count < src_length && src_size > 0; char_count++)
    {
      req_size += intl_char_tolower_utf8 (alphabet, src, src_size,
					  lower, &next);
      src_size -= (next - src);
      src = next;
    }

  return req_size;
}

/*
 * intl_lower_string() - replace all upper case characters with their
 *                      lower case characters
 *   return: character counts
 *   alphabet(in): alphabet data
 *   src(in/out): string to lowercase
 *   dst(out): output string
 *   length_in_chars(in): length of the string measured in characters
 */
int
intl_lower_string (const void *alphabet, unsigned char *src,
		   unsigned char *dst, int length_in_chars)
{
  int char_count = 0;
  int dummy_size;

  assert (alphabet != NULL);
  assert (((ALPHABET_DATA *) alphabet)->codeset == INTL_CODESET_UTF8);

  char_count = intl_tolower_utf8 (alphabet, src, dst, length_in_chars,
				  &dummy_size);
  return char_count;
}

/*
 * intl_zone() - Return the zone for the given category of the
 *               current locale
 *   return: INTL_ZONE enumeration
 *   lang_id(in): language identifier
 */
INTL_ZONE
intl_zone (UNUSED_ARG int category)
{
  switch (lang_id ())
    {
    case INTL_LANG_ENGLISH:
      return INTL_ZONE_US;
    case INTL_LANG_KOREAN:
      return INTL_ZONE_KR;
    default:
      return INTL_ZONE_US;
    }
  return INTL_ZONE_US;
}

/*
 * intl_reverse_string() - reverse characters of source string,
 *			   into destination string
 *   return: character counts
 *   src(in): source string
 *   dst(out): destination string
 *   length_in_chars(in): length of the string measured in characters
 *   size_in_bytes(in): size of the string in bytes
 */
int
intl_reverse_string (unsigned char *src, unsigned char *dst,
		     int length_in_chars, int size_in_bytes)
{
  unsigned char *end, *s, *d;
  int char_count = 0;
  int char_size, i;

  assert (src != NULL);
  assert (dst != NULL);

  s = src;

  d = dst + size_in_bytes - 1;
  end = src + size_in_bytes;
  for (; s < end && char_count < length_in_chars; char_count++)
    {
      intl_nextchar_utf8 (s, &char_size);

      i = char_size;
      while (i > 0)
	{
	  i--;
	  *(d - i) = *s;
	  s++;
	}
      d -= char_size;
    }

  return char_count;
}

/*
 * intl_is_max_bound_chr () -
 *
 * return: check if chr points to a char representing the upper bound
 *	   codepoint in the selected codeset, for LIKE index optimization.
 *
 * chr(in) : upper bound, as bytes
 */
bool
intl_is_max_bound_chr (const unsigned char *chr)
{
  if ((*chr == 0xf4) && (*(chr + 1) == 0x8f) &&
      (*(chr + 2) == 0xbf) && (*(chr + 3) == 0xbf))
    {
      return true;
    }

  return false;
}

/*
 * intl_is_min_bound_chr () -
 *
 * return: check if chr points to a ISO char / UTF-8 codepoint representing
 *	   the lower bound codepoint in the selected codeset, for LIKE
 *         index optimization.
 *
 * chr(in) : upper bound, as UTF-8 bytes
 *
 * Note: 'chr' buffer should be able to store at least 1 more byte, for
 *	  one space char.
 */
bool
intl_is_min_bound_chr (const unsigned char *chr)
{
  if (*chr == ' ')
    {
      return true;
    }

  return false;
}

/*
 * intl_set_min_bound_chr () - sets chr to a byte array representing
 *			       the lowest bound codepoint in the selected
 *			       codeset, for LIKE index optimization.
 *
 * return: the number of bytes added to chr
 *
 * chr(in) : char pointer where to place the bound, as UTF-8 bytes
 */
int
intl_set_min_bound_chr (char *chr)
{
  *chr = ' ';

  return 1;
}

/*
 * intl_set_max_bound_chr () - sets chr to a byte array representing
 *			       the up-most bound codepoint in the selected
 *			       codeset, for LIKE index optimization.
 *
 * return: the number of bytes added to chr
 *
 * chr(in) : char pointer where to place the bound
 *
 * Note: 'chr' buffer should be able to store at least one more char:
 *	 4 bytes (UTF-8), 2 bytes (EUC-KR), 1 byte (ISO-8859-1).
 *
 */
int
intl_set_max_bound_chr (char *chr)
{
  *chr = 0xf4;
  *(chr + 1) = 0x8f;
  *(chr + 2) = 0xbf;
  *(chr + 3) = 0xbf;
  return 4;
}

/*
 * general routines for UTF-8 encoding
 */

static const unsigned char len_utf8_char[256] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 6, 6, 1, 1
};

const unsigned char *const intl_Len_utf8_char = len_utf8_char;

/*
 * intl_nextchar_utf8() - returns a pointer to the next character in the
 *              UTF-8 encoded string.
 *   return: pointer to the next character
 *   s(in): input string
 *   curr_char_length(out): length of the character at s
 */
unsigned char *
intl_nextchar_utf8 (unsigned char *s, int *curr_char_length)
{
  INTL_GET_NEXTCHAR_UTF8 (s, *curr_char_length);
  return s;
}

/*
 * intl_prevchar_utf8() - returns a pointer to the previous character in the
 *                   UTF-8 encoded string.
 *   return: pointer to the previous character
 *   s(in): string
 *   s_start(in) : start of buffer string
 *   prev_char_length(out): length of the previous character
 */
unsigned char *
intl_prevchar_utf8 (unsigned char *s,
		    const unsigned char *s_start, int *prev_char_length)
{
  int l = 0;

  do
    {
      l++;
    }
  while (l < 6 && s - l >= s_start && (*(s - l) & 0xc0) == 0x80);

  l = (*(s - l) & 0xc0) == 0x80 ? 1 : l;
  s -= l;
  *prev_char_length = l;

  return s;
}

/*
 * intl_tolower_utf8() - Replaces all upper case characters inside an UTF-8
 *			 encoded string with their lower case codes.
 *   return: character counts
 *   alphabet(in): alphabet to use
 *   s(in): UTF-8 string to lowercase
 *   d(out): output string
 *   length_in_chars(in): length of the string measured in characters
 *   d_size(out): size in bytes of destination
 */
static int
intl_tolower_utf8 (const ALPHABET_DATA * alphabet, unsigned char *s,
		   unsigned char *d, int length_in_chars, int *d_size)
{
  int char_count, size;
  int s_size;
  unsigned char *next = NULL;

  assert (s != NULL);
  assert (d_size != NULL);

  intl_char_size (s, length_in_chars, &s_size);
  *d_size = 0;

  for (char_count = 0; char_count < length_in_chars; char_count++)
    {
      if (s_size <= 0)
	{
	  break;
	}
      size = intl_char_tolower_utf8 (alphabet, s, s_size, d, &next);
      d += size;
      *d_size += size;

      s_size -= next - s;
      s = next;
    }

  return char_count;
}

/*
 * intl_toupper_utf8() - Replaces all lower case characters inside an UTF-8
 *			 encoded string with their upper case codes.
 *   return: character counts
 *   alphabet(in): alphabet to use
 *   s(in): UTF-8 string to uppercase
 *   d(out): output string
 *   length_in_chars(in): length of the string measured in characters
 *   d_size(out): size in bytes of destination
 */
static int
intl_toupper_utf8 (const ALPHABET_DATA * alphabet, unsigned char *s,
		   unsigned char *d, int length_in_chars, int *d_size)
{
  int char_count, size;
  int s_size;
  unsigned char *next = NULL;

  assert (s != NULL);
  assert (d_size != NULL);

  intl_char_size (s, length_in_chars, &s_size);
  *d_size = 0;

  for (char_count = 0; char_count < length_in_chars; char_count++)
    {
      if (s_size <= 0)
	{
	  break;
	}
      size = intl_char_toupper_utf8 (alphabet, s, s_size, d, &next);
      d += size;
      *d_size += size;

      s_size -= next - s;
      s = next;
    }

  return char_count;
}

/*
 * intl_count_utf8_chars() - Counts the number of UTF-8 encoded characters in
 *                     the string. Embedded NULL characters are counted.
 *   return: none
 *   s(in): string
 *   length_in_bytes(in): length of the string
 *   char_count(out): number of UTF-8 encoded characters found
 *
 * Note: Only whole characters are counted.
 *       if s[length_in_bytes-1] is not the last byte of a multi-byte
 *       character or a single byte character, then that character is not
 *       counted.
 */
int
intl_count_utf8_chars (unsigned char *s, int length_in_bytes)
{
  unsigned char *end;
  int dummy;
  int char_count;

  assert (s != NULL);

  for (end = s + length_in_bytes, char_count = 0; s < end;)
    {
      s = intl_nextchar_utf8 (s, &dummy);
      if (s <= end)
	{
	  char_count++;
	}
    }

  return char_count;
}

/*
 * intl_count_utf8_bytes() - Counts the number of bytes it takes to encode the
 *                     next <length_in_chars> UTF-8 characters in the string
 *   return: byte counts
 *   s(in): UTF-8 encoded string
 *   lenth_in_chars(in): length of the string in characters
 *   byte_count(out): number of bytes used for encode
 */
static int
intl_count_utf8_bytes (unsigned char *s, int length_in_chars)
{
  int char_count;
  int char_width;
  int byte_count;

  assert (s != NULL);

  for (char_count = 0, byte_count = 0; char_count < length_in_chars;
       char_count++)
    {
      s = intl_nextchar_utf8 (s, &char_width);
      byte_count += char_width;
    }

  return byte_count;
}

/*
 * intl_char_tolower_utf8() - convert uppercase character to lowercase
 *   return: size of UTF-8 lowercase character corresponding to the argument
 *   alphabet(in): alphabet to use
 *   s (in): the UTF-8 buffer holding character to be converted
 *   size(in): size of UTF-8 buffer
 *   d (out): output buffer
 *   next (out): pointer to next character
 *
 *  Note : allocated size of 'd' is assumed to be large enough to fit any
 *	   UTF-8 character
 */
static int
intl_char_tolower_utf8 (const ALPHABET_DATA * alphabet, unsigned char *s,
			const int size, unsigned char *d,
			unsigned char **next)
{
  unsigned int cp = intl_utf8_to_cp (s, size, next);

  assert (alphabet != NULL);

  if (cp < (unsigned int) (alphabet->l_count))
    {
      if (alphabet->lower_multiplier == 1)
	{
	  unsigned int lower_cp = alphabet->lower_cp[cp];

	  return intl_cp_to_utf8 (lower_cp, d);
	}
      else
	{
	  const unsigned int *case_p;
	  int count = 0;
	  int bytes;
	  int total_bytes = 0;

	  assert (alphabet->lower_multiplier > 1 &&
		  alphabet->lower_multiplier <=
		  INTL_CASING_EXPANSION_MULTIPLIER);

	  case_p = &(alphabet->lower_cp[cp * alphabet->lower_multiplier]);

	  do
	    {
	      bytes = intl_cp_to_utf8 (*case_p, d);
	      d += bytes;
	      total_bytes += bytes;
	      case_p++;
	      count++;
	    }
	  while (count < alphabet->lower_multiplier && *case_p != 0);

	  return total_bytes;
	}
    }
  else if (cp == 0xffffffff)
    {
      /* this may happen when UTF-8 text validation is disabled (by default)
       */
      *d = *s;
      return 1;
    }

  return intl_cp_to_utf8 (cp, d);
}

/*
 * intl_char_toupper_utf8() - convert lowercase character to uppercase
 *   return: size of UTF-8 uppercase character corresponding to the argument
 *   alphabet(in): alphabet to use
 *   s (in): the UTF-8 buffer holding character to be converted
 *   size(in): size of UTF-8 buffer
 *   d (out): output buffer
 *   next (out): pointer to next character
 *
 *  Note : allocated size of 'd' is assumed to be large enough to fit any
 *	   UTF-8 character
 */
static int
intl_char_toupper_utf8 (const ALPHABET_DATA * alphabet, unsigned char *s,
			const int size, unsigned char *d,
			unsigned char **next)
{
  unsigned int cp = intl_utf8_to_cp (s, size, next);

  assert (alphabet != NULL);

  if (cp < (unsigned int) (alphabet->l_count))
    {
      if (alphabet->upper_multiplier == 1)
	{
	  unsigned upper_cp = alphabet->upper_cp[cp];

	  return intl_cp_to_utf8 (upper_cp, d);
	}
      else
	{
	  const unsigned int *case_p;
	  int count = 0;
	  int bytes;
	  int total_bytes = 0;

	  assert (alphabet->upper_multiplier > 1 &&
		  alphabet->upper_multiplier <=
		  INTL_CASING_EXPANSION_MULTIPLIER);

	  case_p = &(alphabet->upper_cp[cp * alphabet->upper_multiplier]);
	  do
	    {
	      bytes = intl_cp_to_utf8 (*case_p, d);
	      d += bytes;
	      total_bytes += bytes;
	      case_p++;
	      count++;
	    }
	  while (count < alphabet->upper_multiplier && *case_p != 0);

	  return total_bytes;
	}
    }
  else if (cp == 0xffffffff)
    {
      /* this may happen when UTF-8 text validation is disabled (by default)
       */
      *d = *s;
      return 1;
    }

  return intl_cp_to_utf8 (cp, d);
}

/*
 * intl_is_case_match() - performs case insensitive matching
 *   return:  0 if strings are equal, -1 if str1 < str2 , 1 if str1 > str2
 *   lang_id(in):
 *   tok(in): token to check
 *   src(in): string to check for token
 *   size_tok(in): size in bytes of token
 *   size_src(in): size in bytes of source string
 *   matched_size_src(out): size in bytes of matched token in source
 *
 *  Note : Matching is performed by folding to LOWER case;
 *	   it takes into account case expansion (length in chars may differ).
 */
int
intl_case_match_tok (const INTL_LANG lang_id,
		     unsigned char *tok, unsigned char *src,
		     const int size_tok, const int size_src,
		     int *matched_size_src)
{
  unsigned char *tok_end, *src_end;
  unsigned char *dummy;
  unsigned int cp1, cp2;
  const LANG_LOCALE_DATA *loc;
  const ALPHABET_DATA *alphabet;

  assert (tok != NULL);
  assert (src != NULL);

  assert (size_tok > 0);
  assert (size_src >= 0);

  assert (matched_size_src != NULL);

  *matched_size_src = 0;
  loc = lang_get_specific_locale (lang_id);

  assert (loc != NULL);

  alphabet = &(loc->alphabet);

  tok_end = tok + size_tok;
  src_end = src + size_src;

  for (; tok < tok_end && src < src_end;)
    {
      int skip_size_tok = 0, skip_size_src = 0;
      int res;

      cp1 = intl_utf8_to_cp (tok, tok_end - tok, &dummy);
      cp2 = intl_utf8_to_cp (src, src_end - src, &dummy);

      res =
	intl_strcasecmp_utf8_one_cp (alphabet, tok, src,
				     tok_end - tok, src_end - src,
				     cp1, cp2, &skip_size_tok,
				     &skip_size_src);

      if (res != 0)
	{
	  return res;
	}

      tok += skip_size_tok;
      src += skip_size_src;
      *matched_size_src += skip_size_src;
    }

  return (tok < tok_end) ? 1 : 0;
}

/*
 * intl_strcasecmp_utf8_one_cp() - compares the first codepoints from two
 *				   strings case insensitive
 *   return:  0 if strings are equal, -1 if cp1 < cp2 , 1 if cp1 > cp2
 *   str1(in):
 *   str2(in):
 *   size_str1(in): size in bytes of str1
 *   size_str2(in): size in bytes of str2
 *   cp1(in): first codepoint in str1
 *   cp2(in): first codepoint in str2
 *   skip_size1(out):  bytes to skip from str1
 *   skip_size2(out):  bytes to skip from str2
 *   identifier_mode(in): true if compares identifiers, false otherwise
 *
 *  Note : skip_size1, skip_size2 are valid only when strings are equal
 *	   (returned value is zero).
 */
static int
intl_strcasecmp_utf8_one_cp (const ALPHABET_DATA * alphabet,
			     unsigned char *str1, unsigned char *str2,
			     const int size_str1, const int size_str2,
			     unsigned int cp1, unsigned int cp2,
			     int *skip_size1, int *skip_size2)
{
  int alpha_cnt;
  unsigned int l_array_1[INTL_CASING_EXPANSION_MULTIPLIER];
  unsigned int l_array_2[INTL_CASING_EXPANSION_MULTIPLIER];
  int skip_len1 = 1, skip_len2 = 1;
  int l_count_1 = 0, l_count_2 = 0, l_count = 0;
  int res;
  bool use_original_str1, use_original_str2;

  unsigned int *casing_arr;
  int casing_multiplier;

  assert (alphabet != NULL);
  assert (str1 != NULL);
  assert (str2 != NULL);
  assert (skip_size1 != NULL);
  assert (skip_size2 != NULL);

  if (cp1 == cp2)
    {
      (void) intl_char_size (str1, 1, skip_size1);
      (void) intl_char_size (str2, 1, skip_size2);

      return 0;
    }

  alpha_cnt = alphabet->l_count;

  if (alphabet->lower_multiplier == 1 && alphabet->upper_multiplier == 1)
    {
      if (cp1 < (unsigned int) alpha_cnt)
	{
	  cp1 = alphabet->lower_cp[cp1];
	}

      if (cp2 < (unsigned int) alpha_cnt)
	{
	  cp2 = alphabet->lower_cp[cp2];
	}

      if (cp1 != cp2)
	{
	  return (cp1 < cp2) ? (-1) : 1;
	}

      (void) intl_char_size (str1, 1, skip_size1);
      (void) intl_char_size (str2, 1, skip_size2);

      return 0;
    }

  /*
   * Multipliers can be either 1 or 2, as imposed by the LDML parsing code.
   * Currently, alphabets with both multipliers equal to 2 are not supported
   * for case sensitive comparisons.
   */
  assert (alphabet->lower_multiplier == 1 || alphabet->upper_multiplier == 1);
  if (alphabet->lower_multiplier > alphabet->upper_multiplier)
    {
      casing_arr = alphabet->lower_cp;
      casing_multiplier = alphabet->lower_multiplier;
    }
  else
    {
      casing_arr = alphabet->upper_cp;
      casing_multiplier = alphabet->upper_multiplier;
    }

  use_original_str1 = true;
  if (cp1 < (unsigned int) alpha_cnt)
    {
      memcpy (l_array_1,
	      &(casing_arr[cp1 * casing_multiplier]),
	      casing_multiplier * sizeof (unsigned int));

      if (cp1 != l_array_1[0])
	{
	  l_count_1 = casing_multiplier;
	  while (l_count_1 > 1 && l_array_1[l_count_1 - 1] == 0)
	    {
	      l_count_1--;
	    }

	  use_original_str1 = false;
	}
    }

  use_original_str2 = true;
  if (cp2 < (unsigned int) alpha_cnt)
    {
      memcpy (l_array_2,
	      &(casing_arr[cp2 * casing_multiplier]),
	      casing_multiplier * sizeof (unsigned int));

      if (cp2 != l_array_2[0])
	{
	  l_count_2 = casing_multiplier;
	  while (l_count_2 > 1 && l_array_2[l_count_2 - 1] == 0)
	    {
	      l_count_2--;
	    }

	  use_original_str2 = false;
	}
    }

  if (use_original_str1)
    {
      (void) intl_utf8_to_cp_list (str1, size_str1, l_array_1,
				   casing_multiplier, &l_count_1);
    }

  if (use_original_str2)
    {
      (void) intl_utf8_to_cp_list (str2, size_str2, l_array_2,
				   casing_multiplier, &l_count_2);
    }

  l_count = MIN (l_count_1, l_count_2);

  if (use_original_str1)
    {
      l_count_1 = MIN (l_count, l_count_1);
      skip_len1 = l_count_1;
    }
  else
    {
      skip_len1 = 1;
    }

  if (use_original_str2)
    {
      l_count_2 = MIN (l_count, l_count_2);
      skip_len2 = l_count_2;
    }
  else
    {
      skip_len2 = 1;
    }

  if (l_count_1 != l_count_2)
    {
      return (l_count_1 < l_count_2) ? (-1) : (1);
    }

  assert (l_count_1 == l_count_2);

  /* compare lower codepoints */
  res = memcmp (l_array_1, l_array_2, l_count * sizeof (unsigned int));
  if (res != 0)
    {
      return res;
    }

  /* convert supplementary characters in bytes size to skip */
  (void) intl_char_size (str1, skip_len1, skip_size1);
  (void) intl_char_size (str2, skip_len2, skip_size2);

  return 0;
}

/*
 * intl_identifier_casecmp() - compares two identifiers strings
 *			       case insensitive
 *   return: 0 if strings are equal, -1 if str1 < str2 , 1 if str1 > str2
 *   str1(in):
 *   str2(in):
 *
 * NOTE: identifier comparison is special, see intl_identifier_casecmp_w_size
 *	 for details on comparing identifiers of different length.
 */
int
intl_identifier_casecmp (const char *str1, const char *str2)
{
  int str1_size;
  int str2_size;

  assert (str1 != NULL);
  assert (str2 != NULL);

  str1_size = strlen (str1);
  str2_size = strlen (str2);

  if (str1_size != str2_size)
    {
      return (str1_size < str2_size) ? -1 : 1;
    }

  return strncasecmp (str1, str2, str1_size);
}

/*
 * intl_identifier_cmp() - compares two identifiers strings
 *			   case sensitive
 *   return:
 *   str1(in):
 *   str2(in):
 *
 */
int
intl_identifier_cmp (const char *str1, const char *str2)
{
  /* when comparing identifiers, order of current collation is not important */
  return strcmp (str1, str2);
}

/*
 * intl_identifier_lower() - convert given characters to lowercase characters
 *   return: always 0
 *   src(in) : source buffer
 *   dst(out) : destination buffer
 *
 *  Note : 'dst' has always enough size
 */
int
intl_identifier_lower (const char *src, char *dst)
{
  int length_in_bytes = 0;
  unsigned char *d, *s;

  if (src)
    {
      length_in_bytes = strlen (src);
    }

  for (d = (unsigned char *) dst, s = (unsigned char *) src;
       d < (unsigned char *) dst + length_in_bytes; d++, s++)
    {
      *d = char_tolower (*s);
    }

  *d = '\0';

  return 0;
}

/*
 * intl_identifier_upper() - convert given characters to uppercase characters
 *   return: always 0
 *   src(in):
 *   dst(out):
 *
 *  Note : 'dst' has always enough size;
 */
int
intl_identifier_upper (const char *src, char *dst)
{
  int length_in_bytes = 0;
  unsigned char *d, *s;

  if (src)
    {
      length_in_bytes = strlen (src);
    }

  for (d = (unsigned char *) dst, s = (unsigned char *) src;
       d < (unsigned char *) dst + length_in_bytes; d++, s++)
    {
      *d = char_toupper (*s);
    }

  *d = '\0';

  return 0;
}

/*
 * intl_identifier_fix - Checks if a string can be an identifier;
 *			 Truncates the string to a desired size in bytes,
 *			 while making sure that the last char is not truncated
 *			 Checks that lower and upper case versions of string
 *			 do not exceed maximum allowed size.
 *
 *   return: error code : ER_GENERIC_ERROR or NO_ERROR
 *   name(in): identifier name, nul-terminated C string
 *   ident_max_size(in): allowed size of this identifier, may be -1 in which
 *			 case the maximum allowed system size is used
 *   error_on_case_overflow(in): if true, will return error if the lower or
 *				 upper version of truncated identifier exceeds
 *				 allowed size
 *
 *  Note : Identifier string may be truncated if lexer previously truncated it
 *	   in the middle of the last character;
 *	   No error message is outputed by this function - in case of error,
 *	   the error message should be output by the caller.
 *	   DB_MAX_IDENTIFIER_LENGTH is the buffer size for string identifier
 *	   This includes the nul-terminator byte; the useful bytes are
 *	   (DB_MAX_IDENTIFIER_LENGTH - 1).
 */
int
intl_identifier_fix (char *name, int ident_max_size)
{
  int length_bytes;

  assert (name != NULL);

  if (ident_max_size == -1)
    {
      ident_max_size = DB_MAX_IDENTIFIER_LENGTH - 1;
    }

  assert (ident_max_size > 0 && ident_max_size < DB_MAX_IDENTIFIER_LENGTH);

  length_bytes = strlen (name);
  if (length_bytes > ident_max_size)
    {
      name[ident_max_size] = '\0';
    }

  return NO_ERROR;
}

/*
 * intl_put_char() - puts a character into a string buffer
 *   return: size of character
 *   dest(in/out): destination buffer
 *   char_p(in): pointer to character
 *
 *  Note : It is assumed that 'dest' buffer can fit the character.
 *
 */
int
intl_put_char (unsigned char *dest, const unsigned char *char_p)
{
  int char_len;

  assert (char_p != NULL);

  if (*char_p < 0x80)
    {
      *dest = *char_p;
      return 1;
    }
  else
    {
      char_len = intl_Len_utf8_char[*char_p];
      memcpy (dest, char_p, char_len);
      return char_len;
    }
}


/*
 * intl_is_space() - checks if character is white-space
 *   return:
 *   str(in):
 *   str_end(in): end of string (pointer to first character after last
 *		  character of string) or NULL if str is null terminated
 *   space_size(out): size in bytes of 'whitespace' character
 *
 *  Note : White spaces are: ASCII space, TAB character, CR and LF
 *	   If codeset is EUC also the double byte character space (A1 A1) is
 *	   considered;
 *
 */
bool
intl_is_space (const char *str, const char *str_end, int *space_size)
{
  assert (str != NULL);

  if (space_size != NULL)
    {
      *space_size = 1;
    }

  if (str_end == NULL)
    {
      if (char_isspace (*str))
	{
	  return true;
	}
    }
  else
    {
      if (str < str_end && char_isspace (*str))
	{
	  return true;
	}
    }

  return false;
}

/*
 * intl_skip_spaces() - skips white spaces in string
 *   return: begining of non-whitespace characters or end of string
 *   str(in):
 *   str_end(in): end of string (pointer to first character after last
 *		  character of string) or NULL if str is null terminated
 *
 *  Note : White spaces are: ASCII space, TAB character, CR and LF
 *	   If codeset is EUC also the double byte character space (A1 A1) is
 *	   considered;
 *
 */
const char *
intl_skip_spaces (const char *str, const char *str_end)
{
  assert (str != NULL);

  if (str_end == NULL)
    {
      while (char_isspace (*str))
	{
	  str++;
	}
    }
  else
    {
      while (str < str_end && char_isspace (*str))
	{
	  str++;
	}
    }

  return str;
}

/*
 * intl_backskip_spaces() - skips trailing white spaces in end of string
 *   return: end of non-whitespace characters or end of string
 *   str_begin(in): start of string
 *   str_end(in): end of string (pointer to last character)
 *
 *  Note : White spaces are: ASCII space, TAB character, CR and LF
 *	   If codeset is EUC also the double byte character space (A1 A1) is
 *	   considered;
 *
 */
const char *
intl_backskip_spaces (const char *str_begin, const char *str_end)
{
  assert (str_begin != NULL);
  assert (str_end != NULL);

  while (str_end > str_begin && char_isspace (*str_end))
    {
      str_end++;
    }

  return str_end;
}

/*
 * intl_cp_to_utf8() - converts a unicode codepoint to its
 *                            UTF-8 encoding
 *  return: number of bytes for UTF-8; 0 means not encoded
 *  codepoint(in) : Unicode code point (32 bit value)
 *  utf8_seq(in/out) : pre-allocated buffer for UTF-8 sequence
 *
 */
int
intl_cp_to_utf8 (const unsigned int codepoint, unsigned char *utf8_seq)
{
  assert (utf8_seq != NULL);

  if (codepoint <= 0x7f)
    {
      /* 1 byte */
      *utf8_seq = (unsigned char) codepoint;
      return 1;
    }
  if (codepoint <= 0x7ff)
    {
      /* 2 bytes */
      *utf8_seq++ = (unsigned char) (0xc0 | (codepoint >> 6));
      *utf8_seq = (unsigned char) (0x80 | (codepoint & 0x3f));
      return 2;
    }
  if (codepoint <= 0xffff)
    {
      /* 3 bytes */
      *utf8_seq++ = (unsigned char) (0xe0 | (codepoint >> 12));
      *utf8_seq++ = (unsigned char) (0x80 | ((codepoint >> 6) & 0x3f));
      *utf8_seq = (unsigned char) (0x80 | (codepoint & 0x3f));
      return 3;
    }
  if (codepoint <= 0x10ffff)
    {
      /* 4 bytes */
      *utf8_seq++ = (unsigned char) (0xf0 | (codepoint >> 18));
      *utf8_seq++ = (unsigned char) (0x80 | ((codepoint >> 12) & 0x3f));
      *utf8_seq++ = (unsigned char) (0x80 | ((codepoint >> 6) & 0x3f));
      *utf8_seq = (unsigned char) (0x80 | (codepoint & 0x3f));
      return 4;
    }

  assert (false);
  *utf8_seq = '?';
  return 1;
}

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_cp_to_dbcs() - converts a codepoint to DBCS encoding
 *  return: number of bytes for encoding; 0 means not encoded
 *  codepoint(in) : code point (16 bit value)
 *  byte_flag(in): flag array : 0: single byte char,
 *				1: is a leading byte for DBCS,
 *				2: byte value not used
 *  seq(in/out) : pre-allocated buffer for DBCS sequence
 *
 */
int
intl_cp_to_dbcs (const unsigned int codepoint,
		 const unsigned char *byte_flag, unsigned char *seq)
{
  assert (seq != NULL);

  /* is_lead_byte is assumed to have 256 elements */
  assert (byte_flag != NULL);

  if (codepoint <= 0xff)
    {
      if (byte_flag[codepoint] == 0)
	{
	  /* 1 byte */
	  *seq = (unsigned char) codepoint;
	}
      else
	{
	  /* undefined or lead byte */
	  *seq = '?';
	}
      return 1;
    }
  if (codepoint <= 0xffff)
    {
      /* 2 bytes */
      *seq++ = (unsigned char) (0xff & (codepoint >> 8));
      *seq = (unsigned char) (codepoint & 0xff);
      return 2;
    }

  assert (false);
  *seq = '?';
  return 1;
}
#endif

/*
 * intl_utf8_to_cp() - converts a UTF-8 encoded char to unicode codepoint
 *  return: unicode code point; 0xffffffff means error
 *  utf8(in) : buffer for UTF-8 char
 *  size(in) : size of buffer
 *  next_char(in/out): pointer to next character
 *
 */
unsigned int
intl_utf8_to_cp (const unsigned char *utf8, const int size,
		 unsigned char **next_char)
{
  assert (utf8 != NULL);
  assert (size > 0);
  assert (next_char != NULL);

  if (utf8[0] < 0x80)
    {
      *next_char = (unsigned char *) utf8 + 1;
      return (unsigned int) (utf8[0]);
    }
  else if (size >= 2 && utf8[0] >= 0xc0 && utf8[0] < 0xe0)
    {
      *next_char = (unsigned char *) utf8 + 2;
      return (unsigned int) (((utf8[0] & 0x1f) << 6) | (utf8[1] & 0x3f));
    }
  else if (size >= 3 && utf8[0] >= 0xe0 && utf8[0] < 0xf0)
    {
      *next_char = (unsigned char *) utf8 + 3;
      return (unsigned int) (((utf8[0] & 0x0f) << 12) |
			     ((utf8[1] & 0x3f) << 6) | (utf8[2] & 0x3f));
    }
  else if (size >= 4 && utf8[0] >= 0xf0 && utf8[0] < 0xf8)
    {
      *next_char = (unsigned char *) utf8 + 4;
      return (unsigned int) (((utf8[0] & 0x07) << 18)
			     | ((utf8[1] & 0x3f) << 12)
			     | ((utf8[2] & 0x3f) << 6) | (utf8[3] & 0x3f));
    }
#if INTL_UTF8_MAX_CHAR_SIZE > 4
  else if (size >= 5 && utf8[0] >= 0xf8 && utf8[0] < 0xfc)
    {
      *next_char = (unsigned char *) utf8 + 5;
      return (unsigned int) (((utf8[0] & 0x03) << 24)
			     | ((utf8[1] & 0x3f) << 18)
			     | ((utf8[2] & 0x3f) << 12)
			     | ((utf8[3] & 0x3f) << 6) | (utf8[4] & 0x3f));
    }
  else if (size >= 6 && utf8[0] >= 0xfc && utf8[0] < 0xfe)
    {
      *next_char = (unsigned char *) utf8 + 6;
      return (unsigned int) (((utf8[0] & 0x01) << 30)
			     | ((utf8[1] & 0x3f) << 24)
			     | ((utf8[2] & 0x3f) << 18)
			     | ((utf8[3] & 0x3f) << 12)
			     | ((utf8[4] & 0x3f) << 6) | (utf8[5] & 0x3f));
    }
#endif

  *next_char = (unsigned char *) utf8 + 1;
  return 0xffffffff;
}

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_back_utf8_to_cp() - converts a UTF-8 encoded char to unicode codepoint
 *			    but starting from the last byte of a character
 *  return: unicode code point; 0xffffffff means error
 *
 *  utf8_start(in) : start of buffer
 *  utf8_last(in) : pointer to last byte of buffer (and last byte of last
 *		    character)
 *  last_byte__prev_char(in/out) : pointer to last byte of previous character
 *
 */
unsigned int
intl_back_utf8_to_cp (const unsigned char *utf8_start,
		      const unsigned char *utf8_last,
		      unsigned char **last_byte__prev_char)
{
  int char_size = 1;
  unsigned char *dummy;

  assert (utf8_start != NULL);
  assert (utf8_last != NULL);
  assert (utf8_start <= utf8_last);
  assert (last_byte__prev_char != NULL);

  if (*utf8_last < 0x80)
    {
      *last_byte__prev_char = ((unsigned char *) utf8_last) - 1;
      return *utf8_last;
    }

  /* multibyte character */
  do
    {
      if (((*utf8_last--) & 0xc0) != 0x80)
	{
	  break;
	}
      if (utf8_last < utf8_start)
	{
	  /* broken char, invalid CP */
	  *last_byte__prev_char = ((unsigned char *) utf8_start) - 1;
	  return 0xffffffff;
	}
    }
  while (++char_size < INTL_UTF8_MAX_CHAR_SIZE);

  *last_byte__prev_char = (unsigned char *) utf8_last;
  return intl_utf8_to_cp (utf8_last + 1, char_size, &dummy);
}
#endif

/*
 * intl_dbcs_to_cp() - converts a DBCS encoded char to DBCS codepoint
 *  return: DBCS code point; 0xffffffff means error
 *  seq(in) : buffer for DBCS char
 *  size(in) : size of buffer
 *  byte_flag(in) : array of flags for lead bytes
 *  next_char(in/out): pointer to next character
 *
 */
unsigned int
intl_dbcs_to_cp (const unsigned char *seq, const int size,
		 const unsigned char *byte_flag, unsigned char **next_char)
{
  assert (seq != NULL);
  assert (size > 0);
  assert (next_char != NULL);

  assert (byte_flag != NULL);

  if (byte_flag[seq[0]] == 1 && size >= 2)
    {
      *next_char = (unsigned char *) seq + 2;
      return (unsigned int) (((seq[0]) << 8) | (seq[1]));
    }

  *next_char = (unsigned char *) seq + 1;
  return (unsigned int) (seq[0]);
}


/*
 * intl_utf8_to_cp_list() - converts a UTF-8 encoded string to a list of
 *                          unicode codepoint
 *  return: number of codepoints found in string
 *  utf8(in) : buffer for UTF-8 char
 *  size(in) : size of string buffer
 *  cp_array(in/out) : preallocated array to store computed codepoints list
 *  max_array_size(in) : maximum size of computed codepoints list
 *  cp_count(out) : number of codepoints found in string
 *  array_count(out) : number of elements in codepoints list
 */
int
intl_utf8_to_cp_list (const unsigned char *utf8, const int size,
		      unsigned int *cp_array, const int max_array_size,
		      int *array_count)
{
  unsigned char *next = NULL;
  const unsigned char *utf8_end = utf8 + size;
  int i;

  assert (utf8 != NULL);
  assert (size > 0);
  assert (cp_array != NULL);
  assert (max_array_size > 0);
  assert (array_count != NULL);

  for (i = 0, *array_count = 0; utf8 < utf8_end; i++)
    {
      unsigned int cp;
      assert (utf8_end - utf8 > 0);

      cp = intl_utf8_to_cp (utf8, utf8_end - utf8, &next);
      utf8 = next;

      if (i < max_array_size)
	{
	  cp_array[i] = cp;
	  (*array_count)++;
	}
    }

  return i;
}

#define UTF8_BYTE_IN_RANGE(b, r1, r2) (!(b < r1 || b > r2))
#define UTF8_RETURN_INVALID_BYTE(p, pos) \
  do { \
    if ((char **)pos != NULL) { \
	* ((char **)pos) = (char *) p; \
    } \
    return 1; \
  } while (0)

#define UTF8_RETURN_CHAR_TRUNCATED(p, pos) \
  do { \
    if ((char **)pos != NULL) { \
	* ((char **)pos) = (char *) p; \
    } \
    return 2; \
  } while (0)
/*
 * intl_check_utf8 - Checks if a string contains valid UTF-8 sequences
 *
 *   return: 0 if valid,
 *	     1 if contains and invalid byte in one char
 *	     2 if last char is truncated (missing bytes)
 *   buf(in): buffer
 *   size(out): size of buffer (negative values accepted, in this case buffer
 *		is assumed to be NUL terminated)
 *   pos(out): pointer to begining of invalid character
 *
 *  Valid ranges:
 *    - 1 byte : 00 - 7F
 *    - 2 bytes: C2 - DF , 80 - BF		       (U +80 .. U+7FF)
 *    - 3 bytes: E0	 , A0 - BF , 80 - BF	       (U +800 .. U+FFF)
 *		 E1 - EC , 80 - BF , 80 - BF	       (U +1000 .. +CFFF)
 *		 ED	 , 80 - 9F , 80 - BF	       (U +D000 .. +D7FF)
 *		 EE - EF , 80 - BF , 80 - BF	       (U +E000 .. +FFFF)
 *    - 4 bytes: F0	 , 90 - BF , 80 - BF , 80 - BF (U +10000 .. +3FFFF)
 *		 F1 - F3 , 80 - BF , 80 - BF , 80 - BF (U +40000 .. +FFFFF)
 *		 F4	 , 80 - 8F , 80 - BF , 80 - BF (U +100000 .. +10FFFF)
 *
 *  Note:
 *  This function should be used only when the UTF-8 string enters the Rye
 *  system.
 */
bool
intl_check_utf8 (const unsigned char *buf, int size, char **pos)
{
  const unsigned char *p = buf;
  const unsigned char *p_end = NULL;
  const unsigned char *curr_char = NULL;

  if (pos != NULL)
    {
      *pos = NULL;
    }

  if (size < 0)
    {
      size = strlen ((char *) buf);
    }

  p_end = buf + size;

  while (p < p_end)
    {
      curr_char = p;

      if (*p < 0x80)
	{
	  p++;
	  continue;
	}

      /* range 80 - BF is not valid UTF-8 first byte */
      /* range C0 - C1 overlaps 1 byte 00 - 20 (2 byte overlongs) */
      if (*p < 0xc2)
	{
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}

      /* check 2 bytes sequences */
      /* 2 bytes sequence allowed : C2 - DF , 80 - BF */
      if (UTF8_BYTE_IN_RANGE (*p, 0xc2, 0xdf))
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
	    {
	      p++;
	      continue;
	    }
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}

      /* check 3 bytes sequences */
      /* 3 bytes sequence : E0   , A0 - BF , 80 - BF */
      if (*p == 0xe0)
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0xa0, 0xbf))
	    {
	      p++;
	      if (p >= p_end)
		{
		  UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		}

	      if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		{
		  p++;
		  continue;
		}
	    }

	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}
      /* 3 bytes sequence : E1 - EC , 80 - BF , 80 - BF */
      /* 3 bytes sequence : EE - EF , 80 - BF , 80 - BF */
      else if (UTF8_BYTE_IN_RANGE (*p, 0xe1, 0xec) ||
	       UTF8_BYTE_IN_RANGE (*p, 0xee, 0xef))
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
	    {
	      p++;
	      if (p >= p_end)
		{
		  UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		}

	      if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		{
		  p++;
		  continue;
		}
	    }
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}
      /* 3 bytes sequence : ED   , 80 - 9F , 80 - BF */
      else if (*p == 0xed)
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0x9f))
	    {
	      p++;
	      if (p >= p_end)
		{
		  UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		}

	      if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		{
		  p++;
		  continue;
		}
	    }
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}

      /* 4 bytes sequence : F0   , 90 - BF , 80 - BF , 80 - BF */
      if (*p == 0xf0)
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0x90, 0xbf))
	    {
	      p++;
	      if (p >= p_end)
		{
		  UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		}

	      if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		{
		  p++;
		  if (p >= p_end)
		    {
		      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		    }

		  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		    {
		      p++;
		      continue;
		    }
		}
	    }
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}
      /* 4 bytes sequence : F1 - F3 , 80 - BF , 80 - BF , 80 - BF */
      if (UTF8_BYTE_IN_RANGE (*p, 0xf1, 0xf3))
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
	    {
	      p++;
	      if (p >= p_end)
		{
		  UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		}

	      if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		{
		  p++;
		  if (p >= p_end)
		    {
		      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		    }

		  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		    {
		      p++;
		      continue;
		    }
		}
	    }
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}
      /* 4 bytes sequence : F4 , 80 - 8F , 80 - BF , 80 - BF */
      else if (*p == 0xf4)
	{
	  p++;
	  if (p >= p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }

	  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0x8f))
	    {
	      p++;
	      if (p >= p_end)
		{
		  UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		}

	      if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		{
		  p++;
		  if (p >= p_end)
		    {
		      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
		    }

		  if (UTF8_BYTE_IN_RANGE (*p, 0x80, 0xbf))
		    {
		      p++;
		      continue;
		    }
		}
	    }
	  UTF8_RETURN_INVALID_BYTE (curr_char, pos);
	}

      assert (*p > 0xf4);
      UTF8_RETURN_INVALID_BYTE (curr_char, pos);
    }

  return 0;
}

/*
 * intl_check_euckr - Checks if a string contains valid EUC-KR sequences
 *
 *
 *   return: 0 if valid,
 *	     1 if contains and invalid byte in one char
 *	     2 if last char is truncated (missing bytes)
 *   buf(in): buffer
 *   size(out): size of buffer (negative values accepted, in this case buffer
 *		is assumed to be NUL terminated)
 *   pos(out): pointer to begining of invalid character
 *
 *  Valid ranges:
 *    - 1 byte : 00 - 8E ; 90 - A0
 *    - 2 bytes: A1 - FE , 00 - FF
 *    - 3 bytes: 8F	 , 00 - FF , 00 - FF
 */
bool
intl_check_euckr (const unsigned char *buf, int size, char **pos)
{
  const unsigned char *p = buf;
  const unsigned char *p_end = NULL;
  const unsigned char *curr_char = NULL;

  if (pos != NULL)
    {
      *pos = NULL;
    }

  if (size < 0)
    {
      size = strlen ((char *) buf);
    }

  p_end = buf + size;

  while (p < p_end)
    {
      curr_char = p;

      if (*p < 0x80)
	{
	  p++;
	  continue;
	}

      /* SS3 byte value starts a 3 bytes character */
      if (*p == SS3)
	{
	  p++;
	  p++;
	  p++;
	  if (p > p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }
	  continue;
	}

      /* check 2 bytes sequences */
      if (UTF8_BYTE_IN_RANGE (*p, 0xa1, 0xfe))
	{
	  p++;
	  p++;
	  if (p > p_end)
	    {
	      UTF8_RETURN_CHAR_TRUNCATED (curr_char, pos);
	    }
	  continue;
	}

      UTF8_RETURN_INVALID_BYTE (curr_char, pos);
    }

  return 0;
}

/*
 * intl_is_bom_magic - Returns 1 if the buffer contains BOM magic for UTF-8
 *
 *   return: true if BOM, false otherwise
 *   buf(in): buffer
 *   size(out): size of buffer (negative means buffer is NUL terminated)
 */
bool
intl_is_bom_magic (const char *buf, const int size)
{
  const char BOM[] = {
    0xef, 0xbb, 0xbf
  };

  if (buf != NULL && size >= 3)
    {
      if (buf[0] == BOM[0] && buf[1] == BOM[1] && buf[2] == BOM[2])
	{
	  return true;
	}
    }

  return false;
}

/* UTF-8 to console routines */

/*
 * intl_text_single_byte_to_utf8() - converts a buffer containing text with ISO
 *				     8859-X encoding to UTF-8
 *
 *   return: error code
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(int/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(out): size of string (NUL terminator not included)
 */
int
intl_text_single_byte_to_utf8 (const char *in_buf, const int in_size,
			       char **out_buf, int *out_size)
{
  return intl_text_single_byte_to_utf8_ext (lang_get_txt_conv (), in_buf,
					    in_size, out_buf, out_size);
}

/*
 * intl_text_single_byte_to_utf8_ext() - converts a buffer containing text
 *					 with ISO 8859-X encoding to UTF-8
 *
 *   return: error code
 *   t(in): text conversion data
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(in/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(in/out): size of string (NUL terminator not included)
 */
int
intl_text_single_byte_to_utf8_ext (void *t,
				   const char *in_buf, const int in_size,
				   char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  unsigned char *p_out = NULL;
  TEXT_CONVERSION *txt_conv;
  bool is_ascii = true;

  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);
  assert (t != NULL);

  txt_conv = (TEXT_CONVERSION *) t;

  p_in = (const unsigned char *) in_buf;
  while (p_in < (const unsigned char *) in_buf + in_size)
    {
      if (*p_in++ >= 0x80)
	{
	  is_ascii = false;
	  break;
	}
    }

  if (is_ascii)
    {
      *out_buf = NULL;
      return NO_ERROR;
    }

  if (*out_buf == NULL)
    {
      /* a ISO8859-X character is encoded on maximum 2 bytes in UTF-8 */
      *out_buf = malloc (in_size * 2 + 1);
      if (*out_buf == NULL)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_OUT_OF_VIRTUAL_MEMORY,
		  1, in_size * 2 + 1);
	  return ER_OUT_OF_VIRTUAL_MEMORY;
	}
    }
  else
    {
      if (*out_size < in_size * 2 + 1)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_GENERIC_ERROR, 1, "");
	  return ER_GENERIC_ERROR;
	}
    }

  assert (txt_conv->text_last_cp > 0);
  for (p_in = (const unsigned char *) in_buf,
       p_out = (unsigned char *) *out_buf;
       p_in < (const unsigned char *) in_buf + in_size; p_in++)
    {
      if (*p_in >= txt_conv->text_first_cp && *p_in <= txt_conv->text_last_cp)
	{
	  unsigned char *utf8_bytes =
	    txt_conv->text_to_utf8[*p_in - txt_conv->text_first_cp].bytes;
	  int utf8_size =
	    txt_conv->text_to_utf8[*p_in - txt_conv->text_first_cp].size;

	  do
	    {
	      *p_out++ = *utf8_bytes++;
	    }
	  while (--utf8_size > 0);
	}
      else
	{
	  if (*p_in < 0x80)
	    {
	      *p_out++ = *p_in;
	    }
	  else
	    {
	      assert (false);
	      *p_out++ = '?';
	    }
	}
    }

  *(p_out) = '\0';
  *out_size = p_out - (unsigned char *) *(out_buf);

  return NO_ERROR;
}

/*
 * intl_text_utf8_to_single_byte() - converts a buffer containing UTF-8 text
 *				     to ISO 8859-X encoding
 *
 *   return: error code
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(in/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(in/out): size of output string (NUL terminator not counted)
 */
int
intl_text_utf8_to_single_byte (const char *in_buf, const int in_size,
			       char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  unsigned char *p_out = NULL;
  unsigned char *p_next = NULL;
  TEXT_CONVERSION *txt_conv = lang_get_txt_conv ();
  bool is_ascii = true;

  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);
  assert (txt_conv != NULL);

  p_in = (const unsigned char *) in_buf;
  while (p_in < (const unsigned char *) in_buf + in_size)
    {
      if (*p_in++ >= 0x80)
	{
	  is_ascii = false;
	  break;
	}
    }

  if (is_ascii)
    {
      *out_buf = NULL;
      return NO_ERROR;
    }

  if (*out_buf == NULL)
    {
      *out_buf = malloc (in_size + 1);
      if (*out_buf == NULL)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_OUT_OF_VIRTUAL_MEMORY,
		  1, in_size + 1);
	  return ER_OUT_OF_VIRTUAL_MEMORY;
	}
    }
  else
    {
      if (*out_size < in_size + 1)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_GENERIC_ERROR, 1, "");
	  return ER_GENERIC_ERROR;
	}
    }

  for (p_in = (const unsigned char *) in_buf,
       p_out = (unsigned char *) *out_buf;
       p_in < (const unsigned char *) in_buf + in_size;)
    {
      unsigned int cp = 0;

      if (*p_in < 0x80)
	{
	  *p_out++ = *p_in++;
	  continue;
	}

      cp = intl_utf8_to_cp (p_in, in_buf + in_size - (char *) p_in, &p_next);
      if (cp >= txt_conv->utf8_first_cp && cp <= txt_conv->utf8_last_cp)
	{
	  assert (txt_conv->utf8_to_text[cp - txt_conv->utf8_first_cp].size
		  == 1);
	  cp = (unsigned int) *(txt_conv->utf8_to_text
				[cp - txt_conv->utf8_first_cp].bytes);
	}

      if (cp > 0xff)
	{
	  *p_out++ = '?';
	}
      else
	{
	  *p_out++ = (unsigned char) cp;
	}
      p_in = p_next;
    }

  *(p_out) = '\0';
  *out_size = p_out - (unsigned char *) *(out_buf);

  return NO_ERROR;
}

/*
 * intl_init_conv_iso8859_1_to_utf8() - initializes conversion map from
 *				        ISO 8859-1 (Latin 1) to UTF-8
 *  return:
 */
static void
intl_init_conv_iso8859_1_to_utf8 (void)
{
  unsigned int i;

  /* 00 - 7E : mapped to ASCII */
  for (i = 0; i <= 0x7e; i++)
    {
      iso8859_1_to_utf8_conv[i].size = 1;
      *((unsigned char *) (iso8859_1_to_utf8_conv[i].bytes)) =
	(unsigned char) i;
    }

  /* 7F - 9F : not mapped */
  for (i = 0x7f; i <= 0x9f; i++)
    {
      iso8859_1_to_utf8_conv[i].size = 1;
      *((unsigned char *) (iso8859_1_to_utf8_conv[i].bytes)) =
	(unsigned char) '?';
    }

  /* A0 - FF : mapped to Unicode codepoint with the same value */
  for (i = 0xa0; i <= 0xff; i++)
    {
      iso8859_1_to_utf8_conv[i].size =
	intl_cp_to_utf8 (i, iso8859_1_to_utf8_conv[i].bytes);
    }

  con_iso_8859_1_conv.text_first_cp = 0;
  con_iso_8859_1_conv.text_last_cp = 0xff;
  con_iso_8859_1_conv.text_to_utf8 = iso8859_1_to_utf8_conv;

  /* no specific mapping here : Unicode codepoints in range 00-FF map directly
   * onto ISO-8859-1 */
  con_iso_8859_1_conv.utf8_first_cp = 0;
  con_iso_8859_1_conv.utf8_last_cp = 0;
  con_iso_8859_1_conv.utf8_to_text = NULL;
}

/*
 * intl_init_conv_iso8859_9_to_utf8() - initializes conversion map from
 *				        ISO 8859-9 (turkish) to UTF-8
 *  return:
 *
 */
static void
intl_init_conv_iso8859_9_to_utf8 (void)
{
  unsigned int i;
  const unsigned int iso8859_9_special_mapping[][2] = {
    {
     0xd0, 0x11e},		/* capital G with breve */
    {
     0xdd, 0x130},		/* capital I with dot above */
    {
     0xde, 0x15e},		/* capital S with cedilla */
    {
     0xf0, 0x11f},		/* small g with breve */
    {
     0xfd, 0x131},		/* small i dotless */
    {
     0xfe, 0x15f}		/* small s with cedilla */
  };

  /* 00 - 7E : mapped to ASCII */
  for (i = 0; i <= 0x7e; i++)
    {
      iso8859_9_to_utf8_conv[i].size = 1;
      *((unsigned char *) (iso8859_9_to_utf8_conv[i].bytes)) =
	(unsigned char) i;
    }

  /* 7F - 9F : not mapped */
  for (i = 0x7f; i <= 0x9f; i++)
    {
      iso8859_9_to_utf8_conv[i].size = 1;
      *((unsigned char *) (iso8859_9_to_utf8_conv[i].bytes)) =
	(unsigned char) '?';
    }

  /* A0 - FF : mapped to Unicode codepoint with the same value */
  for (i = 0xa0; i <= 0xff; i++)
    {
      iso8859_9_to_utf8_conv[i].size =
	intl_cp_to_utf8 (i, iso8859_9_to_utf8_conv[i].bytes);
    }

  for (i = ISO_8859_9_FIRST_CP; i <= ISO_8859_9_LAST_CP; i++)
    {
      utf8_cp_to_iso_8859_9_conv[i - ISO_8859_9_FIRST_CP].size = 1;
      *(utf8_cp_to_iso_8859_9_conv[i - ISO_8859_9_FIRST_CP].bytes) = '?';
    }

  /* special mapping */
  for (i = 0; i < DIM (iso8859_9_special_mapping); i++)
    {
      unsigned int val8bit = iso8859_9_special_mapping[i][0];
      unsigned int cp = iso8859_9_special_mapping[i][1];

      iso8859_9_to_utf8_conv[val8bit].size =
	intl_cp_to_utf8 (cp, iso8859_9_to_utf8_conv[val8bit].bytes);

      *(utf8_cp_to_iso_8859_9_conv[cp - ISO_8859_9_FIRST_CP].bytes) = val8bit;

      assert (utf8_cp_to_iso_8859_9_conv[cp - ISO_8859_9_FIRST_CP].size == 1);
    }

  con_iso_8859_9_conv.text_first_cp = 0;
  con_iso_8859_9_conv.text_last_cp = 0xff;
  con_iso_8859_9_conv.text_to_utf8 = iso8859_9_to_utf8_conv;

  con_iso_8859_9_conv.utf8_first_cp = ISO_8859_9_FIRST_CP;
  con_iso_8859_9_conv.utf8_last_cp = ISO_8859_9_LAST_CP;
  con_iso_8859_9_conv.utf8_to_text = utf8_cp_to_iso_8859_9_conv;
}

#if defined (ENABLE_UNUSED_FUNCTION)
/*
 * intl_text_dbcs_to_utf8() - converts a buffer containing text with DBCS
 *			      encoding to UTF-8
 *
 *   return: error code
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(int/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(out): size of string (NUL terminator not included)
 */
int
intl_text_dbcs_to_utf8 (const char *in_buf, const int in_size,
			char **out_buf, int *out_size)
{
  return intl_text_dbcs_to_utf8_ext (lang_get_txt_conv (), in_buf, in_size,
				     out_buf, out_size);
}

/*
 * intl_text_dbcs_to_utf8_ext() - converts a buffer containing text with DBCS
 *				  encoding to UTF-8
 *
 *   return: error code
 *   t(in): text conversion data
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(in/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(in/out): size of string (NUL terminator not included)
 */
int
intl_text_dbcs_to_utf8_ext (void *t, const char *in_buf, const int in_size,
			    char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  unsigned char *p_out = NULL;
  TEXT_CONVERSION *txt_conv;
  bool is_ascii = true;

  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);
  assert (t != NULL);

  txt_conv = (TEXT_CONVERSION *) t;

  p_in = (const unsigned char *) in_buf;
  while (p_in < (const unsigned char *) in_buf + in_size)
    {
      if (*p_in++ >= 0x80)
	{
	  is_ascii = false;
	  break;
	}
    }

  if (is_ascii)
    {
      *out_buf = NULL;
      return NO_ERROR;
    }

  if (*out_buf == NULL)
    {
      /* a DBCS text may contain ASCII characters (encoded with 1 byte) which
       * may expand to maximum 2 bytes in UTF-8 and DBCS characters (2 bytes)
       * which may expand to maximum 3 bytes in UTF-8;
       * Also it may contain single byte characters which may expand to 3
       * bytes characters in UTF-8
       * Apply a safe expansion of 3 */
      *out_buf = malloc (in_size * 3 + 1);
      if (*out_buf == NULL)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_OUT_OF_VIRTUAL_MEMORY,
		  1, in_size * 3 + 1);
	  return ER_OUT_OF_VIRTUAL_MEMORY;
	}
    }
  else
    {
      if (*out_size < in_size * 3 + 1)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_GENERIC_ERROR, 1, "");
	  return ER_GENERIC_ERROR;
	}
    }

  assert (txt_conv->text_last_cp > 0);
  for (p_in = (const unsigned char *) in_buf,
       p_out = (unsigned char *) *out_buf;
       p_in < (const unsigned char *) in_buf + in_size;)
    {
      unsigned char *p_next;
      unsigned int text_cp = intl_dbcs_to_cp (p_in,
					      (const unsigned char *) in_buf +
					      in_size - p_in,
					      txt_conv->byte_flag, &p_next);

      if (text_cp >= txt_conv->text_first_cp
	  && text_cp <= txt_conv->text_last_cp)
	{
	  unsigned char *utf8_bytes =
	    txt_conv->text_to_utf8[text_cp - txt_conv->text_first_cp].bytes;
	  int utf8_size =
	    txt_conv->text_to_utf8[text_cp - txt_conv->text_first_cp].size;

	  do
	    {
	      *p_out++ = *utf8_bytes++;
	    }
	  while (--utf8_size > 0);
	}
      else
	{
	  if (text_cp < 0x80)
	    {
	      *p_out++ = *p_in;
	    }
	  else
	    {
	      *p_out++ = '?';
	    }
	}

      assert (p_next <= (const unsigned char *) in_buf + in_size);
      p_in = p_next;
    }

  *(p_out) = '\0';
  *out_size = p_out - (unsigned char *) *(out_buf);

  return NO_ERROR;
}

/*
 * intl_text_utf8_to_dbcs() - converts a buffer containing UTF-8 text
 *			      to DBCS encoding
 *
 *   return: error code
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(in/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(in/out): size of output string (NUL terminator not counted)
 */
int
intl_text_utf8_to_dbcs (const char *in_buf, const int in_size,
			char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  unsigned char *p_out = NULL;
  unsigned char *p_next = NULL;
  TEXT_CONVERSION *txt_conv = lang_get_txt_conv ();
  bool is_ascii = true;

  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);
  assert (txt_conv != NULL);

  p_in = (const unsigned char *) in_buf;
  while (p_in < (const unsigned char *) in_buf + in_size)
    {
      if (*p_in++ >= 0x80)
	{
	  is_ascii = false;
	  break;
	}
    }

  if (is_ascii)
    {
      *out_buf = NULL;
      return NO_ERROR;
    }

  if (*out_buf == NULL)
    {
      *out_buf = malloc (in_size + 1);
      if (*out_buf == NULL)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_OUT_OF_VIRTUAL_MEMORY,
		  1, in_size + 1);
	  return ER_OUT_OF_VIRTUAL_MEMORY;
	}
    }
  else
    {
      if (*out_size < in_size + 1)
	{
	  er_set (ER_ERROR_SEVERITY, ARG_FILE_LINE, ER_GENERIC_ERROR, 1, "");
	  return ER_GENERIC_ERROR;
	}
    }

  assert (txt_conv->utf8_last_cp > 0);

  for (p_in = (const unsigned char *) in_buf,
       p_out = (unsigned char *) *out_buf;
       p_in < (const unsigned char *) in_buf + in_size;)
    {
      unsigned int cp = 0;

      if (*p_in < 0x80)
	{
	  *p_out++ = *p_in++;
	  continue;
	}

      cp = intl_utf8_to_cp (p_in, in_buf + in_size - (char *) p_in, &p_next);
      if (cp >= txt_conv->utf8_first_cp && cp <= txt_conv->utf8_last_cp)
	{
	  unsigned char *text_bytes =
	    txt_conv->utf8_to_text[cp - txt_conv->utf8_first_cp].bytes;
	  int text_size =
	    txt_conv->utf8_to_text[cp - txt_conv->utf8_first_cp].size;

	  assert (text_size >= 1);
	  do
	    {
	      *p_out++ = *text_bytes++;
	    }
	  while (--text_size > 0);
	}
      else if (cp > 0x80)
	{
	  *p_out++ = '?';
	}
      else
	{
	  *p_out++ = (unsigned char) cp;
	}
      p_in = p_next;
    }

  *(p_out) = '\0';
  *out_size = p_out - (unsigned char *) *(out_buf);

  return NO_ERROR;
}

/*
 * intl_fast_iso88591_to_utf8() - converts a buffer containing text with ISO
 *				  8859-1 encoding to UTF-8
 *
 *   return: 0 conversion ok, 1 conversion done, but invalid characters where
 *	     found
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(int/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer; NULL if conversion
 *			is not required
 *   out_size(out): size of string (NUL terminator not included)
 */
int
intl_fast_iso88591_to_utf8 (const unsigned char *in_buf, const int in_size,
			    unsigned char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  const unsigned char *p_end;
  unsigned char *p_out = NULL;
  int status = 0;

  assert (in_size > 0);
  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);

  for (p_in = in_buf, p_end = p_in + in_size,
       p_out = (unsigned char *) *out_buf; p_in < p_end; p_in++)
    {
      if (*p_in < 0x7f)
	{
	  *p_out++ = *p_in;
	}
      else if (*p_in < 0xa0)
	{
	  /* ISO 8859-1 characters in this range are not valid */
	  *p_out++ = '?';
	  status = 1;
	}
      else
	{
	  *p_out++ = (unsigned char) (0xc0 | (*p_in >> 6));
	  *p_out++ = (unsigned char) (0x80 | (*p_in & 0x3f));
	}
    }

  *out_size = p_out - *(out_buf);

  return status;
}

/*
 * intl_euckr_to_utf8() - converts a buffer containing text with EUC-KR
 *			  + JISX0212 to UTF-8
 *
 *   return: 0 conversion ok, 1 conversion done, but invalid characters where
 *	     found
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(int/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer;
 *   out_size(out): size of string (NUL terminator not included)
 */
int
intl_euckr_to_utf8 (const unsigned char *in_buf, const int in_size,
		    unsigned char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  const unsigned char *p_end;
  unsigned char *p_out = NULL;
  unsigned int unicode_cp;
  int utf8_size;
  int status = 0;

  assert (in_size > 0);
  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);

  for (p_in = in_buf, p_end = p_in + in_size,
       p_out = (unsigned char *) *out_buf; p_in < p_end; p_in++)
    {
      if (*p_in < 0x80)
	{
	  *p_out++ = *p_in;
	}
      else if (*p_in >= 0xa1 && *p_in < 0xff && p_end - p_in >= 2)
	{
	  if (*(p_in + 1) >= 0xa1 && *(p_in + 1) < 0xff)
	    {
	      /* KSC5601 two-bytes character */
	      unsigned char ksc_buf[2];

	      ksc_buf[0] = *p_in - 0x80;
	      ksc_buf[1] = *(p_in + 1) - 0x80;

	      if (ksc5601_mbtowc (&unicode_cp, ksc_buf, 2) <= 0)
		{
		  *p_out++ = '?';
		  status = 1;
		}
	      else
		{
		  utf8_size = intl_cp_to_utf8 (unicode_cp, p_out);
		  p_out += utf8_size;
		}
	    }
	  else
	    {
	      *p_out++ = '?';
	      status = 1;
	    }

	  /* skip one additional byte */
	  p_in++;
	}
      else if (*p_in == 0x8f && p_end - p_in >= 3)
	{
	  if (*(p_in + 1) >= 0xa1 && *(p_in + 1) < 0xff
	      && *(p_in + 2) >= 0xa1 && *(p_in + 2) < 0xff)
	    {
	      /* JISX0212 three bytes character */
	      unsigned char jis_buf[2];

	      jis_buf[0] = *(p_in + 1) - 0x80;
	      jis_buf[1] = *(p_in + 2) - 0x80;

	      if (jisx0212_mbtowc (&unicode_cp, jis_buf, 2) <= 0)
		{
		  *p_out++ = '?';
		  status = 1;
		}
	      else
		{
		  utf8_size = intl_cp_to_utf8 (unicode_cp, p_out);
		  p_out += utf8_size;
		}
	    }
	  else
	    {
	      *p_out++ = '?';
	      status = 1;
	    }

	  /* skip two additional bytes */
	  p_in++;
	  p_in++;
	}
      else
	{
	  /* EUC-KR byte not valid */
	  *p_out++ = '?';
	  status = 1;
	}
    }

  *out_size = p_out - *(out_buf);

  return status;
}

/*
 * intl_utf8_to_euckr() - converts a buffer containing UTF8 text to EUC-KR
 *			  + JISX0212 encoding
 *
 *   return: 0 conversion ok, 1 conversion done, but invalid characters where
 *	     found
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(int/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer;
 *   out_size(out): size of string (NUL terminator not included)
 */
int
intl_utf8_to_euckr (const unsigned char *in_buf, const int in_size,
		    unsigned char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  const unsigned char *p_end;
  unsigned char *p_out = NULL;
  int status = 0;

  assert (in_size > 0);
  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);

  for (p_in = in_buf, p_end = p_in + in_size,
       p_out = (unsigned char *) *out_buf; p_in < p_end;)
    {
      if (*p_in < 0x80)
	{
	  *p_out++ = *p_in++;
	}
      else
	{
	  unsigned char euc_buf[2];
	  int euc_bytes;
	  unsigned int unicode_cp;
	  unsigned char *next_utf8;

	  unicode_cp = intl_utf8_to_cp (p_in, p_end - p_in, &next_utf8);
	  if (unicode_cp == 0xffffffff)
	    {
	      goto illegal_char;
	    }

	  /* try to convert to KSC5601 */
	  euc_bytes = ksc5601_wctomb (euc_buf, unicode_cp, next_utf8 - p_in);

	  assert (euc_bytes != 0);
	  if (euc_bytes == 2)
	    {
	      *p_out = euc_buf[0] + 0x80;
	      *(p_out + 1) = euc_buf[1] + 0x80;
	      p_out++;
	      p_out++;
	      p_in = next_utf8;
	      continue;
	    }

	  if (euc_bytes != RET_ILUNI)
	    {
	      goto illegal_char;
	    }
	  assert (euc_bytes == RET_ILUNI);
	  /* not found as KSC encoding, try as JISX0212 */
	  euc_bytes = jisx0212_wctomb (euc_buf, unicode_cp, next_utf8 - p_in);

	  assert (euc_bytes != 0);
	  if (euc_bytes == 2)
	    {
	      *p_out = 0x8f;
	      *(p_out + 1) = euc_buf[0] + 0x80;
	      *(p_out + 2) = euc_buf[1] + 0x80;
	      p_out += 3;
	      p_in = next_utf8;
	      continue;
	    }

	  /* illegal Unicode or impossible to convert to EUC */
	illegal_char:
	  p_in = next_utf8;
	  *p_out = '?';
	  p_out++;
	  status = 1;
	}
    }

  *out_size = p_out - *(out_buf);

  return status;
}

/*
 * intl_iso88591_to_euckr() - converts a buffer containing ISO88591 text to
 *			      EUC-KR encoding
 *
 *   return: 0 conversion ok, 1 conversion done, but invalid characters where
 *	     found
 *   in_buf(in): buffer
 *   in_size(in): size of input string (NUL terminator not included)
 *   out_buf(int/out) : output buffer : uses the pre-allocated buffer passed
 *			as input or a new allocated buffer;
 *   out_size(out): size of string (NUL terminator not included)
 */
int
intl_iso88591_to_euckr (const unsigned char *in_buf, const int in_size,
			unsigned char **out_buf, int *out_size)
{
  const unsigned char *p_in = NULL;
  const unsigned char *p_end;
  unsigned char *p_out = NULL;
  int status = 0;

  assert (in_size > 0);
  assert (in_buf != NULL);
  assert (out_buf != NULL);
  assert (out_size != NULL);

  for (p_in = in_buf, p_end = p_in + in_size,
       p_out = (unsigned char *) *out_buf; p_in < p_end; p_in++)
    {
      if (*p_in < 0x80)
	{
	  *p_out++ = *p_in;
	}
      else
	{
	  unsigned char euc_buf[2];
	  int euc_bytes;

	  if (*p_in < 0xa0)
	    {
	      *p_out = '?';
	      p_out++;
	      status = 1;
	      continue;
	    }

	  /* try to convert to KSC5601 */
	  euc_bytes = ksc5601_wctomb (euc_buf, *p_in, 2);

	  assert (euc_bytes != 0);
	  if (euc_bytes == 2)
	    {
	      *p_out = euc_buf[0] + 0x80;
	      *(p_out + 1) = euc_buf[1] + 0x80;
	      p_out++;
	      p_out++;
	      continue;
	    }

	  /* illegal ISO8859-1 or impossible to convert to KSC */
	  if (euc_bytes != RET_ILUNI)
	    {
	      goto illegal_char;
	    }
	  assert (euc_bytes == RET_ILUNI);

	  /* try to convert to JISX0212 */
	  euc_bytes = jisx0212_wctomb (euc_buf, *p_in, 2);

	  assert (euc_bytes != 0);
	  if (euc_bytes == 2)
	    {
	      *p_out = 0x8f;
	      *(p_out + 1) = euc_buf[0] + 0x80;
	      *(p_out + 2) = euc_buf[1] + 0x80;
	      p_out++;
	      p_out++;
	      p_out++;
	      continue;
	    }

	illegal_char:
	  *p_out = '?';
	  p_out++;
	  status = 1;
	}
    }

  *out_size = p_out - *(out_buf);

  return status;
}
#endif
