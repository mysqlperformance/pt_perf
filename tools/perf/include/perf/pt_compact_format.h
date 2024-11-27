
#ifndef _PT_SCRIPT_FORMAT
#define _PT_SCRIPT_FORMAT

#define PT_FILE_BLOCK_SIZE 65536

typedef unsigned char byte;
enum {
  PT_ACTION_TYPE_UNDEFINE = 0,
  PT_ACTION_TYPE_BRANCH,
  PT_ACTION_TYPE_SYMBOL,
  PT_ACTION_TYPE_ERROR
};

enum {
  PT_ACTION_CALL = 0,
  PT_ACTION_RETURN,
  PT_ACTION_JCC,
  PT_ACTION_JMP,
  PT_ACTION_TR_START,
  PT_ACTION_TR_END_CALL,
  PT_ACTION_TR_END_RETURN,
  PT_ACTION_TR_END_HW_INT,
  PT_ACTION_TR_END_SYSCALL,
  PT_ACTION_TR_END,
  PT_ACTION_INT,
  PT_ACTION_IRET,
  PT_ACTION_SYSCALL,
  PT_ACTION_SYSRET,
  PT_ACTION_ASYNC,
  PT_ACTION_HW_INT,
  PT_ACTION_TX_ABRT,
  PT_ACTION_VMENTRY,
  PT_ACTION_VMEXIT,
  PT_ACTION_UNKNOWN_FLAG
};

static inline void write4bytes(byte *b, uint64_t n) {
  b[0] = (byte)(n >> 24);
  b[1] = (byte)(n >> 16);
  b[2] = (byte)(n >> 8);
  b[3] = (byte)(n);
}
static inline uint32_t read4bytes(const byte *b) {
  return (((uint32_t)(b[0]) << 24) |
          ((uint32_t)(b[1]) << 16) |
          ((uint32_t)(b[2]) << 8) | (uint32_t)(b[3]));
}

static inline void write3bytes(byte *b, uint64_t n) {
  b[0] = (byte)(n >> 16);
  b[1] = (byte)(n >> 8);
  b[2] = (byte)(n);
}

static inline uint32_t read3bytes(const byte *b) {
  return (((uint32_t)(b[0]) << 16) |
          ((uint32_t)(b[1]) << 8) | (uint32_t)(b[2]));
}

static inline void write2bytes(byte *b, uint64_t n) {
  b[0] = (byte)(n >> 8);
  b[1] = (byte)(n);
}

static inline uint16_t read2bytes(const byte *b) {
  return (((uint64_t)(b[0]) << 8) | (uint64_t)(b[1]));
}

static inline void write1bytes(byte *b, uint64_t n) {
  b[0] = (byte)n;
}

static inline uint8_t read1bytes(const byte *b) {
  return ((uint8_t)(b[0]));
}

static inline void write8bytes(byte *b, uint64_t n) {
  write4bytes((byte *)(b), (uint64_t)(n >> 32));
  write4bytes((byte *)(b) + 4, (uint64_t)n);
}

static inline uint64_t read8bytes(const byte *b) {
  uint64_t val;

  val = read4bytes(b);
  val <<= 32;
  val |= read4bytes(b + 4);
  
  return val;
}

static inline uint32_t write_compressed(byte *b, uint64_t n) {
  if (n < 0x80) {
    /* 0nnnnnnn (7 bits) */
    write1bytes(b, n);
    return (1);
  } else if (n < 0x4000) {
    /* 10nnnnnn nnnnnnnn (14 bits) */
    write2bytes(b, n | 0x8000);
    return (2);
  } else if (n < 0x200000) {
    /* 110nnnnn nnnnnnnn nnnnnnnn (21 bits) */
    write3bytes(b, n | 0xC00000);
    return (3);
  } else if (n < 0x10000000) {
    /* 1110nnnn nnnnnnnn nnnnnnnn nnnnnnnn (28 bits) */
    write4bytes(b, n | 0xE0000000);
    return (4);
  } else {
    /* 11110000 nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn (32 bits) */
    write1bytes(b, 0xF0);
    write4bytes(b + 1, n);
    return (5);
  }
}

static inline uint64_t read_compressed(const byte *b) {
  uint64_t val;

  val = read1bytes(b);

  if (val < 0x80) {
    /* 0nnnnnnn (7 bits) */
  } else if (val < 0xC0) {
    /* 10nnnnnn nnnnnnnn (14 bits) */
    val = read2bytes(b) & 0x3FFF;
  } else if (val < 0xE0) {
    /* 110nnnnn nnnnnnnn nnnnnnnn (21 bits) */
    val = read3bytes(b) & 0x1FFFFF;
  } else if (val < 0xF0) {
    /* 1110nnnn nnnnnnnn nnnnnnnn nnnnnnnn (28 bits) */
    val = read4bytes(b) & 0xFFFFFFF;
  } else {
    /* 11110000 nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn (32 bits) */
    val = read4bytes(b + 1);
  }

  return (val);
}

static inline uint32_t get_compressed_size(uint64_t n) {
  if (n < 0x80) {
    /* 0nnnnnnn (7 bits) */
    return (1);
  } else if (n < 0x4000) {
    /* 10nnnnnn nnnnnnnn (14 bits) */
    return (2);
  } else if (n < 0x200000) {
    /* 110nnnnn nnnnnnnn nnnnnnnn (21 bits) */
    return (3);
  } else if (n < 0x10000000) {
    /* 1110nnnn nnnnnnnn nnnnnnnn nnnnnnnn (28 bits) */
    return (4);
  } else {
    /* 11110000 nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn (32 bits) */
    return (5);
  }
}

static inline void pt_fwrite_action(FILE *fp, byte *b, uint32_t len) {
  uint32_t f_off, block_id, new_block_id;
  f_off = ftell(fp);;
  block_id = f_off / PT_FILE_BLOCK_SIZE;
  new_block_id = (f_off + len) / PT_FILE_BLOCK_SIZE;

  if (new_block_id != block_id) {
    /* mark undefine */
    uint8_t type = PT_ACTION_TYPE_UNDEFINE;
    fwrite(&type, 1, 1, fp);
    /* just extend space */
    fwrite(b, 1, len, fp);
    fseek(fp, new_block_id * PT_FILE_BLOCK_SIZE, SEEK_SET);
  }
  fwrite(b, 1, len, fp);
}

static inline uint32_t pt_fwrite_branch_action(FILE *fp, uint32_t tid,
    uint64_t timestamp, uint32_t flag, uint32_t from_id, uint32_t to_id) {
  uint8_t type = PT_ACTION_TYPE_BRANCH;
  uint32_t total = 0, len;
  byte b[1024];
  byte *p = b;

  write1bytes(p, type);
  p += 1;

  len = write_compressed(p, tid);
  p += len;

  write1bytes(p, flag);
  p += 1;

  write8bytes(p, timestamp);
  p += 8;

  len = write_compressed(p, from_id);
  p += len;

  len = write_compressed(p, to_id);
  p += len;
  
  total = p - b;
  pt_fwrite_action(fp, b, total);
  return total;
}

static inline byte* pt_read_branch_action(byte *ptr, uint32_t *tid,
    uint64_t *timestamp, uint8_t *flag, uint32_t *from_id, uint32_t *to_id) {
  uint32_t len;

  *tid = read_compressed(ptr);
  len = get_compressed_size(*tid);
  ptr += len;

  *flag = read1bytes(ptr);
  ptr += 1;
  
  *timestamp = read8bytes(ptr);
  ptr += 8;

  *from_id = read_compressed(ptr);
  len = get_compressed_size(*from_id);
  ptr += len;

  *to_id = read_compressed(ptr);
  len = get_compressed_size(*to_id);
  ptr += len;

  return ptr;
}

static inline uint32_t pt_fwrite_symbol_action(FILE *fp, uint32_t symbol_id,
    uint64_t address, uint32_t offset, const char *name) {
  uint8_t type = PT_ACTION_TYPE_SYMBOL;
  uint32_t total = 0, len;
  byte b[PT_FILE_BLOCK_SIZE];
  byte *p = b;

  write1bytes(p, type);
  p += 1;

  write4bytes(p, symbol_id);
  p += 4;

  write8bytes(p, address);
  p += 8;

  write4bytes(p, offset);
  p += 4;

  len = strlen(name);

  if (p - b - 2 + 4 + len >= PT_FILE_BLOCK_SIZE) {
    fprintf(stderr, "the symbol is too long to fill in the file block");
    exit(1);
  }

  write4bytes(p, len);
  p += 4;

  memcpy(p, name, len);
  p += len;

  total = p - b;
  pt_fwrite_action(fp, b, total);
  return total;
}

static inline byte* pt_read_symbol_action(byte *ptr, uint32_t *symbol_id,
    uint64_t *address, uint32_t *offset, char **name, uint32_t *name_len) {
  *symbol_id = read4bytes(ptr);
  ptr += 4;

  *address = read8bytes(ptr);
  ptr += 8;

  *offset = read4bytes(ptr);
  ptr += 4;

  *name_len = read4bytes(ptr);
  ptr += 4;
  *name = (char *) ptr;
  ptr += *name_len;

  return ptr;
}

static inline uint32_t pt_fwrite_error_action(FILE *fp, uint32_t tid,
    uint64_t timestamp, uint8_t code) {
  uint8_t type = PT_ACTION_TYPE_ERROR;
  uint32_t total = 0, len;
  byte b[1024];
  byte *p = b;

  write1bytes(p, type);
  p += 1;

  len = write_compressed(p, tid);
  p += len;

  write8bytes(p, timestamp);
  p += 8;

  write1bytes(p, code);
  p += 1;

  total = p - b;
  pt_fwrite_action(fp, b, total);
  return total;
}

static inline byte* pt_read_error_action(byte *ptr, uint32_t *tid,
    uint64_t *timestamp, uint8_t *code) {
  uint32_t len;

  *tid = read_compressed(ptr);
  len = get_compressed_size(*tid);
  ptr += len;

  *timestamp = read8bytes(ptr);
  ptr += 8;

  *code = read1bytes(ptr);
  ptr += 1;

  return ptr;
}
#endif /* _PT_SCRIPT_FORMAT */
