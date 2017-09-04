#ifndef _HASHMGR_HXX_
#define _HASHMGR_HXX_

#include "hunvisapi.h"

#include <stdio.h>

#include "htypes.hxx"
#include "filemgr.hxx"

#ifdef HUNSPELL_CHROME_CLIENT
#include <string>
#include <map>

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "third_party/hunspell/google/bdict_reader.h"
#endif

enum flag { FLAG_CHAR, FLAG_LONG, FLAG_NUM, FLAG_UNI };

class LIBHUNSPELL_DLL_EXPORTED HashMgr
{
#ifdef HUNSPELL_CHROME_CLIENT
  // Not owned by this class, owned by the Hunspell object.
  hunspell::BDictReader* bdict_reader;
  std::map<base::StringPiece, int> custom_word_to_affix_id_map_;
  std::vector<std::string*> pointer_to_strings_;
#endif
  int               tablesize;
  struct hentry **  tableptr;
  int               userword;
  flag              flag_mode;
  int               complexprefixes;
  int               utf8;
  unsigned short    forbiddenword;
  int               langnum;
  char *            enc;
  char *            lang;
  struct cs_info *  csconv;
  char *            ignorechars;
  unsigned short *  ignorechars_utf16;
  int               ignorechars_utf16_len;
  int               numaliasf; // flag vector `compression' with aliases
  unsigned short ** aliasf;
  unsigned short *  aliasflen;
  int               numaliasm; // morphological desciption `compression' with aliases
  char **           aliasm;


public:
#ifdef HUNSPELL_CHROME_CLIENT
  HashMgr(hunspell::BDictReader* reader);

  // Return the hentry corresponding to the given word. Returns NULL if the
  // word is not there in the cache.
  hentry* GetHentryFromHEntryCache(char* word);

  // Called before we do a new operation. This will empty the cache of pointers
  // to hentries that we have cached. In Chrome, we make these on-demand, but
  // they must live as long as the single spellcheck operation that they're part
  // of since Hunspell will save pointers to various ones as it works.
  //
  // This function allows that cache to be emptied and not grow infinitely.
  void EmptyHentryCache();
#else
  HashMgr(const char * tpath, const char * apath, const char * key = NULL);
#endif
  ~HashMgr();

  struct hentry * lookup(const char *) const;
  int hash(const char *) const;
  struct hentry * walk_hashtable(int & col, struct hentry * hp) const;

  int add(const char * word);
  int add_with_affix(const char * word, const char * pattern);
  int remove(const char * word);
  int decode_flags(unsigned short ** result, char * flags, FileMgr * af);
  unsigned short        decode_flag(const char * flag);
  char *                encode_flag(unsigned short flag);
  int is_aliasf();
  int get_aliasf(int index, unsigned short ** fvec, FileMgr * af);
  int is_aliasm();
  char * get_aliasm(int index);

private:
  int get_clen_and_captype(const char * word, int wbl, int * captype);
  int load_tables(const char * tpath, const char * key);
  int add_word(const char * word, int wbl, int wcl, unsigned short * ap,
    int al, const char * desc, bool onlyupcase);
  int load_config(const char * affpath, const char * key);
  int parse_aliasf(char * line, FileMgr * af);

#ifdef HUNSPELL_CHROME_CLIENT
  // Loads the AF lines from a BDICT.
  // A BDICT file compresses its AF lines to save memory.
  // This function decompresses each AF line and call parse_aliasf().
  int LoadAFLines();

  // Helper functions that create a new hentry struct, initialize it, and
  // delete it.
  // These functions encapsulate non-trivial operations in creating and
  // initializing a hentry struct from BDICT data to avoid changing code so much
  // even when a hentry struct is changed.
  hentry* InitHashEntry(hentry* entry,
                        size_t item_size,
                        const char* word,
                        int word_length,
                        int affix_index) const;
  hentry* CreateHashEntry(const char* word,
                          int word_length,
                          int affix_index) const;
  void DeleteHashEntry(hentry* entry) const;

  // Converts the list of affix IDs to a linked list of hentry structures. The
  // hentry structures will point to the given word. The returned pointer will
  // be a statically allocated variable that will change for the next call. The
  // |word| buffer must be the same.
  hentry* AffixIDsToHentry(char* word, int* affix_ids, int affix_count) const;

  // See EmptyHentryCache above. Note that each one is actually a linked list
  // followed by the homonym pointer.
  typedef std::map<std::string, hentry*> HEntryCache;
  HEntryCache hentry_cache;
#endif

  int add_hidden_capitalized_word(char * word, int wbl, int wcl,
    unsigned short * flags, int al, char * dp, int captype);
  int parse_aliasm(char * line, FileMgr * af);
  int remove_forbidden_flag(const char * word);

};

#endif
