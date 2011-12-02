/*
Functions to calculate the distance between two strings.

Copyright 2011 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include "distance.h"

#define ADD_PENALTY 10
#define PRE_ADD_PENALTY 1
#define DROP_PENALTY 10
#define END_DROP_PENALTY 10
#define TRANSPOSE_PENALTY 10
#define DELETE_PENALTY 10
#define SWAP_PENALTY 10

static gboolean
ignore_character (gchar inchar)
{
	static gchar * ignore = NULL;
	if (ignore == NULL) {
		/* TRANSLATORS: These are chacaters that should not be considered
		   mistakes in the comparison functions.  Typically they are gramatical
		   characters that can be found in menus. */
		ignore = _(" _->");
	}

	int i;
	for (i = 0; i < 4; i++) {
		if (ignore[i] == inchar) {
			return TRUE;
		}
	}
	return FALSE;
}

static guint
swap_cost (gchar a, gchar b)
{
	if (a == b)
		return 0;
	if (ignore_character(a) || ignore_character(b))
		return 0;
	if (g_unichar_toupper(a) == g_unichar_toupper(b))
		return SWAP_PENALTY / 10; /* Some penalty, but close */
	return SWAP_PENALTY;
}

#define MATRIX_VAL(needle_loc, haystack_loc) (penalty_matrix[(needle_loc + 1) + (haystack_loc + 1) * (len_needle + 1)])

static void
dumpmatrix (const gchar * needle, guint len_needle, const gchar * haystack, guint len_haystack, guint * penalty_matrix)
{
#ifndef DUMP_MATRIX
	return;
#endif
	gint i, j;

	g_printf("\n");
	/* Character Column */
	g_printf("  ");
	/* Base val column */
	g_printf("  ");
	for (i = 0; i < len_needle; i++) {
		g_printf("%c ", needle[i]);
	}
	g_printf("\n");

	/* Character Column */
	g_printf("  ");
	for (i = -1; i < (gint)len_needle; i++) {
		g_printf("%u ", MATRIX_VAL(i, -1));
	}
	g_printf("\n");

	for (j = 0; j < len_haystack; j++) {
		g_printf("%c ", haystack[j]);
		for (i = -1; i < (gint)len_needle; i++) {
			g_printf("%u ", MATRIX_VAL(i, j));
		}
		g_printf("\n");
	}
	g_printf("\n");

	return;
}

static guint
calculate_token_distance (const gchar * needle, const gchar * haystack)
{
	// g_debug("Comparing token '%s' to token '%s'", needle, haystack);

	guint len_needle = 0;
	guint len_haystack = 0;

	if (needle != NULL) {
		len_needle = g_utf8_strlen(needle, -1);
	}

	if (haystack != NULL) {
		len_haystack = g_utf8_strlen(haystack, -1);
	}

	/* Handle the cases of very short or NULL strings quickly */
	if (len_needle == 0) {
		return DROP_PENALTY * len_haystack;
	}

	if (len_haystack == 0) {
		return ADD_PENALTY * len_needle;
	}

	/* Allocate the matrix of penalties */
	guint * penalty_matrix = g_malloc0(sizeof(guint) * (len_needle + 1) * (len_haystack + 1));
	int i;

	/* Take the first row and first column and make them additional letter penalties */
	for (i = 0; i < len_needle + 1; i++) {
		MATRIX_VAL(i - 1, -1) = i * ADD_PENALTY;
	}

	for (i = 0; i < len_haystack + 1; i++) {
		if (i < len_haystack - len_needle) {
			MATRIX_VAL(-1, i - 1) = i * PRE_ADD_PENALTY;
		} else {
			MATRIX_VAL(-1, i - 1) = (len_haystack - len_needle) * PRE_ADD_PENALTY + (i - (len_haystack - len_needle)) * DROP_PENALTY;
		}
	}

	/* Now go through the matrix building up the penalties */
	int ineedle, ihaystack;
	for (ineedle = 0; ineedle < len_needle; ineedle++) {
		for (ihaystack = 0; ihaystack < len_haystack; ihaystack++) {
			char needle_let = needle[ineedle];
			char haystack_let = haystack[ihaystack];

			guint subst_pen = MATRIX_VAL(ineedle - 1, ihaystack - 1) + swap_cost(needle_let, haystack_let);
			guint drop_pen = MATRIX_VAL(ineedle - 1, ihaystack);
			if (ineedle < ihaystack) {
				drop_pen += DROP_PENALTY;
			} else {
				drop_pen += END_DROP_PENALTY;
			}

			guint add_pen = MATRIX_VAL(ineedle, ihaystack - 1);
			if (len_haystack - len_needle - ineedle > 0) {
				add_pen += PRE_ADD_PENALTY;
			} else {
				add_pen += ADD_PENALTY;
			}
			guint transpose_pen = drop_pen + 1; /* ensures won't be chosen */

			if (ineedle > 0 && ihaystack > 0 && needle_let == haystack[ihaystack - 1] && haystack_let == needle[ineedle - 1]) {
				transpose_pen = MATRIX_VAL(ineedle - 2, ihaystack - 2) + TRANSPOSE_PENALTY;
			}

			MATRIX_VAL(ineedle, ihaystack) = MIN(MIN(subst_pen, drop_pen), MIN(add_pen, transpose_pen));
		}
	}

	dumpmatrix(needle, len_needle, haystack, len_haystack, penalty_matrix);

	guint retval = penalty_matrix[(len_needle + 1) * (len_haystack + 1) - 1];
	g_free(penalty_matrix);

	return retval;
}

#define SEPARATORS " .->"

guint
calculate_distance (const gchar * needle, GStrv haystacks, GStrv * matches)
{
	g_return_val_if_fail(needle != NULL || haystacks != NULL, G_MAXUINT);

	if (needle == NULL) {
		return DROP_PENALTY * g_utf8_strlen(haystacks[0], 1024);
	}
	if (haystacks == NULL) {
		return ADD_PENALTY * g_utf8_strlen(needle, 1024);
	}

	gchar * haystack = g_strjoinv(" > ", haystacks);

	GStrv needle_tokens = g_strsplit_set(needle, SEPARATORS, 0);
	g_return_val_if_fail(needle_tokens != NULL, G_MAXUINT);

	GStrv haystack_tokens = g_strsplit_set(haystack, SEPARATORS, 0);
	g_return_val_if_fail(haystack_tokens != NULL, G_MAXUINT);

	/* Set up an array for matches so that we can enter
	   the items as we go, if we have some place to put it
	   when we're done */
	GStrv match_array = NULL;
	if (matches != NULL) {
		guint length = g_strv_length(needle_tokens);
		match_array = g_new0(gchar *, length + 1);
		match_array[length] = NULL;
	}

	// g_debug("Needle tokens: '%s'", g_strjoinv("', '", needle_tokens));
	// g_debug("Haystack tokens: '%s'", g_strjoinv("', '", haystack_tokens));

	guint final_distance = 0;
	guint needle_token = 0;

	for (needle_token = 0; needle_tokens[needle_token] != NULL; needle_token++) {
		gchar * ineedle = needle_tokens[needle_token];

		guint best_fit = G_MAXUINT;
		guint haystack_token = 0;

		for (haystack_token = 0; haystack_tokens[haystack_token] != NULL; haystack_token++) {
			gchar * ihaystack = haystack_tokens[haystack_token];
			guint distance = calculate_token_distance(ineedle, ihaystack);

			if (distance < best_fit) {
				best_fit = distance;

				/* Place the pointer into the array, notice no copy here
				   so this will go away when hastack_tokens is free'd */
				if (match_array != NULL) {
					match_array[needle_token] = ihaystack;
				}
			}
		}

		final_distance += best_fit;
	}

	/* Copy the strings that we care about */
	if (match_array != NULL) {
		int i;
		for (i = 0; i < needle_token; i++) {
			gchar * tmp = g_strdup(match_array[i]);
			match_array[i] = tmp;
		}

		*matches = match_array;
		match_array = NULL;
	}

	g_strfreev(needle_tokens);
	g_strfreev(haystack_tokens);
	g_free(haystack);

	g_return_val_if_fail(needle_token > 0, G_MAXUINT);

	return final_distance / needle_token;
}
