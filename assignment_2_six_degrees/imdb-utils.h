#ifndef __imdb_utils__
#define __imdb_utils__

#include <vector>
#include <string>
#include <iostream>
//#include "imdb.h"
using namespace std;

/**
 * Convenience struct: film
 * ------------------------
 * Bundles the name of a film and the year it was made
 * into a single struct.  It is a true struct in that the 
 * client is free to access both fields and change them at will
 * without issue.  operator== and operator< are implemented
 * so that films can be stored in STL containers requiring 
 * such methods.
 */

struct film
{

  string title;
  int year;

  /** 
   * Methods: operator==
   *          operator<
   * -------------------
   * Compares the two films for equality, where films are considered to be equal
   * if and only if they share the same name and appeared in the same year.  
   * film is considered to be less than another film if its title is 
   * lexicographically less than the second title, or if their titles are the same 
   * but the first's year is precedes the second's.
   *
   * @param rhs the film to which the receiving film is being compared.
   * @return boolean value which is true if and only if the required constraints
   *         between receiving object and argument are met.
   */

  bool operator==(const film &rhs) const
  {
    return this->title == rhs.title && (this->year == rhs.year);
  }

  bool operator<(const film &rhs) const
  {
    return this->title < rhs.title ||
           ((this->title == rhs.title) && (this->year < rhs.year));
  }
};

/*Binding whole information of specific movie into one struct*/
struct movie_informationT
{
  string title;
  int year;
  short num_actors;
  int *actor_offsets = NULL;
  //fill in information after passing infro starting address in file
  void set_info(const void *addr)
  {
    if (addr == NULL)
      return;
    const char *nm = (char *)addr;
    title = string(nm);
    int year_offs = title.length() + 1;
    year = (int)(*((char *)addr + year_offs)) + 1900;
    int num_actors_offs = year_offs + 1;
    num_actors_offs = (num_actors_offs % 2 == 1) ? num_actors_offs + 1 : num_actors_offs;
    num_actors = *(short *)((char *)addr + num_actors_offs);
    actor_offsets = (int *)((char *)addr +
                            ((num_actors_offs % 4 == 2) ? (num_actors_offs + 2) : (num_actors_offs + 4)));
  }
};
/*Structure: actor_informationT
------------------------------------
This structure stores information about certain actor.
pros of this function is flexibility and method which can initialize
baisc actor information based on info_addr of actor*/
struct actor_informationT
{
  string name;
  short num_movies;
  int *movie_offsets;
  //sets parameters to actor object by giving starting byte of actor info
  void set_info(const void *info_addr)
  {
    if (info_addr == NULL)
      return;
    const char *nm = (char *)info_addr;
    name = string(nm);
    int credits_num_offs = name.length() + 1;
    credits_num_offs = (credits_num_offs % 2 == 1) ? credits_num_offs + 1 : credits_num_offs;
    num_movies = *(short *)((char *)info_addr + credits_num_offs);
    movie_offsets = (int *)((char *)info_addr +
                            ((credits_num_offs % 4 == 2) ? (credits_num_offs + 2) : (credits_num_offs + 4)));
  }
};
/**
 * Quick, UNIX-dependent function to determine whether or not the
 * the resident OS is Linux or Solaris.  For our purposes, this
 * tells us whether the machine is big-endian or little-endian, and
 * the endiannees tells us which set of raw binary data files we should
 * be using.
 *
 * @return one of two data paths.
 */

inline const char *determinePathToData(const char *userSelectedPath = NULL)
{
  return "data/little-endian/";
}

#endif