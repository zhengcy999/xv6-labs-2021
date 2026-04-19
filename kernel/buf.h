struct buf {
  int valid;
  int disk;
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  
  // 用于哈希桶的链表（可以是双向也可以是单向，取决于你的实现）
  struct buf *prev; 
  struct buf *next;
  
  uchar data[BSIZE];
  
  // 新增：LRU 时间戳，用于在 bget 发生未命中时寻找牺牲者
  uint timestamp; 
};


