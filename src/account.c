#include "account.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>  // for crypt()

#ifndef CRYPT
#define CRYPT crypt
#endif

// Returns the full path to the account file for a given account name.
static char *account_filename(const char *name) {
  static char path[256];
  snprintf(path, sizeof(path), "%s%s.acct", ACCOUNT_FILE_PATH, name);
  return path;
}

// Creates a new account struct and initializes it with the given name and password.
struct account_data *create_account(const char *name, const char *password) {
  struct account_data *account = calloc(1, sizeof(struct account_data));
  if (!account) return NULL;

  strncpy(account->name, name, ACCOUNT_NAME_LENGTH - 1);
  strncpy(account->password, CRYPT(password, account->name), ACCOUNT_PASS_LENGTH - 1);
  account->password[ACCOUNT_PASS_LENGTH - 1] = '\0';  // safety  
  account->created = time(0);
  account->last_login = account->created;
  account->siteok = true;
  account->max_online = 2; // Default to 2 simultaneous logins
  return account;
}

// Saves account data to disk. Returns 1 on success, 0 on failure.
int save_account(struct account_data *account) {
  const char *filename = account_filename(account->name);  // <-- create filename first
  FILE *fp = fopen(filename, "w");

  if (!fp) {
    perror("save_account fopen error");
    fprintf(stderr, "Tried to open: %s\n", filename);
    return 0;
  }

  // Write each field to the file in key-value format
  fprintf(fp, "Name %s\n", account->name);
  fprintf(fp, "Pass %s\n", account->password);
  fprintf(fp, "Email %s\n", account->email);
  fprintf(fp, "Created %ld\n", (long)account->created);
  fprintf(fp, "LastLogin %ld\n", (long)account->last_login);
  fprintf(fp, "Siteok %s\n", account->siteok ? "true" : "false");
  fprintf(fp, "MaxOnline %d\n", account->max_online);
  for (int i = 0; i < account->num_characters; i++) {
    fprintf(fp, "Char %s\n", account->characters[i]);
  }

  fclose(fp);
  return 1;
}

// Loads account data from disk. Returns a pointer to the account struct, or NULL if failed.
struct account_data *load_account(const char *name) {
  FILE *fp = fopen(account_filename(name), "r");
  if (!fp) return NULL;

  struct account_data *account = calloc(1, sizeof(struct account_data));
  if (!account) return NULL;

  char line[256], key[32], value[224];
  while (fgets(line, sizeof(line), fp)) {
    if (sscanf(line, "%31s %223[^\n]", key, value) != 2) continue;

    if (!strcmp(key, "Name")) {
      strncpy(account->name, value, ACCOUNT_NAME_LENGTH - 1);
      account->name[ACCOUNT_NAME_LENGTH - 1] = '\0';
    }
    else if (!strcmp(key, "Pass")) {
      strncpy(account->password, value, ACCOUNT_PASS_LENGTH - 1);
      account->password[ACCOUNT_PASS_LENGTH - 1] = '\0';
    }
    else if (!strcmp(key, "Email")) {
      strncpy(account->email, value, ACCOUNT_EMAIL_LENGTH - 1);
      account->email[ACCOUNT_EMAIL_LENGTH - 1] = '\0';
    }    
    else if (!strcmp(key, "Created"))
      account->created = atol(value);
    else if (!strcmp(key, "LastLogin"))
      account->last_login = atol(value);
    else if (!strcmp(key, "Siteok"))
      account->siteok = (!strcasecmp(value, "true") || !strcmp(value, "1"));
    else if (!strcmp(key, "MaxOnline"))
      account->max_online = atoi(value);  
    else if (!strcmp(key, "Char")) {
      if (account->num_characters < MAX_CHARS_PER_ACCOUNT) {
        account->characters[account->num_characters++] = strdup(value);
      }
    }
  }

  fclose(fp);
  return account;
}

// Frees all memory associated with an account struct
void free_account(struct account_data *account) {
  if (!account) return;
  for (int i = 0; i < account->num_characters; i++)
    free(account->characters[i]);
  free(account);
}

// Adds a character to the account if under the character limit. Returns 1 if successful.
int account_add_character(struct account_data *account, const char *charname) {
  if (account->num_characters >= MAX_CHARS_PER_ACCOUNT)
    return 0;
  account->characters[account->num_characters++] = strdup(charname);
  return 1;
}

// Checks if a character is associated with this account. Case-insensitive.
int account_has_character(struct account_data *account, const char *charname) {
  for (int i = 0; i < account->num_characters; i++)
    if (!strcasecmp(account->characters[i], charname))
      return 1;
  return 0;
}

// Verifies that the given password matches the stored account password.
// Returns 1 if the password is correct, 0 otherwise.
int verify_password(struct account_data *account, const char *password) {
  if (!account || !password) return 0;

  char *hashed = CRYPT(password, account->name);
  return (hashed && !strcmp(hashed, account->password));
}

// Updates the account's last login timestamp to the current time.
void update_last_login(struct account_data *account) {
  if (account)
    account->last_login = time(0);
}