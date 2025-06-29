/***************************************************************************
*  File: itemmail.h                                       Part of tbaMUD   *
*                                                                         *
*  Usage: Data structures and prototypes for item mail support           *
*                                                                         *
*  Copyright (C) 2024 by Eko. All rights reserved.                        *
*                                                                         *
*  Description:                                                           *
*  - Defines the item_mail_entry struct                                   *
*  - Declares functions for storing mailed objects                       *
*                                                                         *
***************************************************************************/


#ifndef ITEMMAIL_H
#define ITEMMAIL_H


struct item_mail_entry {
  long recipient_id;
  long sender_id;
  long unique_id;
  int vnum;
  time_t date_sent;
  char message[256];
};

void store_item_mail(struct item_mail_entry *entry);
bool has_item_mail(long idnum);

ACMD(do_mailitem);
ACMD(do_collectitem);

/*
 * Macro to check if an object is not allowed to be mailed.
 */
#define ILLEGAL_MAIL_OBJ(obj) \
  (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || \
   GET_OBJ_TYPE(obj) == ITEM_FOOD || \
   GET_OBJ_TYPE(obj) == ITEM_DRINKCON)

#endif /* ITEMMAIL_H */
