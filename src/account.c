/*=============================================================================
 *  Account Management System - JSON-Based
 *
 *  This module handles loading, saving, and managing account data for players.
 *  Account data is stored in JSON format for easy extensibility and clarity.
 *
 *  Features:
 *    - Account creation and initialization
 *    - Secure password storage with hashing
 *    - Character linking to accounts
 *    - Shared chest slot support (JSON-ready)
 *    - JSON-based serialization using cJSON
 *
 *  Author: Eko
 *  License: See tbamud-playground LICENSE
 *============================================================================*/

#include "account.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef CRYPT
#define CRYPT crypt
#endif

// Returns the full file path to the JSON account file based on account name
static char *account_filename(const char *name) {
  static char path[256];
  snprintf(path, sizeof(path), "%s%s.json", ACCOUNT_FILE_PATH, name);
  return path;
}

// Allocates and initializes a new account with the provided name and password
struct account_data *create_account(const char *name, const char *password) {
  struct account_data *account = calloc(1, sizeof(struct account_data));
  if (!account) return NULL;

  strncpy(account->name, name, ACCOUNT_NAME_LENGTH - 1);
  strncpy(account->password, CRYPT(password, account->name), ACCOUNT_PASS_LENGTH - 1);
  account->password[ACCOUNT_PASS_LENGTH - 1] = '\0';
  account->created = time(0);
  account->last_login = account->created;
  account->siteok = true;
  account->max_online = 2;
  return account;
}

// Saves the given account to disk in JSON format. Returns 1 on success, 0 on failure.
int save_account(struct account_data *acc) {
  const char *filename = account_filename(acc->name);
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "name", acc->name);
  cJSON_AddStringToObject(json, "password", acc->password);
  cJSON_AddStringToObject(json, "email", acc->email);
  cJSON_AddNumberToObject(json, "created", (double)acc->created);
  cJSON_AddNumberToObject(json, "last_login", (double)acc->last_login);
  cJSON_AddBoolToObject(json, "siteok", acc->siteok);
  cJSON_AddNumberToObject(json, "max_online", acc->max_online);

  cJSON *chars = cJSON_CreateArray();
  for (int i = 0; i < acc->num_characters; i++) {
    cJSON_AddItemToArray(chars, cJSON_CreateString(acc->characters[i]));
  }
  cJSON_AddItemToObject(json, "characters", chars);

  cJSON_AddNumberToObject(json, "shared_chest_slots", acc->shared_chest_slots);
  // TODO: Add shared_chest serialization later

  char *out = cJSON_PrintBuffered(json, 2048, 1);
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    cJSON_Delete(json);
    free(out);
    return 0;
  }
  fputs(out, fp);
  fclose(fp);

  cJSON_Delete(json);
  free(out);
  return 1;
}

// Loads and returns an account from disk in JSON format. Returns NULL on failure.
struct account_data *load_account(const char *name) {
  FILE *fp = fopen(account_filename(name), "r");
  if (!fp) return NULL;

  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *data = malloc(len + 1);
  if (fread(data, 1, len, fp) != len) {
    free(data);
    fclose(fp);
    return NULL;
  }
  data[len] = '\0';
  fclose(fp);

  cJSON *json = cJSON_Parse(data);
  free(data);
  if (!json) return NULL;

  struct account_data *acc = calloc(1, sizeof(struct account_data));
  strncpy(acc->name, cJSON_GetObjectItem(json, "name")->valuestring, ACCOUNT_NAME_LENGTH - 1);
  strncpy(acc->password, cJSON_GetObjectItem(json, "password")->valuestring, ACCOUNT_PASS_LENGTH - 1);
  strncpy(acc->email, cJSON_GetObjectItem(json, "email")->valuestring, ACCOUNT_EMAIL_LENGTH - 1);
  acc->created = (time_t)cJSON_GetObjectItem(json, "created")->valuedouble;
  acc->last_login = (time_t)cJSON_GetObjectItem(json, "last_login")->valuedouble;
  acc->siteok = cJSON_IsTrue(cJSON_GetObjectItem(json, "siteok"));
  acc->max_online = cJSON_GetObjectItem(json, "max_online")->valueint;

  cJSON *chars = cJSON_GetObjectItem(json, "characters");
  cJSON *ch;
  int i = 0;
  cJSON_ArrayForEach(ch, chars) {
    if (i >= MAX_CHARS_PER_ACCOUNT) break;
    acc->characters[i++] = strdup(ch->valuestring);
  }
  acc->num_characters = i;

  cJSON *slots = cJSON_GetObjectItem(json, "shared_chest_slots");
  acc->shared_chest_slots = slots ? slots->valueint : 0;

  cJSON_Delete(json);
  return acc;
}

// Frees memory allocated for the account structure and character strings
void free_account(struct account_data *acc) {
  if (!acc) return;
  for (int i = 0; i < acc->num_characters; i++)
    free(acc->characters[i]);
  free(acc);
}

// Adds a character name to the account if below max limit. Returns 1 if successful.
int account_add_character(struct account_data *acc, const char *charname) {
  if (acc->num_characters >= MAX_CHARS_PER_ACCOUNT)
    return 0;
  acc->characters[acc->num_characters++] = strdup(charname);
  return 1;
}

// Returns 1 if the given character is part of this account
int account_has_character(struct account_data *acc, const char *charname) {
  for (int i = 0; i < acc->num_characters; i++)
    if (!strcasecmp(acc->characters[i], charname))
      return 1;
  return 0;
}

// Verifies if the given password matches the stored account password
int verify_password(struct account_data *acc, const char *password) {
  if (!acc || !password) return 0;
  char *hashed = CRYPT(password, acc->name);
  return (hashed && !strcmp(hashed, acc->password));
}

// Updates the last login time for the account to the current time
void update_last_login(struct account_data *acc) {
  if (acc)
    acc->last_login = time(0);
}

