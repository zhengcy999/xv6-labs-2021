// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
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

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

#define NBUCKETS 13

struct {
  struct spinlock lock;            // 全局锁：用于保护整个替换（eviction）逻辑
  struct buf buf[NBUF];            // 所有的缓存块

  // 哈希桶：每个桶是一个链表的头部
  struct buf buckets[NBUCKETS];    
  
  // 每个桶对应一把锁：这才是降低竞争的关键
  struct spinlock bucket_locks[NBUCKETS]; 
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.bucket_locks[i],"bcache");
    bcache.buckets[i].next=0;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->next=bcache.buckets[0].next;
    b->prev = &bcache.buckets[0];
    if(bcache.buckets[0].next != 0) // 如果后面本来就有节点
      bcache.buckets[0].next->prev = b;
    bcache.buckets[0].next=b;
    b->timestamp=0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int idx=blockno%NBUCKETS;
  //1. acquire the bucket locker to check
  acquire(&bcache.bucket_locks[idx]);
  //第一阶段：桶内快速查找 (Fast Path) 首先只锁定目标桶，看看块是否已经在缓存里。
  for (b = bcache.buckets[idx].next;b!=0;b=b->next){
    if (b->dev==dev && b->blockno==blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 如果没找到，说明发生了缓存未命中 (Cache Miss)
  // 我们需要释放桶锁，去寻找一个牺牲者 (Victim)
  release(&bcache.bucket_locks[idx]);
  //第二阶段：准备驱逐牺牲者 (Slow Path - Eviction) 当缓存没命中时，我们需要全场寻找最久没用的块（timestamp 最小）。为了保证过程原子性，这里要用全局锁。
  acquire(&bcache.lock);
  // 在我们释放桶锁到拿到全局锁的间隙，可能有其他 CPU 把这个块读进来了
  acquire(&bcache.bucket_locks[idx]);
  for (b = bcache.buckets[idx].next;b!=0;b=b->next){
    if (b->dev==dev && b->blockno==blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[idx]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[idx]);
  //第三阶段：全场搜寻并跨桶迁移寻找 refcnt == 0 且 timestamp 最小的 buffer，并把它从旧桶搬到新桶。
  struct buf *victim=0;
  uint min_ticks=0xffffffff;
  for (b=bcache.buf;b<bcache.buf+NBUF;b++){
    if (b->refcnt==0 && b->timestamp<min_ticks){
      min_ticks=b->timestamp;
      victim=b;
    }
  }
  if(victim == 0) panic("bget: no buffers");
  // 4. 将牺牲者从旧桶搬运到当前桶 (idx)
  int old_idx=victim->blockno%NBUCKETS;
  // 如果牺牲者不在当前桶，需要拿旧桶的锁来修改链表
  if (old_idx!=idx){
    acquire(&bcache.bucket_locks[old_idx]);
    // 从旧桶双向链表中删除 (你的双向链表逻辑在这里派上用场)
    victim->prev->next = victim->next;
    if(victim->next)
      victim->next->prev = victim->prev;
    
    release(&bcache.bucket_locks[old_idx]);
    // 插入到新桶 idx 的头部
    acquire(&bcache.bucket_locks[idx]);
    victim->next = bcache.buckets[idx].next;
    victim->prev = &bcache.buckets[idx];
    if(bcache.buckets[idx].next)
      bcache.buckets[idx].next->prev = victim;
    bcache.buckets[idx].next = victim;
  }else {
    // 如果碰巧在同一个桶，只需要拿一个锁（或者已经拿着了）
    acquire(&bcache.bucket_locks[idx]);
  }
  // 5. 更新 buffer 信息
  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;

  release(&bcache.bucket_locks[idx]);
  release(&bcache.lock);
  acquiresleep(&victim->lock);
  return victim;
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
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int idx=b->blockno%NBUCKETS;

  acquire(&bcache.bucket_locks[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp=ticks;
  }
  
  release(&bcache.bucket_locks[idx]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


