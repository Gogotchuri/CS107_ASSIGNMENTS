#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "hashset.h"
#include "vector.h"
#include "html-utils.h"

/*We always need those 3 together so why don't we group them*/
typedef struct{
  hashset * statistics;
  hashset * swords;
  hashset * used_articles;
}triple_set;

static void Welcome(const char *welcomeTextFileName);
static void BuildIndices(const char *feedsFileName, triple_set * ts);
static void ProcessFeed(const char *remoteDocumentName, triple_set * ts);
static void PullAllNewsItems(urlconnection *urlconn, triple_set * ts);
static bool GetNextItemTag(streamtokenizer *st);
static void ProcessSingleNewsItem(streamtokenizer *st, triple_set * ts);
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength);
static void ParseArticle(const char *articleTitle, const char *articleDescription, 
    const char *articleURL, triple_set * ts);
static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *unused, 
    const char *articleURL, hashset * stop_words, hashset * statistics);
static void QueryIndices(hashset * stop_words, hashset * statistics);
static void ProcessResponse(const char *word, hashset * stop_words, hashset * statistics);
static bool WordIsWellFormed(const char *word);

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";

/*Necessary structs to obtain and transfer all the information*/

/*Connects article with specified word occurring more than once*/
#define title_buffer_size  512
typedef struct{
  int occ_count;
  char title[title_buffer_size];
  char addr[title_buffer_size];
}occurances;

//similarity defined wrongly, if titles are similar it's silimar only when server matches!
int occurances_cmp_simiarity(const void * elem1, const void * elem2){
  occurances occ1 = *(occurances*)elem1;
  occurances occ2 = *(occurances*)elem2;
  int addrcmp = strcasecmp(occ1.addr, occ2.addr);
  int titlecmp = strcasecmp(occ1.title, occ2.title);
  if(!(addrcmp && titlecmp)) return 0;
  return 1;

}
//to sort vector in desending order
int occurances_cmp_num(const void *elemAddr1, const void *elemAddr2){
  occurances occ1 = *(occurances*)elemAddr1;
  occurances occ2 = *(occurances*)elemAddr2;
  int n1 = occ1.occ_count;
  int n2 = occ2.occ_count;
  if(n1 < n2) return 1;
  if(n1 > n2) return -1;
  return 0;
}

/*Convenient struct to save in hashset*/
#define word_buffer_size 256
typedef struct{
  char word[word_buffer_size];
  vector vec;
}word_to_articles;

//-----------------------------Statistics HashSet---------------------------//
/*Hash Function to hash hashset elements, which are structs
  word_to_articles:
      typedef struct{
        char * word;
        vector vec;
      }word_to_articles;

  Hashing by char* word. hashset contains raw stuctures not pointers;
*/
static const signed long kHashMultiplier = -1664117991L;
static int statistics_hash(const void *elemAddr, int numBuckets){
  word_to_articles woa = *(word_to_articles*)elemAddr;
  const char * word = woa.word;            
  unsigned long hashcode = 0;
  
  for (int i = 0; i < strlen(word); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(word[i]);  
  
  return hashcode % numBuckets;                                
}

/*Free function for statistics hashset*/
void statistics_hs_free(void * elem){
  word_to_articles woa = *(word_to_articles*)elem;
  VectorDispose(&(woa.vec));
}

/*compare function for hash set*/
static int statistics_hs_cmp(const void *elem1, const void *elem2){
  word_to_articles woa1 = *(word_to_articles*)elem1;
  word_to_articles woa2 = *(word_to_articles*)elem2;
  const char * s1 = woa1.word;
  const char * s2 = woa2.word;
  return strcasecmp(s1, s2);
}

/*Printing elements of hashset by mapping, for testing purposes*/
void statistics_hs_print(void *elemAddr, void *auxData){
  word_to_articles woa = *(word_to_articles *) elemAddr;
  const char * s = woa.word;
  int n = VectorLength(&(woa.vec));
  printf("Word %s Occured %d times\n", s, n);
}

//-----------------------------String HashSet--------------------------//
/*Printing elements of hashset by mapping, for testing purposes*/
void strings_hs_print(void *elemAddr, void *auxData){
  const char * s = *(char**)elemAddr;
  printf("%s\n", s);
}

/*Free function for hashset*/
void strings_hs_free(void * elem){
  free((*(char **)elem));
}

/*compare function for hash set*/
static int strings_hs_cmp(const void *elem1, const void *elem2){
  const char * s1 = *(char **)elem1;
  const char * s2 = *(char **)elem2;
  return strcasecmp(s1, s2);
}

/*Hash Function for hashset containing char* strings*/
static int strings_hash(const void *elemAddr, int numBuckets){
  const char * s = *(char **)elemAddr;            
  unsigned long hashcode = 0;
  
  for (int i = 0; i < strlen(s); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  
  return hashcode % numBuckets;                                
}

/*Reading stopwords out of stop word file and writing it to the hashset*/
static const char *const stop_words_file = "data/stop-words.txt";
static void read_swords(hashset * hs){
  FILE *infile;
  streamtokenizer st;
  char buffer[32];
  
  infile = fopen(stop_words_file, "r");
  assert(infile != NULL);    
  
  STNew(&st, infile, "\n", true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    char  * tmp = strdup(buffer);
    HashSetEnter(hs, &(tmp));
  }
  
  STDispose(&st);
  fclose(infile);
}

/*Allocates 3 sets and returns triplet of those sets*/
static const int stop_word_buckets = 1009;
static const int articles_buckets = 313;
static const int statistics_buckets = 10007;
void * alloc_sets(){
  triple_set * ts = malloc(sizeof(triple_set));
  //size of hashset
  int alloc_size = sizeof(hashset);

  hashset * stop_words = malloc(alloc_size);//hashset<char*>
  hashset * statistics = malloc(alloc_size); //hashset<word_to_articles>
  hashset * used_articles = malloc(alloc_size);//hashset<char*>
  HashSetNew(stop_words, sizeof(char *), stop_word_buckets, 
    strings_hash, strings_hs_cmp, strings_hs_free);
  HashSetNew(used_articles, sizeof(char *), articles_buckets, 
    strings_hash, strings_hs_cmp, strings_hs_free);
  HashSetNew(statistics, sizeof(word_to_articles), statistics_buckets, 
    statistics_hash, statistics_hs_cmp, statistics_hs_free);

  ts->swords = stop_words;
  ts->statistics = statistics;
  ts->used_articles = used_articles;
  return ts;
}

/*Disposes sets and frees allocated memory.
  optimal way to use this function:
  use on triple_set * returned by alloc_sets function*/
void dispose_sets(triple_set * st){
  HashSetDispose(st->swords);
  HashSetDispose(st->statistics);
  HashSetDispose(st->used_articles);
  free(st->swords);
  free(st->statistics);
  free(st->used_articles);
  free(st);
}
/**
 * Function: main
 * --------------
 * Serves as the entry point of the full application.
 * You'll want to update main to declare several hashsets--
 * one for stop words, another for previously seen urls, etc--
 * and pass them (by address) to BuildIndices and QueryIndices.
 * In fact, you'll need to extend many of the prototypes of the
 * supplied helpers functions to take one or more hashset *s.
 *
 * Think very carefully about how you're going to keep track of
 * all of the stop words, how you're going to keep track of
 * all the previously seen articles, and how you're going to 
 * map words to the collection of news articles where that
 * word appears.
 */
static const char *const kWelcomeTextFile = "data/welcome.txt";
static const char *const filePrefix = "file://";
static const char *const kDefaultFeedsFile = "data/test.txt";

int main(int argc, char **argv) {
  triple_set * ts = alloc_sets();
  read_swords(ts->swords);

  setbuf(stdout, NULL);
  Welcome(kWelcomeTextFile);
  BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1], ts);
  //HashSetMap(&statistics, statistics_hs_print, NULL);
  QueryIndices(ts->swords, ts->statistics);
  dispose_sets(ts); 
  return 0;
}

/** 
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 */
 
static const char *const kNewLineDelimiters = "\r\n";
static void Welcome(const char *welcomeTextFileName)
{
  FILE *infile;
  streamtokenizer st;
  char buffer[1024];
  
  infile = fopen(welcomeTextFileName, "r");
  assert(infile != NULL);    
  
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    printf("%s\n", buffer);
  }
  
  printf("\n");
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one.. 
  fclose(infile);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remore xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 */

static void BuildIndices(const char *feedsFileName, triple_set * ts)
{
  FILE *infile;
  streamtokenizer st;
  char remoteFileName[1024];
  
  infile = fopen(feedsFileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
    STSkipOver(&st, ": ");     // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, remoteFileName, sizeof(remoteFileName));   
    ProcessFeed(remoteFileName, ts);
  }
  
  STDispose(&st);
  fclose(infile);
  printf("\n");
}


/**
 * Function: ProcessFeedFromFile
 * ---------------------
 * ProcessFeed locates the specified RSS document, from locally
 */

static void ProcessFeedFromFile(char *fileName, triple_set * ts)
{
  FILE *infile;
  streamtokenizer st;
  char articleDescription[1024];
  articleDescription[0] = '\0';
  infile = fopen((const char *)fileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kTextDelimiters, true);
  ScanArticle(&st, (const char *)fileName, articleDescription, (const char *)fileName, ts->swords, ts->statistics);
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one..
  fclose(infile);

}

/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and inspect the documentation
 * for ParseArticle for information about what the different response codes mean.
 */

static void ProcessFeed(const char *remoteDocumentName, triple_set * ts)
{

  if(!strncmp(filePrefix, remoteDocumentName, strlen(filePrefix))){
    ProcessFeedFromFile((char *)remoteDocumentName + strlen(filePrefix), ts);
    return;
  }

  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, remoteDocumentName);
  URLConnectionNew(&urlconn, &u);
  
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.serverName);
              break;
      case 200: PullAllNewsItems(&urlconn, ts);
                break;
      case 301: 
      case 302: ProcessFeed(urlconn.newUrl, ts);
                break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
          u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
         break;
  };
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the names and
 * URLs of online news articles.  Check out "datafiles/sample-rss-feed.txt" for an idea of what an
 * RSS feed from the www.nytimes.com (or anything other server that syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * PullAllNewsItems reads and discards all characters up through the opening <item> tag (discarding the <item> tag
 * as well, because once it's read and indentified, it's been pulled,) and then hands the state of the stream to
 * ProcessSingleNewsItem, which handles the job of pulling and analyzing everything up through and including the </item>
 * tag. PullAllNewsItems processes the entire RSS feed and repeatedly advancing to the next <item> tag and then allowing
 * ProcessSingleNewsItem do process everything up until </item>.
 */

static void PullAllNewsItems(urlconnection *urlconn, triple_set * ts)
{
  streamtokenizer st;
  STNew(&st, urlconn->dataStream, kTextDelimiters, false);
  while (GetNextItemTag(&st)) { // if true is returned, then assume that <item ...> has just been read and pulled from the data stream
    ProcessSingleNewsItem(&st, ts);
  }
  
  STDispose(&st);
}

/**
 * Function: GetNextItemTag
 * ------------------------
 * Works more or less like GetNextTag below, but this time
 * we're searching for an <item> tag, since that marks the
 * beginning of a block of HTML that's relevant to us.  
 * 
 * Note that each tag is compared to "<item" and not "<item>".
 * That's because the item tag, though unlikely, could include
 * attributes and perhaps look like any one of these:
 *
 *   <item>
 *   <item rdf:about="Latin America reacts to the Vatican">
 *   <item requiresPassword=true>
 *
 * We're just trying to be as general as possible without
 * going overboard.  (Note that we use strncasecmp so that
 * string comparisons are case-insensitive.  That's the case
 * throughout the entire code base.)
 */

static const char *const kItemTagPrefix = "<item";
static bool GetNextItemTag(streamtokenizer *st)
{
  char htmlTag[1024];
  while (GetNextTag(st, htmlTag, sizeof(htmlTag))) {
    if (strncasecmp(htmlTag, kItemTagPrefix, strlen(kItemTagPrefix)) == 0) {
      return true;
    }
  }  
  return false;
}

/**
 * Function: ProcessSingleNewsItem
 * -------------------------------
 * Code which parses the contents of a single <item> node within an RSS/XML feed.
 * At the moment this function is called, we're to assume that the <item> tag was just
 * read and that the streamtokenizer is currently pointing to everything else, as with:
 *   
 *      <title>Carrie Underwood takes American Idol Crown</title>
 *      <description>Oklahoma farm girl beats out Alabama rocker Bo Bice and 100,000 other contestants to win competition.</description>
 *      <link>http://www.nytimes.com/frontpagenews/2841028302.html</link>
 *   </item>
 *
 * ProcessSingleNewsItem parses everything up through and including the </item>, storing the title, link, and article
 * description in local buffers long enough so that the online new article identified by the link can itself be parsed
 * and indexed.  We don't rely on <title>, <link>, and <description> coming in any particular order.  We do asssume that
 * the link field exists (although we can certainly proceed if the title and article descrption are missing.)  There
 * are often other tags inside an item, but we ignore them.
 */

static const char *const kItemEndTag = "</item>";
static const char *const kTitleTagPrefix = "<title";
static const char *const kDescriptionTagPrefix = "<description";
static const char *const kLinkTagPrefix = "<link";
static void ProcessSingleNewsItem(streamtokenizer *st, triple_set * ts)
{
  char htmlTag[1024];
  char articleTitle[1024];
  char articleDescription[1024];
  char articleURL[1024];
  articleTitle[0] = articleDescription[0] = articleURL[0] = '\0';
  
  while (GetNextTag(st, htmlTag, sizeof(htmlTag)) && (strcasecmp(htmlTag, kItemEndTag) != 0)) {
    if (strncasecmp(htmlTag, kTitleTagPrefix, strlen(kTitleTagPrefix)) == 0) 
      ExtractElement(st, htmlTag, articleTitle, sizeof(articleTitle));
    if (strncasecmp(htmlTag, kDescriptionTagPrefix, strlen(kDescriptionTagPrefix)) == 0) 
      ExtractElement(st, htmlTag, articleDescription, sizeof(articleDescription));
    if (strncasecmp(htmlTag, kLinkTagPrefix, strlen(kLinkTagPrefix)) == 0) 
      ExtractElement(st, htmlTag, articleURL, sizeof(articleURL));
  } 
  char * dummy = articleURL;
  if (strncmp(articleURL, "", sizeof(articleURL)) == 0) return;// punt, since it's not going to take us anywhere
  if(HashSetLookup(ts->used_articles, &dummy) == NULL)
    ParseArticle(articleTitle, articleDescription, articleURL, ts);
  else{
    char * tmp = strdup(articleURL);
    HashSetEnter(ts->used_articles, &tmp);
  }
}

/**
 * Function: ExtractElement
 * ------------------------
 * Potentially pulls text from the stream up through and including the matching end tag.  It assumes that
 * the most recently extracted HTML tag resides in the buffer addressed by htmlTag.  The implementation
 * populates the specified data buffer with all of the text up to but not including the opening '<' of the
 * closing tag, and then skips over all of the closing tag as irrelevant.  Assuming for illustration purposes
 * that htmlTag addresses a buffer containing "<description" followed by other text, these three scanarios are
 * handled:
 *
 *    Normal Situation:     <description>http://some.server.com/someRelativePath.html</description>
 *    Uncommon Situation:   <description></description>
 *    Uncommon Situation:   <description/>
 *
 * In each of the second and third scenarios, the document has omitted the data.  This is not uncommon
 * for the description data to be missing, so we need to cover all three scenarious (I've actually seen all three.)
 * It would be quite unusual for the title and/or link fields to be empty, but this handles those possibilities too.
 */
 
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength)
{
  assert(htmlTag[strlen(htmlTag) - 1] == '>');
  if (htmlTag[strlen(htmlTag) - 2] == '/') return;    // e.g. <description/> would state that a description is not being supplied
  STNextTokenUsingDifferentDelimiters(st, dataBuffer, bufferLength, "<");
  RemoveEscapeCharacters(dataBuffer);
  if (dataBuffer[0] == '<') strcpy(dataBuffer, "");  // e.g. <description></description> also means there's no description
  STSkipUntil(st, ">");
  STSkipOver(st, ">");
}

/** 
 * Function: ParseArticle
 * ----------------------
 * Attempts to establish a network connect to the news article identified by the three
 * parameters.  The network connection is either established of not.  The implementation
 * is prepared to handle a subset of possible (but by far the most common) scenarios,
 * and those scenarios are categorized by response code:
 *
 *    0 means that the server in the URL doesn't even exist or couldn't be contacted.
 *    200 means that the document exists and that a connection to that very document has
 *        been established.
 *    301 means that the document has moved to a new location
 *    302 also means that the document has moved to a new location
 *    4xx and 5xx (which are covered by the default case) means that either
 *        we didn't have access to the document (403), the document didn't exist (404),
 *        or that the server failed in some undocumented way (5xx).
 *
 * The are other response codes, but for the time being we're punting on them, since
 * no others appears all that often, and it'd be tedious to be fully exhaustive in our
 * enumeration of all possibilities.
 */

static void ParseArticle(const char *articleTitle, const char *articleDescription, 
    const char *articleURL, triple_set * ts){
  url u;
  urlconnection urlconn;
  streamtokenizer st;

  URLNewAbsolute(&u, articleURL);
  URLConnectionNew(&urlconn, &u);
  
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
        break;
      case 200: printf("Scanning \"%s\" from \"http://%s\"\n", articleTitle, u.serverName);
        STNew(&st, urlconn.dataStream, kTextDelimiters, false);
        ScanArticle(&st, articleTitle, articleDescription, articleURL, ts->swords, ts->statistics);
        STDispose(&st);
    break;
      case 301:
      case 302: // just pretend we have the redirected URL all along, though index using the new URL and not the old one...
                ParseArticle(articleTitle, articleDescription, urlconn.newUrl, ts);
    break;
      default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
         break;
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/*Takes word and adds to satistics, but for now we havee serious bugs here counting doesn't work properly and 
sometime words that aren't in file is counted, don't know reason yet!*/
void process_word(char * word, hashset * statistics, const char * articleURL, const char * articleTitle){
  word_to_articles woa;
  strcpy(woa.word, word);
  void * addr = HashSetLookup(statistics, &woa);
  occurances occ;
  strcpy(occ.addr, articleURL);
  strcpy(occ.title, articleTitle);
  //The word has never been considered before
  if(addr == NULL){
    VectorNew(&(woa.vec), sizeof(occurances), NULL, 50);
    occ.occ_count = 1;
    VectorAppend(&(woa.vec), &occ);
    HashSetEnter(statistics, &woa);
    return;
  }

  word_to_articles * old_woa = (word_to_articles*)addr;
  vector * vec = &(old_woa->vec);
  int index = VectorSearch(vec, &occ, occurances_cmp_simiarity, 0, false);
  if(index == -1){
    occ.occ_count = 1;
    VectorAppend(vec, &occ);
    return;
  }

  occurances * recent = (occurances*)VectorNth(vec, index);
  recent->occ_count++;
}
/**
 * Function: ScanArticle
 * ---------------------
 * Parses the specified article, skipping over all HTML tags, and counts the numbers
 * of well-formed words that could potentially serve as keys in the set of indices.
 * Once the full article has been scanned, the number of well-formed words is
 * printed, and the longest well-formed word we encountered along the way
 * is printed as well.
 *
 * This is really a placeholder implementation for what will ultimately be
 * code that indexes the specified content.
 */

static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *unused, 
  const char *articleURL, hashset * stop_words, hashset * statistics){
  int numWords = 0;
  char word[1024];
  char * tmp = word;
  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
      continue;
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWellFormed(word) && HashSetLookup(stop_words, &tmp) == NULL) {
        numWords++;
      }else continue;
    }
    process_word(word, statistics, articleURL, articleTitle);

  }

  printf("We got %d words from article named: %s, With url: %s\n", numWords, articleTitle, articleURL);
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */

static void QueryIndices(hashset * stop_words, hashset * statistics)
{
  char response[1024];
  while (true) {
    printf("Please enter a single search term [enter to break]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response, stop_words, statistics);
  }
}

/*Print articles and number of occurances for each article.
  @param curr_list is a c vector of occurances structs*/
static void print_result (vector * curr_list){
  int occ_article_num = VectorLength(curr_list);
  VectorSort(curr_list, occurances_cmp_num);
  for(int i = 1; i <= ((occ_article_num < 10)? occ_article_num : 10); i++){
    occurances * curr_occ = (occurances *)VectorNth(curr_list, i-1);
    char *title = curr_occ->title;
    char *addr = curr_occ->addr;
    printf("%d.) \"%s\" [search term occurs %d time%s]\n\"%s\"\n", i, title,
      curr_occ->occ_count, (curr_occ->occ_count == 1)? "" : "s",addr);
  }

}
/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char *word, hashset * stop_words, hashset * statistics){

  if (WordIsWellFormed(word)) {
    word_to_articles woa;
    strcpy(woa.word, word);
    if(HashSetLookup(stop_words, &word) != NULL){
      printf("Too common a word to be taken seriously. Try something more specific.\n");
      return;
    }
    word_to_articles * struct_addr = (word_to_articles *)HashSetLookup(statistics, &woa);
    int occ_article_num = (struct_addr == NULL) ? 0 : VectorLength(&(struct_addr->vec));

    if(occ_article_num) print_result(&(struct_addr->vec));

    else printf("None of today's news articles contain the word \"%s\".\n", word);

  } else
    printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
}

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with 
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.  
 */

static bool WordIsWellFormed(const char *word)
{
  int i;
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}
