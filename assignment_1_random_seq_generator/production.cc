/**
 * File: production.cc
 * -------------------
 * Provides the implementation of the Production class, which
 * is simply a wrapper for the sequence of items (where items are terminals
 * or nonterminals).  It also completes the implementation of the ifstream
 * constructor, which was really the only thing missing from the .h
 */

#include "production.h"

/**
 * Constructor Implementation: Production
 * --------------------------------------
 * Constructor that initializes the Production based
 * on the content of a file.  infile is presumably
 * positioned at the beginning of a line of textual
 * data representing a CFG production such that the
 * expansion is terminated by a semicolon.  The assumption
 * made is that nonterminals (strings that also can expand
 * to their own productions) are delimited by '<' and '>' and 
 * that no whitespace appears in between '<' and '>'.  The implementation
 * will also read the whitespace and the '\n' appearing after the 
 * semicolon and discard it
 */

Production::Production(ifstream &infile) // phrases is constructed, size is 0
{
  while (true)
  {
    string token;
    infile >> token; // ignores whitespace by default
    if (token == ";")
      break;
    phrases.push_back(token);
  }

  string uselessText;
  getline(infile, uselessText);
}
