/* ************************************************************************
*  File: boards.c                    Dynamic Boards for tbaMUD Playground *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "boards.h"
#include "handler.h"
#include "xdir.h"
#include "modify.h"

/* Global board list */
struct board_info *bboards = NULL;

extern struct descriptor_data *descriptor_list;
extern struct index_data *obj_index;

/* Initialize all boards by scanning the board directory */
void init_boards(void) {
  int i, j;
  long board_vnum;
  struct xap_dir xd;
  struct board_info *tmp_board;
  char dir_name[128];

  if (!insure_directory(BOARD_DIRECTORY, 0)) {
    log("Unable to open/create directory '%s' - Exiting", BOARD_DIRECTORY);
    exit(1);
  }

#if defined(CIRCLE_WINDOWS)
  snprintf(dir_name, sizeof(dir_name), "%s/*", BOARD_DIRECTORY);
#else
  snprintf(dir_name, sizeof(dir_name), "%s", BOARD_DIRECTORY);
#endif

  if ((i = xdir_scan(dir_name, &xd)) <= 0) {
    log("No board files found.");
    return;
  }

  /* Load each board file found */
  for (j = 0; j < i; j++) {
    const char *fname = xdir_get_name(&xd, j);

    if (strcmp(fname, "..") &&
        strcmp(fname, ".") &&
        strcmp(fname, ".cvsignore")) {

      if (sscanf(fname, "%ld", &board_vnum) != 1) {
        log("Skipping invalid board filename '%s'", fname);
        continue;
      }

      if ((tmp_board = load_board(board_vnum)) != NULL) {
        tmp_board->next = bboards;
        bboards = tmp_board;
      }
    }
  }

  /* Log summary info */
  look_at_boards();
}


/* Create a new board object dynamically */
struct board_info *create_new_board(obj_vnum board_vnum) {
  char buf[512];
  FILE *fl;
  struct board_info *temp = NULL, *backup;
  struct obj_data *obj = NULL;

  snprintf(buf, sizeof(buf), "%s/%d", BOARD_DIRECTORY, board_vnum);

  /* object exists, but no board file (yet) */
  if ((fl = fopen(buf, "r"))) {
    fclose(fl);
    log("Preexisting board file when attempting to create new board [vnum: %d]. Attempting to correct.", board_vnum);

    /* unlink file, clear existing board */
    unlink(buf);

    for (temp = bboards, backup = NULL; temp && !backup; temp = temp->next) {
      if (BOARD_VNUM(temp) == board_vnum) {
        backup = temp;
      }
    }
    if (backup) {
      REMOVE_FROM_LIST(backup, bboards, next);
      clear_one_board(backup);
    }
  }

  CREATE(temp, struct board_info, 1);

  if (real_object(board_vnum) == NOTHING) {
    log("Creating board [vnum: %d] though no associated object with that vnum can be found. Using defaults.", board_vnum);
    READ_LVL(temp) = LVL_IMMORT;
    WRITE_LVL(temp) = LVL_IMMORT;
    REMOVE_LVL(temp) = LVL_IMMORT;
  } else {
    obj = &(obj_proto[real_object(board_vnum)]);
    READ_LVL(temp) = GET_OBJ_VAL(obj, VAL_BOARD_READ);
    WRITE_LVL(temp) = GET_OBJ_VAL(obj, VAL_BOARD_WRITE);
    REMOVE_LVL(temp) = GET_OBJ_VAL(obj, VAL_BOARD_ERASE);
  }

  BOARD_VNUM(temp) = board_vnum;
  BOARD_MNUM(temp) = 0;
  BOARD_VERSION(temp) = CURRENT_BOARD_VER;
  temp->next = NULL;
  BOARD_MESSAGES(temp) = NULL;

  if (!save_board(temp)) {
    log("Hm. Error while creating new board file [vnum: %d]. Unable to create new file.", board_vnum);
    free(temp);
    return NULL;
  }

  return temp;
}


struct board_info *load_board(obj_vnum board_vnum) {
  struct board_info *temp_board;
  struct board_msg *bmsg;
  struct obj_data *obj = NULL;
  struct stat st;
  struct board_memory *memboard, *list;
  int t[5], mnum, poster, timestamp, msg_num, retval = 0;
  char filebuf[512], buf[512], poster_name[128];
  FILE *fl;
  int sflag;

  snprintf(filebuf, sizeof(filebuf), "%s/%d", BOARD_DIRECTORY, board_vnum);
  if (!(fl = fopen(filebuf, "r"))) {
    log("Request to open board [vnum %d] failed - unable to open file '%s'.", board_vnum, filebuf);
    return NULL;
  }

  get_line(fl, buf);
  if (strcmp("Board File", buf)) {
    log("Invalid board file '%s' [vnum: %d] - failed to load.", filebuf, board_vnum);
    fclose(fl);
    return NULL;
  }

  CREATE(temp_board, struct board_info, 1);
  temp_board->vnum = board_vnum;

  get_line(fl, buf);

  if ((retval = sscanf(buf, "%d %d %d %d %d", t, t+1, t+2, t+3, t+4)) != 5) {
    if (retval == 4) {
      log("Parse error on board [vnum: %d], file '%s' - attempting to correct [4] args expecting 5.", board_vnum, filebuf);
      t[4] = 1;
    } else {
      log("Parse error on board [vnum: %d], file '%s' - attempting to correct [<4] args expecting 5.", board_vnum, filebuf);
      t[0] = t[1] = t[2] = LVL_IMMORT;
      t[3] = -1;
      t[4] = 1;
    }
  }

  if (real_object(board_vnum) == NOTHING) {
    log("No associated object exists when attempting to create a board [vnum %d].", board_vnum);
    stat(filebuf, &st);
    if (time(NULL) - st.st_mtime > (60*60*24*7)) {
      log("Deleting old board file '%s' [vnum %d].", filebuf, board_vnum);
      unlink(filebuf);
      free(temp_board);
      fclose(fl);
      return NULL;
    }
    READ_LVL(temp_board) = t[0];
    WRITE_LVL(temp_board) = t[1];
    REMOVE_LVL(temp_board) = t[2];
    BOARD_MNUM(temp_board) = t[3];
    BOARD_VERSION(temp_board) = t[4];
  } else {
    obj = &obj_proto[real_object(board_vnum)];
    if (t[0] != GET_OBJ_VAL(obj, VAL_BOARD_READ) ||
        t[1] != GET_OBJ_VAL(obj, VAL_BOARD_WRITE) ||
        t[2] != GET_OBJ_VAL(obj, VAL_BOARD_ERASE)) {
      log("Mismatch in board <-> object read/write/remove settings for board [vnum: %d]. Correcting.", board_vnum);
    }
    READ_LVL(temp_board) = GET_OBJ_VAL(obj, VAL_BOARD_READ);
    WRITE_LVL(temp_board) = GET_OBJ_VAL(obj, VAL_BOARD_WRITE);
    REMOVE_LVL(temp_board) = GET_OBJ_VAL(obj, VAL_BOARD_ERASE);
    BOARD_MNUM(temp_board) = t[3];
    BOARD_VERSION(temp_board) = t[4];
  }

  BOARD_NEXT(temp_board) = NULL;
  BOARD_MESSAGES(temp_board) = NULL;

  msg_num = 0;
  while (get_line(fl, buf)) {
    if (*buf == 'S' && BOARD_VERSION(temp_board) != CURRENT_BOARD_VER) {
      if (sscanf(buf, "S %d %d %d ", &mnum, &poster, &timestamp) == 3) {
        CREATE(memboard, struct board_memory, 1);
        MEMORY_READER(memboard) = poster;
        MEMORY_TIMESTAMP(memboard) = timestamp;
      }
    } else if (*buf == 'S' && BOARD_VERSION(temp_board) == CURRENT_BOARD_VER) {
      if (sscanf(buf, "S %d %s %d ", &mnum, poster_name, &timestamp) == 3) {
        CREATE(memboard, struct board_memory, 1);
        MEMORY_READER_NAME(memboard) = strdup(poster_name);
        MEMORY_TIMESTAMP(memboard) = timestamp;

        if (poster_name[0] == '\0') {
          free(memboard);
        } else {
          if (BOARD_VERSION(temp_board) == CURRENT_BOARD_VER) {
            for (bmsg = BOARD_MESSAGES(temp_board), sflag = 0; bmsg && !sflag; bmsg = MESG_NEXT(bmsg)) {
              if (MESG_TIMESTAMP(bmsg) == MEMORY_TIMESTAMP(memboard) &&
                  (mnum == ((MESG_TIMESTAMP(bmsg) % 301 +
                             get_id_by_name(MESG_POSTER_NAME(bmsg)) % 301) % 301))) {
                sflag = 1;
              }
            }
          } else {
            for (bmsg = BOARD_MESSAGES(temp_board), sflag = 0; bmsg && !sflag; bmsg = MESG_NEXT(bmsg)) {
              if (MESG_TIMESTAMP(bmsg) == MEMORY_TIMESTAMP(memboard) &&
                  (mnum == ((MESG_TIMESTAMP(bmsg) % 301 +
                             MESG_POSTER(bmsg) % 301) % 301))) {
                sflag = 1;
              }
            }
          }

          if (sflag) {
            if (BOARD_MEMORY(temp_board, mnum)) {
              list = BOARD_MEMORY(temp_board, mnum);
              BOARD_MEMORY(temp_board, mnum) = memboard;
              MEMORY_NEXT(memboard) = list;
            } else {
              BOARD_MEMORY(temp_board, mnum) = memboard;
              MEMORY_NEXT(memboard) = NULL;
            }
          } else {
            free(memboard);
          }
        }
      }
    } else if (*buf == '#') {
      if (parse_message(fl, temp_board))
        msg_num++;
    }
  }

  fclose(fl);

if (temp_board) {
  if (msg_num != BOARD_MNUM(temp_board)) {
    log("Board [vnum: %d] message count (%d) not equal to actual message count (%d). Correcting.",
        BOARD_VNUM(temp_board), BOARD_MNUM(temp_board), msg_num);
    BOARD_MNUM(temp_board) = msg_num;
  }

  save_board(temp_board);
}
return temp_board;
}


int parse_message(FILE *fl, struct board_info *temp_board) {
  struct board_msg *tmsg, *t2msg;
  char subject[81];
  char buf[MAX_MESSAGE_LENGTH + 1], poster[128];

  CREATE(tmsg, struct board_msg, 1);

  if (BOARD_VERSION(temp_board) != CURRENT_BOARD_VER) {
    if (fscanf(fl, "%ld\n", &(MESG_POSTER(tmsg))) != 1 ||
        fscanf(fl, "%ld\n", &(MESG_TIMESTAMP(tmsg))) != 1) {
      log("Parse error in message for board [vnum: %d].  Skipping.", BOARD_VNUM(temp_board));
      free(tmsg);
      return 0;
    }
  } else {
    if (fscanf(fl, "%s\n", poster) != 1 ||
        fscanf(fl, "%ld\n", &(MESG_TIMESTAMP(tmsg))) != 1) {
      log("Parse error in message for board [vnum: %d].  Skipping.", BOARD_VNUM(temp_board));
      free(tmsg);
      return 0;
    }
    // Här måste vi strdup för att kopiera stackbuffer
    MESG_POSTER_NAME(tmsg) = strdup(poster);
  }

  get_line(fl, subject);
  MESG_SUBJECT(tmsg) = strdup(subject);
  MESG_DATA(tmsg) = fread_string(fl, buf);

  // Init länkar
  MESG_NEXT(tmsg) = NULL;
  MESG_PREV(tmsg) = NULL;

  // Lägg till sist i listan
  if (BOARD_MESSAGES(temp_board)) {
    t2msg = BOARD_MESSAGES(temp_board);
    while (MESG_NEXT(t2msg))
      t2msg = MESG_NEXT(t2msg);
    MESG_NEXT(t2msg) = tmsg;
    MESG_PREV(tmsg) = t2msg;
  } else {
    BOARD_MESSAGES(temp_board) = tmsg;
  }

  return 1;
}


/* Save a board to disk */
int save_board(struct board_info *ts) {
  struct board_msg *message;
  struct board_memory *memboard;
  FILE *fl;
  char buf[512];
  int i = 1;

  snprintf(buf, sizeof(buf), "%s/%d", BOARD_DIRECTORY, BOARD_VNUM(ts));

  if (!(fl = fopen(buf, "w"))) {
    log("Hm. Error while attempting to save board [vnum: %d]. Unable to create file '%s'.", BOARD_VNUM(ts), buf);
    return 0;
  }

  fprintf(fl, "Board File\n%d %d %d %d %d\n",
          READ_LVL(ts), WRITE_LVL(ts), REMOVE_LVL(ts), BOARD_MNUM(ts), CURRENT_BOARD_VER);

  for (message = BOARD_MESSAGES(ts); message; message = MESG_NEXT(message)) {
    if (BOARD_VERSION(ts) != CURRENT_BOARD_VER) {
      fprintf(fl, "#%d\n"
                  "%ld\n"
                  "%ld\n"
                  "%s\n"
                  "%s~\n",
              i++,
              (long)MESG_POSTER(message),
              MESG_TIMESTAMP(message),
              MESG_SUBJECT(message) ? MESG_SUBJECT(message) : "No Subject",
              MESG_DATA(message) ? MESG_DATA(message) : "");
    } else {
      fprintf(fl, "#%d\n"
                  "%s\n"
                  "%ld\n"
                  "%s\n"
                  "%s~\n",
              i++,
              MESG_POSTER_NAME(message) ? MESG_POSTER_NAME(message) : "Unknown",
              MESG_TIMESTAMP(message),
              MESG_SUBJECT(message) ? MESG_SUBJECT(message) : "No Subject",
              MESG_DATA(message) ? MESG_DATA(message) : "");
    }
  }

  /* Write memory */
  for (i = 0; i < 301; i++) {
    memboard = BOARD_MEMORY(ts, i);
    while (memboard) {
      if (BOARD_VERSION(ts) != CURRENT_BOARD_VER) {
        fprintf(fl, "S%d %ld %ld\n", i,
                (long)MEMORY_READER(memboard),
                MEMORY_TIMESTAMP(memboard));
      } else {
        fprintf(fl, "S%d %s %ld\n", i,
                MEMORY_READER_NAME(memboard) ? MEMORY_READER_NAME(memboard) : "Unknown",
                MEMORY_TIMESTAMP(memboard));
      }
      memboard = MEMORY_NEXT(memboard);
    }
  }

  fclose(fl);
  return 1;
}


/* Clear all boards in memory */
void clear_boards(void) {
  struct board_info *tmp, *next;
  for (tmp = bboards; tmp; tmp = next) {
    next = tmp->next;
    clear_one_board(tmp);
  }
}

/* Free all memory used by a single board */
void clear_one_board(struct board_info *tmp) {
  struct board_msg *m1, *m2;
  int i;

  for (m1 = BOARD_MESSAGES(tmp); m1; m1 = m2) {
    m2 = m1->next;
    free(m1->subject);
    free(m1->data);
    free(m1);
  }

  for (i = 0; i < 301; i++) {
    struct board_memory *mem1 = tmp->memory[i], *mem2;
    while (mem1) {
      mem2 = mem1->next;
      free(mem1);
      mem1 = mem2;
    }
  }

  free(tmp);
}

/* Locate a board in memory by VNUM */
struct board_info *locate_board(obj_vnum board_vnum) {
  struct board_info *b;
  for (b = bboards; b; b = b->next)
    if (BOARD_VNUM(b) == board_vnum)
      return b;
  return NULL;
}

/* Show the board index to the player */
void show_board(obj_vnum board_vnum, struct char_data *ch) {
  struct board_info *thisboard;
  struct board_msg *message;
  char *tmstr;
  int msgcount = 0, yesno = 0;
  char buf[MAX_STRING_LENGTH];
  char name[127];

  *buf = '\0';
  *name = '\0';

  if (IS_NPC(ch)) {
    send_to_char(ch, "Gosh.. now .. if only mobs could read.. you'd be doing good.\r\n");
    return;
  }

  thisboard = locate_board(board_vnum);
  if (!thisboard) {
    log("Creating new board - board #%d", board_vnum);
    thisboard = create_new_board(board_vnum);
    thisboard->next = bboards;
    bboards = thisboard;
  }

  if (GET_LEVEL(ch) < READ_LVL(thisboard)) {
    send_to_char(ch, "You try but fail to understand the holy words.\r\n");
    return;
  }

  snprintf(buf, sizeof(buf),
           "\tnThis is a bulletin board.\r\n"
           "\tnUsage: \tnREAD/REMOVE \tW<messg #>\tn, \tnRESPOND \tW<messg #>\tn, \tnWRITE \tW<header>\tn.\r\n");

  if (!BOARD_MNUM(thisboard) || !BOARD_MESSAGES(thisboard)) {
    strcat(buf, "The board is empty.\r\n");
    send_to_char(ch, "%s", buf);
    return;
  } else {
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
             "\tnThere \tn%s \tW%d\tn %s on the board.\r\n",
             (BOARD_MNUM(thisboard) == 1) ? "is" : "are",
             BOARD_MNUM(thisboard),
             (BOARD_MNUM(thisboard) == 1) ? "message" : "messages");
  }

  message = BOARD_MESSAGES(thisboard);
  if (PRF_FLAGGED(ch, PRF_VIEWORDER)) {
    while (MESG_NEXT(message))
      message = MESG_NEXT(message);
  }

  while (message) {
    tmstr = (char *)asctime(localtime(&MESG_TIMESTAMP(message)));
    *(tmstr + strlen(tmstr) - 1) = '\0';
    yesno = mesglookup(message, ch, thisboard);

    if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER)
      snprintf(name, sizeof(name), "%s", get_name_by_id(MESG_POSTER(message)));
    else
      snprintf(name, sizeof(name), "%s", MESG_POSTER_NAME(message));

    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
             "\tn[\tW%3s\tn] \tn(\tW%2d\tn) \tn: \tW%6.10s \tn(\tW%-10s\tn) \tn:: \tW%s\tn\r\n",
             yesno ? "---" : "\tYNEW\tn",
             ++msgcount,
             tmstr,
             CAP(name),
             MESG_SUBJECT(message) ? MESG_SUBJECT(message) : "No Subject");

    if (PRF_FLAGGED(ch, PRF_VIEWORDER))
      message = MESG_PREV(message);
    else
      message = MESG_NEXT(message);
  }

  page_string(ch->desc, buf, 1);
}


/* Display a specific message */
void board_display_msg(obj_vnum board_vnum, struct char_data *ch, int arg) {
  struct board_info *thisboard = bboards;
  struct board_msg *message;
  char *tmstr;
  int msgcount, mem, sflag;
  char name[127];
  struct board_memory *mboard_type, *list;
  char buf[MAX_STRING_LENGTH + 1];

  if (IS_NPC(ch)) {
    send_to_char(ch, "Silly mob - reading is for pcs!\r\n");
    return;
  }

  thisboard = locate_board(board_vnum);
  if (!thisboard) {
    log("Creating new board - board #%d", board_vnum);
    thisboard = create_new_board(board_vnum);
  }

  if (GET_LEVEL(ch) < READ_LVL(thisboard)) {
    send_to_char(ch, "You try but fail to understand the holy words.\r\n");
    return;
  }

  if (!BOARD_MESSAGES(thisboard)) {
    send_to_char(ch, "The board is empty!\r\n");
    return;
  }

  message = BOARD_MESSAGES(thisboard);

  if (arg < 1) {
    send_to_char(ch, "You must specify the (positive) number of the message to be read!\r\n");
    return;
  }

  if (PRF_FLAGGED(ch, PRF_VIEWORDER)) {
    while (MESG_NEXT(message))
      message = MESG_NEXT(message);
  }

  for (msgcount = arg; message && msgcount != 1; msgcount--) {
    if (PRF_FLAGGED(ch, PRF_VIEWORDER))
      message = MESG_PREV(message);
    else
      message = MESG_NEXT(message);
  }

  if (!message) {
    send_to_char(ch, "That message exists only in your imagination.\r\n");
    return;
  }

  if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER) {
    mem = ((MESG_TIMESTAMP(message) % 301) +
           (MESG_POSTER(message) % 301)) % 301;
  } else {
    mem = ((MESG_TIMESTAMP(message) % 301) +
           (get_id_by_name(MESG_POSTER_NAME(message)) % 301)) % 301;
  }

  CREATE(mboard_type, struct board_memory, 1);

  if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER) {
    MEMORY_READER(mboard_type) = GET_IDNUM(ch);
  } else {
    MEMORY_READER_NAME(mboard_type) = GET_NAME(ch);
  }

  MEMORY_TIMESTAMP(mboard_type) = MESG_TIMESTAMP(message);
  MEMORY_NEXT(mboard_type) = NULL;

  list = BOARD_MEMORY(thisboard, mem);
  sflag = 1;

  while (list && sflag) {
    if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER) {
      if (MEMORY_READER(list) == MEMORY_READER(mboard_type) &&
          MEMORY_TIMESTAMP(list) == MEMORY_TIMESTAMP(mboard_type)) {
        sflag = 0;
      }
    } else {
      if (MEMORY_READER_NAME(list) &&
          MEMORY_READER_NAME(mboard_type) &&
          !strcmp(MEMORY_READER_NAME(list), MEMORY_READER_NAME(mboard_type)) &&
          MEMORY_TIMESTAMP(list) == MEMORY_TIMESTAMP(mboard_type)) {
        sflag = 0;
      }
    }
    list = MEMORY_NEXT(list);
  }

  if (sflag) {
    list = BOARD_MEMORY(thisboard, mem);
    BOARD_MEMORY(thisboard, mem) = mboard_type;
    MEMORY_NEXT(mboard_type) = list;
  }

  tmstr = (char *)asctime(localtime(&MESG_TIMESTAMP(message)));
  *(tmstr + strlen(tmstr) - 1) = '\0';

  if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER)
    snprintf(name, sizeof(name), "%s", get_name_by_id(MESG_POSTER(message)));
  else
    snprintf(name, sizeof(name), "%s", MESG_POSTER_NAME(message));

  snprintf(buf, sizeof(buf),
          "\tnMessage \tW%d \tn: \tW%6.10s \tn(\tW%s\tn) \tn:: \tW%s\r\n\r\n%s\r\n\tn",
          arg,
          tmstr,
          CAP(name),
          MESG_SUBJECT(message) ? MESG_SUBJECT(message) : "\tWNo Subject\tn",
          MESG_DATA(message) ? MESG_DATA(message) : "Looks like this message is empty.");

  page_string(ch->desc, buf, 1);

  if (sflag) {
    save_board(thisboard);
  }

  return;
}


/* Start writing a new message */
void write_board_message(obj_vnum board_vnum, struct char_data *ch, char *arg)
{
  struct board_info *thisboard;
  struct board_msg *message;

  if (IS_NPC(ch)) {
    send_to_char(ch, "Orwellian police thwart your attempt at free speech.\r\n");
    return;
  }

  thisboard = locate_board(board_vnum);

  if (!thisboard) {
    send_to_char(ch, "Error: Your board could not be found. Please report.\r\n");
    mudlog(BRF, LVL_IMPL, TRUE, "Error in write_board_message: board #%d not found.", board_vnum);
    return;
  }

  if (GET_LEVEL(ch) < WRITE_LVL(thisboard)) {
    send_to_char(ch, "You are not holy enough to write on this board.\r\n");
    return;
  }

  if (!*arg)
    arg = "No Subject";

  skip_spaces(&arg);
  delete_doubledollar(arg);
  arg[81] = '\0';

  CREATE(message, struct board_msg, 1);
    if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER) {
      MESG_POSTER(message) = GET_IDNUM(ch);
    } else {
      MESG_POSTER_NAME(message) = strdup(GET_NAME(ch));
    }
  MESG_TIMESTAMP(message) = time(0);
  MESG_SUBJECT(message) = strdup(arg);
  MESG_DATA(message) = NULL;
  MESG_NEXT(message) = BOARD_MESSAGES(thisboard);
  MESG_PREV(message) = NULL;

  if (BOARD_MESSAGES(thisboard))
    MESG_PREV(BOARD_MESSAGES(thisboard)) = message;

  BOARD_MESSAGES(thisboard) = message;
  BOARD_MNUM(thisboard) = MAX(BOARD_MNUM(thisboard) + 1, 1);

  send_to_char(ch, "Write your message. (/s saves, /h for help)\r\n");
  act("$n starts to write a message.", TRUE, ch, 0, 0, TO_ROOM);

  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  string_write(ch->desc, &(MESG_DATA(message)),
               MAX_MESSAGE_LENGTH, board_vnum + BOARD_MAGIC, NULL);
}


/* Respond to a message */
void board_respond(obj_vnum board_vnum, struct char_data *ch, int mnum) {
  struct board_info *thisboard = locate_board(board_vnum);
  struct board_msg *message, *other;
  char buf[MAX_STRING_LENGTH];
  int gcount = 0;

  if (!thisboard) {
    send_to_char(ch, "Error: Your board could not be found. Please report.\r\n");
    mudlog(BRF, LVL_IMPL, TRUE, "Error in board_respond: board #%d not found.", board_vnum);
    return;
  }

  if (GET_LEVEL(ch) < WRITE_LVL(thisboard)) {
    send_to_char(ch, "You are not holy enough to write on this board.\r\n");
    return;
  }

  if (GET_LEVEL(ch) < READ_LVL(thisboard)) {
    send_to_char(ch, "You are not holy enough to respond to this board.\r\n");
    return;
  }

  if (mnum < 1 || mnum > BOARD_MNUM(thisboard)) {
    send_to_char(ch, "You can only respond to an actual message.\r\n");
    return;
  }

  other = BOARD_MESSAGES(thisboard);
  for (gcount = 1; other && gcount < mnum; gcount++)
    other = MESG_NEXT(other);

  if (!other) {
    send_to_char(ch, "That message does not exist.\r\n");
    return;
  }

  CREATE(message, struct board_msg, 1);
  if (BOARD_VERSION(thisboard) != CURRENT_BOARD_VER) {
    MESG_POSTER(message) = GET_IDNUM(ch);
  } else {
    MESG_POSTER_NAME(message) = strdup(GET_NAME(ch));
  }
  MESG_TIMESTAMP(message) = time(0);
  snprintf(buf, sizeof(buf), "Re: %s", MESG_SUBJECT(other));
  MESG_SUBJECT(message) = strdup(buf);
  MESG_DATA(message) = NULL;
  MESG_NEXT(message) = BOARD_MESSAGES(thisboard);
  MESG_PREV(message) = NULL;

  if (BOARD_MESSAGES(thisboard))
    MESG_PREV(BOARD_MESSAGES(thisboard)) = message;

  BOARD_MESSAGES(thisboard) = message;
  BOARD_MNUM(thisboard)++;

  send_to_char(ch, "Write your message. (/s saves, /h for help)\r\n\r\n");
  act("$n starts to write a message.", TRUE, ch, 0, 0, TO_ROOM);

  snprintf(buf, sizeof(buf), "\t------- Quoted message -------\r\n%s\t------- End Quote -------\r\n", MESG_DATA(other));
  write_to_output(ch->desc, "%s", buf);

  string_write(ch->desc, &(MESG_DATA(message)), MAX_MESSAGE_LENGTH, board_vnum + BOARD_MAGIC, NULL);
}


/* Remove a message */
void remove_board_msg(obj_vnum board_vnum, struct char_data *ch, int msg_num) {
  struct board_info *b = locate_board(board_vnum);
  struct board_msg *msg;
  int i = 1;
  char buf[MAX_STRING_LENGTH];

  if (IS_NPC(ch)) {
    send_to_char(ch, "Nuts.. looks like you forgot your eraser back in mobland...\r\n");
    return;
  }

  if (!b) {
    send_to_char(ch, "This board doesn't exist.\r\n");
    mudlog(BRF, LVL_IMPL, TRUE, "Error in remove_board_msg: board #%d not found.", board_vnum);
    return;
  }

  /* Support PRF_VIEWORDER */
  if (PRF_FLAGGED(ch, PRF_VIEWORDER))
    msg_num = b->num_messages - msg_num + 1;

  for (msg = b->messages; msg; msg = msg->next, i++) {
    if (i == msg_num) {
      if (GET_IDNUM(ch) != msg->poster && GET_LEVEL(ch) < REMOVE_LVL(b)) {
        send_to_char(ch, "You are not allowed to remove other people's messages.\r\n");
        return;
      }

      /* Check if someone is editing */
      struct descriptor_data *d;
      for (d = descriptor_list; d; d = d->next) {
        if (d->connected >= BOARD_MAGIC && d->str == &msg->data) {
          send_to_char(ch, "Wait until the author finishes writing.\r\n");
          return;
        }
      }

      /* Remove message */
      if (msg->prev)
        msg->prev->next = msg->next;
      if (msg->next)
        msg->next->prev = msg->prev;
      if (b->messages == msg)
        b->messages = msg->next;

      free(msg->subject);
      free(msg->data);
      free(msg);
      b->num_messages--;

      send_to_char(ch, "Message removed.\r\n");
      sprintf(buf, "$n just removed message %d.", msg_num);
      act(buf, FALSE, ch, 0, 0, TO_ROOM);
      save_board(b);
      return;
    }
  }

  send_to_char(ch, "No such message.\r\n");
}


/* Mark message as read */
int mesglookup(struct board_msg *message, struct char_data *ch, struct board_info *board) {
  int mem = 0;
  struct board_memory *mboard_type;

  // Räkna ut hash beroende på boards version
  if (BOARD_VERSION(board) != CURRENT_BOARD_VER) {
    mem = ( (MESG_TIMESTAMP(message) % 301) + 
            (MESG_POSTER(message) % 301) ) % 301;
  } else {
    mem = ( (MESG_TIMESTAMP(message) % 301) + 
            (get_id_by_name(MESG_POSTER_NAME(message)) % 301) ) % 301;
  }

  mboard_type = BOARD_MEMORY(board, mem);

  // Gammalt boards-format: matcha på IDNUM
  if (BOARD_VERSION(board) != CURRENT_BOARD_VER) {
    while (mboard_type) {
      if (MEMORY_READER(mboard_type) == GET_IDNUM(ch) &&
          MEMORY_TIMESTAMP(mboard_type) == MESG_TIMESTAMP(message)) {
        return 1;
      }
      mboard_type = MEMORY_NEXT(mboard_type);
    }
  } else {
    // Nytt boards-format: matcha på namn
    const char *ch_name = GET_NAME(ch);
    while (mboard_type) {
      if (MEMORY_READER_NAME(mboard_type) &&
          ch_name &&
          !strcmp(MEMORY_READER_NAME(mboard_type), ch_name) &&
          MEMORY_TIMESTAMP(mboard_type) == MESG_TIMESTAMP(message)) {
        return 1;
      }
      mboard_type = MEMORY_NEXT(mboard_type);
    }
  }

  return 0;
}


/* Show all boards (for debugging) */
void look_at_boards(void)
{
  int counter = 0, messages = 0;
  struct board_info *tboard = bboards;
  struct board_msg *msg;

  for (; tboard; tboard = BOARD_NEXT(tboard)) {
    counter++;
    msg = BOARD_MESSAGES(tboard);
    while (msg) {
      messages++;
      msg = MESG_NEXT(msg);
    }
  }

  log("There are %d boards located; %d messages", counter, messages);
}

