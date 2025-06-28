#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <time.h>
#include <stdbool.h>

#define MAX_CHARS_PER_ACCOUNT 10
#define ACCOUNT_NAME_LENGTH 32
#define ACCOUNT_PASS_LENGTH 128
#define ACCOUNT_EMAIL_LENGTH 64
#define ACCOUNT_FILE_PATH "../lib/accounts/"  // ensure this exists

struct account_data {
  char name[ACCOUNT_NAME_LENGTH];
  char password[ACCOUNT_PASS_LENGTH]; // Use hashed password
  char email[ACCOUNT_EMAIL_LENGTH];
  char *characters[MAX_CHARS_PER_ACCOUNT]; // NULL-terminated
  int num_characters;
  time_t created;
  time_t last_login;
  bool siteok;
  int max_online; // Max simultaneous players allowed
};

struct descriptor_data;

struct account_data *create_account(const char *name, const char *password);
int save_account(struct account_data *account);
struct account_data *load_account(const char *name);
void free_account(struct account_data *account);
int account_add_character(struct account_data *account, const char *charname);
int account_has_character(struct account_data *account, const char *charname);
void display_account_menu(struct descriptor_data *d);
// Verifies if a given password matches the account's stored (hashed) password
int verify_password(struct account_data *account, const char *password);
// Updates the last login time to the current time
void update_last_login(struct account_data *account);

#endif
