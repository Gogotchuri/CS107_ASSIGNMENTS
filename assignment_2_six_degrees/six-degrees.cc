#include <vector>
#include <list>
#include <set>
#include <string>
#include <iostream>
#include <map>
#include <iomanip>
#include "imdb.h"
#include "path.h"
using namespace std;


/*Standard BFS algorithm with a little tweaks*/
bool generateShortestPath(string source, string target, imdb &db){
  vector<string> players;
  vector<film> films;
  map<string, vector<film> > done_credits; //Saving already generated movies for the actor
  if(!db.getCredits(source, films)) return false;
  done_credits[source] = films;
  if(!db.getCredits(target, films)) return false;
  done_credits[target] = films;
  //Assigning actor with least movies as starting actor;
  string st_act = (done_credits[source].size() > done_credits[target].size()) ? target : source;
  string en_act = (st_act == target) ? source : target;//other one should be the target; 
  list<path> part_paths; // functions as a queue
  set<string> visited_actors;
  set<film> visited_films;

  part_paths.push_back(path(st_act));
  //main BFS loop
  while((!part_paths.empty()) && part_paths.front().getLength() < 6){
    path curr_elem = part_paths.front();
    part_paths.pop_front();
    string curr_player = curr_elem.getLastPlayer();
    //Checking whether we already called getCredits for curr_player
    if(done_credits.count(curr_player) != 0){
      films = done_credits[curr_player];
    }else{
      db.getCredits(curr_player, films);//Vector is automatically cleared in both function
      done_credits[curr_player] = films;
    }

    for(film flm : films){
      if(visited_films.count(flm) != 0) continue;
      visited_films.insert(flm);
      db.getCast(flm, players);
      for(string name : players){
        if(visited_actors.count(name) != 0) continue;
        visited_actors.insert(name);
        path clone_path = curr_elem;
        clone_path.addConnection(flm, name);
        if(name == en_act){ //reached the target
          if(en_act == source)
            clone_path.reverse();
          cout << clone_path << endl;
          return true;
        }
        part_paths.push_back(clone_path);
      }
    }
  }
  return false;
}
/**
 * Using the specified prompt, requests that the user supply
 * the name of an actor or actress.  The code returns
 * once the user has supplied a name for which some record within
 * the referenced imdb existsif (or if the user just hits return,
 * which is a signal that the empty string should just be returned.)
 *
 * @param prompt the text that should be used for the meaningful
 *               part of the user prompt.
 * @param db a reference to the imdb which can be used to confirm
 *           that a user's response is a legitimate one.
 * @return the name of the user-supplied actor or actress, or the
 *         empty string.
 */

static string promptForActor(const string& prompt, const imdb& db)
{
  string response;
  while (true) {
    cout << prompt << " [or <enter> to quit]: ";
    getline(cin, response);
    if (response == "") return "";
    vector<film> credits;
    if (db.getCredits(response, credits)) return response;
    cout << "We couldn't find \"" << response << "\" in the movie database. "
	 << "Please try again." << endl;
  }
}

/**
 * Serves as the main entry point for the six-degrees executable.
 * There are no parameters to speak of.
 *
 * @param argc the number of tokens passed to the command line to
 *             invoke this executable.  It's completely ignored
 *             here, because we don't expect any arguments.
 * @param argv the C strings making up the full command line.
 *             We expect argv[0] to be logically equivalent to
 *             "six-degrees" (or whatever absolute path was used to
 *             invoke the program), but otherwise these are ignored
 *             as well.
 * @return 0 if the program ends normally, and undefined otherwise.
 */

int main(int argc, const char *argv[])
{
  imdb db(determinePathToData(argv[1])); // inlined in imdb-utils.h
  if (!db.good()) {
    cout << "Failed to properly initialize the imdb database." << endl;
    cout << "Please check to make sure the source files exist and that you have permission to read them." << endl;
    return 1;
  }
  
  while (true) {
    string source = promptForActor("Actor or actress", db);
    if (source == "") break;
    string target = promptForActor("Another actor or actress", db);
    if (target == "") break;
    if (source == target) {
      cout << "Good one.  This is only interesting if you specify two different people." << endl;
    } else {
      if(!generateShortestPath(source, target, db))
        cout << endl << "No path between those two people could be found." << endl << endl;
    }
  }
  
  cout << "Thanks for playing!" << endl;
  return 0;
}

