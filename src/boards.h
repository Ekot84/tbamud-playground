#ifndef _BOARDS_H_
#define _BOARDS_H_

/* Dynamic Boards v2.4 - TBA Mud Conversion */

/* Directory where boards are stored */
#define BOARD_DIRECTORY "boards"

/* Max length of a message */
#define MAX_MESSAGE_LENGTH 4096

/* Magic number used to detect board editing */
#define BOARD_MAGIC 1048575

/* Current board version (for file format) */
#define CURRENT_BOARD_VER 2

/* Message on the board */
struct board_msg {
  long poster;                 /* ID of the poster (old format) */
  time_t timestamp;            /* When the message was posted */
  char *subject;               /* Subject line */
  char *data;                  /* Message text */
  struct board_msg *next;      /* Next message in the list */
  struct board_msg *prev;      /* Previous message in the list */
  char *name;                  /* Name of the poster (new format) */
  time_t reply_to;             /* Timestamp of the message this replies to, 0 if none */
};

/* Per-player memory to track read messages */
struct board_memory {
  int timestamp;
  int reader;
  struct board_memory *next;
  char *name;
};

/* Board structure */
struct board_info {
  int read_lvl;
  int write_lvl;
  int remove_lvl;
  int num_messages;
  int vnum;
  int version;
  struct board_info *next;
  struct board_msg *messages;
  struct board_memory *memory[301];
};

/* Macros for convenience */
#define READ_LVL(i)           ((i)->read_lvl)
#define WRITE_LVL(i)          ((i)->write_lvl)
#define REMOVE_LVL(i)         ((i)->remove_lvl)
#define BOARD_MNUM(i)         ((i)->num_messages)
#define BOARD_VNUM(i)         ((i)->vnum)
#define BOARD_NEXT(i)         ((i)->next)
#define BOARD_MESSAGES(i)     ((i)->messages)
#define BOARD_MEMORY(i,j)     ((i)->memory[j])
#define BOARD_VERSION(i)      ((i)->version)

#define MESG_POSTER(i)        ((i)->poster)
#define MESG_TIMESTAMP(i)     ((i)->timestamp)
#define MESG_SUBJECT(i)       ((i)->subject)
#define MESG_DATA(i)          ((i)->data)
#define MESG_NEXT(i)          ((i)->next)
#define MESG_PREV(i)          ((i)->prev)
#define MESG_POSTER_NAME(i)   ((i)->name)
#define MESG_REPLY_TO(i)      ((i)->reply_to)

#define MEMORY_TIMESTAMP(i)   ((i)->timestamp)
#define MEMORY_READER(i)      ((i)->reader)
#define MEMORY_NEXT(i)        ((i)->next)
#define MEMORY_READER_NAME(i) ((i)->name)

#define VAL_BOARD_READ 0
#define VAL_BOARD_WRITE 1
#define VAL_BOARD_ERASE 2

/* Function prototypes */
void init_boards(void);
struct board_info *create_new_board(obj_vnum board_vnum);
struct board_info *load_board(obj_vnum board_vnum);
int save_board(struct board_info *temp_board);
void clear_boards(void);
void clear_one_board(struct board_info *temp_board);
int parse_message(FILE *fl, struct board_info *temp_board);
void look_at_boards(void);
void show_board(obj_vnum board_vnum, struct char_data *ch);
void board_display_msg(obj_vnum board_vnum, struct char_data *ch, int arg);
int mesglookup(struct board_msg *message, struct char_data *ch, struct board_info *board);
void write_board_message(obj_vnum board_vnum, struct char_data *ch, char *arg);
void board_respond(obj_vnum board_vnum, struct char_data *ch, int mnum);
struct board_info *locate_board(obj_vnum board_vnum);
void remove_board_msg(obj_vnum board_vnum, struct char_data *ch, int arg);

#endif /* _BOARDS_H_ */
