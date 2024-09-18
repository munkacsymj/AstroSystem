/*  eval.h -- Interpreter that executes the tokens from a strategy script.
 *
 *  Copyright (C) 2007 Mark J. Munkacsy
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (file: COPYING).  If not, see
 *   <http://www.gnu.org/licenses/>. 
 */
#include <stdio.h>
#include "lex.h"
#include "script_out.h"

void print_variables(FILE *fp);

// must_be_variable: when set, a string that's never been seen before
// will be used to create a new variable. If unset, a string that's
// not a variable name will be kept as a string variable.
//
// need_val: when set, the value of the expression is needed (e.g.,
// the "true" clause in an "if" when the condition is true. If
// cleared, the value of the expression will be discarded, so all
// that's needed is to skip over the expression.
//
// execute: when set, references to "brighter" will results in the
// reading of an image file.
//
// "eval" reads a single expression.
// "eval_exp_seq" reads a sequence of expressions (may be zero)
//

Token *eval(FILE *fp, int execute, int need_val, int must_be_variable = 0);
Token *eval_exp_seq(FILE *fp, int execute, int need_val, int must_be_variable = 0);
void dump_vars_to_output(Script_Output *output);
