/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "sys/platform.h"
#include "idlib/math/Vector.h"
#include "idlib/Heap.h"
#include "framework/Common.h"
#include <limits.h>

#include "idlib/Str.h"

#undef sprintf
#undef vsprintf
#include <stdio.h>

// DG: idDynamicBlockAlloc isn't thread-safe and idStr is used both in the main thread
//     and the async thread! For some reason this seems to cause lots of problems on
//     newer Linux distros if dhewm3 is built with GCC9 or newer (see #391).
//     No idea why it apparently didn't cause that (noticeable) issues before..
#if 0 // !defined( ID_REDIRECT_NEWDELETE ) && !defined( MACOS_X )
	#define USE_STRING_DATA_ALLOCATOR
#endif

#ifdef USE_STRING_DATA_ALLOCATOR
static idDynamicBlockAlloc<char, 1<<18, 128>	stringDataAllocator;
#endif

idVec4	g_color_table[16] =
{
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
	idVec4(1.0f, 0.0f, 0.0f, 1.0f), // S_COLOR_RED
	idVec4(0.0f, 1.0f, 0.0f, 1.0f), // S_COLOR_GREEN
	idVec4(1.0f, 1.0f, 0.0f, 1.0f), // S_COLOR_YELLOW
	idVec4(0.0f, 0.0f, 1.0f, 1.0f), // S_COLOR_BLUE
	idVec4(0.0f, 1.0f, 1.0f, 1.0f), // S_COLOR_CYAN
	idVec4(1.0f, 0.0f, 1.0f, 1.0f), // S_COLOR_MAGENTA
	idVec4(1.0f, 1.0f, 1.0f, 1.0f), // S_COLOR_WHITE
	idVec4(0.5f, 0.5f, 0.5f, 1.0f), // S_COLOR_GRAY
	idVec4(0.0f, 0.0f, 0.0f, 1.0f), // S_COLOR_BLACK
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
	idVec4(0.0f, 0.0f, 0.0f, 1.0f),
};

const char *units[2][4] =
{
	{ "B", "KB", "MB", "GB" },
	{ "B/s", "KB/s", "MB/s", "GB/s" }
};

/*
============
idStr::ColorForIndex
============
*/
idVec4 & idStr::ColorForIndex( int i ) {
	return g_color_table[ i & 15 ];
}

/*
============
idStr::ReAllocate
============
*/
void idStr::ReAllocate( int amount, bool keepold ) {
	char	*newbuffer;
	int		newsize;
	int		mod;

	//assert( data );
	assert( amount > 0 );

	mod = amount % STR_ALLOC_GRAN;
	if ( !mod ) {
		newsize = amount;
	}
	else {
		newsize = amount + STR_ALLOC_GRAN - mod;
	}
	alloced = newsize;

#ifdef USE_STRING_DATA_ALLOCATOR
	newbuffer = stringDataAllocator.Alloc( alloced );
	if ( keepold && data ) {
		data[ len ] = '\0';
		strcpy( newbuffer, data );
	}

	if ( data && data != baseBuffer ) {
		stringDataAllocator.Free( data );
	}

	data = newbuffer;
#else
	if ( data && data != baseBuffer ) {
		data = (char *)realloc( data, newsize );
	} else {
		newbuffer = (char *)malloc( newsize );
		if ( data && keepold ) {
			memcpy( newbuffer, data, len );
			newbuffer[ len ] = '\0';
		} else {
			newbuffer[ 0 ] = '\0';
		}
		data = newbuffer;
	}
#endif
}

/*
============
idStr::FreeData
============
*/
void idStr::FreeData( void ) {
	if ( data && data != baseBuffer ) {
#ifdef USE_STRING_DATA_ALLOCATOR
		stringDataAllocator.Free( data );
#else
		free( data );
#endif
		data = baseBuffer;
	}
}

/*
============
idStr::operator=
============
*/
void idStr::operator=( const char *text ) {
	int l;
	int diff;
	int i;

	if ( !text ) {
		// safe behaviour if NULL
		EnsureAlloced( 1, false );
		data[ 0 ] = '\0';
		len = 0;
		return;
	}

	if ( text == data ) {
		return; // copying same thing
	}

	// check if we're aliasing
	if ( text >= data && text <= data + len ) {
		diff = text - data;

		assert( strlen( text ) < (unsigned)len );

		for ( i = 0; text[ i ]; i++ ) {
			data[ i ] = text[ i ];
		}

		data[ i ] = '\0';

		len -= diff;

		return;
	}

	l = strlen( text );
	EnsureAlloced( l + 1, false );
	strcpy( data, text );
	len = l;
}

/*
============
idStr::FindChar

returns -1 if not found otherwise the index of the char
============
*/
int idStr::FindChar( const char *str, const char c, int start, int end ) {
	int i;

	if ( end == -1 ) {
		end = strlen( str ) - 1;
	}
	for ( i = start; i <= end; i++ ) {
		if ( str[i] == c ) {
			return i;
		}
	}
	return -1;
}

/*
============
idStr::FindText

returns -1 if not found otherwise the index of the text
============
*/
int idStr::FindText( const char *str, const char *text, bool casesensitive, int start, int end ) {
	int l, i, j;

	if ( end == -1 ) {
		end = strlen( str );
	}
	l = end - strlen( text );
	for ( i = start; i <= l; i++ ) {
		if ( casesensitive ) {
			for ( j = 0; text[j]; j++ ) {
				if ( str[i+j] != text[j] ) {
					break;
				}
			}
		} else {
			for ( j = 0; text[j]; j++ ) {
				if ( ::toupper( str[i+j] ) != ::toupper( text[j] ) ) {
					break;
				}
			}
		}
		if ( !text[j] ) {
			return i;
		}
	}
	return -1;
}

/*
============
idStr::Filter

Returns true if the string conforms the given filter.
Several metacharacter may be used in the filter.

*          match any string of zero or more characters
?          match any single character
[abc...]   match any of the enclosed characters; a hyphen can
		   be used to specify a range (e.g. a-z, A-Z, 0-9)

============
*/
bool idStr::Filter( const char *filter, const char *name, bool casesensitive ) {
	idStr buf;
	int i, found, index;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			buf.Empty();
			for (i = 0; *filter; i++) {
				if ( *filter == '*' || *filter == '?' || (*filter == '[' && *(filter+1) != '[') ) {
					break;
				}
				buf += *filter;
				if ( *filter == '[' ) {
					filter++;
				}
				filter++;
			}
			if ( buf.Length() ) {
				index = idStr(name).Find( buf.c_str(), casesensitive );
				if ( index == -1 ) {
					return false;
				}
				name += index + strlen(buf);
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[') {
			if ( *(filter+1) == '[' ) {
				if ( *name != '[' ) {
					return false;
				}
				filter += 2;
				name++;
			}
			else {
				filter++;
				found = false;
				while(*filter && !found) {
					if (*filter == ']' && *(filter+1) != ']') {
						break;
					}
					if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
						if (casesensitive) {
							if (*name >= *filter && *name <= *(filter+2)) {
								found = true;
							}
						}
						else {
							if ( ::toupper(*name) >= ::toupper(*filter) && ::toupper(*name) <= ::toupper(*(filter+2)) ) {
								found = true;
							}
						}
						filter += 3;
					}
					else {
						if (casesensitive) {
							if (*filter == *name) {
								found = true;
							}
						}
						else {
							if ( ::toupper(*filter) == ::toupper(*name) ) {
								found = true;
							}
						}
						filter++;
					}
				}
				if (!found) {
					return false;
				}
				while(*filter) {
					if ( *filter == ']' && *(filter+1) != ']' ) {
						break;
					}
					filter++;
				}
				filter++;
				name++;
			}
		}
		else {
			if (casesensitive) {
				if (*filter != *name) {
					return false;
				}
			}
			else {
				if ( ::toupper(*filter) != ::toupper(*name) ) {
					return false;
				}
			}
			filter++;
			name++;
		}
	}
	return true;
}

/*
=============
idStr::StripMediaName

  makes the string lower case, replaces backslashes with forward slashes, and removes extension
=============
*/
void idStr::StripMediaName( const char *name, idStr &mediaName ) {
	char c;

	mediaName.Empty();

	for ( c = *name; c; c = *(++name) ) {
		// truncate at an extension
		if ( c == '.' ) {
			break;
		}
		// convert backslashes to forward slashes
		if ( c == '\\' ) {
			mediaName.Append( '/' );
		} else {
			mediaName.Append( idStr::ToLower( c ) );
		}
	}
}

/*
=============
idStr::CheckExtension
=============
*/
bool idStr::CheckExtension( const char *name, const char *ext ) {
	const char *s1 = name + Length( name ) - 1;
	const char *s2 = ext + Length( ext ) - 1;
	int c1, c2, d;

	do {
		c1 = *s1--;
		c2 = *s2--;

		d = c1 - c2;
		while( d ) {
			if ( c1 <= 'Z' && c1 >= 'A' ) {
				d += ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c2 <= 'Z' && c2 >= 'A' ) {
				d -= ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			return false;
		}
	} while( s1 > name && s2 > ext );

	return ( s1 >= name );
}

/*
=============
idStr::FloatArrayToString
=============
*/
const char *idStr::FloatArrayToString( const float *array, const int length, const int precision ) {
	static int index = 0;
	static char str[4][16384];	// in case called by nested functions
	int i, n;
	char format[16], *s;

	// use an array of string so that multiple calls won't collide
	s = str[ index ];
	index = (index + 1) & 3;

	idStr::snPrintf( format, sizeof( format ), "%%.%df", precision );
	n = idStr::snPrintf( s, sizeof( str[0] ), format, array[0] );
	if ( precision > 0 ) {
		while( n > 0 && s[n-1] == '0' ) s[--n] = '\0';
		while( n > 0 && s[n-1] == '.' ) s[--n] = '\0';
	}
	idStr::snPrintf( format, sizeof( format ), " %%.%df", precision );
	for ( i = 1; i < length; i++ ) {
		n += idStr::snPrintf( s + n, sizeof( str[0] ) - n, format, array[i] );
		if ( precision > 0 ) {
			while( n > 0 && s[n-1] == '0' ) s[--n] = '\0';
			while( n > 0 && s[n-1] == '.' ) s[--n] = '\0';
		}
	}
	return s;
}

/*
============
idStr::Last

returns -1 if not found otherwise the index of the char
============
*/
int idStr::Last( const char c ) const {
	int i;

	for( i = Length(); i > 0; i-- ) {
		if ( data[ i - 1 ] == c ) {
			return i - 1;
		}
	}

	return -1;
}

/*
============
idStr::StripLeading
============
*/
void idStr::StripLeading( const char c ) {
	while( data[ 0 ] == c ) {
		memmove( &data[ 0 ], &data[ 1 ], len );
		len--;
	}
}

/*
============
idStr::StripLeading
============
*/
void idStr::StripLeading( const char *string ) {
	int l;

	l = strlen( string );
	if ( l > 0 ) {
		while ( !Cmpn( string, l ) ) {
			memmove( data, data + l, len - l + 1 );
			len -= l;
		}
	}
}

/*
============
idStr::StripLeadingOnce
============
*/
bool idStr::StripLeadingOnce( const char *string ) {
	int l;

	l = strlen( string );
	if ( ( l > 0 ) && !Cmpn( string, l ) ) {
		memmove( data, data + l, len - l + 1 );
		len -= l;
		return true;
	}
	return false;
}

/*
============
idStr::StripTrailing
============
*/
void idStr::StripTrailing( const char c ) {
	int i;

	for( i = Length(); i > 0 && data[ i - 1 ] == c; i-- ) {
		data[ i - 1 ] = '\0';
		len--;
	}
}

/*
============
idStr::StripLeading
============
*/
void idStr::StripTrailing( const char *string ) {
	int l;

	l = strlen( string );
	if ( l > 0 ) {
		while ( ( len >= l ) && !Cmpn( string, data + len - l, l ) ) {
			len -= l;
			data[len] = '\0';
		}
	}
}

/*
============
idStr::StripTrailingOnce
============
*/
bool idStr::StripTrailingOnce( const char *string ) {
	int l;

	l = strlen( string );
	if ( ( l > 0 ) && ( len >= l ) && !Cmpn( string, data + len - l, l ) ) {
		len -= l;
		data[len] = '\0';
		return true;
	}
	return false;
}

/*
============
idStr::Replace
============
*/
void idStr::Replace( const char *old, const char *nw ) {
	int		oldLen, newLen, i, j, count;
	idStr	oldString( data );

	oldLen = strlen( old );
	newLen = strlen( nw );

	// Work out how big the new string will be
	count = 0;
	for( i = 0; i < oldString.Length(); i++ ) {
		if( !idStr::Cmpn( &oldString[i], old, oldLen ) ) {
			count++;
			i += oldLen - 1;
		}
	}

	if( count ) {
		EnsureAlloced( len + ( ( newLen - oldLen ) * count ) + 2, false );

		// Replace the old data with the new data
		for( i = 0, j = 0; i < oldString.Length(); i++ ) {
			if( !idStr::Cmpn( &oldString[i], old, oldLen ) ) {
				memcpy( data + j, nw, newLen );
				i += oldLen - 1;
				j += newLen;
			} else {
				data[j] = oldString[i];
				j++;
			}
		}
		data[j] = 0;
		len = strlen( data );
	}
}

/*
============
idStr::Mid
============
*/
const char *idStr::Mid( int start, int len, idStr &result ) const {
	int i;

	result.Empty();

	i = Length();
	if ( i == 0 || len <= 0 || start >= i ) {
		return NULL;
	}

	if ( start + len >= i ) {
		len = i - start;
	}

	result.Append( &data[ start ], len );
	return result;
}

/*
============
idStr::Mid
============
*/
idStr idStr::Mid( int start, int len ) const {
	int i;
	idStr result;

	i = Length();
	if ( i == 0 || len <= 0 || start >= i ) {
		return result;
	}

	if ( start + len >= i ) {
		len = i - start;
	}

	result.Append( &data[ start ], len );
	return result;
}

/*
============
idStr::StripTrailingWhitespace
============
*/
void idStr::StripTrailingWhitespace( void ) {
	int i;

	// cast to unsigned char to prevent stripping off high-ASCII characters
	for( i = Length(); i > 0 && (unsigned char)(data[ i - 1 ]) <= ' '; i-- ) {
		data[ i - 1 ] = '\0';
		len--;
	}
}

/*
============
idStr::StripQuotes

Removes the quotes from the beginning and end of the string
============
*/
idStr& idStr::StripQuotes ( void )
{
	if ( data[0] != '\"' )
	{
		return *this;
	}

	// Remove the trailing quote first
	if ( data[len-1] == '\"' )
	{
		data[len-1] = '\0';
		len--;
	}

	// Strip the leading quote now
	len--;
	memmove( &data[ 0 ], &data[ 1 ], len );
	data[len] = '\0';

	return *this;
}

/*
=====================================================================

  filename methods

=====================================================================
*/

/*
============
idStr::FileNameHash
============
*/
int idStr::FileNameHash( void ) const {
	int		i;
	int		hash;
	char	letter;

	hash = 0;
	i = 0;
	while( data[i] != '\0' ) {
		letter = idStr::ToLower( data[i] );
		if ( letter == '.' ) {
			break;				// don't include extension
		}
		if ( letter =='\\' ) {
			letter = '/';
		}
		hash += (int)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

/*
============
idStr::BackSlashesToSlashes
============
*/
idStr &idStr::BackSlashesToSlashes( void ) {
	int i;

	for ( i = 0; i < len; i++ ) {
		if ( data[ i ] == '\\' ) {
			data[ i ] = '/';
		}
	}
	return *this;
}

/*
============
idStr::SetFileExtension
============
*/
idStr &idStr::SetFileExtension( const char *extension ) {
	StripFileExtension();
	if ( *extension != '.' ) {
		Append( '.' );
	}
	Append( extension );
	return *this;
}

// DG: helper-function that returns true if the character c is a directory separator
//     on the current platform
static ID_INLINE bool isDirSeparator( int c )
{
	if ( c == '/' ) {
		return true;
	}
#ifdef _WIN32
	if ( c == '\\' ) {
		return true;
	}
#elif defined(__AROS__)
	if ( c == ':' ) {
		return true;
	}
#endif
	return false;
}
// DG end

/*
============
idStr::StripFileExtension
============
*/
idStr &idStr::StripFileExtension( void ) {
	int i;

	for ( i = len-1; i >= 0; i-- ) {
		// DG: we're at a directory separator, nothing to strip at filename
		if ( isDirSeparator( data[i] ) ) {
			break;
		} // DG end
		if ( data[i] == '.' ) {
			data[i] = '\0';
			len = i;
			break;
		}
	}
	return *this;
}

/*
============
idStr::StripAbsoluteFileExtension
============
*/
idStr &idStr::StripAbsoluteFileExtension( void ) {
	int i;
	// FIXME DG: seems like this is unused, but it probably doesn't do what's expected
	//           (if you wanna strip .tar.gz this will fail with dots in path,
	//            if you indeed wanna strip the first dot in *path* (even in some directory) this is right)
	for ( i = 0; i < len; i++ ) {
		if ( data[i] == '.' ) {
			data[i] = '\0';
			len = i;
			break;
		}
	}

	return *this;
}

/*
==================
idStr::DefaultFileExtension
==================
*/
idStr &idStr::DefaultFileExtension( const char *extension ) {
	int i;

	// do nothing if the string already has an extension
	for ( i = len-1; i >= 0; i-- ) {
		// DG: we're at a directory separator, there was no file extension
		if ( isDirSeparator( data[i] ) ) {
			break;
		} // DG end
		if ( data[i] == '.' ) {
			return *this;
		}
	}
	if ( *extension != '.' ) {
		Append( '.' );
	}
	Append( extension );
	return *this;
}

/*
==================
idStr::DefaultPath
==================
*/
idStr &idStr::DefaultPath( const char *basepath ) {
	if ( isDirSeparator( ( *this )[ 0 ] ) ) {
		// absolute path location
		return *this;
	}

	*this = basepath + *this;
	return *this;
}

/*
====================
idStr::AppendPath
====================
*/
void idStr::AppendPath( const char *text ) {
	int pos;
	int i = 0;

	if ( text && text[i] ) {
		pos = len;
		EnsureAlloced( len + strlen( text ) + 2 );

		if ( pos ) {
			if ( !isDirSeparator( data[ pos-1 ] ) ) {
				data[ pos++ ] = '/';
			}
		}

		if ( isDirSeparator( text[ i ] ) ) {
			i++;
		}

		for ( ; text[ i ]; i++ ) {
			if ( text[ i ] == '\\' ) {
				data[ pos++ ] = '/';
			} else {
				data[ pos++ ] = text[ i ];
			}
		}
		len = pos;
		data[ pos ] = '\0';
	}
}

/*
==================
idStr::StripFilename
==================
*/
idStr &idStr::StripFilename( void ) {
	int pos;

	pos = Length() - 1;
	while( ( pos > 0 ) && !isDirSeparator( ( *this )[ pos ] ) ) {
		pos--;
	}

	if ( pos < 0 ) {
		pos = 0;
	}

	CapLength( pos );
	return *this;
}

/*
==================
idStr::StripPath
==================
*/
idStr &idStr::StripPath( void ) {
	int pos;

	pos = Length();
	while( ( pos > 0 ) && !isDirSeparator( ( *this )[ pos - 1 ] ) ) {
		pos--;
	}

	*this = Right( Length() - pos );
	return *this;
}

/*
====================
idStr::ExtractFilePath
====================
*/
void idStr::ExtractFilePath( idStr &dest ) const {
	int pos;

	//
	// back up until a \ or the start
	//
	pos = Length();
	while( ( pos > 0 ) &&  !isDirSeparator( ( *this )[ pos - 1 ] ) ) {
		pos--;
	}

	Left( pos, dest );
}

/*
====================
idStr::ExtractFileName
====================
*/
void idStr::ExtractFileName( idStr &dest ) const {
	int pos;

	//
	// back up until a \ or the start
	//
	pos = Length() - 1;
	while( ( pos > 0 ) && !isDirSeparator( ( *this )[ pos - 1 ] ) ) {
		pos--;
	}

	Right( Length() - pos, dest );
}

/*
====================
idStr::ExtractFileBase
====================
*/
void idStr::ExtractFileBase( idStr &dest ) const {
	int pos;
	int start;

	//
	// back up until a \ or the start
	//
	pos = Length() - 1;
	while( ( pos > 0 ) && !isDirSeparator( ( *this )[ pos - 1 ] ) ) {
		pos--;
	}

	start = pos;
	while( ( pos < Length() ) && ( ( *this )[ pos ] != '.' ) ) {
		pos++;
	}

	Mid( start, pos - start, dest );
}

/*
====================
idStr::ExtractFileExtension
====================
*/
void idStr::ExtractFileExtension( idStr &dest ) const {
	int pos;

	//
	// back up until a . or the start
	//
	pos = Length() - 1;
	while( ( pos > 0 ) && ( ( *this )[ pos - 1 ] != '.' ) ) {
		pos--;
		if( isDirSeparator( ( *this )[ pos ] ) ) { // DG: check for directory separator
			// no extension in the whole filename
			dest.Empty();
		} // DG end
	}

	if ( !pos ) {
		// no extension
		dest.Empty();
	} else {
		Right( Length() - pos, dest );
	}
}


/*
=====================================================================

  char * methods to replace library functions

=====================================================================
*/

/*
============
idStr::IsNumeric

Checks a string to see if it contains only numerical values.
============
*/
bool idStr::IsNumeric( const char *s ) {
	int		i;
	bool	dot;

	if ( *s == '-' ) {
		s++;
	}

	dot = false;
	for ( i = 0; s[i]; i++ ) {
		if ( !isdigit( s[i] ) ) {
			if ( ( s[ i ] == '.' ) && !dot ) {
				dot = true;
				continue;
			}
			return false;
		}
	}

	return true;
}

/*
============
idStr::HasLower

Checks if a string has any lowercase chars
============
*/
bool idStr::HasLower( const char *s ) {
	if ( !s ) {
		return false;
	}

	while ( *s ) {
		if ( CharIsLower( *s ) ) {
			return true;
		}
		s++;
	}

	return false;
}

/*
============
idStr::HasUpper

Checks if a string has any uppercase chars
============
*/
bool idStr::HasUpper( const char *s ) {
	if ( !s ) {
		return false;
	}

	while ( *s ) {
		if ( CharIsUpper( *s ) ) {
			return true;
		}
		s++;
	}

	return false;
}

/*
================
idStr::Cmp
================
*/
int idStr::Cmp( const char *s1, const char *s2 ) {
	int c1, c2, d;

	do {
		c1 = *s1++;
		c2 = *s2++;

		d = c1 - c2;
		if ( d ) {
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;		// strings are equal
}

/*
================
idStr::Cmpn
================
*/
int idStr::Cmpn( const char *s1, const char *s2, int n ) {
	int c1, c2, d;

	assert( n >= 0 );

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- ) {
			return 0;		// strings are equal until end point
		}

		d = c1 - c2;
		if ( d ) {
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;		// strings are equal
}

/*
================
idStr::Icmp
================
*/
int idStr::Icmp( const char *s1, const char *s2 ) {
	int c1, c2, d;

	do {
		c1 = *s1++;
		c2 = *s2++;

		d = c1 - c2;
		while( d ) {
			if ( c1 <= 'Z' && c1 >= 'A' ) {
				d += ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c2 <= 'Z' && c2 >= 'A' ) {
				d -= ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;		// strings are equal
}

/*
================
idStr::Icmpn
================
*/
int idStr::Icmpn( const char *s1, const char *s2, int n ) {
	int c1, c2, d;

	assert( n >= 0 );

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- ) {
			return 0;		// strings are equal until end point
		}

		d = c1 - c2;
		while( d ) {
			if ( c1 <= 'Z' && c1 >= 'A' ) {
				d += ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c2 <= 'Z' && c2 >= 'A' ) {
				d -= ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;		// strings are equal
}

/*
================
idStr::Icmp
================
*/
int idStr::IcmpNoColor( const char *s1, const char *s2 ) {
	int c1, c2, d;

	do {
		while ( idStr::IsColor( s1 ) ) {
			s1 += 2;
		}
		while ( idStr::IsColor( s2 ) ) {
			s2 += 2;
		}
		c1 = *s1++;
		c2 = *s2++;

		d = c1 - c2;
		while( d ) {
			if ( c1 <= 'Z' && c1 >= 'A' ) {
				d += ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c2 <= 'Z' && c2 >= 'A' ) {
				d -= ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;		// strings are equal
}

/*
================
idStr::IcmpPath
================
*/
int idStr::IcmpPath( const char *s1, const char *s2 ) {
	int c1, c2, d;

#if 0
//#if !defined( _WIN32 )
	idLib::common->Printf( "WARNING: IcmpPath used on a case-sensitive filesystem?\n" );
#endif

	do {
		c1 = *s1++;
		c2 = *s2++;

		d = c1 - c2;
		while( d ) {
			if ( c1 <= 'Z' && c1 >= 'A' ) {
				d += ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c1 == '\\' ) {
				d += ('/' - '\\');
				if ( !d ) {
					break;
				}
			}
			if ( c2 <= 'Z' && c2 >= 'A' ) {
				d -= ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c2 == '\\' ) {
				d -= ('/' - '\\');
				if ( !d ) {
					break;
				}
			}
			// make sure folders come first
			while( c1 ) {
				if ( c1 == '/' || c1 == '\\' ) {
					break;
				}
				c1 = *s1++;
			}
			while( c2 ) {
				if ( c2 == '/' || c2 == '\\' ) {
					break;
				}
				c2 = *s2++;
			}
			if ( c1 && !c2 ) {
				return -1;
			} else if ( !c1 && c2 ) {
				return 1;
			}
			// same folder depth so use the regular compare
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;
}

/*
================
idStr::IcmpnPath
================
*/
int idStr::IcmpnPath( const char *s1, const char *s2, int n ) {
	int c1, c2, d;

#if 0
//#if !defined( _WIN32 )
	idLib::common->Printf( "WARNING: IcmpPath used on a case-sensitive filesystem?\n" );
#endif

	assert( n >= 0 );

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- ) {
			return 0;		// strings are equal until end point
		}

		d = c1 - c2;
		while( d ) {
			if ( c1 <= 'Z' && c1 >= 'A' ) {
				d += ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c1 == '\\' ) {
				d += ('/' - '\\');
				if ( !d ) {
					break;
				}
			}
			if ( c2 <= 'Z' && c2 >= 'A' ) {
				d -= ('a' - 'A');
				if ( !d ) {
					break;
				}
			}
			if ( c2 == '\\' ) {
				d -= ('/' - '\\');
				if ( !d ) {
					break;
				}
			}
			// make sure folders come first
			while( c1 ) {
				if ( c1 == '/' || c1 == '\\' ) {
					break;
				}
				c1 = *s1++;
			}
			while( c2 ) {
				if ( c2 == '/' || c2 == '\\' ) {
					break;
				}
				c2 = *s2++;
			}
			if ( c1 && !c2 ) {
				return -1;
			} else if ( !c1 && c2 ) {
				return 1;
			}
			// same folder depth so use the regular compare
			return ( INTSIGNBITNOTSET( d ) << 1 ) - 1;
		}
	} while( c1 );

	return 0;
}

/*
=============
idStr::Copynz

Safe strncpy that ensures a trailing zero
=============
*/
void idStr::Copynz( char *dest, const char *src, int destsize ) {
	if ( !src ) {
		idLib::common->Warning( "idStr::Copynz: NULL src" );
		return;
	}
	if ( destsize < 1 ) {
		idLib::common->Warning( "idStr::Copynz: destsize < 1" );
		return;
	}

	strncpy( dest, src, destsize-1 );
	dest[destsize-1] = 0;
}

/*
================
idStr::Append

  never goes past bounds or leaves without a terminating 0
================
*/
void idStr::Append( char *dest, int size, const char *src ) {
	int		l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		idLib::common->Error( "idStr::Append: already overflowed" );
	}
	idStr::Copynz( dest + l1, src, size - l1 );
}

/*
================
idStr::LengthWithoutColors
================
*/
int idStr::LengthWithoutColors( const char *s ) {
	int len;
	const char *p;

	if ( !s ) {
		return 0;
	}

	len = 0;
	p = s;
	while( *p ) {
		if ( idStr::IsColor( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}

/*
================
idStr::RemoveColors
================
*/
char *idStr::RemoveColors( char *string ) {
	char *d;
	char *s;
	int c;

	s = string;
	d = string;
	while( (c = *s) != 0 ) {
		if ( idStr::IsColor( s ) ) {
			s++;
		}
		else {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

/*
================
idStr::snPrintf
================
*/
int idStr::snPrintf( char *dest, int size, const char *fmt, ...) {
	va_list argptr;
	int len;
	va_start( argptr, fmt );
	len = D3_vsnprintfC99(dest, size, fmt, argptr);
	va_end( argptr );
	if ( len >= 32000 ) {
		// TODO: Previously this function used a 32000 byte buffer to write into
		//       with vsprintf(), and raised this error if that was overflowed
		//       (more likely that'd have lead to a crash..).
		//       Technically we don't have that restriction anymore, so I'm unsure
		//       if this error should really still be raised to preserve
		//       the old intended behavior, maybe for compat with mod DLLs using
		//       the old version of the function or something?
		idLib::common->Error( "idStr::snPrintf: overflowed buffer" );
	}
	if ( len >= size ) {
		idLib::common->Warning( "idStr::snPrintf: overflow of %i in %i\n", len, size );
		len = size;
	}
	return len;
}

/*
============
idStr::vsnPrintf

vsnprintf portability:

C99 standard: vsnprintf returns the number of characters (excluding the trailing
'\0') which would have been written to the final string if enough space had been available
snprintf and vsnprintf do not write more than size bytes (including the trailing '\0')

win32: _vsnprintf returns the number of characters written, not including the terminating null character,
or a negative value if an output error occurs. If the number of characters to write exceeds count, then count
characters are written and -1 is returned and no trailing '\0' is added.

idStr::vsnPrintf: always appends a trailing '\0', returns number of characters written (not including terminal \0)
or returns -1 on failure or if the buffer would be overflowed.
============
*/
int idStr::vsnPrintf( char *dest, int size, const char *fmt, va_list argptr ) {
	int ret = D3_vsnprintfC99(dest, size, fmt, argptr);
	if ( ret < 0 || ret >= size ) {
		return -1;
	}
	return ret;
}

/*
============
sprintf

Sets the value of the string using a printf interface.
============
*/
int sprintf( idStr &string, const char *fmt, ... ) {
	int l;
	va_list argptr;
	char buffer[32000];

	va_start( argptr, fmt );
	l = idStr::vsnPrintf( buffer, sizeof(buffer)-1, fmt, argptr );
	va_end( argptr );
	buffer[sizeof(buffer)-1] = '\0';

	string = buffer;
	return l;
}

/*
============
vsprintf

Sets the value of the string using a vprintf interface.
============
*/
int vsprintf( idStr &string, const char *fmt, va_list argptr ) {
	int l;
	char buffer[32000];

	l = idStr::vsnPrintf( buffer, sizeof(buffer)-1, fmt, argptr );
	buffer[sizeof(buffer)-1] = '\0';

	string = buffer;
	return l;
}

/*
============
va

does a varargs printf into a temp buffer
NOTE: not thread safe
============
*/
char *va( const char *fmt, ... ) {
	va_list argptr;
	static int index = 0;
	static char string[4][16384];	// in case called by nested functions
	char *buf;

	buf = string[index];
	index = (index + 1) & 3;

	va_start( argptr, fmt );
	vsprintf( buf, fmt, argptr );
	va_end( argptr );

	return buf;
}



/*
============
idStr::BestUnit
============
*/
int idStr::BestUnit( const char *format, float value, Measure_t measure ) {
	int unit = 1;
	while ( unit <= 3 && ( 1 << ( unit * 10 ) < value ) ) {
		unit++;
	}
	unit--;
	value /= 1 << ( unit * 10 );
	D3_sprintf( *this, format, value );
	*this += " ";
	*this += units[ measure ][ unit ];
	return unit;
}

/*
============
idStr::SetUnit
============
*/
void idStr::SetUnit( const char *format, float value, int unit, Measure_t measure ) {
	value /= 1 << ( unit * 10 );
	D3_sprintf( *this, format, value );
	*this += " ";
	*this += units[ measure ][ unit ];
}

/*
================
idStr::InitMemory
================
*/
void idStr::InitMemory( void ) {
#ifdef USE_STRING_DATA_ALLOCATOR
	stringDataAllocator.Init();
#endif
}

/*
================
idStr::ShutdownMemory
================
*/
void idStr::ShutdownMemory( void ) {
#ifdef USE_STRING_DATA_ALLOCATOR
	stringDataAllocator.Shutdown();
#endif
}

/*
================
idStr::PurgeMemory
================
*/
void idStr::PurgeMemory( void ) {
#ifdef USE_STRING_DATA_ALLOCATOR
	stringDataAllocator.FreeEmptyBaseBlocks();
#endif
}

/*
================
idStr::ShowMemoryUsage_f
================
*/
void idStr::ShowMemoryUsage_f( const idCmdArgs &args ) {
#ifdef USE_STRING_DATA_ALLOCATOR
	idLib::common->Printf( "%6d KB string memory (%d KB free in %d blocks, %d empty base blocks)\n",
		stringDataAllocator.GetBaseBlockMemory() >> 10, stringDataAllocator.GetFreeBlockMemory() >> 10,
			stringDataAllocator.GetNumFreeBlocks(), stringDataAllocator.GetNumEmptyBaseBlocks() );
#endif
}

/*
================
idStr::FormatNumber
================
*/
struct formatList_t {
	int			gran;
	int			count;
};

// elements of list need to decend in size
formatList_t formatList[] = {
	{ 1000000000, 0 },
	{ 1000000, 0 },
	{ 1000, 0 }
};

int numFormatList = sizeof(formatList) / sizeof( formatList[0] );


idStr idStr::FormatNumber( int number ) {
	idStr string;
	bool hit;

	// reset
	for ( int i = 0; i < numFormatList; i++ ) {
		formatList_t *li = formatList + i;
		li->count = 0;
	}

	// main loop
	do {
		hit = false;

		for ( int i = 0; i < numFormatList; i++ ) {
			formatList_t *li = formatList + i;

			if ( number >= li->gran ) {
				li->count++;
				number -= li->gran;
				hit = true;
				break;
			}
		}
	} while ( hit );

	// print out
	bool found = false;

	for ( int i = 0; i < numFormatList; i++ ) {
		formatList_t *li = formatList + i;

		if ( li->count ) {
			if ( !found ) {
				string += va( "%i,", li->count );
			} else {
				string += va( "%3.3i,", li->count );
			}
			found = true;
		}
		else if ( found ) {
			string += va( "%3.3i,", li->count );
		}
	}

	if ( found ) {
		string += va( "%3.3i", number );
	}
	else {
		string += va( "%i", number );
	}

	// pad to proper size
	int count = 11 - string.Length();

	for ( int i = 0; i < count; i++ ) {
		string.Insert( " ", 0 );
	}

	return string;
}

int D3_sprintf(char * s, const char * fmt, ...) {
	int ret = 0;
	va_list argptr;
	va_start( argptr, fmt );
	ret = vsprintf( s, fmt, argptr );
	va_end( argptr );
	return ret;
}

int D3_vsprintf(char * s, const char * fmt, va_list argptr) {
	return vsprintf( s, fmt, argptr );
}

int D3_sprintf( idStr &dest, const char *fmt, ... ) {
	int ret = 0;
	va_list argptr;
	va_start( argptr, fmt );
	ret = vsprintf( dest, fmt, argptr );
	va_end( argptr );
	return ret;
}

int D3_vsprintf( idStr &dest, const char * fmt, va_list argptr ) {
	return vsprintf( dest, fmt, argptr );
}

// behaves like C99's vsnprintf() by returning the amount of bytes that
// *would* have been written into a big enough buffer, even if that's > size
// unlike idStr::vsnPrintf() which returns -1 in that case
int D3_vsnprintfC99(char *dst, size_t size, const char *format, va_list ap)
{
	// before VS2015, it didn't have a standards-conforming (v)snprintf()-implementation
	// same might be true for other windows compilers if they use old CRT versions, like MinGW does
#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER < 1900)
  #undef _vsnprintf
	// based on DG_vsnprintf() from https://github.com/DanielGibson/Snippets/blob/master/DG_misc.h
	int ret = -1;
	if(dst != NULL && size > 0)
	{
#if defined(_MSC_VER) && _MSC_VER >= 1400
		// I think MSVC2005 introduced _vsnprintf_s().
		// this shuts up _vsnprintf() security/deprecation warnings.
		ret = _vsnprintf_s(dst, size, _TRUNCATE, format, ap);
#else
		ret = _vsnprintf(dst, size, format, ap);
		dst[size-1] = '\0'; // ensure '\0'-termination
#endif
	}

	if(ret == -1)
	{
		// _vsnprintf() returns -1 if the output is truncated
		// it's also -1 if dst or size were NULL/0, so the user didn't want to write
		// we want to return the number of characters that would've been
		// needed, though.. fortunately _vscprintf() calculates that.
		ret = _vscprintf(format, ap);
	}
	return ret;
  #define _vsnprintf	use_idStr_vsnPrintf
#else // other operating systems and VisualC++ >= 2015 should have a proper vsnprintf()
  #undef vsnprintf
	return vsnprintf(dst, size, format, ap);
  #define vsnprintf	use_idStr_vsnPrintf
#endif
}

// behaves like C99's snprintf() by returning the amount of bytes that
// *would* have been written into a big enough buffer, even if that's > size
// unlike idStr::snPrintf() which returns the written bytes in that case
// and also calls common->Warning() in case of overflows
int D3_snprintfC99(char *dst, size_t size, const char *format, ...)
{
	int ret = 0;
	va_list argptr;
	va_start( argptr, format );
	ret = D3_vsnprintfC99(dst, size, format, argptr);
	va_end( argptr );
	return ret;
}
