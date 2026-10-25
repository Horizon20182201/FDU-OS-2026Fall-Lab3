// Buffer cache.
//
// The buffer cache is a hash table of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct bucket {
  struct spinlock lock;
  struct buf *head;
};

struct {
  struct bucket bucket[NBUCKET];
  // Serializes cache misses and protects allocation/eviction cursors.
  struct spinlock evictlock;
  struct buf buf[NBUF];
  uint nused;
  uint victim;
} bcache;

static uint
bhash(uint dev, uint blockno)
{
  return (blockno + dev) % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.evictlock, "bcache.evict");
  for(int i = 0; i < NBUCKET; i++)
    initlock(&bcache.bucket[i].lock, "bcache.bucket");

  // Unassigned buffers enter the hash table on their first allocation.
  for(b = bcache.buf; b < bcache.buf+NBUF; b++)
    initsleeplock(&b->lock, "buffer");
}

// The caller holds the bucket lock, which protects membership and refcnt.
static struct buf*
blookup(struct bucket *bucket, uint dev, uint blockno)
{
  for(struct buf *b = bucket->head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      return b;
    }
  }
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bucket *bucket = &bcache.bucket[bhash(dev, blockno)];

  acquire(&bucket->lock);
  b = blookup(bucket, dev, blockno);
  release(&bucket->lock);
  if(b){
    acquiresleep(&b->lock);
    return b;
  }

  // Never acquire evictlock while holding a bucket lock. Recheck after
  // acquiring it: another miss may have installed this block meanwhile.
  acquire(&bcache.evictlock);
  acquire(&bucket->lock);
  b = blookup(bucket, dev, blockno);
  release(&bucket->lock);
  if(b){
    release(&bcache.evictlock);
    acquiresleep(&b->lock);
    return b;
  }

  if(bcache.nused < NBUF){
    b = &bcache.buf[bcache.nused++];
  } else {
    // Round-robin replacement can reuse any idle buffer, in any bucket.
    for(int i = 0; i < NBUF; i++){
      struct buf *candidate = &bcache.buf[bcache.victim];
      bcache.victim = (bcache.victim + 1) % NBUF;
      // Only the evictlock holder can change a buffer's identity.
      struct bucket *old =
        &bcache.bucket[bhash(candidate->dev, candidate->blockno)];
      acquire(&old->lock);
      if(candidate->refcnt == 0){
        struct buf **link = &old->head;
        while(*link != candidate)
          link = &(*link)->next;
        *link = candidate->next;
        b = candidate;
        release(&old->lock);
        break;
      }
      release(&old->lock);
    }
  }
  if(b == 0)
    panic("bget: no buffers");

  // The buffer is now private: it has no references and is not in a bucket.
  // Hold only one bucket lock at a time, even when old == bucket.
  acquire(&bucket->lock);
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  b->next = bucket->head;
  bucket->head = b;
  release(&bucket->lock);
  release(&bcache.evictlock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // Our reference keeps the buffer's identity (and bucket) stable.
  struct bucket *bucket = &bcache.bucket[bhash(b->dev, b->blockno)];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}

void
bpin(struct buf *b) {
  struct bucket *bucket = &bcache.bucket[bhash(b->dev, b->blockno)];
  acquire(&bucket->lock);
  b->refcnt++;
  release(&bucket->lock);
}

void
bunpin(struct buf *b) {
  struct bucket *bucket = &bcache.bucket[bhash(b->dev, b->blockno)];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}


