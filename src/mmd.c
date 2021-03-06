/**

	MultiMarkdown 6 -- Lightweight markup processor to produce HTML, LaTeX, and more.

	@file mmd.c

	@brief Create MMD parsing engine


	@author	Fletcher T. Penney
	@bug	

**/

/*

	Copyright © 2016 - 2017 Fletcher T. Penney.


	The `MultiMarkdown 6` project is released under the MIT License..
	
	GLibFacade.c and GLibFacade.h are from the MultiMarkdown v4 project:
	
		https://github.com/fletcher/MultiMarkdown-4/
	
	MMD 4 is released under both the MIT License and GPL.
	
	
	CuTest is released under the zlib/libpng license. See CuTest.c for the text
	of the license.
	
	
	## The MIT License ##
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char.h"
#include "d_string.h"
#include "i18n.h"
#include "lexer.h"
#include "libMultiMarkdown.h"
#include "mmd.h"
#include "object_pool.h"
#include "parser.h"
#include "scanners.h"
#include "stack.h"
#include "token.h"
#include "token_pairs.h"
#include "writer.h"


// Basic parser function declarations
void * ParseAlloc();
void Parse();
void ParseFree();

void mmd_pair_tokens_in_block(token * block, token_pair_engine * e, stack * s);



/// Build MMD Engine
mmd_engine * mmd_engine_create(DString * d, unsigned long extensions) {
	mmd_engine * e = malloc(sizeof(mmd_engine));

	if (e) {
		e->dstr = d;

		e->root = NULL;

		e->extensions = extensions;

		e->allow_meta = (extensions & EXT_COMPATIBILITY) ? false : true;

		e->language = LC_EN;
		e->quotes_lang = ENGLISH;

		e->citation_stack = stack_new(0);
		e->definition_stack = stack_new(0);
		e->footnote_stack = stack_new(0);
		e->header_stack = stack_new(0);
		e->link_stack = stack_new(0);
		e->metadata_stack = stack_new(0);

		e->pairings1 = token_pair_engine_new();
		e->pairings2 = token_pair_engine_new();
		e->pairings3 = token_pair_engine_new();

		// CriticMarkup
		if (extensions & EXT_CRITIC) {
			token_pair_engine_add_pairing(e->pairings1, CRITIC_ADD_OPEN, CRITIC_ADD_CLOSE, PAIR_CRITIC_ADD, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings1, CRITIC_DEL_OPEN, CRITIC_DEL_CLOSE, PAIR_CRITIC_DEL, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings1, CRITIC_COM_OPEN, CRITIC_COM_CLOSE, PAIR_CRITIC_COM, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings1, CRITIC_SUB_OPEN, CRITIC_SUB_DIV_A, PAIR_CRITIC_SUB_DEL, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings1, CRITIC_SUB_DIV_B, CRITIC_SUB_CLOSE, PAIR_CRITIC_SUB_ADD, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings1, CRITIC_HI_OPEN, CRITIC_HI_CLOSE, PAIR_CRITIC_HI, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		}

		// Brackets, Parentheses, Angles
		token_pair_engine_add_pairing(e->pairings2, BRACKET_LEFT, BRACKET_RIGHT, PAIR_BRACKET, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, BRACKET_CITATION_LEFT, BRACKET_RIGHT, PAIR_BRACKET_CITATION, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, BRACKET_FOOTNOTE_LEFT, BRACKET_RIGHT, PAIR_BRACKET_FOOTNOTE, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, BRACKET_IMAGE_LEFT, BRACKET_RIGHT, PAIR_BRACKET_IMAGE, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, BRACKET_VARIABLE_LEFT, BRACKET_RIGHT, PAIR_BRACKET_VARIABLE, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, PAREN_LEFT, PAREN_RIGHT, PAIR_PAREN, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, ANGLE_LEFT, ANGLE_RIGHT, PAIR_ANGLE, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings2, BRACE_DOUBLE_LEFT, BRACE_DOUBLE_RIGHT, PAIR_BRACES, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);

		// Strong/Emph
		token_pair_engine_add_pairing(e->pairings3, STAR, STAR, PAIR_STAR, 0);
		token_pair_engine_add_pairing(e->pairings3, UL, UL, PAIR_UL, 0);

		// Quotes and Backticks
		token_pair_engine_add_pairing(e->pairings2, BACKTICK, BACKTICK, PAIR_BACKTICK, PAIRING_PRUNE_MATCH | PAIRING_MATCH_LENGTH);

		token_pair_engine_add_pairing(e->pairings3, BACKTICK,   QUOTE_RIGHT_ALT,   PAIR_QUOTE_ALT, PAIRING_ALLOW_EMPTY | PAIRING_MATCH_LENGTH);
		token_pair_engine_add_pairing(e->pairings3, QUOTE_SINGLE, QUOTE_SINGLE, PAIR_QUOTE_SINGLE, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		token_pair_engine_add_pairing(e->pairings3, QUOTE_DOUBLE, QUOTE_DOUBLE, PAIR_QUOTE_DOUBLE, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);

		// Math
		if (!(extensions & EXT_COMPATIBILITY)) {
			token_pair_engine_add_pairing(e->pairings2, MATH_PAREN_OPEN, MATH_PAREN_CLOSE, PAIR_MATH, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings2, MATH_BRACKET_OPEN, MATH_BRACKET_CLOSE, PAIR_MATH, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings2, MATH_DOLLAR_SINGLE, MATH_DOLLAR_SINGLE, PAIR_MATH, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
			token_pair_engine_add_pairing(e->pairings2, MATH_DOLLAR_DOUBLE, MATH_DOLLAR_DOUBLE, PAIR_MATH, PAIRING_ALLOW_EMPTY | PAIRING_PRUNE_MATCH);
		}
	
		// Superscript/Subscript
		if (!(extensions & EXT_COMPATIBILITY)) {
			token_pair_engine_add_pairing(e->pairings3, SUPERSCRIPT, SUPERSCRIPT, PAIR_SUPERSCRIPT, 0);
			token_pair_engine_add_pairing(e->pairings3, SUBSCRIPT, SUBSCRIPT, PAIR_SUPERSCRIPT, 0);
		}

	}

	return e;
}

/// Create MMD Engine using an existing DString (A new copy is *not* made)
mmd_engine * mmd_engine_create_with_dstring(DString * d, unsigned long extensions) {
	return mmd_engine_create(d, extensions);
}


/// Create MMD Engine using a C string (A private copy of the string will be
/// made.  The one passed here can be freed by the calling function)
mmd_engine * mmd_engine_create_with_string(const char * str, unsigned long extensions) {
	DString * d = d_string_new(str);

	return mmd_engine_create(d, extensions);
}


/// Set language and smart quotes language
void mmd_engine_set_language(mmd_engine * e, short language) {
	e->language = language;

	switch (language) {
		case LC_EN:
			e->quotes_lang = ENGLISH;
			break;
		case LC_DE:
			e->quotes_lang = GERMAN;
			break;
		case LC_ES:
			e->quotes_lang = ENGLISH;
			break;
		default:
			e->quotes_lang = ENGLISH;
	}
}


/// Free an existing MMD Engine
void mmd_engine_free(mmd_engine * e, bool freeDString) {
	if (e == NULL)
		return;

	if (freeDString)
		d_string_free(e->dstr, true);

	if (e->extensions & EXT_CRITIC)
		token_pair_engine_free(e->pairings1);
	
	token_pair_engine_free(e->pairings2);
	token_pair_engine_free(e->pairings3);

	token_tree_free(e->root);

	// Pointers to blocks that are freed elsewhere
	stack_free(e->definition_stack);
	stack_free(e->header_stack);

	// Links need to be freed
	while (e->link_stack->size) {
		link_free(stack_pop(e->link_stack));
	}
	stack_free(e->link_stack);

	// Footnotes need to be freed
	while (e->footnote_stack->size) {
		footnote_free(stack_pop(e->footnote_stack));
	}
	stack_free(e->footnote_stack);

	// Metadata needs to be freed
	while (e->metadata_stack->size) {
		meta_free(stack_pop(e->metadata_stack));
	}
	stack_free(e->metadata_stack);

	free(e);
}


bool line_is_empty(token * t) {
	while (t) {
		switch (t->type) {
			case NON_INDENT_SPACE:
			case INDENT_TAB:
			case INDENT_SPACE:
				t = t->next;
				break;
			case TEXT_LINEBREAK:
			case TEXT_NL:
				return true;
			default:
				return false;
		}
	}

	return true;
}


/// Determine what sort of line this is
void mmd_assign_line_type(mmd_engine * e, token * line) {
	if (!line)
		return;

	if (!line->child) {
		line->type = LINE_EMPTY;
		return;
	}

	const char * source = e->dstr->str;
	
	token * t = NULL;
	short temp_short;
	size_t scan_len;

	// Skip non-indenting space
	if (line->child->type == NON_INDENT_SPACE) {
		token_remove_first_child(line);
	} else if (line->child->type == TEXT_PLAIN && line->child->len == 1) {
		if (source[line->child->start] == ' ')
			token_remove_first_child(line);
	}

	if (line->child == NULL) {
		line->type = LINE_EMPTY;
		return;
	}
	
	switch (line->child->type) {
		case INDENT_TAB:
			if (line_is_empty(line->child)) {
				line->type = LINE_EMPTY;
				e->allow_meta = false;
			} else
				line->type = LINE_INDENTED_TAB;
			break;
		case INDENT_SPACE:
			if (line_is_empty(line->child)) {
				line->type = LINE_EMPTY;
				e->allow_meta = false;
			} else
				line->type = LINE_INDENTED_SPACE;
			break;
		case ANGLE_LEFT:
			if (scan_html_block(&source[line->start]))
				line->type = LINE_HTML;
			else
				line->type = LINE_PLAIN;
			break;
		case ANGLE_RIGHT:
			line->type = LINE_BLOCKQUOTE;
			line->child->type = MARKER_BLOCKQUOTE;
			break;
		case BACKTICK:
			if (e->extensions & EXT_COMPATIBILITY) {
				line->type = LINE_PLAIN;
				break;
			}
			scan_len = scan_fence_end(&source[line->child->start]);
			if (scan_len) {
				line->type = LINE_FENCE_BACKTICK;
				break;
			} else {
				scan_len = scan_fence_start(&source[line->child->start]);
				if (scan_len) {
					line->type = LINE_FENCE_BACKTICK_START;
					break;
				}
			}
			line->type = LINE_PLAIN;
			break;
		case HASH1:
		case HASH2:
		case HASH3:
		case HASH4:
		case HASH5:
		case HASH6:
			line->type = (line->child->type - HASH1) + LINE_ATX_1;
			line->child->type = (line->type - LINE_ATX_1) + MARKER_H1;

			// Strip trailing whitespace from '#' sequence
			line->child->len = line->child->type - MARKER_H1 + 1;

			// Strip trailing '#' sequence if present
			if (line->child->tail->type == TEXT_NL) {
				if ((line->child->tail->prev->type >= HASH1) &&
					(line->child->tail->prev->type <= HASH6))
					line->child->tail->prev->type = TEXT_EMPTY;
			} else {
				token_describe(line->child->tail, NULL);
				if ((line->child->tail->type >= HASH1) &&
					(line->child->tail->type <= HASH6))
					line->child->tail->type = TEXT_EMPTY;
			}
			break;
		case TEXT_NUMBER_POSS_LIST:
			switch(source[line->child->next->start]) {
				case '.':
					switch(source[line->child->next->start + 1]) {
						case ' ':
						case '\t':
							line->type = LINE_LIST_ENUMERATED;
							line->child->type = MARKER_LIST_ENUMERATOR;

							// Strip period
							line->child->next->type = TEXT_EMPTY;

							switch (line->child->next->next->type) {
								case TEXT_PLAIN:
									// Strip whitespace between bullet and text
									while (char_is_whitespace(source[line->child->next->next->start])) {
										line->child->next->next->start++;
										line->child->next->next->len--;
									}
									break;
								case INDENT_SPACE:
								case INDENT_TAB:
								case NON_INDENT_SPACE:
									t = line->child->next;
									while(t->next && ((t->next->type == INDENT_SPACE) ||
										(t->next->type == INDENT_TAB) ||
										(t->next->type == NON_INDENT_SPACE))) {
										tokens_prune(t->next, t->next);
									}
									break;
							}
							break;
						default:
							line->type = LINE_PLAIN;
							line->child->type = TEXT_PLAIN;
							break;
					}
					break;
				default:
					line->type = LINE_PLAIN;
					line->child->type = TEXT_PLAIN;
					break;
			}
			break;
		case DASH_N:
		case DASH_M:
		case STAR:
		case UL:
			// Could this be a horizontal rule?
			t = line->child->next;
			temp_short = line->child->len;
			while (t) {
				switch (t->type) {
					case DASH_N:
					case DASH_M:
						if (t->type == line->child->type) {
							t = t->next;
							temp_short += t->len;
						} else {
							temp_short = 0;
							t = NULL;
						}
						break;
					case STAR:
					case UL:
						if (t->type == line->child->type) {
							t = t->next;
							temp_short++;
						} else {
							temp_short = 0;
							t = NULL;
						}
						break;
					case NON_INDENT_SPACE:
					case INDENT_TAB:
					case INDENT_SPACE:
						t = t->next;
						break;
					case TEXT_PLAIN:
						if ((t->len == 1) && (source[t->start] == ' ')) {
							t = t->next;
							break;
						}
						temp_short = 0;
						t = NULL;
						break;
					case TEXT_NL:
					case TEXT_LINEBREAK:
						t = NULL;
						break;
					default:
						temp_short = 0;
						t = NULL;
						break;
				}
			}
			if (temp_short > 2) {
				// This is a horizontal rule, not a list item
				line->type = LINE_HR;
				break;
			}
			if (line->child->type == UL) {
				// Revert to plain for this type
				line->type = LINE_PLAIN;
				break;
			}
			// If longer than 1 character, then it can't be a list marker, so it's a 
			// plain line
			if (line->child->len > 1) {
				line->type = LINE_PLAIN;
				break;
			}
		case PLUS:
			if (!line->child->next) {
				// TODO: Should this be an empty list item instead??
				line->type = LINE_PLAIN;
			} else {
				switch(source[line->child->next->start]) {
					case ' ':
					case '\t':
						line->type = LINE_LIST_BULLETED;
						line->child->type = MARKER_LIST_BULLET;

						switch (line->child->next->type) {
							case TEXT_PLAIN:
								// Strip whitespace between bullet and text
								while (char_is_whitespace(source[line->child->next->start])) {
									line->child->next->start++;
									line->child->next->len--;
								}
								break;
							case INDENT_SPACE:
							case INDENT_TAB:
							case NON_INDENT_SPACE:
								t = line->child;
								while(t->next && ((t->next->type == INDENT_SPACE) ||
									(t->next->type == INDENT_TAB) ||
									(t->next->type == NON_INDENT_SPACE))) {
									tokens_prune(t->next, t->next);
								}
								break;
						}
						break;
					default:
						line->type = LINE_PLAIN;
						break;
				}
			}
			break;
		case TEXT_LINEBREAK:
		case TEXT_NL:
			e->allow_meta = false;
			line->type = LINE_EMPTY;
			break;
		case BRACKET_LEFT:
			if (e->extensions & EXT_COMPATIBILITY) {
				scan_len = scan_ref_link_no_attributes(&source[line->start]);
				line->type = (scan_len) ? LINE_DEF_LINK : LINE_PLAIN;
			} else {
				scan_len = scan_ref_link(&source[line->start]);
				line->type = (scan_len) ? LINE_DEF_LINK : LINE_PLAIN;
			}
			break;
		case BRACKET_CITATION_LEFT:
			if (e->extensions & EXT_NOTES) {
				scan_len = scan_ref_citation(&source[line->start]);
				line->type = (scan_len) ? LINE_DEF_CITATION : LINE_PLAIN;
			} else {
				line->type = LINE_PLAIN;
			}
			break;
		case BRACKET_FOOTNOTE_LEFT:
			if (e->extensions & EXT_NOTES) {
				scan_len = scan_ref_foot(&source[line->start]);
				line->type = (scan_len) ? LINE_DEF_FOOTNOTE : LINE_PLAIN;
			} else {
				line->type = LINE_PLAIN;
			}
			break;
		case TEXT_PLAIN:
			if (e->allow_meta && !(e->extensions & EXT_COMPATIBILITY)) {
				scan_len = scan_url(&source[line->start]);
				if (scan_len == 0) {
					scan_len = scan_meta_line(&source[line->start]);
					line->type = (scan_len) ? LINE_META : LINE_PLAIN;
					break;
				}
			}
		default:
			line->type = LINE_PLAIN;
			break;
	}

	if (line->type == LINE_PLAIN) {
		token * walker = line->child;

		while (walker != NULL) {
			if (walker->type == PIPE) {
				line->type = LINE_TABLE;

				return;
			}

			walker = walker->next;
		}
	}
}


/// Strip leading indenting space from line (if present)
void deindent_line(token  * line) {
	if (!line || !line->child)
		return;
	
	token * t;

	switch (line->child->type) {
		case INDENT_TAB:
		case INDENT_SPACE:
			t = line->child;
			line->child = t->next;
			t->next = NULL;
			if (line->child) {
				line->child->prev = NULL;
				line->child->tail = t->tail;
			}
			token_free(t);
			break;
	}
}


/// Strip leading indenting space from block
/// (for recursively parsing nested lists)
void deindent_block(mmd_engine * e, token * block) {
	if (!block || !block->child)
		return;

	token * t = block->child;

	while (t != NULL) {
		deindent_line(t);
		mmd_assign_line_type(e, t);

		t = t->next;
	}
}


/// Strip leading blockquote marker from line
void strip_quote_markers_from_line(token * line, const char * source) {
	if (!line || !line->child)
		return;

	token * t;
	
	switch (line->child->type) {
		case MARKER_BLOCKQUOTE:
		case NON_INDENT_SPACE:
			t = line->child;
			line->child = t->next;
			t->next = NULL;
			if (line->child) {
				line->child->prev = NULL;
				line->child->tail = t->tail;
			}
			token_free(t);
			break;
	}

	if (line->child && (line->child->type == TEXT_PLAIN)) {
		// Strip leading whitespace from first text token
		t = line->child;
		
		while (t->len && char_is_whitespace(source[t->start])) {
			t->start++;
			t->len--;
		}
		
		if (t->len == 0) {
			line->child = t->next;
			t->next = NULL;
			if (line->child) {
				line->child->prev = NULL;
				line->child->tail = t->tail;
			}
			
			token_free(t);
		}
	}
}


/// Strip leading blockquote markers and non-indent space
/// (for recursively parsing blockquotes)
void strip_quote_markers_from_block(mmd_engine * e, token * block) {
	if (!block || !block->child)
		return;

	token * t = block->child;

	while (t != NULL) {
		strip_quote_markers_from_line(t, e->dstr->str);
		mmd_assign_line_type(e, t);

		t = t->next;
	}
}


/// Create a token chain from source string
token * mmd_tokenize_string(mmd_engine * e, const char * str, size_t len) {
	// Create a scanner (for re2c)
	Scanner s;
	s.start = str;
	s.cur = s.start;

	// Strip trailing whitespace
//	while (len && char_is_whitespace_or_line_ending(str[len - 1]))
//		len--;
	
	// Where do we stop parsing?
	const char * stop = str + len;

	int type;								// TOKEN type
	token * t;								// Create tokens for incorporation

	token * root = token_new(0,0,0);		// Store the final parse tree here
	token * line = token_new(0,0,0);		// Store current line here

	const char * last_stop = str;			// Remember where last token ended

	do {
		// Scan for next token (type of 0 means there is nothing left);
		type = scan(&s, stop);

		//if (type && s.start != last_stop) {
        if (s.start != last_stop) {
			// We skipped characters between tokens

            if (type) {
				// Create a default token type for the skipped characters
				t = token_new(TEXT_PLAIN, (size_t)(last_stop - str), (size_t)(s.start - last_stop));

				token_append_child(line, t);
            } else {
				if (stop > last_stop) {
					// Source text ends without newline
					t = token_new(TEXT_PLAIN, (size_t)(last_stop - str), (size_t)(stop - last_stop));
                
					token_append_child(line, t);
				}
            }
		}

		switch (type) {
			case 0:
				// 0 means we finished with input
				// Add current line to root

				// What sort of line is this?
				mmd_assign_line_type(e, line);

				token_append_child(root, line);
				break;
			case TEXT_LINEBREAK:
			case TEXT_NL:
				// We hit the end of a line
				t = token_new(type, (size_t)(s.start - str), (size_t)(s.cur - s.start));
				token_append_child(line, t);

				// What sort of line is this?
				mmd_assign_line_type(e, line);

				token_append_child(root, line);
				line = token_new(0,s.cur - str,0);
				break;
			default:
				t = token_new(type, (size_t)(s.start - str), (size_t)(s.cur - s.start));
				token_append_child(line, t);
				break;
		}

		// Remember where token ends to detect skipped characters
		last_stop = s.cur;
	} while (type != 0);

	
	return root;
}


/// Parse token tree
void mmd_parse_token_chain(mmd_engine * e, token * chain) {

	void* pParser = ParseAlloc (malloc);		// Create a parser (for lemon)
	token * walker = chain->child;				// Walk the existing tree
	token * remainder;							// Hold unparsed tail of chain

	// Remove existing token tree
	e->root = NULL;

	while (walker != NULL) {
		remainder = walker->next;

		// Snip token from remainder
		walker->next = NULL;
		walker->tail = walker;

		if (remainder)
			remainder->prev = NULL;

		Parse(pParser, walker->type, walker, e);

		walker = remainder;
	}

	// Signal finish to parser
	Parse(pParser, 0, NULL, e);

	// Disconnect of (now empty) root
	chain->child = NULL;
	token_append_child(chain, e->root);
	e->root = NULL;

	ParseFree(pParser, free);
}


void mmd_pair_tokens_in_chain(token * head, token_pair_engine * e, stack * s) {

	while (head != NULL) {
		mmd_pair_tokens_in_block(head, e, s);

		head = head->next;
	}
}


/// Match token pairs inside block
void mmd_pair_tokens_in_block(token * block, token_pair_engine * e, stack * s) {
	if (block == NULL || e == NULL)
		return;

	switch (block->type) {
		case BLOCK_BLOCKQUOTE:
		case BLOCK_DEF_CITATION:
		case BLOCK_DEF_FOOTNOTE:
		case BLOCK_DEF_LINK:
		case BLOCK_H1:
		case BLOCK_H2:
		case BLOCK_H3:
		case BLOCK_H4:
		case BLOCK_H5:
		case BLOCK_H6:
		case BLOCK_PARA:
			token_pairs_match_pairs_inside_token(block, e, s);
			break;
		case DOC_START_TOKEN:
		case BLOCK_LIST_BULLETED:
		case BLOCK_LIST_BULLETED_LOOSE:
		case BLOCK_LIST_ENUMERATED:
		case BLOCK_LIST_ENUMERATED_LOOSE:
			mmd_pair_tokens_in_chain(block->child, e, s);
			break;
		case BLOCK_LIST_ITEM:
		case BLOCK_LIST_ITEM_TIGHT:
			token_pairs_match_pairs_inside_token(block, e, s);
			mmd_pair_tokens_in_chain(block->child, e, s);
			break;
		case LINE_TABLE:
		case BLOCK_TABLE:
			// TODO: Need to parse into cells first
			token_pairs_match_pairs_inside_token(block, e, s);
			mmd_pair_tokens_in_chain(block->child, e, s);
			break;
		case BLOCK_EMPTY:
		case BLOCK_CODE_INDENTED:
		case BLOCK_CODE_FENCED:
		default:
			// Nothing to do here
			return;
	}
}


/// Ambidextrous tokens can open OR close a pair.  This routine gives the opportunity
/// to change this behavior on case-by-case basis.  For example, in `foo **bar** foo`, the 
/// first set of asterisks can open, but not close a pair.  The second set can close, but not
/// open a pair.  This allows for complex behavior without having to bog down the tokenizer
/// with figuring out which type of asterisk we have.  Default behavior is that open and close
/// are enabled, so we just have to figure out when to turn it off.
void mmd_assign_ambidextrous_tokens_in_block(mmd_engine * e, token * block, const char * str, size_t start_offset) {
	if (block == NULL || block->child == NULL)
		return;

	size_t offset;		// Temp variable for use below
	size_t lead_count, lag_count, pre_count, post_count;
	
	token * t = block->child;

	while (t != NULL) {
		switch (t->type) {
			case BLOCK_META:
				// Do we treat this like metadata?
				if (!(e->extensions & EXT_COMPATIBILITY) &&
					!(e->extensions & EXT_NO_METADATA))
					return;
				// This is not metadata
				t->type = BLOCK_PARA;
			case DOC_START_TOKEN:
			case BLOCK_BLOCKQUOTE:
			case BLOCK_H1:
			case BLOCK_H2:
			case BLOCK_H3:
			case BLOCK_H4:
			case BLOCK_H5:
			case BLOCK_H6:
			case BLOCK_LIST_BULLETED:
			case BLOCK_LIST_BULLETED_LOOSE:
			case BLOCK_LIST_ENUMERATED:
			case BLOCK_LIST_ENUMERATED_LOOSE:
			case BLOCK_LIST_ITEM:
			case BLOCK_LIST_ITEM_TIGHT:
			case BLOCK_PARA:
			case BLOCK_TABLE:
				// Assign child tokens of blocks
				mmd_assign_ambidextrous_tokens_in_block(e, t, str, start_offset);
				break;
			case CRITIC_SUB_DIV:
				// Divide this into two tokens
				t->child = token_new(CRITIC_SUB_DIV_B, t->start + 1, 1);
				t->child->next = t->next;
				t->next = t->child;
				t->child = NULL;
				t->len = 1;
				t->type = CRITIC_SUB_DIV_A;
				break;
			case STAR:
				// Look left and skip over neighboring '*' characters
				offset = t->start;
				
				while ((offset != 0) && ((str[offset] == '*') || (str[offset] == '_'))) {
					offset--;
				}
				
				// We can only close if there is something to left besides whitespace
				if ((offset == 0) || (char_is_whitespace_or_line_ending(str[offset]))) {
					// Whitespace or punctuation to left, so can't close
					t->can_close = 0;
				}
				
				// Look right and skip over neighboring '*' characters
				offset = t->start + 1;
				
				while ((str[offset] == '*') || (str[offset] == '_'))
					offset++;
				
				// We can only open if there is something to right besides whitespace/punctuation
				if (char_is_whitespace_or_line_ending(str[offset])) {
					// Whitespace to right, so can't open
					t->can_open = 0;
				}

				// If we're in the middle of a word, then we need to be more precise
				if (t->can_open && t->can_close) {
					lead_count = 0;
					lag_count = 0;
					pre_count = 0;
					post_count = 0;

					offset = t->start - 1;

					// How many '*' in this run before current token?
					while (offset && (str[offset] == '*')) {
						lead_count++;
						offset--;
					}

					while (offset && (!char_is_whitespace_or_line_ending_or_punctuation(str[offset]))) {
						offset--;
					}

					// Are there '*' at the beginning of this word?
					while ((offset != -1) && (str[offset] == '*')) {
						pre_count++;
						offset--;
					}

					offset = t->start + 1;

					// How many '*' in this run after current token?
					while (str[offset] == '*') {
						lag_count++;
						offset++;
					}

					while (!char_is_whitespace_or_line_ending_or_punctuation(str[offset])) {
						offset++;
					}

					// Are there '*' at the end of this word?
					while (offset && (str[offset] == '*')) {
						post_count++;
						offset++;
					}

					if (pre_count + post_count > 0) {
						if (pre_count + post_count == lead_count + lag_count + 1) {
							if (pre_count == post_count) {
								t->can_open = 0;
								t->can_close = 0;
							} else if (pre_count == 0) {
								t->can_close = 0;
							} else if (post_count == 0) {
								t->can_open = 0;
							}
						} else if (pre_count == lead_count + lag_count + 1 + post_count) {
							t->can_open = 0;
						} else if (post_count == pre_count + lead_count + lag_count + 1) {
							t->can_close = 0;
						} else {
							if (pre_count != lead_count + lag_count + 1) {
								t->can_close = 0;
							}

							if (post_count != lead_count + lag_count + 1) {
								t->can_open = 0;
							}
						}
					}
				}
				break;
			case UL:
				// Look left and skip over neighboring '_' characters
				offset = t->start;
				
				while ((offset != 0) && ((str[offset] == '_') || (str[offset] == '*'))) {
					offset--;
				}
				
				if ((offset == 0) || (char_is_whitespace_or_line_ending_or_punctuation(str[offset]))) {
					// Whitespace or punctuation to left, so can't close
					t->can_close = 0;
				}

				// We don't allow intraword underscores (e.g.  `foo_bar_foo`)
				if ((offset > 0) && (char_is_alphanumeric(str[offset]))) {
					// Letters to left, so can't open
					t->can_open = 0;
				}
				
				// Look right and skip over neighboring '_' characters
				offset = t->start + 1;
				
				while ((str[offset] == '*') || (str[offset] == '_'))
					offset++;
				
				if (char_is_whitespace_or_line_ending_or_punctuation(str[offset])) {
					// Whitespace to right, so can't open
					t->can_open = 0;
				}
				
				if (char_is_alphanumeric(str[offset])) {
					// Letters to right, so can't close
					t->can_close = 0;
				}

				break;
			case BACKTICK:
				// Backticks are used for code spans, but also for ``foo'' double quote syntax.
				// We care only about the quote syntax.
				offset = t->start;
				
				// TODO: This does potentially prevent ``foo `` from closing due to space before closer?
				// Bug or feature??
				if (t->len != 2)
					break;
				
				if ((offset == 0) || (str[offset] != '`' && char_is_whitespace_or_line_ending_or_punctuation(str[offset - 1]))) {
					// Whitespace or punctuation to left, so can't close
					t->can_close = 0;
				}
				break;
			case QUOTE_SINGLE:
				if (!(e->extensions & EXT_SMART))
					break;
				// Some of these are actually APOSTROPHE's and should not be paired
				offset = t->start;

				if (!((offset == 0) || (char_is_whitespace_or_line_ending_or_punctuation(str[offset - 1])) ||
					(char_is_whitespace_or_line_ending_or_punctuation(str[offset + 1])))) {
					t->type = APOSTROPHE;
					break;
				}

				if (offset && (char_is_punctuation(str[offset - 1])) &&
					(char_is_alphanumeric(str[offset + 1]))) {
					t->type = APOSTROPHE;
					break;
				}
			case QUOTE_DOUBLE:
				if (!(e->extensions & EXT_SMART))
					break;
				offset = t->start;

				if ((offset == 0) || (char_is_whitespace_or_line_ending(str[offset - 1]))) {
					t->can_close = 0;
				}

				if (char_is_whitespace_or_line_ending(str[offset + 1])) {
					t->can_open = 0;
				}

				break;
			case DASH_N:
				if (!(e->extensions & EXT_SMART))
					break;
				// We want `1-2` to trigger a DASH_N, but regular hyphen otherwise (`a-b`)
				// This doesn't apply to `--` or `---`
				offset = t->start;
				if (t->len == 1) {
					// Check whether we have '1-2'
					if ((offset == 0) || (!char_is_digit(str[offset - 1])) ||
						(!char_is_digit(str[offset + 1]))) {
						t->type = TEXT_PLAIN;
					}
				}
				break;
			case MATH_DOLLAR_SINGLE:
			case MATH_DOLLAR_DOUBLE:
				if (e->extensions & EXT_COMPATIBILITY)
					break;

				offset = t->start;

				// Look left
				if ((offset == 0) || (char_is_whitespace_or_line_ending(str[offset - 1]))) {
					// Whitespace to left, so can't close
					t->can_close = 0;
				} else if ((offset != 0) && (!char_is_whitespace_or_line_ending_or_punctuation(str[offset - 1]))){
					// No whitespace or punctuation to left, can't open
					t->can_open = 0;
				}
				
				// Look right
				offset = t->start + t->len;
				
				if (char_is_whitespace_or_line_ending(str[offset])) {
					// Whitespace to right, so can't open
					t->can_open = 0;
				} else if (!char_is_whitespace_or_line_ending_or_punctuation(str[offset])) {
					// No whitespace or punctuation to right, can't close
					t->can_close = 0;
				}
				break;
			case SUPERSCRIPT:
			case SUBSCRIPT:
				if (e->extensions & EXT_COMPATIBILITY)
					break;

				offset = t->start;

				// Look left -- no whitespace to left
				if ((offset == 0) || (char_is_whitespace_or_line_ending_or_punctuation(str[offset - 1]))) {
					t->can_open = 0;
				}

				if ((offset != 0) && (char_is_whitespace_or_line_ending(str[offset - 1]))) {
					t->can_close = 0;
				}

				offset = t->start + t->len;

				if (char_is_whitespace_or_line_ending_or_punctuation(str[offset])) {
					t->can_open = 0;
				}

				// We need to be contiguous in order to match
				if (t->can_open) {
					offset = t->start + t->len;
					t->can_open = 0;

					while (!(char_is_whitespace_or_line_ending(str[offset]))) {
						if (str[offset] == str[t->start])
							t->can_open = 1;
						offset++;
					}

					// Are we a standalone, e.g x^2
					if (!t->can_open) {
						offset = t->start + t->len;
						while (!char_is_whitespace_or_line_ending_or_punctuation(str[offset]))
							offset++;

						t->len = offset-t->start;
						t->can_close = 0;

						// Shift next token right and move those characters as child node
						if ((t->next != NULL) && ((t->next->type == TEXT_PLAIN) || (t->next->type == TEXT_NUMBER_POSS_LIST))) {
							t->next->start += t->len - 1;
							t->next->len -= t->len - 1;

							t->child = token_new(TEXT_PLAIN, t->start + 1, t->len - 1);
						}
					}
				}

				// We need to be contiguous in order to match
				if (t->can_close) {
					offset = t->start;
					t->can_close = 0;

					while ((offset > 0) && !(char_is_whitespace_or_line_ending(str[offset - 1]))) {
						if (str[offset - 1] == str[t->start])
							t->can_close = 1;
						offset--;
					}
				}
				break;
		}
		
		t = t->next;
	}

}


/// Strong/emph parsing is done using single `*` and `_` characters, which are
/// then combined in a separate routine here to determine when 
/// consecutive characters should be interpreted as STRONG instead of EMPH
/// \todo: Perhaps combining this with the routine when they are paired
/// would improve performance?
void pair_emphasis_tokens(token * t) {
	token * closer;
	
	while (t != NULL) {
		if (t->mate != NULL) {
			switch (t->type) {
				case STAR:
				case UL:
					closer = t->mate;
					if ((t->next->mate == closer->prev) &&
						(t->type == t->next->type) &&
						(t->next->mate != t) &&
						(t->start+t->len == t->next->start) &&
						(closer->start == closer->prev->start + closer->prev->len)) {
						
						// We have a strong pair
						t->type = STRONG_START;
						t->len = 2;
						closer->type = STRONG_STOP;
						closer->len = 2;
						closer->start--;
						
						tokens_prune(t->next, t->next);
						tokens_prune(closer->prev, closer->prev);
					} else {
						t->type = EMPH_START;
						closer->type = EMPH_STOP;
					}
					break;
					
				default:
					break;
			}

		}
		
		if (t->child != NULL)
			pair_emphasis_tokens(t->child);
		
		t = t->next;
	}
}


void recursive_parse_list_item(mmd_engine * e, token * block) {
	// Strip list marker from first line
	token_remove_first_child(block->child);

	// Remove all leading space from first line of list item
//	strip_all_leading_space(block->child)

	// Remove one indent level from all lines to allow recursive parsing
	deindent_block(e, block);

	mmd_parse_token_chain(e, block);
}


void is_list_loose(token * list) {
	bool loose = false;
	
	token * walker = list->child;

	while (walker->next != NULL) {
		if (walker->type == BLOCK_LIST_ITEM) {
			if (walker->child->type == BLOCK_PARA) {
				loose = true;
			} else {
				walker->type = BLOCK_LIST_ITEM_TIGHT;
			}
		}
		
		walker = walker->next;
	}

	if (loose) {
		switch (list->type) {
			case BLOCK_LIST_BULLETED:
				list->type = BLOCK_LIST_BULLETED_LOOSE;
				break;
			case BLOCK_LIST_ENUMERATED:
				list->type = BLOCK_LIST_ENUMERATED_LOOSE;
				break;
		}
	}
}


/// Is this actually an HTML block?
void is_para_html(mmd_engine * e, token * block) {
	if (block->child->type != LINE_PLAIN)
		return;
	token * t = block->child->child;

	if (t->type != ANGLE_LEFT)
		return;

	if (scan_html_block(&(e->dstr->str[t->start]))) {
		block->type = BLOCK_HTML;
		return;
	}	

	if (scan_html_line(&(e->dstr->str[t->start]))) {
		block->type = BLOCK_HTML;
		return;
	}
}


void recursive_parse_blockquote(mmd_engine * e, token * block) {
	// Strip blockquote markers (if present)
	strip_quote_markers_from_block(e, block);

	mmd_parse_token_chain(e, block);
}


void metadata_stack_describe(mmd_engine * e) {
	meta * m;

	for (int i = 0; i < e->metadata_stack->size; ++i)
	{
		m = stack_peek_index(e->metadata_stack, i);
		fprintf(stderr, "'%s': '%s'\n", m->key, m->value);
	}
}


void strip_line_tokens_from_metadata(mmd_engine * e, token * metadata) {
	token * l = metadata->child;
	char * source = e->dstr->str;

	meta * m = NULL;
	size_t start, len;

	DString * d = d_string_new("");

	while (l) {
		switch (l->type) {
			case LINE_META:
				if (m) {
					meta_set_value(m, d->str);
					d_string_erase(d, 0, -1);
				}
				len = scan_meta_key(&source[l->start]);
				m = meta_new(source, l->start, len);
				start = l->start + len + 1;
				len = l->start + l->len - start - 1;
				d_string_append_c_array(d, &source[start], len);
				stack_push(e->metadata_stack, m);
				break;
			case LINE_INDENTED_TAB:
			case LINE_INDENTED_SPACE:
				while (l->len && char_is_whitespace(source[l->start])) {
					l->start++;
					l->len--;
				}
			case LINE_PLAIN:
				d_string_append_c(d, '\n');
				d_string_append_c_array(d, &source[l->start], l->len);
				break;
			default:
				fprintf(stderr, "ERROR!\n");
				token_describe(l, NULL);
				break;
		}

		l = l->next;
	}

	// Finish last line
	if (m) {
		meta_set_value(m, d->str);
	}

	d_string_free(d, true);
}


void strip_line_tokens_from_block(mmd_engine * e, token * block) {
	if ((block == NULL) || (block->child == NULL))
		return;

#ifndef NDEBUG
	fprintf(stderr, "Strip line tokens from %d (%lu:%lu) (child %d)\n", block->type, block->start, block->len, block->child->type);
	token_tree_describe(block, NULL);
#endif

	token * l = block->child;

	// Custom actions
	switch (block->type) {
		case BLOCK_META:
			// Handle metadata differently
			return strip_line_tokens_from_metadata(e, block);
		case BLOCK_CODE_INDENTED:
			// Strip trailing empty lines from indented code blocks
			while (l->tail->type == LINE_EMPTY)
				token_remove_last_child(block);
			break;
	}

	token * children = NULL;
	block->child = NULL;

	token * temp;

	// Move contents of line directly into the parent block
	while (l != NULL) {
		switch (l->type) {
			case LINE_ATX_1:
			case LINE_ATX_2:
			case LINE_ATX_3:
			case LINE_ATX_4:
			case LINE_ATX_5:
			case LINE_ATX_6:
			case LINE_BLOCKQUOTE:
			case LINE_CONTINUATION:
			case LINE_DEF_CITATION:
			case LINE_DEF_FOOTNOTE:
			case LINE_DEF_LINK:
			case LINE_EMPTY:
			case LINE_LIST_BULLETED:
			case LINE_LIST_ENUMERATED:
			case LINE_META:
			case LINE_PLAIN:
				// Remove leading non-indent space from line
				if (l->child && l->child->type == NON_INDENT_SPACE)
				token_remove_first_child(l);

			case LINE_INDENTED_TAB:
			case LINE_INDENTED_SPACE:
				// Strip leading indent (Only the first one)
				if (l->child && ((l->child->type == INDENT_SPACE) || (l->child->type == INDENT_TAB)))
					token_remove_first_child(l);

				// If we're not a code block, strip additional indents
				if ((block->type != BLOCK_CODE_INDENTED) && 
					(block->type != BLOCK_CODE_FENCED)) {
					while (l->child && ((l->child->type == INDENT_SPACE) || (l->child->type == INDENT_TAB)))
						token_remove_first_child(l);
				}
				// Add contents of line to parent block
				token_append_child(block, l->child);

				// Disconnect line from it's contents
				l->child = NULL;

				// Need to remember first line we strip
				if (children == NULL)
					children = l;

				// Advance to next line
				l = l->next;
				break;
			case LINE_TABLE:
				l->type = ROW_TABLE;
				break;
			default:
				// This is a block, need to remove it from chain and
				// Add to parent
				temp = l->next;

				token_pop_link_from_chain(l);
				token_append_child(block, l);

				// Advance to next line
				l = temp;
				break;
		}
	}

	// Free token chain of line types
	token_tree_free(children);
}


/// Parse part of the string into a token tree
token * mmd_engine_parse_substring(mmd_engine * e, size_t byte_start, size_t byte_len) {
#ifdef kUseObjectPool
	// Ensure token pool is available and ready
	token_pool_init();
#endif

	// Reset definition stack
	e->definition_stack->size = 0;
	
	// Tokenize the string
	token * doc = mmd_tokenize_string(e, &e->dstr->str[byte_start], byte_len);

	// Parse tokens into blocks
	mmd_parse_token_chain(e, doc);

	if (doc) {
		// Parse blocks for pairs
		mmd_assign_ambidextrous_tokens_in_block(e, doc, &e->dstr->str[byte_start], 0);

		// Prepare stack to be used for token pairing
		// This avoids allocating/freeing one for each iteration.
		stack * pair_stack = stack_new(0);

		mmd_pair_tokens_in_block(doc, e->pairings1, pair_stack);
		mmd_pair_tokens_in_block(doc, e->pairings2, pair_stack);
		mmd_pair_tokens_in_block(doc, e->pairings3, pair_stack);

		// Free stack
		stack_free(pair_stack);

		pair_emphasis_tokens(doc);

#ifndef NDEBUG
		token_tree_describe(doc, &e->dstr->str[byte_start]);
#endif
	}

	return doc;
}


/// Parse the entire string into a token tree
void mmd_engine_parse_string(mmd_engine * e) {
	// Free existing parse tree
	if (e->root)
		token_tree_free(e->root);

	// New parse tree
	e->root = mmd_engine_parse_substring(e, 0, e->dstr->currentStringLength);
}

