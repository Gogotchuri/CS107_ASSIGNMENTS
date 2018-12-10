/**
 * File: rsg.cc
 * ------------
 * Provides the implementation of the full RSG application, which
 * relies on the services of the built-in string, ifstream, vector,
 * and map classes as well as the custom Production and Definition
 * classes provided with the assignment.
 */
//Need to correct spacing for commas and dots;
#include <map>
#include <fstream>
#include <ctype.h>
#include "definition.h"
#include "production.h"
using namespace std;

int find_index(vector<string> &vec, string str);
void replace_nonterms(vector<string> &vec, map<string, Definition> &grammar);
void display_text(vector<string> &vec);
/**
 * Takes a reference to a legitimate infile (one that's been set up
 * to layer over a file) and populates the grammar map with the
 * collection of definitions that are spelled out in the referenced
 * file.  The function is written under the assumption that the
 * referenced data file is really a grammar file that's properly
 * formatted.  You may assume that all grammars are in fact properly
 * formatted.
 *
 * @param infile a valid reference to a flat text file storing the grammar.
 * @param grammar a reference to the STL map, which maps nonterminal strings
 *                to their definitions.
 */

static void readGrammar(ifstream &infile, map<string, Definition> &grammar)
{
	while (true)
	{
		string uselessText;
		getline(infile, uselessText, '{');
		if (infile.eof())
			return; // true? we encountered EOF before we saw a '{': no more productions!
		infile.putback('{');
		Definition def(infile);
		grammar[def.getNonterminal()] = def;
	}
}

/**
 * Performs the rudimentary error checking needed to confirm that
 * the client provided a grammar file.  It then continues to
 * open the file, read the grammar into a map<string, Definition>,
 * and then print out the total number of Definitions that were read
 * in.  You're to update and decompose the main function to print
 * three randomly generated sentences, as illustrated by the sample
 * application.
 *
 * @param argc the number of tokens making up the command that invoked
 *             the RSG executable.  There must be at least two arguments,
 *             and only the first two are used.
 * @param argv the sequence of tokens making up the command, where each
 *             token is represented as a '\0'-terminated C string.
 */

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		cerr << "You need to specify the name of a grammar file." << endl;
		cerr << "Usage: rsg <path to grammar text file>" << endl;
		return 1; // non-zero return value means something bad happened
	}

	ifstream grammarFile(argv[1]);
	if (grammarFile.fail())
	{
		cerr << "Failed to open the file named \"" << argv[1] << "\".  Check to ensure the file exists. " << endl;
		return 2; // each bad thing has its own bad return value
	}

	// things are looking good...
	map<string, Definition> grammar;
	readGrammar(grammarFile, grammar);
	vector<string> final_vec;

	for (int i = 0; i < 3; i++)
	{
		cout << "Version #" << i + 1 << ":-----------------------------------" << endl;
		final_vec.clear();
		string s = "<start>";
		final_vec.push_back(s);
		replace_nonterms(final_vec, grammar);
		display_text(final_vec);
		for (int i = 0; i < 2; i++)
			cout << endl;
	}

	return 0;
}

/* Function description:
	This function looks for specific string in vector and returns its index;
	If there isn't this string in vector, function returns -1;

	@param vec vector to look inside
	@param str specific string should consist this string
*/
int find_index(vector<string> &vec, string str)
{
	int i = 0;
	for (vector<string>::iterator it = vec.begin(); it != vec.end(); it++)
	{
		if (it->find(str) != string::npos)
			return i;
		i++;
	}
	return -1;
}

/* Function description:
This function looks for nonterms and replaces them with random pre-defined sentences recursively

@param vec vector to make changes inside;
@param grammar specific map of Definition objects as values;

*/
void replace_nonterms(vector<string> &vec, map<string, Definition> &grammar)
{
	int index_of_nonterm = find_index(vec, "<");
	if (index_of_nonterm == -1)
		return;
	string nonterm = vec[index_of_nonterm];
	Production prod = grammar[nonterm].getRandomProduction();
	vec.erase(vec.begin() + index_of_nonterm);
	int i = 0;
	for (Production::iterator iter = prod.begin(); iter != prod.end(); iter++)
	{
		string curr_word = *iter;
		vec.insert(vec.begin() + index_of_nonterm + (i++), curr_word);
	}
	replace_nonterms(vec, grammar);
}

/*This function just prints given vector
	@param vec vector object to print
*/
void display_text(vector<string> &vec)
{
	const int MAX_CHARS_IN_LINE = 65;
	int curr_line_size = 5;
	cout << "     ";
	for (vector<string>::iterator it = vec.begin(); it != vec.end(); it++)
	{
		if (curr_line_size + it->size() > MAX_CHARS_IN_LINE)
		{
			cout << endl;
			curr_line_size = 0;
		}
		else
		{
			curr_line_size += it->size();
		}
		cout << " " << *it;
	}
}
