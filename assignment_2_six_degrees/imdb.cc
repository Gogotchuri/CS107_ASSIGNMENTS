using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

imdb::imdb(const string &directory)
{
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;

  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const
{
  return !((actorInfo.fd == -1) ||
           (movieInfo.fd == -1));
}

/*Struct to store and pass name and file address as one to bsearch*/
struct key_addrT
{
  const char *key;       //Entity name
  const void *file_addr; //File in which the entity is stored
};

/*General compare function to use in bsearch*/
int cmp_foo(const void *first, const void *second)
{
  //*this function compares two units only by name
  //*first is always a key and second is unit to compare
  const void *fl = ((key_addrT *)first)->file_addr;
  const char *str1 = ((key_addrT *)first)->key;
  int offset = *(int *)second;
  const char *str2 = (char *)fl + offset;
  return strcmp(str1, str2);
}

/*General function to search and if exists get address of entity with 'name' in 'file'*/
void *imdb::get_addr(const string name, const void *file, void **offset_addr) const
{
  int num_actors = *(int *)file;
  void *arr = (int *)file + 1;
  key_addrT pr = {name.c_str(), file}; //structure we are passing to bsearch
  void *actor_addr_offs = bsearch(&pr, arr, num_actors, sizeof(int), cmp_foo);
  if (actor_addr_offs == NULL)
    return NULL;
  int offst = *(int *)actor_addr_offs;
  if (offset_addr != NULL)
    *offset_addr = actor_addr_offs; // returning offset_addr if needed
  void *res = (char *)file + offst; //returning address of entity if found
  return res;
}

/*Returns film list for 'player' (detailed description in header file)*/
bool imdb::getCredits(const string &player, vector<film> &films) const
{
  films.clear(); //just in case we pass !empty vector
  void *info_addr = get_addr(player, actorFile, NULL);
  if (info_addr == NULL)
    return 0;
  actor_informationT curr_actor;
  curr_actor.set_info(info_addr); //getting and storing actor info
  //for every movie actor acted in, getting info creating fil object and pushing to vector
  movie_informationT tmp_movie;
  for (int i = 0; i < curr_actor.num_movies; i++)
  {
    int mv_offs = curr_actor.movie_offsets[i];
    tmp_movie.set_info((char *)movieFile + mv_offs);
    films.push_back({tmp_movie.title, tmp_movie.year});
  }
  return true;
}

/*Returns actors/actresses list for 'movie' (detailed description in header file)*/
bool imdb::getCast(const film &movie, vector<string> &players) const
{
  players.clear();   //just in case we pass !empty vector
  void *offset_addr; //address to the int array element which stores offset of 'movie';
  void *info_addr;   //address to 'movie' info starting byte
  info_addr = get_addr(movie.title, movieFile, &offset_addr);
  if (info_addr == NULL)
    return 0; //no such movie
  movie_informationT curr_movie;
  curr_movie.set_info(info_addr); //picks up iformation from address and writes it convenient struct
  /*There might be few movies with different year, so if bsearch didn't return one we wanted
    we check and easily look for right one by checking previuos or following movies (they are sorted)*/
  for (int i = 1; true; i++)
  {
    if (curr_movie.year < movie.year)
    {
      int offset = *((int *)offset_addr + i);
      curr_movie.set_info((char *)movieFile + offset);
    }
    else if (curr_movie.year > movie.year)
    {
      int offset = *((int *)offset_addr - i);
      curr_movie.set_info((char *)movieFile + offset);
    }
    else
      break;
  }
  //Getting actor information for each actor and pushing it into vector;
  actor_informationT tmp_actor;
  for (int i = 0; i < curr_movie.num_actors; i++)
  {
    int act_offs = curr_movie.actor_offsets[i];
    tmp_actor.set_info((char *)actorFile + act_offs);
    players.push_back(tmp_actor.name);
  }
  return true;
}

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM..
const void *imdb::acquireFileMap(const string &fileName, struct fileInfo &info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo &info)
{
  if (info.fileMap != NULL)
    munmap((char *)info.fileMap, info.fileSize);
  if (info.fd != -1)
    close(info.fd);
}