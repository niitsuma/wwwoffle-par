/***************************************
  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8d-par.
  Header file for URL hash table functions.

  Written by Paul A. Rombouts.

  This file Copyright (C) 2005  Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef URLHASH_H
#define URLHASH_H    /*+ To stop multiple inclusions. +*/

/* The file name of the url hash table */
#define urlhash_filename "urlhashtable"
#define urlhash_filename_new urlhash_filename ".new"
#define urlhash_filename_old urlhash_filename ".old"

#define NUMURLHASHBUCKETS 0x4000

typedef struct _urlhash_node {
  unsigned next;        /* offset in bytes w.r.t. urlhash_start to the next hash node. */
  md5hash_t h;
  char used;   /* Used to mark nodes during garbage collection. */
  char url[1]; /* The url associated with the hash */
}
urlhash_node;

typedef struct _urlhash_info {
  volatile unsigned cursize;
  unsigned numbuckets;
  unsigned hashtable[0];
}
urlhash_info;


/*+ The starting address of the url hash table +*/
extern void *urlhash_start;

#define urlhash_info_p ((urlhash_info *)urlhash_start)

/*+ The smallest valid offset in the url hash table +*/
extern unsigned urlhash_minsize;

#define urlhash_maxsize (256*1024*1024)

#define urlhash_align(i) (((i)+(sizeof(unsigned)-1)) & ~(sizeof(unsigned)-1))


int urlhash_open();
void urlhash_close();
char *urlhash_lookup(md5hash_t *h);
int urlhash_add(const char *url,md5hash_t *h);
int urlhash_clearmarks();
int urlhash_markhash(md5hash_t *h);
int urlhash_copycompact();
int urlhash_lock_create();
void urlhash_lock_destroy();
int urlhash_lock_rw();
int urlhash_unlock_rw();

#endif /* URLHASH_H */
