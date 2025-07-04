/***************************************************************************
*  File: itemmail.c                                       Part of tbaMUD   *
*                                                                         *
*  Usage: Functions for handling item mail (sending and storing objects) *
*                                                                         *
*  Copyright (C) 2024 by Eko. All rights reserved.                        *
*                                                                         *
*  Description:                                                           *
*  - Provides functions to save and later retrieve mailed objects        *
*  - Items are stored with unique IDs to ensure authenticity             *
*  - Data is saved in lib/plritemmail                                     *
*                                                                         *
*  Dependencies:                                                          *
*  - structs.h, utils.h, mail.h, etc.                                     *
*                                                                         *
***************************************************************************/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "config.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "handler.h"
#include "mail.h"
#include "itemmail.h"


/*
 * Save a mailed item to disk.
 */
void store_item_mail(struct item_mail_entry *entry) {
char filename[MAX_INPUT_LENGTH];
snprintf(filename, sizeof(filename), "%s%ld", LIB_ITEMMAIL, entry->recipient_id);

FILE *fp = fopen(filename, "ab");
if (!fp) {
  perror("store_item_mail: cannot open itemmail file");
  return;
}
fwrite(entry, sizeof(struct item_mail_entry), 1, fp);
fclose(fp);
}

/*
 * Check if a player has any item mail.
 * Returns TRUE if there is item mail for the given idnum.
 */
bool has_item_mail(long idnum) {
  char filename[MAX_INPUT_LENGTH];
  snprintf(filename, sizeof(filename), "%s%ld", LIB_ITEMMAIL, idnum);

  FILE *fp = fopen(filename, "rb");
  if (fp) {
    fclose(fp);
    return TRUE;
  }
  return FALSE;
}


/*
 * Command to mail an item to another player.
 */
ACMD(do_mailitem) {
  char target_name[MAX_INPUT_LENGTH], item_name[MAX_INPUT_LENGTH];
  struct char_data *recipient = NULL;
  struct obj_data *obj;
  struct item_mail_entry entry;
  long idnum = -1;
  /* Calculate postage cost */
  int base_cost = 10;   /* Base price */
  int per_weight = 2;   /* Price per weight unit */

  /* Parse arguments: mailitem <player> <item> */
  two_arguments(argument, target_name, item_name);

  if (!*target_name || !*item_name) {
    send_to_char(ch, "Usage: mailitem <player> <item>\r\n");
    return;
  }

  /* Look for recipient online first */
  if ((recipient = get_player_vis(ch, target_name, NULL, FIND_CHAR_WORLD)) != NULL) {
    idnum = GET_IDNUM(recipient);
  } else if ((idnum = get_id_by_name(target_name)) < 0) {
    send_to_char(ch, "No such player.\r\n");
    return;
  }

  /* Find the object */
  if (!(obj = get_obj_in_list_vis(ch, item_name, NULL, ch->carrying))) {
    send_to_char(ch, "You don't have that item.\r\n");
    return;
  }

    /* Check if item is valid to mail */
  if (OBJ_FLAGGED(obj, ITEM_NOMAIL)) {
    send_to_char(ch, "You cannot mail that item.\r\n");
    return;
  }

  /* Calculate Price based on weight */
  int postage = base_cost + (GET_OBJ_WEIGHT(obj) * per_weight);

  /* Check weight limit */
  if (GET_OBJ_WEIGHT(obj) > max_mail_weight) {
    send_to_char(ch, "That item is too heavy to mail.\r\n");
    return;
  }

  /* Check if the item type is allowed */
  if (ILLEGAL_MAIL_OBJ(obj)) {
    send_to_char(ch, "You can't mail containers, food, or drink.\r\n");
    return;
  }

  if (GET_GOLD(ch) < postage) {
    send_to_char(ch, "It would cost you %d gold coins to mail that, but you don't have enough.\r\n", postage);
    return;
  }

  /* Fill item mail entry */
  entry.recipient_id = idnum;
  entry.sender_id = GET_IDNUM(ch);
  entry.unique_id = obj->unique_id;
  entry.vnum = GET_OBJ_VNUM(obj);
  entry.date_sent = time(0);
  entry.message[0] = '\0'; /* Empty message for now */

  /* Store the entry */
  store_item_mail(&entry);

  /* Remove the item from the player */
  obj_from_char(obj);
  extract_obj(obj);
  GET_GOLD(ch) -= postage;
  send_to_char(ch, "You have successfully mailed the item.\r\n");
  send_to_char(ch, "You pay %d gold coins in postage.\r\n", postage);
}

/*
 * Command to collect all mailed items addressed to the player.
 */
ACMD(do_collectitem) {
  FILE *fp;
  struct item_mail_entry entry;
  bool found = FALSE;
  char filename[MAX_INPUT_LENGTH];

  snprintf(filename, sizeof(filename), "%s%ld", LIB_ITEMMAIL, GET_IDNUM(ch));

  fp = fopen(filename, "rb");
  if (!fp) {
    send_to_char(ch, "You have no mailed items.\r\n");
    return;
  }

  while (fread(&entry, sizeof(struct item_mail_entry), 1, fp)) {
    /* Create the object */
    struct obj_data *obj = read_object(entry.vnum, VIRTUAL);
    if (!obj) {
      send_to_char(ch, "One of your mailed items could not be loaded.\r\n");
      continue;
    }

    /* Assign unique_id */
    obj->unique_id = entry.unique_id;

    /* Create parcel box */
    struct obj_data *parcel = read_object(3016, VIRTUAL);
    if (!parcel) {
      send_to_char(ch, "Parcel object missing (vnum 3016).\r\n");
      extract_obj(obj);
      continue;
    }

/* Attach extra description label */
struct extra_descr_data *new_descr;
CREATE(new_descr, struct extra_descr_data, 1);

char buf[MAX_STRING_LENGTH];
snprintf(buf, sizeof(buf),
  "This parcel has a small label attached.\r\n"
  "It reads:\r\n"
  "  To: %s\r\n"
  "  From: %s\r\n"
  "  Message: %s",
  GET_NAME(ch),
  get_name_by_id(entry.sender_id),
  *entry.message ? entry.message : "(none)"
);

  //parcel->action_description = strdup(buf);
  new_descr->keyword = strdup("parcel label tag");
  new_descr->description = strdup(buf);
  new_descr->next = parcel->ex_description;
  parcel->ex_description = new_descr;

  /* Keep standard long description for when it lies in the room */
  parcel->description = strdup("A sturdy parcel wrapped in twine has been left here.");



    /* Put item in parcel */
    obj_to_obj(obj, parcel);

    /* Give parcel to player */
    obj_to_char(parcel, ch);

    send_to_char(ch, "You collect a parcel addressed to you.\r\n");
    found = TRUE;
  }

  fclose(fp);

  /* Remove file after collecting */
  remove(filename);

  if (!found) {
    send_to_char(ch, "You have no mailed items.\r\n");
  }
}

